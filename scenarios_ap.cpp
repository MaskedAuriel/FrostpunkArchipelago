// scenarios_ap.cpp
// ---------------------------------------------------------------------------
// Verrou des scenarios au menu principal (2026-11-01).
//
// Decompiles du 2026-11-01 (adresses Ghidra) :
//  - FUN_141283ea0(profil, liste) -> char = prerequis de profil satisfaits ?
//    liste = tableau dyn. de liens (GUID, pas 0x10, compte liste +8) vers des
//    "profile progress" ; 0 des qu'un lien resolu n'est pas acquis
//    (FUN_1412827b0). DAT_142c343cd (debug) = tout debloque.
//  - FrostpunkScenarioEntry : +0x00 nom interne, +0x18 visible au menu,
//    **+0x20 "Scenario unlock profile progress"** (la liste ci-dessus : le
//    jour 20 de A New Home, etc.), +0x38 liste du conseil "Maybe play Main
//    Scenario", +0x238 / +0x240 titre / description, +0x263 lien de DLC.
//    Scenarios : FrostpunkScenariosConfig (base + 0x2C34490) +0x88, compte
//    +0x90 (DAT_142c34518 / DAT_142c34520).
//  - FUN_141b2b1b0(panneau, etat) = FrostpunkUIMainMenuScenariosPanel, etat 0
//    (liste) : pour chaque scenario visible, debloque =
//    FUN_141283ea0(profil, scn + 0x20) ; seuls les debloques entrent dans la
//    liste selectionnable (panneau +0x198, compte +0x1A0 : fleches, details),
//    puis FUN_141b2cb40(panneau, scn, debloque, indice) cree le bouton :
//    FUN_14105a3a0(bouton, debloque, 1) (actif ?), et si verrouille presets
//    "LOCKED" et "LOCKED_<nom>" sans ecouteur de clic ; sinon clic ->
//    FUN_141b2d910 (lancement, via les details).
//  - Autres appelants de FUN_141283ea0 sur scn + 0x20 : FUN_141b2e010 (points
//    de la vue de details, memes indices que la liste), FUN_141b28f10 (bouton
//    "Scenarios" du menu principal actif s'il y a des sauvegardes ou un
//    scenario debloque, presets SCENARIO_UNLOCK / NEW_DLC_ON),
//    FUN_14131e800 (un scenario autre que le principal est-il debloque ?),
//    FUN_14131eca0 (notification de scenario debloque par un progres).
//
// Le hook ne change que la reponse pour la liste +0x20 d'un des 6 scenarios
// Archipelago : tous les appelants voient donc le meme verrou que le jeu.
//
// 1er test (2026-11-01) : verrou OK, mais le bouton affiche la condition du
// jeu (« necessite le jour 20 »). FUN_141b2cb40 joue, pour un verrouille,
// FUN_1410240d0(bouton, "LOCKED") puis FUN_1410240d0(bouton, "LOCKED_<nom>")
// (nom = scn +0x00) : le second preset, propre au scenario, est presume
// porter ce texte. -> pendant la creation du bouton d'un scenario verrouille
// PAR LE MOD, le preset "LOCKED_<nom>" etait saute (hook global de
// FUN_1410240d0). Valide, mais les icones AP des lois ont disparu dans la
// meme version (hook global des presets suspecte). Arbre releve :
// SCENARIO_BUTTON > MENU_BUTTON_LAYOUT > MENU_OPTION_CONTAINER >
// DISABLED_TEXT, TITLE_TEXT ("SCENARIO LOCKED"), MAIN_STORY_TEXT,
// UNLOCK_SCENARIO_TEXT ("UNLOCKED ON DAY 20 OF A NEW HOME").
// -> plus de hook des presets : apres la creation du bouton d'un scenario
// verrouille par le mod, le texte de UNLOCK_SCENARIO_TEXT est remplace par un
// espace (FUN_14111bef0 ; un texte vide ne remplace pas celui de la recette).
// Le bouton est le nouvel enfant de la colonne **(panneau + 0xD8) (ajoute a
// la fin, ou en tete pour le scenario principal).
//
// Difficulte imposee (2026-11-03, docs/chantiers/difficulte.md). Decisions de
// l'auteur : difficulte du YAML IMPOSEE et curseurs FIGES, personnalisation =
// toutes les lignes au niveau cible, 6 scenarios AP, ironman libre.
// Decompiles du panneau des scenarios :
//  - +0x140 : poignee du curseur principal ; +0x1C8 : paires {curseur de
//    FrostpunkUIConfig +0xDC0 (0xB8), widget du curseur de la ligne}, compte
//    +0x1D0 (FUN_141b2ac50) ; +0x198/+0x1A0/+0x1B0 : scenarios selectionnables
//    et indice du scenario affiche ; +0x1E0 : mode ironman.
//  - Widget curseur : +0x314 = niveau (0 Easy .. 3 Very Hard) ;
//    FUN_141518b70(widget, niveau, forcer) le pose (poignee + ecouteur : le
//    principal recopie sur les lignes, FUN_141b2d810).
//  - FUN_141b2e410 : a chaque changement de scenario, tous les curseurs a
//    Medium, visibilite des lignes, puis FUN_141b2e8c0(panneau, 0).
//  - FUN_141b2e8c0(panneau, ironman) : pose +0x1E0, bouton Customize actif
//    si pas ironman, masque flou. Aussi appelee par le bouton ironman
//    (FUN_141b2d7b0). -> point du verrou (apres l'original).
//  - FUN_141b2a9c0 (ApplyConfig, appele par Start via FUN_141b2ea10) : sans
//    ironman, applique le niveau +0x314 de chaque ligne au systeme de
//    difficulte (FUN_1412f9b20). -> filet : niveaux reecrits avant l'original.
//  - FUN_14105a3a0(element, actif, 1) : active / desactive un element.
// ---------------------------------------------------------------------------
#include "scenarios_ap.h"
#include "archipelago.h"
#include "mod_common.h"
#include "MinHook.h"
#include <cstring>

namespace {

// Regle n. 5 : 4 arguments declares (le jeu en passe 2).
typedef uintptr_t (*PrerequisProfil_t)(uintptr_t profil, uintptr_t liste, uintptr_t p3, uintptr_t p4);
typedef void      (*CreerBouton_t)(uintptr_t panneau, uintptr_t scn, uintptr_t debloque, uintptr_t indice);
typedef void      (*PoserTexte_t)(uintptr_t texte, const wchar_t* chaine, uintptr_t cfg, uintptr_t p4);

static const uintptr_t OFF_PREREQUIS_PROFIL = Adr(0x1283EA0);   // FUN_141283ea0
static const uintptr_t OFF_CREER_BOUTON     = Adr(0x1B2CB40);   // FUN_141b2cb40
static const uintptr_t OFF_POSER_TEXTE      = Adr(0x111BEF0);   // FUN_14111bef0 : texte brut
static const uintptr_t OFF_CFG_TEXTE        = Adr(0x2BFD628);
static const uintptr_t OFF_SCENARIOS_CONFIG = Adr(0x2C34490);
constexpr int PAN_COLONNE     = 0xD8;   // poignee de la colonne des boutons
constexpr int CFG_SCENARIOS   = 0x88;
constexpr int SCN_DEBLOCAGE   = 0x20;   // "Scenario unlock profile progress"
constexpr int EL_NOM          = 0x1D8;
constexpr int EL_ENFANT       = 0x220;
constexpr int EL_FRERE        = 0x240;
constexpr int EL_DRAPEAUX     = 0x270;  // bit 0 = visible
constexpr int TXT_TEXTE       = 0x3E8;  // texte affiche (vu sur UITextInput)

PrerequisProfil_t oPrerequisProfil = nullptr;
CreerBouton_t     oCreerBouton = nullptr;
uintptr_t g_base = 0;

// Scenarios geres par Archipelago (noms internes).
const char* const SCENARIOS_AP[] = {
    "DEFAULT", "TechHaven", "REFUGEES", "WINTERHOME", "BUILDERS", "Reconquest",
};

constexpr int NB_SCENARIOS_AP = (int)(sizeof(SCENARIOS_AP) / sizeof(SCENARIOS_AP[0]));

// Deblocages recus (item "Scenario unlock" par ReceiveScenarioUnlock), en
// memoire seulement : le serveur renvoie les items recus a chaque connexion.
// Ecrit par le thread de reception, lu par le thread du jeu (octets).
volatile bool g_debloque[NB_SCENARIOS_AP] = {};

int IndiceScenarioAP(const char* nom)
{
    for (int i = 0; i < NB_SCENARIOS_AP; ++i) if (strcmp(nom, SCENARIOS_AP[i]) == 0) return i;
    return -1;
}

bool EstScenarioAP(const char* nom) { return IndiceScenarioAP(nom) >= 0; }

bool LireNomScenario(uintptr_t scn, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)scn, &p, 8) || p <= 0x10000) return false;
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        if (c < 0x20 || c >= 0x7F) return false;
        out[i] = c;
    }
    out[i] = 0;
    return i > 0;
}

// Scenario dont la liste de deblocage est `liste`, ou 0.
uintptr_t ScenarioDeLaListe(uintptr_t liste)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(g_base + OFF_SCENARIOS_CONFIG + CFG_SCENARIOS), &tab, 8) ||
        !SafeRead((void*)(g_base + OFF_SCENARIOS_CONFIG + CFG_SCENARIOS + 8), &n, 4) ||
        tab <= 0x10000 || n <= 0 || n > 256) return 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t scn = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &scn, 8) && scn > 0x10000 && scn + SCN_DEBLOCAGE == liste)
            return scn;
    }
    return 0;
}

// Journal : une ligne par scenario et par changement de verdict.
struct Vu { char nom[32]; int verdict; };
Vu g_vus[32];
int g_nVus = 0;

void Journaliser(const char* nom, bool jeu, bool ap)
{
    int verdict = (jeu ? 1 : 0) | (ap ? 2 : 0);
    for (int i = 0; i < g_nVus; ++i) {
        if (strcmp(g_vus[i].nom, nom) != 0) continue;
        if (g_vus[i].verdict == verdict) return;
        g_vus[i].verdict = verdict;
        printf("[SCN] \"%s\" : jeu %s, Archipelago %s -> %s\n", nom, jeu ? "debloque" : "verrouille",
               ap ? "debloque" : "verrouille", (jeu && ap) ? "LANCABLE" : "VERROUILLE");
        return;
    }
    if (g_nVus < 32) {
        strcpy_s(g_vus[g_nVus].nom, sizeof(g_vus[g_nVus].nom), nom);
        g_vus[g_nVus].verdict = verdict;
        ++g_nVus;
    }
    printf("[SCN] \"%s\" : jeu %s, Archipelago %s -> %s\n", nom, jeu ? "debloque" : "verrouille",
           ap ? "debloque" : "verrouille", (jeu && ap) ? "LANCABLE" : "VERROUILLE");
}

uintptr_t hkPrerequisProfil(uintptr_t profil, uintptr_t liste, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oPrerequisProfil(profil, liste, p3, p4);
    uintptr_t scn = ScenarioDeLaListe(liste);
    if (!scn) return r;
    char nom[32];
    if (!LireNomScenario(scn, nom, sizeof(nom)) || !EstScenarioAP(nom)) return r;
    bool jeu = (r & 0xFF) != 0;
    bool ap = ScenarioDebloque(nom);
    Journaliser(nom, jeu, ap);
    // Le verrou du jeu (jour 20 de A New Home...) est remplace par celui
    // d'Archipelago : seul l'item decide.
    return ap ? 1 : 0;
}

// --- Bouton d'un scenario verrouille par le mod -----------------------------------
bool NomElementEst(uintptr_t el, const char* nom)
{
    char n[64] = "";
    uintptr_t pn = 0;
    if (!SafeRead((void*)(el + EL_NOM), &pn, 8) || pn <= 0x10000) return false;
    ReadAscii(pn, n, sizeof(n));
    return strcmp(n, nom) == 0;
}

// Premier descendant nomme 'nom' (profondeur d'abord), 'el' compris.
uintptr_t ChercherDescendant(uintptr_t el, const char* nom, int prof)
{
    if (el < 0x10000 || prof > 10) return 0;
    if (NomElementEst(el, nom)) return el;
    uintptr_t enfant = 0;
    SafeRead((void*)(el + EL_ENFANT), &enfant, 8);
    for (int garde = 0; enfant > 0x10000 && garde < 64; ++garde) {
        uintptr_t r = ChercherDescendant(enfant, nom, prof + 1);
        if (r) return r;
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
    return 0;
}

uintptr_t Colonne(uintptr_t panneau)
{
    uintptr_t p = 0, col = 0;
    if (!SafeRead((void*)(panneau + PAN_COLONNE), &p, 8) || p <= 0x10000 ||
        !SafeRead((void*)p, &col, 8) || col <= 0x10000) return 0;
    return col;
}

int ListerEnfants(uintptr_t el, uintptr_t* out, int max)
{
    int n = 0;
    uintptr_t enfant = 0;
    SafeRead((void*)(el + EL_ENFANT), &enfant, 8);
    while (enfant > 0x10000 && n < max) {
        out[n++] = enfant;
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
    return n;
}

void hkCreerBouton(uintptr_t panneau, uintptr_t scn, uintptr_t debloque, uintptr_t indice)
{
    char nom[32];
    bool verrouAP = (debloque & 0xFF) == 0 && LireNomScenario(scn, nom, sizeof(nom)) &&
                    EstScenarioAP(nom) && !ScenarioDebloque(nom);
    if (!verrouAP) { oCreerBouton(panneau, scn, debloque, indice); return; }

    uintptr_t col = Colonne(panneau);
    uintptr_t avant[128];
    int nAvant = col ? ListerEnfants(col, avant, 128) : 0;
    oCreerBouton(panneau, scn, debloque, indice);
    if (!col) return;

    // Le nouvel enfant de la colonne = l'enveloppe du bouton cree.
    uintptr_t apres[128], bouton = 0;
    int nApres = ListerEnfants(col, apres, 128);
    for (int i = 0; i < nApres && !bouton; ++i) {
        bool vu = false;
        for (int j = 0; j < nAvant && !vu; ++j) vu = apres[i] == avant[j];
        if (!vu) bouton = apres[i];
    }
    uintptr_t texte = bouton ? ChercherDescendant(bouton, "UNLOCK_SCENARIO_TEXT", 0) : 0;
    if (!texte) {
        printf("[SCN] \"%s\" : UNLOCK_SCENARIO_TEXT introuvable (bouton 0x%llX)\n", nom, (unsigned long long)bouton);
        return;
    }
    __try {
        ((PoserTexte_t)(g_base + OFF_POSER_TEXTE))(texte, L" ", g_base + OFF_CFG_TEXTE, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[SCN] EXCEPTION 0x%08lX en vidant la condition de \"%s\"\n", GetExceptionCode(), nom);
        return;
    }
}

// --- Difficulte imposee ------------------------------------------------------------
typedef void (*Ironman_t)(uintptr_t panneau, uintptr_t ironman, uintptr_t p3, uintptr_t p4);
typedef void (*PoserNiveau_t)(uintptr_t widget, uintptr_t niveau, uintptr_t forcer, uintptr_t p4);
typedef void (*ActiverElement_t)(uintptr_t element, uintptr_t actif, uintptr_t p3, uintptr_t p4);

static const uintptr_t OFF_IRONMAN          = Adr(0x1B2E8C0);   // FUN_141b2e8c0
static const uintptr_t OFF_POSER_NIVEAU     = Adr(0x1518B70);   // FUN_141518b70
static const uintptr_t OFF_ACTIVER_ELEMENT  = Adr(0x105A3A0);   // FUN_14105a3a0
constexpr int PAN_CURSEUR_PRINCIPAL = 0x140;
constexpr int PAN_SCENARIOS         = 0x198;
constexpr int PAN_NB_SCENARIOS      = 0x1A0;
constexpr int PAN_INDICE            = 0x1B0;
constexpr int PAN_LIGNES            = 0x1C8;  // paires {curseur, widget}, compte +0x1D0
constexpr int PAN_IRONMAN           = 0x1E0;
constexpr int CUR_NIVEAU            = 0x314;

// Difficulte generale du YAML (0 Easy, 1 Medium, 2 Hard, 3 Very Hard),
// option goal_difficulty du slot_data (DifficulteReglerCible). -1 = pas de
// verrou. Avant le slot_data : Hard (TEMPORAIRE, ancienne DIFFICULTE_TEST).
int g_difficulteCible = 2;
const char* const NOMS_NIVEAUX[] = { "Easy", "Medium", "Hard", "Very Hard" };

Ironman_t oIronman = nullptr;
bool g_curseursFiges = false;   // les curseurs ont ete desactives par le mod

// Scenario affiche par le panneau (nom interne), sinon false.
bool ScenarioDuPanneau(uintptr_t panneau, char* nom, int taille)
{
    nom[0] = 0;
    uintptr_t tab = 0, scn = 0;
    int32_t n = 0, i = -1;
    if (!SafeRead((void*)(panneau + PAN_SCENARIOS), &tab, 8) ||
        !SafeRead((void*)(panneau + PAN_NB_SCENARIOS), &n, 4) ||
        !SafeRead((void*)(panneau + PAN_INDICE), &i, 4) ||
        tab <= 0x10000 || i < 0 || i >= n || n > 256 ||
        !SafeRead((void*)(tab + (uintptr_t)i * 8), &scn, 8) || scn <= 0x10000) return false;
    return LireNomScenario(scn, nom, taille);
}

// Niveau impose pour le scenario affiche, ou -1 (hors AP, ironman, pas de cible).
int NiveauImpose(uintptr_t panneau, char* nom, int taille)
{
    if (g_difficulteCible < 0 || g_difficulteCible > 3) return -1;
    if (!ScenarioDuPanneau(panneau, nom, taille) || !EstScenarioAP(nom)) return -1;
    uint8_t ironman = 0;
    if (!SafeRead((void*)(panneau + PAN_IRONMAN), &ironman, 1) || ironman) return -1;
    return g_difficulteCible;
}

// Widgets des curseurs : principal puis lignes. Renvoie le nombre.
int WidgetsCurseurs(uintptr_t panneau, uintptr_t* out, int max)
{
    int n = 0;
    uintptr_t p = 0, w = 0;
    if (SafeRead((void*)(panneau + PAN_CURSEUR_PRINCIPAL), &p, 8) && p > 0x10000 &&
        SafeRead((void*)p, &w, 8) && w > 0x10000 && n < max) out[n++] = w;
    uintptr_t tab = 0;
    int32_t nb = 0;
    if (!SafeRead((void*)(panneau + PAN_LIGNES), &tab, 8) ||
        !SafeRead((void*)(panneau + PAN_LIGNES + 8), &nb, 4) || tab <= 0x10000 || nb <= 0 || nb > 64) return n;
    for (int i = 0; i < nb && n < max; ++i)
        if (SafeRead((void*)(tab + (uintptr_t)i * 0x10 + 8), &w, 8) && w > 0x10000) out[n++] = w;
    return n;
}

// Pose (ou retire) le verrou sur les curseurs du panneau.
void VerrouillerCurseurs(uintptr_t panneau)
{
    char nom[32];
    int niveau = NiveauImpose(panneau, nom, sizeof(nom));
    uintptr_t w[65];
    int n = WidgetsCurseurs(panneau, w, 65);
    if (niveau < 0) {
        if (!g_curseursFiges) return;
        g_curseursFiges = false;
        __try {
            for (int i = 0; i < n; ++i) ((ActiverElement_t)(g_base + OFF_ACTIVER_ELEMENT))(w[i], 1, 1, 0);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            printf("[DIFF] EXCEPTION 0x%08lX en liberant les curseurs\n", GetExceptionCode());
            return;
        }
        printf("[DIFF] \"%s\" : curseurs liberes (%d)\n", nom[0] ? nom : "?", n);
        return;
    }
    __try {
        // Principal d'abord (son ecouteur recopie sur les lignes), puis
        // chaque ligne explicitement, puis desactivation.
        for (int i = 0; i < n; ++i) ((PoserNiveau_t)(g_base + OFF_POSER_NIVEAU))(w[i], (uintptr_t)niveau, 1, 0);
        for (int i = 0; i < n; ++i) ((ActiverElement_t)(g_base + OFF_ACTIVER_ELEMENT))(w[i], 0, 1, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[DIFF] EXCEPTION 0x%08lX en figeant les curseurs de \"%s\"\n", GetExceptionCode(), nom);
        return;
    }
    g_curseursFiges = true;
    printf("[DIFF] \"%s\" : %d curseurs figes a %s\n", nom, n, NOMS_NIVEAUX[niveau]);
}

void hkIronman(uintptr_t panneau, uintptr_t ironman, uintptr_t p3, uintptr_t p4)
{
    oIronman(panneau, ironman, p3, p4);
    VerrouillerCurseurs(panneau);
}

} // namespace

// Filet au lancement : quel que soit l'etat des curseurs, chaque ligne
// est reecrite au niveau impose juste avant l'application. Appele par le
// hook d'ApplyConfig de scenario.cpp (un seul hook possible par fonction :
// le second hook pose le 2026-11-03 faisait echouer celui de scenario.cpp).
void DifficulteImposerAuLancement(uintptr_t panneau)
{
    char nom[32];
    int niveau = NiveauImpose(panneau, nom, sizeof(nom));
    if (niveau >= 0) {
        uintptr_t w[65];
        int n = WidgetsCurseurs(panneau, w, 65), corriges = 0;
        for (int i = 0; i < n; ++i) {
            int32_t v = -1;
            if (!SafeRead((void*)(w[i] + CUR_NIVEAU), &v, 4)) continue;
            if (v != niveau) {
                int32_t nv = niveau;
                if (SafeWrite((void*)(w[i] + CUR_NIVEAU), &nv, 4)) ++corriges;
            }
        }
        printf("[DIFF] lancement de \"%s\" : difficulte imposee %s (%d curseurs, %d corriges)\n", nom,
               NOMS_NIVEAUX[niveau], n, corriges);
    }
    else if (nom[0]) {
        uint8_t ironman = 0;
        SafeRead((void*)(panneau + PAN_IRONMAN), &ironman, 1);
        printf("[DIFF] lancement de \"%s\" : difficulte libre (%s)\n", nom,
               ironman ? "ironman" : "hors Archipelago");
    }
}

void DifficulteReglerCible(int niveau)
{
    g_difficulteCible = (niveau >= 0 && niveau <= 3) ? niveau : -1;
    printf("[DIFF] difficulte cible : %s\n", g_difficulteCible >= 0 ? NOMS_NIVEAUX[g_difficulteCible] : "aucune");
}

int DifficulteCible()
{
    return (g_difficulteCible >= 0 && g_difficulteCible <= 3) ? g_difficulteCible : -1;
}

// TEMPORAIRE (2026-11-10, demande de l'auteur) : tous les scenarios
// debloques, le temps du releve des dilemmes d'exploration dans chaque
// scenario ([DILTAB] / [SCONT]). Remis a false pour la release 0.3
// (2026-11-11, accord de l'auteur).
static const bool TOUS_DEBLOQUES_TEMPORAIRE = false;

bool ScenarioDebloque(const char* nomInterne)
{
    if (TOUS_DEBLOQUES_TEMPORAIRE) return true;
    if (!nomInterne) return true;
    int i = IndiceScenarioAP(nomInterne);
    return i < 0 || g_debloque[i];
}

int ScenarioDebloquer(const char* nomInterne)
{
    int i = nomInterne ? IndiceScenarioAP(nomInterne) : -1;
    if (i < 0) return -1;
    if (g_debloque[i]) return 0;
    g_debloque[i] = true;
    return 1;
}

void ScenariosReverrouiller()
{
    // Changement de session (graine / slot) : les deblocages de la session
    // precedente ne valent plus ; ceux de la nouvelle sont rejoues par
    // ApDefinirSession. (Remplace ScenariosRecevoirItemsDeTest, TEMPORAIRE :
    // les items de deblocage viennent desormais du serveur.)
    for (int i = 0; i < NB_SCENARIOS_AP; ++i) g_debloque[i] = false;
}

void InstallHooksScenarios(uintptr_t modBase)
{
    g_base = modBase;
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_PREREQUIS_PROFIL, (void*)&hkPrerequisProfil, (void**)&oPrerequisProfil, "des prerequis de profil" },
        { OFF_CREER_BOUTON, (void*)&hkCreerBouton, (void**)&oCreerBouton, "du bouton de scenario" },
        { OFF_IRONMAN, (void*)&hkIronman, (void**)&oIronman, "du mode ironman (verrou de difficulte)" },    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[SCN] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}
