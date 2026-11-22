// hooks_loc.cpp
#include "hooks_loc.h"
#include "lois.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "MinHook.h"
#include "tech_brain.h"
#include "scenario.h"
#include "scooting.h"
#include "ap_data.h"
#include "groupes.h"
#include "evenements.h"
#include "quetes.h"        // QuetesTexteObjectifAP (objectifs valides par exploration)
#include "archipelago.h"   // ApNomDuCheckSite (nom des sites sur la carte)


static volatile bool     g_logAll = false;
static volatile DWORD    g_logUntil = 0;
static const int SEEN_MAX = 2048;
static char  g_seen[SEEN_MAX][256];
static int   g_seenCount = 0;
static int   seenTxtN = 0;
void LogLocKey(const char* key);


static volatile bool  g_silent = false;   // amorcage : enregistre sans afficher


typedef uintptr_t (*Translate_t)(uintptr_t loc, const char* key,
                                 uintptr_t p3, uintptr_t p4);
typedef uintptr_t(*HashMapLookup_t)(uintptr_t map, const char** key);
static HashMapLookup_t oHashMapLookup = nullptr;
static Translate_t oTranslate = nullptr;

volatile bool g_apTexteActif = true;
// --- Cache : un buffer "<nom> [Researched]" par segment de cle ---------------
struct NameCache {
    char    key[192];       // cle complete (2026-11-11 : les cles "Generated/.../[Builders] ..." depassent 95 caracteres)
    uint8_t entete[0x14];   // en-tete de chaine du moteur (regles 2 et 3), juste avant buf
    wchar_t buf[128];
    bool    used;
};
static NameCache g_cache[256];
static int       g_cacheCount = 0;

// Regles 2 et 3 (correctif du 2026-11-10, crashs c0000374) : un texte que le
// jeu va detenir (pointeur ecrit dans SON objet de traduction) doit etre une
// chaine du moteur : en-tete de 0x14 octets avant les caracteres, self en
// -0x0C, compteur en -0x04. Sans lui, le jeu comptait sur les 4 octets
// precedant le texte (nuls) et, a 0, liberait notre tampon STATIQUE
// (_aligned_free) : tas corrompu (releve [CRASH] : rafraichissement d'un
// noeud de l'arbre des technos). En-tete copie d'une chaine valide, self
// pose, compteur fige a 0x40000000 : jamais liberee.
static bool PoserEnteteFigee(uint8_t* entete, uintptr_t modele)
{
    if (modele <= 0x10000) return false;
    uintptr_t self = 0;
    if (!SafeRead((void*)(modele - 0x0C), &self, 8) || self != modele - 0x14) return false;
    if (!SafeRead((void*)(modele - 0x14), entete, 0x14)) return false;
    uintptr_t debut = (uintptr_t)entete;
    memcpy(entete + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(entete + 0x10, &refs, 4);
    return true;
}

struct KeyOverride { const char* key; const char* text; };

static const KeyOverride g_overrides[] = {
    { "UI/Menu/Characters/Desmond2", "Mère de la p'tite Clara, cette grognasse" },
    // { "UI/OtherPanels/TechnologyHUD/ButtonStart", "Envoyer" },
};
static const int g_overrideCount =
(int)(sizeof(g_overrides) / sizeof(g_overrides[0]));

static const int OV_MAX = 32;
struct OvBloc { uint8_t entete[0x14]; wchar_t buf[256]; };   // en-tete juste avant buf
static OvBloc  g_ovBlocs[OV_MAX];
static bool    g_ovBuilt[OV_MAX];

static bool KeyEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}


// Renvoie true si la cle a ete remplacee (et le pointeur redirige).
static bool ApplyOverride(const char* k, uintptr_t ret)
{
    for (int i = 0; i < g_overrideCount && i < OV_MAX; ++i) {
        if (!KeyEq(k, g_overrides[i].key)) continue;

        if (!g_ovBuilt[i]) {                       // conversion ASCII -> UTF-16
            uintptr_t chars = 0;
            if (!SafeRead((void*)ret, &chars, 8) || !PoserEnteteFigee(g_ovBlocs[i].entete, chars)) return false;
            const char* s = g_overrides[i].text;
            int j = 0;
            for (; s[j] && j < 255; ++j) g_ovBlocs[i].buf[j] = (wchar_t)(unsigned char)s[j];
            g_ovBlocs[i].buf[j] = 0;
            g_ovBuilt[i] = true;
        }

        uintptr_t p = (uintptr_t)g_ovBlocs[i].buf;
        if (SafeWrite((void*)ret, &p, 8)) {
            printf("[OVERRIDE] %s -> \"%s\"\n", k, g_overrides[i].text);
            return true;
        }
        return false;
    }
    return false;
}


// Construit (une seule fois par techno) le nom suffixe et renvoie le buffer.
static wchar_t* GetResearchedName(const char* seg, uintptr_t origChars)
{
    for (int i = 0; i < g_cacheCount; ++i)
        if (g_cache[i].used && StrEq(g_cache[i].key, seg)) return g_cache[i].buf;

    if (g_cacheCount >= 256) return nullptr;
    NameCache* c = &g_cache[g_cacheCount];
    if (!PoserEnteteFigee(c->entete, origChars)) return nullptr;   // pas d'en-tete valide : texte du jeu
    ++g_cacheCount;

    int s = 0; for (; seg[s] && s < 191; ++s) c->key[s] = seg[s]; c->key[s] = 0;

    int i = 0;
    for (; i < 100; ++i) {
        wchar_t ch = 0;
        if (!SafeRead((void*)(origChars + i * 2), &ch, 2) || ch == 0) break;
        c->buf[i] = ch;
    }
    const wchar_t* suffix = L" [Researched]";
    for (int j = 0; suffix[j] && i < 126; ++j, ++i) c->buf[i] = suffix[j];
    c->buf[i] = 0;
    c->used = true;
    return c->buf;
}

static void DumpClesSurveillees(const char* k, uintptr_t ret)
{
    static const char* cles[] = {
        "Expeditions/EffectBaseResources",
        "Expeditions/DefaultSeparator",
        "Expeditions/DefaultComma",
        "Expeditions/SiteName",
        "Expeditions/SitesDiscovered",
        "Resources/Names/Steel",
        "Resources/Names/Wood",
    };
    static bool vues[7] = {};

    for (int i = 0; i < 7; ++i) {
        if (vues[i]) continue;
        bool eq = true;
        for (int j = 0; ; ++j) {
            if (cles[i][j] != k[j]) { eq = false; break; }
            if (k[j] == 0) break;
        }
        if (!eq) continue;
        vues[i] = true;
        DumpWide(k, ret);
        return;
    }
}

// --- Objet de traduction fabrique -------------------------------------------
// Le jeu renvoie un objet statique PARTAGE pour les cles absentes : y ecrire
// le corrompt. On clone donc la structure d'une traduction valide une fois
// pour toutes, et on renvoie notre propre copie.
static uint8_t  g_apLoc[0x100];
static bool     g_apLocPret = false;
static wchar_t  g_apTexte[4096];

uintptr_t g_modeleChars = 0;      // chaine de reference, pour l'en-tete

static void ApLocCapturerModele(uintptr_t ret)
{
    if (g_apLocPret || ret <= 0x10000) return;
    uintptr_t chars = 0;
    if (!SafeRead((void*)ret, &chars, 8) || chars <= 0x10000) return;
    if (!SafeRead((void*)ret, g_apLoc, sizeof(g_apLoc))) return;
    g_modeleChars = chars;
    g_apLocPret = true;
}

// Prepare notre objet avec le texte voulu, et renvoie son adresse (0 si pas pret).
static uintptr_t ApLocAvecTexte(const wchar_t* texte)
{
    if (!g_apLocPret || !texte) return 0;
    int j = 0;
    for (; texte[j] && j < 4095; ++j) g_apTexte[j] = texte[j];
    g_apTexte[j] = 0;
    uintptr_t p = (uintptr_t)g_apTexte;
    memcpy(g_apLoc, &p, 8);
    return (uintptr_t)g_apLoc;
}

// Texte de repli quand aucun site n'est identifie.
static const wchar_t* ApTexteParDefaut()
{
    static wchar_t buf[64];
    static bool pret = false;
    if (!pret) {
        const char* s = "Archipelago Item";
        int i = 0;
        for (; s[i] && i < 63; ++i) buf[i] = (wchar_t)(unsigned char)s[i];
        buf[i] = 0;
        pret = true;
    }
    return buf;
}



static bool SeenEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

void LogLocKey(const char* key)
{
    if (!key) return;

    char k[96] = { 0 };
    if (!SafeRead((void*)key, k, sizeof(k) - 1)) return;

    // Ne garder que les cles de technologies.
    const char* pfx = "Technologies/";
    for (int i = 0; pfx[i]; ++i) if (k[i] != pfx[i]) return;

    // Deja vue ? (evite d'inonder la console, la traduction est appelee en boucle)
    for (int i = 0; i < g_seenCount; ++i)
        if (SeenEq(g_seen[i], k)) return;

    if (g_seenCount >= 512) return;
    int j = 0; for (; k[j] && j < 95; ++j) g_seen[g_seenCount][j] = k[j];
    g_seen[g_seenCount][j] = 0;
    g_seenCount++;

    printf("[KEY] %s\n", k);
}
static int g_locationAffichee = -1;

// Detourne les textes d'expedition :
//  - la cle de nom d'un site nous dit lequel est affiche
//  - "{baseResources}" est remplace par la description du check


static void LogKey2(const char* key)
{
    if (!g_logAll) return;
    if (GetTickCount() > g_logUntil) {
        g_logAll = false;
        printf("=== FIN DE CAPTURE (%d cles) ===\n", g_seenCount);
        return;
    }
    if (!key) return;

    char k[128] = { 0 };
    if (!SafeRead((void*)key, k, sizeof(k) - 1)) return;

    for (int i = 0; i < g_seenCount; ++i) {          // deduplique
        bool eq = true;
        for (int j = 0; ; ++j) { if (g_seen[i][j] != k[j]) { eq = false; break; } if (k[j] == 0) break; }
        if (eq) return;
    }
    if (g_seenCount >= 512) return;
    int j = 0; for (; k[j] && j < 127; ++j) g_seen[g_seenCount][j] = k[j];
    g_seen[g_seenCount][j] = 0;
    g_seenCount++;

    printf("[K2] %s\n", k);
}



// Detourne les textes d'expedition :
//  - la cle de nom d'un site nous dit lequel est affiche
//  - "{baseResources}" est remplace par la description du check
static DWORD g_instantSite = 0;

static bool TexteExpedition(const char* k, uintptr_t ret)
{
    int loc = ScootingLocationPourCleSite(k);
    if (loc >= 0) {
        g_locationAffichee = loc;
        g_instantSite = GetTickCount();
        return false;
    }

    if (!StrEqLoc(k, "Expeditions/EffectBaseResources")) return false;
    if (g_locationAffichee < 0) return false;

    // Chaine vide : sans le marqueur {baseResources}, le formateur n'a plus
    // d'emplacement ou inserer l'enumeration, et rien ne s'affiche.
    // L'information est deja donnee ailleurs (nom de l'item, survol de
    // l'inventaire), ce texte fait doublon.
    static const wchar_t vide[2] = { L' ', 0 };

    uintptr_t chars = 0;
    if (!SafeRead((void*)ret, &chars, 8) || chars <= 0x10000) return false;

    wchar_t* nouvelle = ApChaine(vide, chars, 0);
    if (!nouvelle) return false;

    uintptr_t p = (uintptr_t)nouvelle;
    return SafeWrite((void*)ret, &p, 8);
}

// --- BISSECTION : remettre a true un bloc a la fois ---
static const bool BLOC_LOGKEY = true;
static const bool BLOC_MODELE = true;
static const bool BLOC_APITEM = true;
static const bool BLOC_OVERRIDE = true;
static const bool BLOC_SITE = true;
static const bool BLOC_TECHNO = true;
volatile bool g_dumpDilemmes = false;

static void TracerCleEnumeration(const char* k)
{
    if (!StrEqLoc(k, "Expeditions/EffectBaseResources")) return;

    static DWORD dernier = 0;
    if (GetTickCount() - dernier < 500) return;      // une fois par demi-seconde
    dernier = GetTickCount();

    printf("[ENUM] EffectBaseResources passe  (locationAffichee=%d)\n",
        g_locationAffichee);
}

// Objet de traduction de l'item AP d'un groupe de refugies : nom du check
// porte (scout du serveur, sinon nom de la location), chaine propre au groupe
// (l'infobulle garde la chaine) ; 0 si 'vague' hors 1..13 (2026-11-10).
// A New Home : vagues 1..3 ("Refugees Arrival k") ; The Refugees : 1..10
// [OurPeople], 11..13 Lords. Location cherchee par sa cle dans le scenario
// courant (ApNomDuCheckRefugies).
wchar_t* ApChaine(const wchar_t* texte, uintptr_t modele, int slot);
static uintptr_t ApLocRefugies(int vague)
{
    if (vague < 1 || vague > 13 || !g_apLocPret || !g_modeleChars) return 0;
    char nom[200];
    if (!ApNomDuCheckRefugies(vague, nom, sizeof(nom)))
        _snprintf_s(nom, sizeof(nom), _TRUNCATE, "Refugees Arrival %d", vague);
    static wchar_t large[256];
    int j = 0;
    for (; nom[j] && j < 255; ++j) large[j] = (wchar_t)(unsigned char)nom[j];
    large[j] = 0;
    wchar_t* chaine = ApChaine(large, g_modeleChars, 4 + 16 + (vague - 1));
    if (!chaine) return 0;
    uintptr_t p = (uintptr_t)chaine;
    memcpy(g_apLoc, &p, 8);
    return (uintptr_t)g_apLoc;
}

static uintptr_t hkTranslate(uintptr_t loc, const char* key,
    uintptr_t p3, uintptr_t p4)
{
    NoterThread("Translate");
    LogKey(key);
    uintptr_t ret = oTranslate(loc, key, p3, p4);
    if (!key) return ret;

    char k[256] = { 0 };
    if (!SafeRead((void*)key, k, sizeof(k) - 1)) return ret;
    const char* kp = (k[0] == '@') ? k + 1 : k;
    LoisNoterTraduction(kp, ret);   // noms affiches des lois (F4)

    // --- Cles de nos notifications : "AP/Notif/<id>/T" ou ".../D" ----------
    {
        const char* pre = "AP/Notif/";
        int i = 0;
        while (pre[i] && kp[i] == pre[i]) i++;
        if (!pre[i]) {
            int id = 0;
            const char* q = kp + i;
            while (*q >= '0' && *q <= '9') { id = id * 10 + (*q - '0'); ++q; }
            bool titre = (q[0] == '/' && q[1] == 'T');
            const char* t = ApNotifTexte(id, titre);
            if (t) {
                uintptr_t o = ApLocNotif((id * 2 + (titre ? 0 : 1)) % 16, t);
                if (o) return o;
            }
            return ret;
        }
    }

    // --- Choix unique des dilemmes d'exploration : "AP/Prevail" (2026-11-10,
    //     dilemmes_exploration.cpp). Objet a nous, jamais celui du jeu.
    if (strcmp(kp, "AP/Prevail") == 0) {
        uintptr_t o = ApLocPrevail();
        return o ? o : ret;
    }
    // --- Cles de nos dilemmes pieges : "AP/Dil/<slot>/T", "/X", "/C<i>", "/S"
    //     (7 objets de traduction par slot : titre, texte, 4 choix, sous-titre).
    {
        const char* pre = "AP/Dil/";
        int i = 0;
        while (pre[i] && kp[i] == pre[i]) i++;
        if (!pre[i]) {
            int slot = 0;
            const char* q = kp + i;
            while (*q >= '0' && *q <= '9') { slot = slot * 10 + (*q - '0'); ++q; }
            char champ = (q[0] == '/') ? q[1] : 0;
            int indice = (champ == 'C' && q[2] >= '0' && q[2] <= '9') ? q[2] - '0' : 0;
            const char* t = DilemmePiegeTexte(slot, champ, indice);
            if (t) {
                int n = champ == 'T' ? 0 : champ == 'X' ? 1 : champ == 'S' ? 6 : 2 + indice;
                uintptr_t o = ApLocDilemme(slot * 7 + n, t);
                if (o) return o;
            }
            return ret;
        }
    }
    // --- Cles de nos groupes de survivants : "AP/Groupe/<slot>/<champ>",
    //     champ N nom, D description, T / X / C titre, texte, choix du
    //     dilemme d'arrivee (5 objets de traduction par slot).
    {
        const char* pre = "AP/Groupe/";
        int i = 0;
        while (pre[i] && kp[i] == pre[i]) i++;
        if (!pre[i]) {
            int slot = 0;
            const char* q = kp + i;
            while (*q >= '0' && *q <= '9') { slot = slot * 10 + (*q - '0'); ++q; }
            char champ = (q[0] == '/') ? q[1] : 0;
            int indice = champ == 'N' ? 0 : champ == 'D' ? 1 : champ == 'T' ? 2 : champ == 'X' ? 3 : 4;
            const char* t = GroupeTexte(slot, champ);
            if (t) {
                uintptr_t o = ApLocGroupe(slot * 5 + indice, t);
                if (o) return o;
            }
            return ret;
        }
    }
    // --- Notre item : liste des checks portes, ou description du site -----
    if (StrEqLoc(kp, "Resources/Names/AP_Item")) {
        if (!g_apLocPret || !g_modeleChars) return ret;

        // Groupe de refugies de l'apres-005 (2026-11-10) : sa case montre le
        // check qu'il porte ("Refugees Arrival k", scout du serveur), dans une
        // chaine propre a chaque groupe (l'infobulle garde la chaine).
        {
            uintptr_t o = ApLocRefugies(GroupesVagueRefugiesAffichee());
            if (o) return o;
        }

        // Pendant le remplissage de la liste de droite, la liste est celle
        // de l'expedition affichee, dans une chaine propre a cette expedition :
        // l'infobulle de la case garde la chaine (+1 ref.), une chaine partagee
        // afficherait le texte de la derniere case remplie.
        static wchar_t liste[512];
        int exp = -1;
        int nItems = ScootingListePourAffichage(liste, 512, &exp);
        const wchar_t* t = nullptr;

        if (nItems > 0) t = liste;
        if (nItems <= 0) {
            uintptr_t o = ApLocRefugies(GroupesVagueRefugiesVisee());
            if (o) return o;
        }
        if (!t && g_locationAffichee >= 0) {
            const wchar_t* d = ApDescriptionW(g_locationAffichee);
            if (d && d[0]) t = d;
        }
        if (!t) {
            static wchar_t fixe[64];
            static bool pret = false;
            if (!pret) {
                const char* s = "Check Archipelago";
                int i = 0;
                for (; s[i] && i < 63; ++i) fixe[i] = (wchar_t)(unsigned char)s[i];
                fixe[i] = 0;
                pret = true;
            }
            t = fixe;
        }

        int slotChaine = (nItems > 0 && exp >= 0 && exp < 16) ? 4 + exp : 1;
        wchar_t* chaine = ApChaine(t, g_modeleChars, slotChaine);
        if (!chaine) return ret;
        uintptr_t p = (uintptr_t)chaine;
        memcpy(g_apLoc, &p, 8);
        return (uintptr_t)g_apLoc;
    }

    ApLocCapturerModele(ret);
    if (ret <= 0x10000) return ret;

    if (ApplyOverride(k, ret)) return ret;
    if (TexteExpedition(k, ret)) return ret;

    // --- Objectifs "Rescue survivors" valides par l'exploration du site :
    //     texte remplace (demande de l'auteur, 2026-11-10 ; quetes.cpp).
    //     Objet de traduction du pool des sites, cle = texte de remplacement.
    {
        const char* t = QuetesTexteObjectifAP(kp);
        if (t) {
            uintptr_t o = ApLocSite(t, t);
            if (o) return o;
        }
    }
    // --- Description d'un site de la carte : nom du check qu'il contient --
    //     ("<joueur> - <item>", scout du serveur ; demande de l'auteur du
    //     2026-11-10). Les 4 descriptions par etat du gabarit ; le nom du site
    //     reste vanilla. Sans scout : vanilla.
    {
        char site[128], check[200];
        if (ScootingSitePourCleDescription(kp, site, sizeof(site)) &&
            ApNomDuCheckSite(site, check, sizeof(check))) {
            uintptr_t o = ApLocSite(site, check);
            if (o) return o;
        }
    }

    uint64_t th = 0; int slot = -1;
    if (LookupLocKey(k, &th, &slot)) {
        uintptr_t origChars = 0;
        if (!SafeRead((void*)ret, &origChars, 8) || origChars <= 0x10000) return ret;

        if (slot == 0) {
            TechRuntime* r = RT_Get(th);
            if (r && r->researched) {
                wchar_t* nn = GetResearchedName(k, origChars);
                if (nn) { uintptr_t p = (uintptr_t)nn; SafeWrite((void*)ret, &p, 8); }
            }
        }
        else if (g_showApInfo) {
            wchar_t* nb = ApDescBuffer(k, th, origChars);
            if (nb) { uintptr_t p = (uintptr_t)nb; SafeWrite((void*)ret, &p, 8); }
        }
        return ret;
    }

    return ret;
}

static uintptr_t hkHashMapLookup(uintptr_t map, const char** key)
{
    if (key) {
        const char* real = nullptr;
        if (SafeRead((void*)key, &real, 8) && real)
            LogKey2(real);            // meme logger, prefixe [K2]
    }
    return oHashMapLookup(map, key);
}

void InstallLocHooks(uintptr_t modBase)
{
    void* target = (void*)(modBase + GameOffsets::Translate);
    if (MH_CreateHook(target, (void*)&hkTranslate, (void**)&oTranslate) != MH_OK
        || MH_EnableHook(target) != MH_OK)
        printf("[!] hook Translate KO\n");

    void* t2 = (void*)(modBase + Adr(0x1079AA0));
    if (MH_CreateHook(t2, (void*)&hkHashMapLookup, (void**)&oHashMapLookup) != MH_OK
        || MH_EnableHook(t2) != MH_OK)
        printf("[!] hook HashMapLookup KO\n");
}

static void DumpWide(const char* key, uintptr_t ret)
{
    uintptr_t chars = 0;
    if (!SafeRead((void*)ret, &chars, 8) || chars <= 0x10000) return;

    char out[512];
    int i = 0;
    for (; i < 500; ++i) {
        wchar_t ch = 0;
        if (!SafeRead((void*)(chars + i * 2), &ch, 2) || ch == 0) break;
        out[i] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '?';
    }
    out[i] = 0;
    printf("[TXT] %s\n      = \"%s\"\n", key, out);
}

static bool EndsWith(const char* s, const char* suf)
{
    int ls = 0; while (s[ls]) ls++;
    int lf = 0; while (suf[lf]) lf++;
    if (lf > ls) return false;
    for (int i = 0; i < lf; ++i) if (s[ls - lf + i] != suf[i]) return false;
    return true;
}

// Buffer persistant par segment, REECRIT a chaque appel : le contenu depend de
// l'etat Archipelago et du reglage on/off, donc on ne peut pas le figer.
static wchar_t* ApDescBuffer(const char* seg, uint64_t hash, uintptr_t origChars)
{
    struct Slot { char key[192]; uint8_t entete[0x14]; wchar_t buf[256]; bool used; };   // en-tete juste avant buf
    // 256 au depart : test grandeur nature du 2026-11-04, "passe un certain
    // point les textes ne sont plus remplaces" (pool plein, nullptr -> texte
    // vanilla : Generator efficiency upgrade, Improved Heater 2). Une entree par
    // cle (nom, description, courte ; tous scenarios confondus) : 2048.
    static Slot pool[2048];
    static int  poolN = 0;

    Slot* s = nullptr;
    for (int i = 0; i < poolN; ++i) {
        bool eq = true;
        for (int j = 0; ; ++j) { if (pool[i].key[j] != seg[j]) { eq = false; break; } if (seg[j] == 0) break; }
        if (eq) { s = &pool[i]; break; }
    }
    if (!s) {
        if (poolN >= 2048) { static bool dit = false; if (!dit) { dit = true; printf("[TECHDESC] pool des descriptions plein (2048)\n"); } return nullptr; }
        if (!PoserEnteteFigee(pool[poolN].entete, origChars)) return nullptr;   // regles 2 et 3 (2026-11-10)
        s = &pool[poolN++];
        // Cle COMPLETE (bug du 2026-11-11, Last Autumn) : tronquee a 63 caracteres,
        // une cle longue ("Generated/TechnologiesConfig/Technologies/[Builders] ...")
        // ne retrouvait jamais son emplacement et en prenait un neuf a chaque appel.
        int j = 0; for (; seg[j] && j < 191; ++j) s->key[j] = seg[j]; s->key[j] = 0;
    }

    char txt[256];
    AP_LocationText(hash, txt, sizeof(txt));
    int i = 0;
    for (; txt[i] && i < 255; ++i) s->buf[i] = (wchar_t)(unsigned char)txt[i];
    s->buf[i] = 0;
    return s->buf;
}



// Ajoute la cle si elle est inconnue. true = premiere rencontre.
static bool SeenAdd(const char* k)
{
    for (int i = 0; i < g_seenCount; ++i) {
        bool eq = true;
        for (int j = 0; ; ++j) {
            if (g_seen[i][j] != k[j]) { eq = false; break; }
            if (k[j] == 0) break;
        }
        if (eq) return false;
    }
    if (g_seenCount >= SEEN_MAX) return false;
    int j = 0; for (; k[j] && j < 256; ++j) g_seen[g_seenCount][j] = k[j];
    g_seen[g_seenCount][j] = 0;
    g_seenCount++;
    return true;
}

// Phase d'amorcage : absorbe le bruit ambiant sans rien afficher.
void BaselineKeys(int ms)
{
    g_silent = true;
    g_logUntil = GetTickCount() + ms;
    g_logAll = true;
    printf("\n=== AMORCAGE %d ms (silencieux) ===\n", ms);
}

// Repart de zero si tu changes de contexte (autre scenario, autre menu).
void ResetSeenKeys()
{
    g_seenCount = 0;
    printf("[KEYS] liste videe\n");
}


void OpenKeyLogWindow(int ms)
{
    g_silent = false;
    g_logUntil = GetTickCount() + ms;
    g_logAll = true;
    printf("\n=== CAPTURE %d ms (%d cles deja connues) ===\n", ms, g_seenCount);
}

static void LogKey(const char* key)
{
    if (!g_logAll) return;
    if (GetTickCount() > g_logUntil) {
        g_logAll = false;
        printf("=== FIN (%s, %d cles connues) ===\n",
            g_silent ? "amorcage" : "capture", g_seenCount);
        return;
    }
    if (!key) return;

    char k[128] = { 0 };
    if (!SafeRead((void*)key, k, sizeof(k) - 1)) return;

    if (SeenAdd(k) && !g_silent) printf("[K] %s\n", k);
}

void DumpClesDilemme(const char* k, uintptr_t ret)
{
    // On ne s'interesse qu'aux dilemmes
    const char* pref = "Generated/DilemmasConfig/";
    for (int i = 0; pref[i]; ++i) if (k[i] != pref[i]) return;

    static char vues[64][256];
    static int  nVues = 0;

    for (int i = 0; i < nVues; ++i) {
        bool eq = true;
        for (int j = 0; ; ++j) {
            if (vues[i][j] != k[j]) { eq = false; break; }
            if (k[j] == 0) break;
        }
        if (eq) return;
    }
    if (nVues >= 64) return;
    int j = 0; for (; k[j] && j < 255; ++j) vues[nVues][j] = k[j];
    vues[nVues][j] = 0; nVues++;

    DumpWide(k, ret);
}


static const int AP_SLOTS = 4 + 16 + 13;  // 0..3 usages fixes, 4..19 item AP par expedition, 20..32 refugies (2026-11-10 ; 13 groupes de The Refugees)

wchar_t* ApChaine(const wchar_t* texte, uintptr_t modele, int slot)
{
    static uint8_t blocs[AP_SLOTS][0x14 + 512 * 2];
    static bool    prets[AP_SLOTS] = {};

    if (slot < 0 || slot >= AP_SLOTS) return nullptr;
    uint8_t* bloc = blocs[slot];

    if (!prets[slot]) {
        if (modele <= 0x10000) return nullptr;
        if (!SafeRead((void*)(modele - 0x14), bloc, 0x14)) return nullptr;
        prets[slot] = true;

        // Longueur de la chaine modele, pour identifier le champ
        wchar_t m[128] = {};
        SafeRead((void*)modele, m, sizeof(m) - 2);
        int lm = 0; while (lm < 127 && m[lm]) lm++;

        printf("[CHAINE] modele len=%d  en-tete (0x14 octets) :", lm);
        for (int z = 0; z < 0x14; z += 4)
            printf(" %08X", *(uint32_t*)(bloc + z));
        printf("\n");
    }

    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    wchar_t* dst = (wchar_t*)(bloc + 0x14);
    int i = 0;
    for (; texte[i] && i < 511; ++i) dst[i] = texte[i];
    dst[i] = 0;
    return dst;
}

bool TexteArbreTechno(const char* k)
{
    const char* pref = "UI/OtherPanels/TechnologyHUD/";
    for (int i = 0; pref[i]; ++i) if (k[i] != pref[i]) return false;
    g_instantArbre = GetTickCount();
    return false;                       // on ne modifie rien, on observe
}

static uint8_t g_notifLoc[16][0x40];
static uint8_t g_notifChaine[16][0x14 + 512 * 2];

uintptr_t ApLocNotif(int slot, const char* texte)
{
    if (slot < 0 || slot >= 16 || !g_apLocPret || !g_modeleChars) return 0;

    uint8_t* bloc = g_notifChaine[slot];
    if (!SafeRead((void*)(g_modeleChars - 0x14), bloc, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    wchar_t* w = (wchar_t*)(bloc + 0x14);
    int i = 0; for (; texte[i] && i < 511; ++i) w[i] = (wchar_t)(unsigned char)texte[i];
    w[i] = 0;

    memcpy(g_notifLoc[slot], g_apLoc, 0x40);
    uintptr_t p = (uintptr_t)w;
    memcpy(g_notifLoc[slot], &p, 8);
    return (uintptr_t)g_notifLoc[slot];
}

// Objets de traduction des groupes de survivants (5 par groupe : nom,
// description, titre / texte / choix du dilemme d'arrivee ; 16 groupes).
static uint8_t g_groupeLoc[80][0x40];
static uint8_t g_groupeChaine[80][0x14 + 1024 * 2];

uintptr_t ApLocGroupe(int slot, const char* texte)
{
    if (slot < 0 || slot >= 80 || !g_apLocPret || !g_modeleChars) return 0;

    uint8_t* bloc = g_groupeChaine[slot];
    if (!SafeRead((void*)(g_modeleChars - 0x14), bloc, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    wchar_t* w = (wchar_t*)(bloc + 0x14);
    int i = 0; for (; texte[i] && i < 1023; ++i) w[i] = (wchar_t)(unsigned char)texte[i];
    w[i] = 0;

    memcpy(g_groupeLoc[slot], g_apLoc, 0x40);
    uintptr_t p = (uintptr_t)w;
    memcpy(g_groupeLoc[slot], &p, 8);
    return (uintptr_t)g_groupeLoc[slot];
}

// Objets de traduction des dilemmes pieges (7 par slot : titre, texte,
// 4 choix, sous-titre ; 8 slots).
static uint8_t g_dilemmeLoc[56][0x40];
static uint8_t g_dilemmeChaine[56][0x14 + 1024 * 2];

uintptr_t ApLocDilemme(int slot, const char* texte)
{
    if (slot < 0 || slot >= 56 || !g_apLocPret || !g_modeleChars) return 0;

    uint8_t* bloc = g_dilemmeChaine[slot];
    if (!SafeRead((void*)(g_modeleChars - 0x14), bloc, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    wchar_t* w = (wchar_t*)(bloc + 0x14);
    int i = 0; for (; texte[i] && i < 1023; ++i) w[i] = (wchar_t)(unsigned char)texte[i];
    w[i] = 0;

    memcpy(g_dilemmeLoc[slot], g_apLoc, 0x40);
    uintptr_t p = (uintptr_t)w;
    memcpy(g_dilemmeLoc[slot], &p, 8);
    return (uintptr_t)g_dilemmeLoc[slot];
}

// Objet de traduction du choix unique "Archipelago Prevail" des dilemmes
// d'exploration (2026-11-10, dilemmes_exploration.cpp).
static uint8_t g_prevailLoc[0x40];
static uint8_t g_prevailChaine[0x14 + 64 * 2];

uintptr_t ApLocPrevail()
{
    if (!g_apLocPret || !g_modeleChars) return 0;

    uint8_t* bloc = g_prevailChaine;
    if (!SafeRead((void*)(g_modeleChars - 0x14), bloc, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    const char* texte = "Archipelago Prevail";
    wchar_t* w = (wchar_t*)(bloc + 0x14);
    int i = 0; for (; texte[i] && i < 63; ++i) w[i] = (wchar_t)(unsigned char)texte[i];
    w[i] = 0;

    memcpy(g_prevailLoc, g_apLoc, 0x40);
    uintptr_t p = (uintptr_t)w;
    memcpy(g_prevailLoc, &p, 8);
    return (uintptr_t)g_prevailLoc;
}
// Objets de traduction des descriptions de sites de la carte (2026-11-10) : un par
// site (cle = nom interne), jamais reutilise pour un autre site ; le texte
// est recopie a chaque appel (le scout peut arriver apres le 1er affichage).
static const int AP_SITES_NOMS = 128;
static char    g_siteNom[AP_SITES_NOMS][128];
static uint8_t g_siteLoc[AP_SITES_NOMS][0x40];
static uint8_t g_siteChaine[AP_SITES_NOMS][0x14 + 256 * 2];
static int     g_nSitesNoms = 0;

uintptr_t ApLocSite(const char* site, const char* texte)
{
    if (!site || !texte || !g_apLocPret || !g_modeleChars) return 0;
    int slot = -1;
    for (int i = 0; i < g_nSitesNoms; ++i)
        if (strcmp(g_siteNom[i], site) == 0) { slot = i; break; }
    if (slot < 0) {
        if (g_nSitesNoms >= AP_SITES_NOMS) {
            static bool dit = false;
            if (!dit) { dit = true; printf("[SITE] plus de %d sites : descriptions vanilla au-dela\n", AP_SITES_NOMS); }
            return 0;
        }
        slot = g_nSitesNoms++;
        strncpy_s(g_siteNom[slot], site, _TRUNCATE);
        printf("[SITE] description du site : \"%s\" -> \"%s\"\n", site, texte);
    }

    uint8_t* bloc = g_siteChaine[slot];
    if (!SafeRead((void*)(g_modeleChars - 0x14), bloc, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)bloc;
    memcpy(bloc + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(bloc + 0x10, &refs, 4);

    wchar_t* w = (wchar_t*)(bloc + 0x14);
    int i = 0; for (; texte[i] && i < 255; ++i) w[i] = (wchar_t)(unsigned char)texte[i];
    w[i] = 0;

    memcpy(g_siteLoc[slot], g_apLoc, 0x40);
    uintptr_t p = (uintptr_t)w;
    memcpy(g_siteLoc[slot], &p, 8);
    return (uintptr_t)g_siteLoc[slot];
}