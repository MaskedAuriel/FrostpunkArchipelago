// hooks_loc.cpp
#include "hooks_loc.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "MinHook.h"
#include "tech_brain.h"
#include "scenario.h"
#include "scooting.h"
#include "ap_data.h"


static volatile bool     g_logAll = false;
static volatile DWORD    g_logUntil = 0;
static const int SEEN_MAX = 2048;
static char  g_seen[SEEN_MAX][256];
static int   g_seenCount = 0;
static int   seenTxtN = 0;
void LogLocKey(const char* key);


static volatile bool  g_silent = false;   // amorcage : enregistre sans afficher


// PIEGE : ici rdx est un char* DIRECT (contrairement a HashMapLookup 0x1079AA0
// qui recoit un char** et double-dereference).
typedef uintptr_t (*Translate_t)(uintptr_t loc, const char* key,
                                 uintptr_t p3, uintptr_t p4);
typedef uintptr_t(*HashMapLookup_t)(uintptr_t map, const char** key);
static HashMapLookup_t oHashMapLookup = nullptr;
static Translate_t oTranslate = nullptr;

volatile bool g_apTexteActif = true;
// --- Cache : un buffer "<nom> [Researched]" par segment de cle ---------------
struct NameCache {
    char    key[96];
    wchar_t buf[128];
    bool    used;
};
static NameCache g_cache[256];
static int       g_cacheCount = 0;

struct KeyOverride { const char* key; const char* text; };

static const KeyOverride g_overrides[] = {
    { "UI/Menu/Characters/Desmond2", "Mère de la p'tite Clara, cette grognasse" },
    // { "UI/OtherPanels/TechnologyHUD/ButtonStart", "Envoyer" },
};
static const int g_overrideCount =
(int)(sizeof(g_overrides) / sizeof(g_overrides[0]));

static const int OV_MAX = 32;
static wchar_t g_ovBuf[OV_MAX][256];
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
            const char* s = g_overrides[i].text;
            int j = 0;
            for (; s[j] && j < 255; ++j) g_ovBuf[i][j] = (wchar_t)(unsigned char)s[j];
            g_ovBuf[i][j] = 0;
            g_ovBuilt[i] = true;
        }

        uintptr_t p = (uintptr_t)g_ovBuf[i];
        if (SafeWrite((void*)ret, &p, 8)) {
            printf("[OVERRIDE] %s -> \"%s\"\n", k, g_overrides[i].text);
            return true;
        }
        return false;
    }
    return false;
}


static bool StrEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

// Construit (une seule fois par techno) le nom suffixe et renvoie le buffer.
static wchar_t* GetResearchedName(const char* seg, uintptr_t origChars)
{
    for (int i = 0; i < g_cacheCount; ++i)
        if (g_cache[i].used && StrEq(g_cache[i].key, seg)) return g_cache[i].buf;

    if (g_cacheCount >= 256) return nullptr;
    NameCache* c = &g_cache[g_cacheCount++];

    int s = 0; for (; seg[s] && s < 95; ++s) c->key[s] = seg[s]; c->key[s] = 0;

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
        const char* s = "Objet Archipelago";
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

static bool StrEqLoc(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
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
    const ApCheck* ac = ApCheckById(g_locationAffichee);
    if (!ac) return false;

    // Sans le marqueur {baseResources}, le formateur n'a plus d'emplacement
    // ou inserer l'enumeration : le logo et la quantite disparaissent.
    wchar_t tmp[256];
    int n = 0;
    for (int i = 0; ac->description[i] && n < 255; ++i)
        tmp[n++] = (wchar_t)(unsigned char)ac->description[i];
    tmp[n] = 0;

    uintptr_t chars = 0;
    if (!SafeRead((void*)ret, &chars, 8) || chars <= 0x10000) return false;

    wchar_t* nouvelle = ApChaine(tmp, chars,0);
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

static uintptr_t hkTranslate(uintptr_t loc, const char* key,
    uintptr_t p3, uintptr_t p4)
{
    if (BLOC_LOGKEY) LogKey(key);

    uintptr_t ret = oTranslate(loc, key, p3, p4);
    if (!key) return ret;

    char k[256] = { 0 };
    if (!SafeRead((void*)key, k, sizeof(k) - 1)) return ret;

    const char* kp = (k[0] == '@') ? k + 1 : k;
    TracerCleEnumeration(k);

    if (BLOC_APITEM && StrEqLoc(kp, "Resources/Names/AP_Item")) {
        if (!g_apLocPret || !g_modeleChars) return ret;

        static wchar_t liste[512];
        int nItems = ScootingListePortee(liste, 512);

        const wchar_t* t = nullptr;

        // Une expedition selectionnee porte des items -> c'est son inventaire.
        if (nItems > 0) t = liste;

        // Sinon, un site est affiche -> on montre ce que contient la location.
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

        wchar_t* chaine = ApChaine(t, g_modeleChars, 1);
        if (!chaine) return ret;

        uintptr_t p = (uintptr_t)chaine;
        memcpy(g_apLoc, &p, 8);
        return (uintptr_t)g_apLoc;
    }

    if (BLOC_MODELE) ApLocCapturerModele(ret);
    DumpClesLandmark(k, ret);
    if (g_dumpDilemmes) DumpClesDilemme(k, ret);
    if (ret <= 0x10000) return ret;

    if (BLOC_OVERRIDE && ApplyOverride(k, ret)) return ret;
    if (BLOC_SITE && TexteExpedition(k, ret)) return ret;  
    //if (TexteDescriptionSite(k, ret))  return ret;

    if (BLOC_TECHNO) {
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
                wchar_t* nb = ApDescBuffer(k, th);
                if (nb) { uintptr_t p = (uintptr_t)nb; SafeWrite((void*)ret, &p, 8); }
            }
            return ret;
        }
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

    void* t2 = (void*)(modBase + 0x1079AA0);
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
static wchar_t* ApDescBuffer(const char* seg, uint64_t hash)
{
    struct Slot { char key[96]; wchar_t buf[256]; bool used; };
    static Slot pool[256];
    static int  poolN = 0;

    Slot* s = nullptr;
    for (int i = 0; i < poolN; ++i) {
        bool eq = true;
        for (int j = 0; ; ++j) { if (pool[i].key[j] != seg[j]) { eq = false; break; } if (seg[j] == 0) break; }
        if (eq) { s = &pool[i]; break; }
    }
    if (!s) {
        if (poolN >= 256) return nullptr;
        s = &pool[poolN++];
        int j = 0; for (; seg[j] && j < 63; ++j) s->key[j] = seg[j]; s->key[j] = 0;
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


static const int AP_SLOTS = 4;

static wchar_t* ApChaine(const wchar_t* texte, uintptr_t modele, int slot)
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


bool TexteDescriptionSite(const char* k, uintptr_t ret)
{
    if (g_locationAffichee < 0) return false;

    // La cle doit finir par "description" (Unvisited/Explored/Depleted inclus)
    int len = 0;
    while (len < 255 && k[len]) len++;
    const char* suf = "description";
    int ls = 11;
    if (len < ls) return false;
    for (int i = 0; i < ls; ++i) {
        char c = k[len - ls + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != suf[i]) return false;
    }

    const ApCheck* ac = ApCheckById(g_locationAffichee);
    if (!ac) return false;

    uintptr_t chars = 0;
    if (!SafeRead((void*)ret, &chars, 8) || chars <= 0x10000) return false;

    // Deja prefixee ? (le hook peut repasser plusieurs fois)
    wchar_t orig[400] = {};
    if (!SafeRead((void*)chars, orig, sizeof(orig) - 2)) return false;
    if (orig[0] == L'[') return false;

    wchar_t tmp[512];
    int n = 0;
    for (int i = 0; ac->description[i] && n < 200; ++i)
        tmp[n++] = (wchar_t)(unsigned char)ac->description[i];
    tmp[n++] = L'\n';
    tmp[n++] = L'\n';
    for (int i = 0; orig[i] && n < 511; ++i) tmp[n++] = orig[i];
    tmp[n] = 0;

    wchar_t* nouvelle = ApChaine(tmp, chars, 2);
    if (!nouvelle) return false;

    uintptr_t p = (uintptr_t)nouvelle;
    return SafeWrite((void*)ret, &p, 8);
}

static void DumpClesLandmark(const char* k, uintptr_t ret)
{
    static const char* cles[] = {
        "Expeditions/GenericSiteDescription",
        "Expeditions/LandmarkName",
        "Expeditions/LandmarkSubtitle",
        "Expeditions/LandmarkDescription",
        "Expeditions/LandmarkCarryOn",
        "Expeditions/LandmarkTooltip",
        "Expeditions/LandsmarkArcSubtitle",
        "Expeditions/GenericSiteDepletedDescription",
    };
    static const int N = 8;
    static bool vues[N] = {};

    for (int i = 0; i < N; ++i) {
        if (vues[i]) continue;
        if (!StrEqLoc(k, cles[i])) continue;
        vues[i] = true;
        DumpWide(k, ret);
        return;
    }
}
