// evenements.cpp
// ---------------------------------------------------------------------------
// Evenements du jeu. Decompiles du 2026-10-28 :
// - commande console "InvokeEvent" (FUN_1413a3ec0) : repertoires d'evenements
//   DAT_14406dda8 (compte DAT_14406ddb0), chacun : +0x18 entrees, +0x20
//   compte ; entree FrostpunkGameEventEntry : +0x00 nom, +0x20 Enabled,
//   +0x28 declencheurs (compte +0x30), +0x58 effets ;
// - invocation FUN_1417b9b70(systeme DAT_1440935c8, entree, contexte,
//   forcer) : forcer != 0 -> conditions verifiees (FUN_1417aebc0) ; effets
//   executes par FUN_14169ae90(DAT_14408e4b8, entree + 0x58, contexte, nom) ;
//   contexte : +0x00 poignee de l'entite (*poignee = entite), +0x10 position ;
// - FrostpunkCreateSurvivoursDilemmaEffect (0x28 octets, FUN_141937a70) :
//   +0x18 Dilemma = lien (GUID 16 octets) vers un FrostpunkDilemmaEntry ;
// - resolution d'un lien de dilemme (FUN_141a0f440) : gestionnaire
//   DAT_144069970, (*gestionnaire)[0x88](gestionnaire, &guid) -> entree ;
// - FrostpunkDilemmaEntry (0x250, reflexion FUN_1419dbde0) : +0x18
//   Variables, +0x30 Text parameters, +0x48 Initial effects, +0x60 Ignored
//   effects, +0x78 Choices (pointeurs de FrostpunkDilemmaChoiceEntry),
//   +0x90 Sequence flags, +0xD8 Displayed name, +0xE0 Arc subtitle, +0xE8
//   Description, +0xF0 Type text, +0xF8 Chat settlement greeting, +0x108
//   Main icon, +0x150 Template, +0x188 Subject type, +0x1B8 Dilemma type,
//   +0x1D4 Countdown, +0x1E0 Time to countdown ;
// - FrostpunkDilemmaChoiceEntry (0x150, FUN_1419daf40) : +0x78 Effects,
//   +0x90 Outcomes, +0xA8 texte du choix, +0xB0 Description, +0xB8 Effect
//   text, +0xC0 Disabled text ;
// - execution de CreateSurvivoursDilemmaEffect (FUN_141697870, vers
//   0x1416984dd) : si le composant de groupe du contexte a un dilemme en
//   +0x80, c'est LUI qui est cree (FUN_1417cde10(DAT_144095a98, dilemme,
//   contexte, survivants FUN_1418b0e10)) ; sinon le dilemme du lien +0x18.
// ---------------------------------------------------------------------------
#include "evenements.h"
#include "mod_common.h"
#include "groupes.h"
#include "resources.h"
#include "MessageManager.h"
#include "IconSubManager.h"
#include "dilemmes_ap.h"
#include "MinHook.h"
#include <cstdio>
#include <cstring>

typedef void(*InvoquerEvenement_t)(uintptr_t sys, uintptr_t entree, uintptr_t contexte, uintptr_t forcer);
typedef uintptr_t(*ResoudreLien_t)(uintptr_t gestionnaire, const void* guid);

constexpr uintptr_t OFF_INVOQUER_EVENEMENT = 0x17B9B70;
constexpr uintptr_t OFF_GESTIONNAIRE_DILEMMES = 0x4069970;
constexpr int       VT_RESOUDRE_LIEN       = 0x88;
constexpr int       EVT_NOM                = 0x00;
constexpr int       EVT_EFFETS             = 0x58;
constexpr int       EFFET_DILEMME_GUID     = 0x18;
constexpr int       DIL_CHOIX              = 0x78;
constexpr int       DIL_TITRE              = 0xD8;
constexpr int       DIL_DESCRIPTION        = 0xE8;
constexpr int       TAILLE_DILEMME         = 0x250;
constexpr int       TAILLE_CHOIX           = 0x150;  // FrostpunkDilemmaChoiceEntry (FUN_1419db810)
constexpr int       CHOIX_TEXTE            = 0xA8;
constexpr int       ENT_COMPOUND           = 0x28;   // CompoundTemplate de l'entite
constexpr int       CT_CHEMIN              = 0x20;   // chemin de ressource du CompoundTemplate
constexpr int       MAX_SUIVIES            = 16;
constexpr long      MAX_LOGS_EVT           = 300;
constexpr int       PROFONDEUR_MAX         = 4;

typedef uintptr_t(*CreerDilemme_t)(uintptr_t sys, uintptr_t dilemme, uintptr_t contexte, uintptr_t p4);
constexpr uintptr_t OFF_CREER_DILEMME      = 0x17CDE10;

InvoquerEvenement_t g_origInvoquer = nullptr;
CreerDilemme_t      g_origCreerDilemme = nullptr;
uintptr_t           g_suivies[MAX_SUIVIES] = {};
int                 g_nSuivies = 0;
// Regles de remplacement, par entite suivie (meme indice que g_suivies).
uintptr_t           g_remplOrig[MAX_SUIVIES] = {};
uintptr_t           g_remplNouveau[MAX_SUIVIES] = {};
int                 g_suivieEnCours = -1;   // indice de l'entite de l'evenement en cours
volatile long       g_logsEvt = 0;
int                 g_lignesReleve = 0;   // budget de lignes par releve detaille

int IndiceSuivie(uintptr_t entite)
{
    for (int i = 0; i < g_nSuivies; ++i) if (entite && g_suivies[i] == entite) return i;
    return -1;
}

void EvenementsSuivreEntite(uintptr_t entite)
{
    if (!entite || IndiceSuivie(entite) >= 0) return;
    int i = g_nSuivies < MAX_SUIVIES ? g_nSuivies++ : MAX_SUIVIES - 1;
    g_suivies[i] = entite;
    g_remplOrig[i] = g_remplNouveau[i] = 0;
}

void EvenementsRemplacerDilemme(uintptr_t entite, uintptr_t orig, uintptr_t remplacant)
{
    EvenementsSuivreEntite(entite);
    int i = IndiceSuivie(entite);
    if (i < 0) return;
    g_remplOrig[i] = orig;
    g_remplNouveau[i] = remplacant;
}

// Entite du contexte : *poignee, ou 0.
uintptr_t EntiteDuContexte(uintptr_t contexte)
{
    uintptr_t poignee = 0, entite = 0;
    if (!contexte || !SafeRead((void*)contexte, &poignee, 8) || poignee <= 0x10000 ||
        !SafeRead((void*)poignee, &entite, 8) || entite <= 0x10000) return 0;
    return entite;
}

// Classe de l'entite et chemin de son CompoundTemplate (s'il y en a un).
void DecrireEntiteContexte(uintptr_t entite, char* out, int maxlen)
{
    char cls[96] = "?";
    char chemin[160] = "";
    GetClassNameFP(entite, cls, sizeof(cls));
    uintptr_t compound = 0, p = 0;
    if (SafeRead((void*)(entite + ENT_COMPOUND), &compound, 8) && compound > 0x10000 &&
        SafeRead((void*)(compound + CT_CHEMIN), &p, 8) && p > 0x10000) ReadAscii(p, chemin, sizeof(chemin));
    snprintf(out, maxlen, "0x%llX %s %s", (unsigned long long)entite, cls, chemin);
}

// Effets de l'entree : 4 qwords a +0x58 et, si le premier ressemble a un
// tableau (compte plausible au qword suivant), la classe de chaque element.
void DecrireEffets(uintptr_t entree)
{
    uintptr_t q[4] = {};
    if (!SafeRead((void*)(entree + EVT_EFFETS), q, sizeof(q))) return;
    printf("[EVT]   effets +0x58 : 0x%llX 0x%llX 0x%llX 0x%llX\n", (unsigned long long)q[0],
        (unsigned long long)q[1], (unsigned long long)q[2], (unsigned long long)q[3]);
    int32_t n = (int32_t)(q[1] & 0xFFFFFFFF);
    if (q[0] <= 0x10000 || n <= 0 || n > 16) return;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        char cls[96] = "?";
        if (!SafeRead((void*)(q[0] + i * 8), &e, 8)) break;
        if (e > 0x10000) GetClassNameFP(e, cls, sizeof(cls));
        printf("[EVT]     [%d] 0x%llX %s\n", i, (unsigned long long)e, cls);
    }
}

// --- Releve detaille (groupes Archipelago) ----------------------------------

bool ClasseDe(uintptr_t p, char* cls, int maxlen)
{
    cls[0] = 0;
    return p > 0x10000 && GetClassNameFP(p, cls, maxlen) && cls[0];
}

// Texte 8 bits plausible pointe par p (cle "@..." ou nom lisible).
bool TexteDe(uintptr_t p, char* out, int maxlen)
{
    out[0] = 0;
    if (p <= 0x10000 || !ReadAscii(p, out, maxlen)) return false;
    int n = (int)strlen(out);
    if (n < 3) return false;
    for (int i = 0; i < n; ++i) if ((unsigned char)out[i] < 0x20 || (unsigned char)out[i] > 0x7E) return false;
    return true;
}

void Indenter(int profondeur) { for (int i = 0; i < profondeur; ++i) printf("  "); }

uintptr_t DilemmeParGuid(const uint8_t* guid)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t gest = base + OFF_GESTIONNAIRE_DILEMMES, vt = 0, fn = 0;
    if (!SafeRead((void*)gest, &vt, 8) || !SafeRead((void*)(vt + VT_RESOUDRE_LIEN), &fn, 8) || !fn) return 0;
    __try {
        return ((ResoudreLien_t)fn)(gest, guid);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[EVT]   EXCEPTION 0x%08lX dans la resolution du dilemme\n", GetExceptionCode());
        return 0;
    }
}

// Copie superficielle : les tableaux d'effets, conditions et resultats des
// choix restent partages avec l'original (jamais liberes : les entrees de
// dilemme vivent toute la partie). Seuls le tableau des choix et les choix
// eux-memes sont dupliques, pour porter nos cles.
uintptr_t DilemmeCloner(uintptr_t orig, const char* cleTitre, const char* cleDescription, const char* cleChoix)
{
    // Pas de RTTI sur FrostpunkDilemmaEntry (releve du 2026-10-28) : on
    // verifie son nom (+0x00, ex. "[Beacon]S: Survivors group").
    char nom[128];
    uintptr_t pNom = 0;
    if (!orig || !SafeRead((void*)orig, &pNom, 8) || !TexteDe(pNom, nom, sizeof(nom)) || !MoteurAlloc) return 0;
    uintptr_t clone = (uintptr_t)MoteurAlloc(TAILLE_DILEMME);
    if (!clone || !SafeRead((void*)orig, (void*)clone, TAILLE_DILEMME)) return 0;

    // Modele d'en-tete : le titre d'origine (cle valide).
    uintptr_t modele = 0;
    SafeRead((void*)(orig + DIL_TITRE), &modele, 8);
    uintptr_t k;
    if (cleTitre && (k = CleMoteur(cleTitre, modele))) SafeWrite((void*)(clone + DIL_TITRE), &k, 8);
    if (cleDescription && (k = CleMoteur(cleDescription, modele))) SafeWrite((void*)(clone + DIL_DESCRIPTION), &k, 8);

    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(orig + DIL_CHOIX), &tab, 8);
    SafeRead((void*)(orig + DIL_CHOIX + 8), &n, 4);
    if (cleChoix && tab && n > 0 && n <= 16) {
        uintptr_t nouveauTab = (uintptr_t)MoteurAlloc(n * 8);
        uintptr_t kChoix = CleMoteur(cleChoix, modele);
        if (!nouveauTab || !kChoix) return clone;
        for (int i = 0; i < n; ++i) {
            uintptr_t c = 0, cc = 0;
            SafeRead((void*)(tab + i * 8), &c, 8);
            cc = c ? (uintptr_t)MoteurAlloc(TAILLE_CHOIX) : 0;
            if (cc && SafeRead((void*)c, (void*)cc, TAILLE_CHOIX)) SafeWrite((void*)(cc + CHOIX_TEXTE), &kChoix, 8);
            else cc = c;
            SafeWrite((void*)(nouveauTab + i * 8), &cc, 8);
        }
        SafeWrite((void*)(clone + DIL_CHOIX), &nouveauTab, 8);
    }
    printf("[DIL] copie du dilemme \"%s\" 0x%llX -> 0x%llX (%d choix)\n", nom, (unsigned long long)orig,
        (unsigned long long)clone, n);
    return clone;
}

void DecrireObjet(uintptr_t obj, int profondeur);

// Dilemme : textes (+0xD8..+0xF8), choix (+0x78), effets initiaux (+0x48).
void DecrireDilemme(uintptr_t d, int profondeur)
{
    static const struct { int off; const char* nom; } TEXTES[] = {
        { 0xD8, "Displayed name" }, { 0xE0, "Arc subtitle" }, { 0xE8, "Description" },
        { 0xF0, "Type text" },      { 0xF8, "Chat greeting" },
    };
    char cls[96], t[200];
    ClasseDe(d, cls, sizeof(cls));
    Indenter(profondeur); printf("[EVT] DILEMME 0x%llX %s\n", (unsigned long long)d, cls);
    for (int off = 0; off < 0x18; off += 8) {
        uintptr_t p = 0;
        if (SafeRead((void*)(d + off), &p, 8) && TexteDe(p, t, sizeof(t))) {
            Indenter(profondeur + 1); printf("+0x%02X \"%s\"\n", off, t);
        }
    }
    for (auto& x : TEXTES) {
        uintptr_t p = 0;
        SafeRead((void*)(d + x.off), &p, 8);
        Indenter(profondeur + 1);
        printf("%s +0x%X = 0x%llX \"%s\"\n", x.nom, x.off, (unsigned long long)p,
            TexteDe(p, t, sizeof(t)) ? t : "");
    }
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(d + DIL_CHOIX), &tab, 8);
    SafeRead((void*)(d + DIL_CHOIX + 8), &n, 4);
    Indenter(profondeur + 1); printf("Choix +0x78 : 0x%llX, %d\n", (unsigned long long)tab, n);
    for (int i = 0; i < n && i < 8 && tab; ++i) {
        uintptr_t c = 0;
        if (!SafeRead((void*)(tab + i * 8), &c, 8) || c <= 0x10000) continue;
        ClasseDe(c, cls, sizeof(cls));
        Indenter(profondeur + 2); printf("choix [%d] 0x%llX %s\n", i, (unsigned long long)c, cls);
        for (int off = 0; off < 0x100; off += 8) {
            uintptr_t p = 0;
            if (!SafeRead((void*)(c + off), &p, 8) || p <= 0x10000) continue;
            char c2[96];
            if (TexteDe(p, t, sizeof(t))) { Indenter(profondeur + 3); printf("+0x%02X \"%s\"\n", off, t); }
            else if (ClasseDe(p, c2, sizeof(c2))) { Indenter(profondeur + 3); printf("+0x%02X 0x%llX %s\n", off, (unsigned long long)p, c2); }
        }
    }
}

// Objet d'effet : classe, textes et objets RTTI de ses 0x80 premiers octets ;
// recursion dans les objets pointes et les tableaux de pointeurs.
void DecrireObjet(uintptr_t obj, int profondeur)
{
    if (profondeur > PROFONDEUR_MAX || ++g_lignesReleve > 400) return;
    char cls[96], t[200];
    if (!ClasseDe(obj, cls, sizeof(cls))) return;
    Indenter(profondeur); printf("[EVT] 0x%llX %s\n", (unsigned long long)obj, cls);

    if (!strcmp(cls, "FrostpunkCreateSurvivoursDilemmaEffect")) {
        uint8_t guid[16] = {};
        SafeRead((void*)(obj + EFFET_DILEMME_GUID), guid, sizeof(guid));
        Indenter(profondeur + 1); printf("lien de dilemme :");
        for (int i = 0; i < 16; ++i) printf(" %02X", guid[i]);
        printf("\n");
        uintptr_t d = DilemmeParGuid(guid);
        if (d) DecrireDilemme(d, profondeur + 1);
        else { Indenter(profondeur + 1); printf("dilemme non resolu\n"); }
        return;
    }
    for (int off = 0x08; off < 0x80; off += 8) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(obj + off), &p, 8) || p <= 0x10000) continue;
        char c2[96];
        if (TexteDe(p, t, sizeof(t))) { Indenter(profondeur + 1); printf("+0x%02X \"%s\"\n", off, t); continue; }
        if (ClasseDe(p, c2, sizeof(c2))) {
            Indenter(profondeur + 1); printf("+0x%02X ->\n", off);
            DecrireObjet(p, profondeur + 1);
            continue;
        }
        // Tableau de pointeurs : {ptr, compte 1..16}
        int32_t n = 0;
        if (!SafeRead((void*)(obj + off + 8), &n, 4) || n <= 0 || n > 16) continue;
        uintptr_t e0 = 0;
        if (!SafeRead((void*)p, &e0, 8) || !ClasseDe(e0, c2, sizeof(c2))) continue;
        Indenter(profondeur + 1); printf("+0x%02X tableau de %d\n", off, n);
        for (int i = 0; i < n; ++i) {
            uintptr_t e = 0;
            if (SafeRead((void*)(p + i * 8), &e, 8)) DecrireObjet(e, profondeur + 2);
        }
        off += 8;
    }
}

void ReleverEvenement(uintptr_t entree)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(entree + EVT_EFFETS), &tab, 8);
    SafeRead((void*)(entree + EVT_EFFETS + 8), &n, 4);
    g_lignesReleve = 0;
    for (int i = 0; i < n && i < 16 && tab; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(tab + i * 8), &e, 8)) DecrireObjet(e, 1);
    }
}

// Nom de l'evenement en cours d'execution (thread du jeu), pour les journaux
// d'autres modules (ex. quete demarree par un evenement, quetes.cpp).
char g_nomEvtEnCours[128] = "";

void hkInvoquerEvenement(uintptr_t sys, uintptr_t entree, uintptr_t contexte, uintptr_t forcer)
{
    char nom[128] = "?";
    uintptr_t p = 0;
    if (SafeRead((void*)(entree + EVT_NOM), &p, 8) && p > 0x10000) ReadAscii(p, nom, sizeof(nom));
    uintptr_t entite = EntiteDuContexte(contexte);

    int indice = IndiceSuivie(entite);
    bool suivie = indice >= 0;

    if (suivie || InterlockedIncrement(&g_logsEvt) <= MAX_LOGS_EVT) {
        char ctx[320] = "aucune";
        if (entite) DecrireEntiteContexte(entite, ctx, sizeof(ctx));
        printf("[EVT] %s \"%s\" (entree 0x%llX, forcer %d) contexte : %s\n",
            suivie ? "GROUPE AP" : "invoque", nom, (unsigned long long)entree, (int)(forcer & 0xFF), ctx);
        if (suivie) ReleverEvenement(entree);
        else DecrireEffets(entree);
    }
    // Les effets internes peuvent changer de contexte (SelectEffect sur les
    // survivants secourus) : on retient l'entite de l'evenement exterieur.
    int avant = g_suivieEnCours;
    if (suivie) g_suivieEnCours = indice;
    char nomAvant[128];
    strcpy_s(nomAvant, g_nomEvtEnCours);
    strcpy_s(g_nomEvtEnCours, nom);
    g_origInvoquer(sys, entree, contexte, forcer);
    strcpy_s(g_nomEvtEnCours, nomAvant);
    g_suivieEnCours = avant;
}

void SauverBulleGroupe(int id, uintptr_t copie);   // sauvegarde AP des bulles (plus bas)

// Hook de FUN_1417cde10(systeme des dilemmes, dilemme, contexte, p4) =
// creer un dilemme (bulle). Renvoie l'identifiant du dilemme ou -1.
uintptr_t hkCreerDilemme(uintptr_t sys, uintptr_t dilemme, uintptr_t contexte, uintptr_t p4)
{
    char nom[128] = "?", titre[128] = "";
    uintptr_t p = 0;
    if (SafeRead((void*)dilemme, &p, 8)) TexteDe(p, nom, sizeof(nom));
    uintptr_t remplace = 0;
    int i = g_suivieEnCours;
    if (i >= 0 && i < g_nSuivies && g_remplOrig[i] && g_remplOrig[i] == dilemme) remplace = g_remplNouveau[i];
    uintptr_t cree = remplace ? remplace : dilemme;
    if (SafeRead((void*)(cree + DIL_TITRE), &p, 8)) TexteDe(p, titre, sizeof(titre));
    printf("[DIL] creation du dilemme \"%s\" 0x%llX%s%llX, titre \"%s\" (evenement d'un groupe AP : %s)\n",
        nom, (unsigned long long)dilemme, remplace ? " REMPLACE par 0x" : " ", (unsigned long long)remplace,
        titre, i >= 0 ? "oui" : "non");
    uintptr_t r = g_origCreerDilemme(sys, cree, contexte, p4);
    printf("[DIL]   -> %lld\n", (long long)(int32_t)r);
    // Bulle d'arrivee d'un groupe AP : la sauvegarde du jeu la relirait comme
    // l'original (meme GUID) ; on garde ses textes dans notre sauvegarde.
    if (remplace && (int32_t)r >= 0) SauverBulleGroupe((int32_t)r, remplace);
    return r;
}

// --- Dilemmes Archipelago (pieges) -------------------------------------------
// Decompiles du 2026-10-28 :
// - FUN_1417d40c0(systeme, id, choix, drapeaux) = resoudre un dilemme
//   (frostpunkdilemmasystem.cpp) : dilemmes actifs en systeme +0x21A0
//   (enregistrements de 0x90, +0x10 = entree) ; choix -1 = aucun ; drapeaux
//   & 1 -> effets "ignores" (entree +0x60) ; sinon effets du choix (+0x78)
//   puis de son resultat ; log "[Dilemma][{0}] Dilemma resolved".
// - FUN_1417cde10 renvoie l'identifiant du dilemme cree (ou -1). Sans entite
//   dans le contexte, le marqueur (bulle) est pose sur la premiere entite de
//   *(DAT_14408dc20 + 0x2208). Contexte par defaut de la commande InvokeEvent :
//   DAT_1440b1b30 (= base + 0x40B1B30).
// Le piege est une copie de "[Beacon]S: Survivors group" (bulle avec compte a
// rebours) dont on vide TOUS les effets du jeu ; l'effet de chaque choix est
// applique par le hook de la resolution.
typedef uintptr_t(*ResoudreDilemme_t)(uintptr_t sys, uintptr_t id, uintptr_t choix, uintptr_t drapeaux);

constexpr uintptr_t OFF_RESOUDRE_DILEMME   = 0x17D40C0;
constexpr uintptr_t OFF_SYSTEME_DILEMMES   = 0x4095A98;
constexpr uintptr_t OFF_CONTEXTE_DEFAUT    = 0x40B1B30;
constexpr int       DIL_EFFETS_INITIAUX    = 0x48;
constexpr int       DIL_EFFETS_IGNORES     = 0x60;
constexpr int       CHOIX_EFFETS           = 0x78;
constexpr int       CHOIX_RESULTATS        = 0x90;
constexpr int       MAX_PIEGES             = 8;
// Type de sujet (FUN_1417ceae0, verification du contexte) : -1 = celui du
// template, 0 = global (contexte par defaut), 1 = entite, 2 = lieu.
constexpr int       DIL_SUJET              = 0x188;
// UITextureTile (0x18, FUN_141028100) : +0x00 texture, +0x08 case (x, y),
// +0x10 nombre de cases (x, y).
constexpr int       DIL_IMAGE_MARQUEUR     = 0x230;  // optionnelle : drapeau (+0x230) puis tuile
constexpr int       DIL_TUILE_MARQUEUR     = 0x238;
constexpr int       TAILLE_TUILE           = 0x18;
constexpr int       DIL_SOUS_TITRE         = 0xE0;   // Arc subtitle ("New Arrivals")
constexpr int32_t   SUJET_GLOBAL           = 0;

const uint8_t GUID_SURVIVANTS[16] = {
    0xFF, 0xDC, 0x2F, 0x34, 0xB3, 0x3E, 0x65, 0x9F, 0x85, 0x4F, 0x08, 0x02, 0x53, 0x01, 0x0C, 0x5A };

// Etat d'un dilemme AP cree : textes copies (traduits par hkTranslate tant
// que le slot vit) et definition d'origine (effets des choix).
struct PiegeSuivi {
    bool  actif;
    int   id;                 // identifiant du dilemme dans le systeme (-2 : resolu)
    const DilemmeAP* def;     // nullptr : bulle d'arrivee de groupe rechargee (pas d'effet AP)
    uintptr_t clone;          // notre copie du dilemme
    char  titre[128];
    char  sousTitre[64];      // sous-titre d'arc (+0xE0), "New Arrivals" sinon
    char  texte[1024];
    int   nChoix, choixSiIgnore;
    char  choix[MAX_CHOIX_DILEMME_AP][128];
};
PiegeSuivi        g_pieges[MAX_PIEGES] = {};
ResoudreDilemme_t g_origResoudre = nullptr;

// --- Sauvegarde AP des bulles (2026-10-29) -----------------------------------
// Une bulle AP en attente est sauvegardee par le jeu avec le GUID de
// l'original ("Survivors group") : au chargement, c'est l'original qui
// revient (textes et effets du jeu, plus de perte de piege). Decompile de
// FUN_1417cde10 : dilemmes actifs en systeme +0x21A0 (compte +0x21A8),
// enregistrements de 0x90 : +0x00 id (compteur du systeme +0x2198,
// sauvegarde), +0x10 entree du dilemme, +0x20 contexte, +0x78 date
// d'expiration, +0x80 marqueur. Le couple (id, expiration) identifie la bulle.
// Fichier mods\ap_dilemmes.txt, jamais efface :
//   P <id> <expiration 16 hex>\t<id du catalogue>            (piege)
//   G <id> <expiration 16 hex>\t<titre>\t<texte>\t<choix>    (arrivee de groupe)
constexpr int SYS_DIL_ACTIFS  = 0x21A0;
constexpr int ENR_DIL_TAILLE  = 0x90;
constexpr int ENR_DIL_ENTREE  = 0x10;
constexpr int ENR_DIL_EXPIRE  = 0x78;

bool CheminFichierBulles(char* out, size_t taille)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_dilemmes.txt", base);
    return true;
}

// Enregistrement du dilemme actif 'id', ou 0.
uintptr_t EnregistrementDilemme(int id)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sys = 0, tab = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(base + OFF_SYSTEME_DILEMMES), &sys, 8) || !sys ||
        !SafeRead((void*)(sys + SYS_DIL_ACTIFS), &tab, 8) || !SafeRead((void*)(sys + SYS_DIL_ACTIFS + 8), &n, 4) ||
        !tab || n <= 0 || n > 256) return 0;
    for (int i = 0; i < n; ++i) {
        int32_t v = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * ENR_DIL_TAILLE), &v, 4) && v == id) return tab + (uintptr_t)i * ENR_DIL_TAILLE;
    }
    return 0;
}

void EcrireChampBulle(FILE* f, const char* t)
{
    fputc('\t', f);
    for (; t && *t; ++t) fputc((*t == '\t' || *t == '\r' || *t == '\n') ? ' ' : *t, f);
}

// type 'P' : a = id du catalogue ; type 'G' : a = titre, b = texte, c = choix.
void SauverBulle(char type, int id, const char* a, const char* b, const char* c)
{
    uintptr_t enr = EnregistrementDilemme(id);
    int64_t expire = 0;
    if (!enr || !SafeRead((void*)(enr + ENR_DIL_EXPIRE), &expire, 8)) {
        printf("[DIL] sauvegarde de la bulle %d impossible (enregistrement 0x%llX)\n", id, (unsigned long long)enr);
        return;
    }
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichierBulles(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "a") != 0 || !f) {
        printf("[DIL] ecriture de mods\\ap_dilemmes.txt impossible\n");
        return;
    }
    fprintf(f, "%c %d %016llX", type, id, (unsigned long long)expire);
    EcrireChampBulle(f, a);
    if (type == 'G') { EcrireChampBulle(f, b); EcrireChampBulle(f, c); }
    else if (type == 'P' && b && b[0]) EcrireChampBulle(f, b);   // sous-titre (expediteur)
    fputc('\n', f);
    fclose(f);
    printf("[DIL] bulle %d (%c) enregistree, expiration 0x%llX\n", id, type, (unsigned long long)expire);
}

// Bulle d'arrivee d'un groupe AP : textes du slot du groupe, lus d'apres la
// cle de titre de la copie ("@AP/Groupe/<slot>/T").
void SauverBulleGroupe(int id, uintptr_t copie)
{
    uintptr_t cle = 0;
    char k[64] = "";
    if (!SafeRead((void*)(copie + DIL_TITRE), &cle, 8) || !TexteDe(cle, k, sizeof(k)) ||
        strncmp(k, "@AP/Groupe/", 11)) return;
    int slot = atoi(k + 11);
    const char* t = GroupeTexte(slot, 'T');
    const char* x = GroupeTexte(slot, 'X');
    const char* ch = GroupeTexte(slot, 'C');
    SauverBulle('G', id, t ? t : "", x ? x : "", ch ? ch : "");
}

// Tableau dynamique vide (ptr, compte, capacite a 0) : l'entree copiee
// n'execute plus les effets de l'original.
void ViderTableau(uintptr_t adr)
{
    uint8_t zero[16] = {};
    SafeWrite((void*)adr, zero, sizeof(zero));
}

const char* DilemmePiegeTexte(int slot, char champ, int indice)
{
    if (slot < 0 || slot >= MAX_PIEGES || !g_pieges[slot].actif) return nullptr;
    const PiegeSuivi& p = g_pieges[slot];
    if (champ == 'T') return p.titre;
    if (champ == 'X') return p.texte;
    if (champ == 'S') return p.sousTitre;
    if (champ == 'C' && indice >= 0 && indice < p.nChoix) return p.choix[indice];
    return nullptr;
}

// Texture d'une tuile : NameString (pointeur). On tente le texte pointe,
// puis le texte pointe par le premier qword pointe.
void DecrireTuile(const char* libelle, uintptr_t t)
{
    uintptr_t tex = 0, tex2 = 0;
    int32_t v[4] = {};
    char nom[160] = "";
    SafeRead((void*)t, &tex, 8);
    SafeRead((void*)(t + 8), v, sizeof(v));
    if (tex > 0x10000 && !TexteDe(tex, nom, sizeof(nom)) &&
        SafeRead((void*)tex, &tex2, 8) && tex2 > 0x10000) TexteDe(tex2, nom, sizeof(nom));
    printf("[DIL]   %s : texture 0x%llX \"%s\" case (%d,%d) sur (%d,%d)\n", libelle,
        (unsigned long long)tex, nom, v[0], v[1], v[2], v[3]);
}

// Tuile de l'icone 'icone' dans l'atlas des bulles. L'image du marqueur
// d'un dilemme est une tuile OPTIONNELLE : drapeau en +0x230, UITextureTile en
// +0x238 (texture +0x238, case +0x240, nombre de cases +0x248). Celle de
// "Survivors group" est la case (14,6) de "UI\Tiles\Icons.dds" (entree 2350
// de textures-s3, 16 x 16). Tests du 2026-10-28 : une autre texture (atlas de
// l'arbre) n'affiche RIEN dans la bulle. On reprend donc texture et nombre de
// cases de ce marqueur, avec la case (bulleCol, bulleRow) de l'icone, ou nos
// icones sont injectees par la substitution de l'entree 2350.
bool TuileAtlasBulles(const char* icone, uint8_t* tuile)
{
    uintptr_t orig = DilemmeParGuid(GUID_SURVIVANTS);
    uint8_t valide = 0;
    const TechIcons* ic = nullptr;
    for (const TechIcons& t : ARCHIPELAGO_ICONS_TECH) if (icone && !strcmp(t.nom, icone)) ic = &t;
    if (!orig || !ic || !SafeRead((void*)(orig + DIL_IMAGE_MARQUEUR), &valide, 1) || !valide ||
        !SafeRead((void*)(orig + DIL_TUILE_MARQUEUR), tuile, TAILLE_TUILE)) return false;
    int32_t cr[2] = { ic->bulleCol, ic->bulleRow };
    memcpy(tuile + 8, cr, 8);
    return true;
}

void PoserIconePiege(uintptr_t d, const char* icone)
{
    uint8_t tuile[TAILLE_TUILE];
    if (!TuileAtlasBulles(icone, tuile)) {
        printf("[DIL]   icone \"%s\" non posee (inconnue, ou marqueur d'origine absent)\n", icone ? icone : "?");
        return;
    }
    SafeWrite((void*)(d + DIL_TUILE_MARQUEUR), tuile, TAILLE_TUILE);
    DecrireTuile("tuile du marqueur du dilemme", d + DIL_TUILE_MARQUEUR);
}

// Copie de "Survivors group" pour le slot : titre, texte, n choix (copies du
// 1er choix d'origine, effets et resultats vides), effets du dilemme vides.
uintptr_t ConstruirePiege(int slot, int nChoix, const char* icone)
{
    uintptr_t orig = DilemmeParGuid(GUID_SURVIVANTS);
    uintptr_t tabOrig = 0, choixOrig = 0;
    if (!orig || !SafeRead((void*)(orig + DIL_CHOIX), &tabOrig, 8) || !tabOrig ||
        !SafeRead((void*)tabOrig, &choixOrig, 8) || !choixOrig || !MoteurAlloc) {
        printf("[DIL] piege : dilemme de base introuvable (0x%llX)\n", (unsigned long long)orig);
        return 0;
    }
    uintptr_t modele = 0;
    SafeRead((void*)(orig + DIL_TITRE), &modele, 8);

    char cle[48];
    snprintf(cle, sizeof(cle), "@AP/Dil/%d/T", slot);
    uintptr_t kTitre = CleMoteur(cle, modele);
    snprintf(cle, sizeof(cle), "@AP/Dil/%d/X", slot);
    uintptr_t kTexte = CleMoteur(cle, modele);
    snprintf(cle, sizeof(cle), "@AP/Dil/%d/S", slot);
    uintptr_t kSousTitre = CleMoteur(cle, modele);
    uintptr_t d = (uintptr_t)MoteurAlloc(TAILLE_DILEMME);
    uintptr_t tab = (uintptr_t)MoteurAlloc(nChoix * 8);
    if (!kTitre || !kTexte || !d || !tab || !SafeRead((void*)orig, (void*)d, TAILLE_DILEMME)) return 0;

    SafeWrite((void*)(d + DIL_TITRE), &kTitre, 8);
    SafeWrite((void*)(d + DIL_DESCRIPTION), &kTexte, 8);
    // Sous-titre d'arc : "New Arrivals" chez "Survivors group" (test F6).
    if (kSousTitre) SafeWrite((void*)(d + DIL_SOUS_TITRE), &kSousTitre, 8);
    // Sujet global : "Survivors group" exige une entite (le groupe) et la
    // creation echouait sans elle (test F6 du 2026-10-28 : id -1).
    int32_t sujet = SUJET_GLOBAL;
    SafeWrite((void*)(d + DIL_SUJET), &sujet, 4);
    PoserIconePiege(d, icone);
    ViderTableau(d + DIL_EFFETS_INITIAUX);
    ViderTableau(d + DIL_EFFETS_IGNORES);
    for (int i = 0; i < nChoix; ++i) {
        uintptr_t c = (uintptr_t)MoteurAlloc(TAILLE_CHOIX);
        snprintf(cle, sizeof(cle), "@AP/Dil/%d/C%d", slot, i);
        uintptr_t kChoix = CleMoteur(cle, modele);
        if (!c || !kChoix || !SafeRead((void*)choixOrig, (void*)c, TAILLE_CHOIX)) return 0;
        SafeWrite((void*)(c + CHOIX_TEXTE), &kChoix, 8);
        ViderTableau(c + CHOIX_EFFETS);
        ViderTableau(c + CHOIX_RESULTATS);
        SafeWrite((void*)(tab + i * 8), &c, 8);
    }
    int32_t compte[2] = { nChoix, nChoix };
    SafeWrite((void*)(d + DIL_CHOIX), &tab, 8);
    SafeWrite((void*)(d + DIL_CHOIX + 8), compte, 8);
    return d;
}

int DilemmeAPCreer(const DilemmeAP& def, const char* sousTitre)
{
    int nChoix = 0;
    while (nChoix < MAX_CHOIX_DILEMME_AP && def.choix[nChoix].texte) ++nChoix;
    if (nChoix < 1) { printf("[DIL] dilemme \"%s\" sans choix : ignore\n", def.id); return -1; }
    int slot = -1;
    for (int i = 0; i < MAX_PIEGES && slot < 0; ++i) if (!g_pieges[i].actif || g_pieges[i].id == -2) slot = i;
    if (slot < 0) { printf("[DIL] plus de slot libre (%d dilemmes en attente)\n", MAX_PIEGES); return -1; }

    // Les textes doivent exister avant la creation : l'UI peut les traduire tout de suite.
    PiegeSuivi& p = g_pieges[slot];
    memset(&p, 0, sizeof(p));
    p.def = &def;
    strncpy_s(p.titre, def.titre ? def.titre : "", _TRUNCATE);
    strncpy_s(p.texte, def.texte ? def.texte : "", _TRUNCATE);
    strncpy_s(p.sousTitre, sousTitre && sousTitre[0] ? sousTitre : def.sousTitre ? def.sousTitre : "Archipelago",
        _TRUNCATE);
    p.nChoix = nChoix;
    p.choixSiIgnore = (def.choixSiIgnore >= 0 && def.choixSiIgnore < nChoix) ? def.choixSiIgnore : 0;
    for (int i = 0; i < nChoix; ++i) strncpy_s(p.choix[i], def.choix[i].texte, _TRUNCATE);
    p.actif = true;

    uintptr_t d = ConstruirePiege(slot, nChoix, def.icone);
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sys = 0;
    SafeRead((void*)(base + OFF_SYSTEME_DILEMMES), &sys, 8);
    if (!d || !sys) {
        printf("[DIL] piege : construction impossible (dilemme 0x%llX, systeme 0x%llX)\n",
            (unsigned long long)d, (unsigned long long)sys);
        p.actif = false;
        return -1;
    }
    JournalImmediat("[DIL] piege slot %d : creation du dilemme 0x%llX (%d choix) ...\n", slot,
        (unsigned long long)d, nChoix);
    int32_t id = -1;
    __try {
        id = (int32_t)g_origCreerDilemme(sys, d, base + OFF_CONTEXTE_DEFAUT, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[DIL] EXCEPTION 0x%08lX a la creation du piege\n", GetExceptionCode());
        id = -1;
    }
    printf("[DIL] dilemme \"%s\" slot %d \"%s\" : dilemme id %d\n", def.id, slot, p.titre, id);
    if (id < 0) { p.actif = false; return -1; }
    p.id = id;
    p.clone = d;
    SauverBulle('P', id, def.id, p.sousTitre, nullptr);
    return id;
}

// Effet d'un choix : credit signe de la ressource (applique sur le thread du
// jeu par le hook du setter) puis popup, et l'effet special s'il y en a un.
void AppliquerChoixPiege(const PiegeSuivi& p, int i, bool ignore)
{
    const ChoixDilemmeAP& c = p.def->choix[i];
    printf("[DIL] dilemme \"%s\" : choix %d \"%s\"%s -> %s %+d%s\n", p.def->id, i, p.choix[i],
        ignore ? " (bulle ignoree)" : "", c.ressource ? c.ressource : "rien", c.quantite,
        c.effetSpecial ? " + effet special" : "");
    if (c.ressource && c.quantite != 0 && Res_Add(c.ressource, c.quantite)) {
        char texte[160];
        sprintf_s(texte, "%+d %s (%s)", c.quantite, c.ressource, p.titre);
        AfficherNotificationAP(p.def->titrePopup ? p.def->titrePopup : "Archipelago", texte,
            p.def->icone ? p.def->icone : "icon_vanilla");
    }
    if (c.effetSpecial) c.effetSpecial();
}

uintptr_t hkResoudreDilemme(uintptr_t sys, uintptr_t id, uintptr_t choix, uintptr_t drapeaux)
{
    int slot = -1;
    for (int i = 0; i < MAX_PIEGES && slot < 0; ++i)
        if (g_pieges[i].actif && g_pieges[i].id == (int32_t)id) slot = i;
    uintptr_t r = g_origResoudre(sys, id, choix, drapeaux);
    int32_t c = (int32_t)choix;
    if (slot >= 0) printf("[DIL] resolution du piege slot %d : choix %d, drapeaux 0x%llX, retour %d\n",
        slot, c, (unsigned long long)drapeaux, (int)(r & 0xFF));
    if (slot < 0 || !(r & 0xFF)) return r;

    PiegeSuivi& p = g_pieges[slot];
    bool ignore = c < 0 || c >= p.nChoix;
    if (p.def) AppliquerChoixPiege(p, ignore ? p.choixSiIgnore : c, ignore);
    else printf("[DIL] bulle de groupe rechargee \"%s\" resolue (effets du jeu)\n", p.titre);
    // Le slot reste actif (le panneau peut encore traduire ses textes) mais
    // plus aucune resolution ne le vise ; il est reutilisable.
    p.id = -2;
    return r;
}

// --- Recollage des bulles AP au chargement (2026-10-29) -----------------------
struct LigneBulle { char type; char a[1024]; char b[1024]; char c[128]; };
bool g_bullesReinit = false;   // slots des pieges remis a zero pour ce chargement

void ChampBulle(const char* ligne, int n, char* out, size_t taille)
{
    const char* p = strchr(ligne, '\t');
    for (int i = 0; p && i < n; ++i) p = strchr(p + 1, '\t');
    out[0] = 0;
    if (!p) return;
    ++p;
    size_t k = 0;
    while (p[k] && p[k] != '\t' && p[k] != '\n' && p[k] != '\r' && k + 1 < taille) { out[k] = p[k]; ++k; }
    out[k] = 0;
}

// Ligne (la derniere) de la bulle (id, expiration) dans notre sauvegarde.
bool ChercherBulle(int id, int64_t expire, LigneBulle& out)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichierBulles(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) return false;
    static char ligne[4096];
    char suffixe[48];
    _snprintf_s(suffixe, sizeof(suffixe), _TRUNCATE, " %d %016llX\t", id, (unsigned long long)expire);
    bool ok = false;
    while (fgets(ligne, sizeof(ligne), f)) {
        if ((ligne[0] != 'P' && ligne[0] != 'G') || strncmp(ligne + 1, suffixe, strlen(suffixe))) continue;
        out.type = ligne[0];
        ChampBulle(ligne, 0, out.a, sizeof(out.a));
        ChampBulle(ligne, 1, out.b, sizeof(out.b));
        ChampBulle(ligne, 2, out.c, sizeof(out.c));
        ok = true;
    }
    fclose(f);
    return ok;
}

// Table des occurrences du systeme des dilemmes (+0x21B8 : hachage indexe par
// l'ADRESSE de l'entree, elements de 0x38 = {entree, date, compte, ...}).
// FUN_1417cde10 y inscrit l'entree creee ; FUN_1417d40c0 (resolution) l'y
// cherche et prend l'element SANS verifier l'index (-1 -> lecture hors du
// tableau). Test du 2026-10-29 : plantage au clic sur un choix d'un piege
// recharge, car la table du chargement porte l'entree d'origine et non notre
// copie. On inscrit donc la copie, comme le fait la creation :
// FUN_1417cfc40(table, &entree) = chercher (-1 si absente),
// FUN_1417d16d0(table, &valeur) = inserer, valeur = {entree, DAT_142c2fca8,
// 0, 0, 0, 0}.
typedef int(*ChercherOccurrence_t)(uintptr_t table, const uint64_t* cle);
typedef int(*InsererOccurrence_t)(uintptr_t table, const uint64_t* valeur);
constexpr uintptr_t OFF_CHERCHER_OCCURRENCE = 0x17CFC40;
constexpr uintptr_t OFF_INSERER_OCCURRENCE  = 0x17D16D0;
constexpr uintptr_t OFF_DATE_NULLE          = 0x2C2FCA8;
constexpr int       SYS_DIL_OCCURRENCES     = 0x21B8;

bool InscrireOccurrence(uintptr_t sys, uintptr_t entree)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uint64_t valeur[6] = { entree, 0, 0, 0, 0, 0 };
    SafeRead((void*)(base + OFF_DATE_NULLE), &valeur[1], 8);
    int trouve = -1, insere = -1;
    __try {
        trouve = ((ChercherOccurrence_t)(base + OFF_CHERCHER_OCCURRENCE))(sys + SYS_DIL_OCCURRENCES, valeur);
        if (trouve < 0) insere = ((InsererOccurrence_t)(base + OFF_INSERER_OCCURRENCE))(sys + SYS_DIL_OCCURRENCES, valeur);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[DIL] EXCEPTION 0x%08lX a l'inscription de l'entree 0x%llX\n", GetExceptionCode(),
            (unsigned long long)entree);
        return false;
    }
    printf("[DIL]   table des occurrences : entree 0x%llX %s (index %d)\n", (unsigned long long)entree,
        trouve >= 0 ? "deja inscrite" : "inscrite", trouve >= 0 ? trouve : insere);
    return trouve >= 0 || insere >= 0;
}

// Pour chaque dilemme actif present dans notre sauvegarde : un slot, notre
// copie (piege : reconstruite depuis le catalogue ; arrivee de groupe :
// copie de "Survivors group" avec ses textes), posee dans l'enregistrement
// (+0x10) a la place de l'original. Thread du jeu.
void RecollerBulles(const char* quand)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sys = 0, tab = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(base + OFF_SYSTEME_DILEMMES), &sys, 8) || !sys ||
        !SafeRead((void*)(sys + SYS_DIL_ACTIFS), &tab, 8) || !SafeRead((void*)(sys + SYS_DIL_ACTIFS + 8), &n, 4) ||
        !tab || n <= 0 || n > 256) return;
    if (!g_bullesReinit) { memset(g_pieges, 0, sizeof(g_pieges)); g_bullesReinit = true; }
    int recollees = 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t enr = tab + (uintptr_t)i * ENR_DIL_TAILLE, entree = 0;
        int32_t id = 0;
        int64_t expire = 0;
        if (!SafeRead((void*)enr, &id, 4) || !SafeRead((void*)(enr + ENR_DIL_ENTREE), &entree, 8) ||
            !SafeRead((void*)(enr + ENR_DIL_EXPIRE), &expire, 8)) continue;
        bool deja = false;
        for (const PiegeSuivi& p : g_pieges) if (p.actif && p.clone && p.clone == entree) deja = true;
        LigneBulle L = {};
        if (deja || !ChercherBulle(id, expire, L)) continue;

        int slot = -1;
        for (int s = 0; s < MAX_PIEGES && slot < 0; ++s) if (!g_pieges[s].actif || g_pieges[s].id == -2) slot = s;
        if (slot < 0) { printf("[DIL] (%s) plus de slot pour la bulle %d\n", quand, id); break; }
        PiegeSuivi& p = g_pieges[slot];
        memset(&p, 0, sizeof(p));
        uintptr_t d = 0;
        if (L.type == 'P') {
            const DilemmeAP* def = DilemmeAPParId(L.a);
            if (!def) { printf("[DIL] (%s) bulle %d : \"%s\" absent du catalogue\n", quand, id, L.a); continue; }
            int nChoix = 0;
            while (nChoix < MAX_CHOIX_DILEMME_AP && def->choix[nChoix].texte) ++nChoix;
            p.def = def;
            strncpy_s(p.titre, def->titre ? def->titre : "", _TRUNCATE);
            strncpy_s(p.texte, def->texte ? def->texte : "", _TRUNCATE);
            // Sous-titre garde a la creation (expediteur), sinon celui du catalogue.
            strncpy_s(p.sousTitre, L.b[0] ? L.b : def->sousTitre ? def->sousTitre : "Archipelago", _TRUNCATE);
            p.nChoix = nChoix;
            p.choixSiIgnore = (def->choixSiIgnore >= 0 && def->choixSiIgnore < nChoix) ? def->choixSiIgnore : 0;
            for (int c = 0; c < nChoix; ++c) strncpy_s(p.choix[c], def->choix[c].texte, _TRUNCATE);
            p.actif = true;
            d = ConstruirePiege(slot, nChoix, def->icone);
        }
        else {
            strncpy_s(p.titre, L.a, _TRUNCATE);
            strncpy_s(p.texte, L.b, _TRUNCATE);
            strncpy_s(p.choix[0], L.c, _TRUNCATE);
            p.nChoix = 1;
            p.actif = true;
            char kT[32], kX[32], kC[32];
            snprintf(kT, sizeof(kT), "@AP/Dil/%d/T", slot);
            snprintf(kX, sizeof(kX), "@AP/Dil/%d/X", slot);
            snprintf(kC, sizeof(kC), "@AP/Dil/%d/C0", slot);
            d = DilemmeCloner(DilemmeParGuid(GUID_SURVIVANTS), kT, kX, kC);
        }
        if (!d) { printf("[DIL] (%s) bulle %d : copie impossible\n", quand, id); memset(&p, 0, sizeof(p)); continue; }
        // La copie doit etre connue de la table des occurrences AVANT d'etre
        // posee : sinon la resolution lit hors du tableau.
        if (!InscrireOccurrence(sys, d)) {
            printf("[DIL] (%s) bulle %d : inscription impossible, bulle laissee au jeu\n", quand, id);
            memset(&p, 0, sizeof(p));
            continue;
        }
        SafeWrite((void*)(enr + ENR_DIL_ENTREE), &d, 8);
        p.id = id;
        p.clone = d;
        ++recollees;
        printf("[DIL] (%s) bulle %d recollee dans le slot %d : %s \"%s\" (entree 0x%llX -> 0x%llX)\n", quand, id, slot,
            L.type == 'P' ? "piege" : "arrivee de groupe", p.titre, (unsigned long long)entree, (unsigned long long)d);
    }
    printf("[DIL] (%s) %d dilemme(s) actif(s), %d bulle(s) AP recollee(s)\n", quand, n, recollees);
}

// --- Icone de la bulle d'un piege recharge (2026-10-29) ----------------------
// La bulle en ville est dessinee par FrostpunkUIDillemaTracker (vtable
// 0x142217550). Sa mise a jour FUN_141c19460(tracker) ne pose l'icone QU'UNE
// FOIS (drapeau tracker +0x80) : marqueur = *(tracker +0x28) -> indice du
// dilemme FUN_1417cfbf0(systeme, marqueur) -> entree FUN_1417cfd70(systeme,
// indice) -> tuile entree +0x238 (si drapeau +0x230) posee sur l'image
// **(tracker +0x60) par FUN_141a33780(image, tuile). Au chargement, ce passage
// a lieu avant notre recollage (entree d'origine) et ne se refait jamais. On
// repose donc la tuile une fois pour chaque tracker dont l'entree est une de
// nos copies de piege.
typedef uintptr_t(*MajTrackerDilemme_t)(uintptr_t tracker);
typedef int(*IndiceDilemmeMarqueur_t)(uintptr_t sys, uintptr_t marqueur);
typedef uintptr_t(*EntreeDilemmeIndice_t)(uintptr_t sys, int indice);
typedef void(*PoserTuileImage_t)(uintptr_t image, uintptr_t tuile);
constexpr uintptr_t OFF_MAJ_TRACKER_DILEMME = 0x1C19460;
constexpr uintptr_t OFF_INDICE_MARQUEUR     = 0x17CFBF0;
constexpr uintptr_t OFF_ENTREE_INDICE       = 0x17CFD70;
constexpr uintptr_t OFF_POSER_TUILE_IMAGE   = 0x1A33780;
constexpr int       TRACKER_MARQUEUR        = 0x28;
constexpr int       TRACKER_IMAGE           = 0x60;
constexpr int       TRACKER_ICONE_POSEE     = 0x80;
MajTrackerDilemme_t g_origMajTracker = nullptr;
uintptr_t           g_trackersCorriges[32] = {};
int                 g_nTrackersCorriges = 0;

uintptr_t hkMajTrackerDilemme(uintptr_t tracker)
{
    uintptr_t r = g_origMajTracker(tracker);
    uint8_t posee = 0;
    if (!SafeRead((void*)(tracker + TRACKER_ICONE_POSEE), &posee, 1) || !posee) return r;
    for (int i = 0; i < g_nTrackersCorriges && i < 32; ++i) if (g_trackersCorriges[i] == tracker) return r;
    bool aPiege = false;
    for (const PiegeSuivi& p : g_pieges) if (p.actif && p.def && p.clone) aPiege = true;
    if (!aPiege) return r;

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sys = 0, poignee = 0, marqueur = 0, poigneeImg = 0, image = 0, entree = 0;
    __try {
        if (!SafeRead((void*)(base + OFF_SYSTEME_DILEMMES), &sys, 8) || !sys ||
            !SafeRead((void*)(tracker + TRACKER_MARQUEUR), &poignee, 8) || !poignee ||
            !SafeRead((void*)poignee, &marqueur, 8) || !marqueur) return r;
        int indice = ((IndiceDilemmeMarqueur_t)(base + OFF_INDICE_MARQUEUR))(sys, marqueur);
        if (indice < 0) return r;
        entree = ((EntreeDilemmeIndice_t)(base + OFF_ENTREE_INDICE))(sys, indice);
        bool notre = false;
        for (const PiegeSuivi& p : g_pieges) if (p.actif && p.def && p.clone && p.clone == entree) notre = true;
        if (!notre) return r;
        uint8_t drapeau = 0;
        if (!SafeRead((void*)(entree + DIL_IMAGE_MARQUEUR), &drapeau, 1) || !drapeau ||
            !SafeRead((void*)(tracker + TRACKER_IMAGE), &poigneeImg, 8) || !poigneeImg ||
            !SafeRead((void*)poigneeImg, &image, 8) || !image) return r;
        ((PoserTuileImage_t)(base + OFF_POSER_TUILE_IMAGE))(image, entree + DIL_TUILE_MARQUEUR);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[DIL] EXCEPTION 0x%08lX a la correction de l'icone d'une bulle\n", GetExceptionCode());
        return r;
    }
    g_trackersCorriges[g_nTrackersCorriges++ % 32] = tracker;
    printf("[DIL] icone de la bulle reposee (tracker 0x%llX, entree 0x%llX)\n", (unsigned long long)tracker,
        (unsigned long long)entree);
    return r;
}

void DilemmesAPChargement(int etape)
{
    if (etape == 0) { g_bullesReinit = false; return; }
    RecollerBulles(etape == 1 ? "fin de la lecture" : "sortie de LoadGame");
    if (etape == 2) g_bullesReinit = false;
}

void InstallHooksEvenements(uintptr_t modBase)
{
    void* cible = (void*)(modBase + OFF_INVOQUER_EVENEMENT);
    bool ok = MH_CreateHook(cible, (void*)&hkInvoquerEvenement, (void**)&g_origInvoquer) == MH_OK &&
              MH_EnableHook(cible) == MH_OK;
    printf("[EVT] hook invocation d'evenement (+%llX) %s\n", (unsigned long long)OFF_INVOQUER_EVENEMENT,
        ok ? "OK" : "KO");
    void* creer = (void*)(modBase + OFF_CREER_DILEMME);
    bool okC = MH_CreateHook(creer, (void*)&hkCreerDilemme, (void**)&g_origCreerDilemme) == MH_OK &&
               MH_EnableHook(creer) == MH_OK;
    printf("[DIL] hook creation de dilemme (+%llX) %s\n", (unsigned long long)OFF_CREER_DILEMME, okC ? "OK" : "KO");
    void* resoudre = (void*)(modBase + OFF_RESOUDRE_DILEMME);
    bool okR = MH_CreateHook(resoudre, (void*)&hkResoudreDilemme, (void**)&g_origResoudre) == MH_OK &&
               MH_EnableHook(resoudre) == MH_OK;
    printf("[DIL] hook resolution de dilemme (+%llX) %s\n", (unsigned long long)OFF_RESOUDRE_DILEMME, okR ? "OK" : "KO");
    void* tracker = (void*)(modBase + OFF_MAJ_TRACKER_DILEMME);
    bool okT = MH_CreateHook(tracker, (void*)&hkMajTrackerDilemme, (void**)&g_origMajTracker) == MH_OK &&
               MH_EnableHook(tracker) == MH_OK;
    printf("[DIL] hook mise a jour d'une bulle (+%llX) %s\n", (unsigned long long)OFF_MAJ_TRACKER_DILEMME, okT ? "OK" : "KO");
}
