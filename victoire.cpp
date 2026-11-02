// victoire.cpp
// ---------------------------------------------------------------------------
// Fin de partie et checks de victoire (2026-11-01).
//
// Decompiles du 2026-11-01 (adresses Ghidra) :
//  - FUN_141301490(etat, fin, cinematique) = FrostpunkGameStateGameplay::
//    EndGame (frostpunkgamestategameplay.cpp). Seul appelant : l'effet
//    FrostpunkEndGameEffect (FUN_14168dea0 : fin = lien +0x18, cinematique =
//    lien +0x28). etat = DAT_142c30158 : +0x50 phase (3 = GAMEPLAY, sinon
//    refus), +0x58 fin en attente (une seconde demande est ignoree). Puis
//    ecouteurs DAT_142c2fed8 (compte DAT_142c2fee0) et cinematique.
//  - FrostpunkGameEndingEntry (0x20, reflexion FUN_1412b02d0) : +0x00 nom,
//    +0x08 GUID, **+0x18 Victory** (bool).
//  - ScenarioSystem +0x78 = "Ignored_LastScenarioInSessionVictory" (souvenir
//    de session, PAS un drapeau de victoire en direct : l'ancienne
//    CheckScenarioVictory etait fausse).
//  - Difficulte : FrostpunkUIConfig (base + 0x40615C0, reflexion
//    FUN_14130ef90) +0xDC0 "DifficultySliders" = Dynarray<FrostpunkDifficulty
//    SliderDefinition> (objets integres de 0xB8, compte +0xDC8) ; +0xDD8
//    "Ironman Difficulty Entries" ; +0xDF0 "Main difficulty slider
//    descriptions" (4 niveaux). Curseur (FUN_141977ee0) : +0x00
//    DifficultyName (cle de texte), +0x08 "Difficulty levels" = 4 x 0x20
//    (eSliderDifficultyLevel : 0 Easy, 1 Medium, 2 Hard, 3 Very Hard ;
//    niveau : +0x00 lien GUID vers un FrostpunkDifficultyEntry, +0x18
//    Description), +0x88 "Visible in scenarios" (liens 0x10, compte +0x90,
//    vide = tous), +0xA0 "Invisible in scenarios" (compte +0xA8).
//    En partie, la difficulte choisie = FrostpunkDifficultySystem +0x30
//    (paires {reglage, difficulte}, voir scenario.cpp) : le niveau d'une
//    ligne est celui dont le GUID est l'une des difficultes en vigueur.
//    Le niveau general n'est stocke nulle part en partie : on le deduit
//    (plus faible des lignes, decision de l'auteur du 2026-11-01).
//  - Fins de partie : FrostpunkGameConfig (base + 0x2C38DE0, deduit de
//    PhotoModeCameraEffects +0x1E0 = DAT_142c38fc0 dans FUN_141ba79e0 ;
//    reflexion FUN_1412af2b0) +0xB0 "Game endings" = pointeurs de
//    FrostpunkGameEndingEntry (compte +0xB8). Collection des fins
//    (FUN_1412adde0) = DAT_142c38e70.
//  - Test (Maj / Ctrl + F9) : EndGame(*(base + 0x2C30158), fin, 0) sur le
//    thread du jeu ; cinematique nulle = journal du jeu « Game ended with no
//    ending provided », sans autre effet relu dans le decompile.
// ---------------------------------------------------------------------------
#include "victoire.h"
#include "archipelago.h"
#include "scenario.h"
#include "quetes.h"          // TraduireCleJeu
#include "mod_common.h"
#include "MinHook.h"
#include <cstring>

namespace {

// Regle n. 5 : 4 arguments declares (le jeu en passe 3).
typedef void (*EndGame_t)(uintptr_t etat, uintptr_t fin, uintptr_t cinematique, uintptr_t p4);

constexpr uintptr_t OFF_END_GAME   = 0x1301490;   // FUN_141301490
constexpr uintptr_t OFF_ETAT_JEU   = 0x2C30158;   // DAT_142c30158 : FrostpunkGameStateGameplay*
constexpr uintptr_t OFF_FINS       = 0x2C38E90;   // FrostpunkGameConfig +0xB0 "Game endings"
constexpr uintptr_t OFF_UI_CONFIG  = 0x40615C0;   // FrostpunkUIConfig
constexpr int UI_CURSEURS          = 0xDC0;       // "DifficultySliders"
constexpr int CURSEUR_TAILLE       = 0xB8;
constexpr int CURSEUR_NOM          = 0x00;
constexpr int CURSEUR_NIVEAUX      = 0x08;
constexpr int NIVEAU_TAILLE        = 0x20;
constexpr int CURSEUR_VISIBLE      = 0x88;
constexpr int CURSEUR_INVISIBLE    = 0xA0;
constexpr int ETAT_PHASE           = 0x50;
constexpr int ETAT_FIN_EN_ATTENTE  = 0x58;
constexpr int FIN_VICTOIRE         = 0x18;
constexpr int SCN_SYS_SCENARIO     = 0x68;        // ScenarioSystem "CurrentScenario"
constexpr int NB_NIVEAUX           = 4;
constexpr int MAX_LIGNES           = 32;

// TEMPORAIRE : mode de test, en attendant les options du slot.
constexpr ModeVictoire MODE_TEST = ModeVictoire::Detail;

const char* const NOMS_NIVEAUX[NB_NIVEAUX] = { "Easy", "Medium", "Hard", "Very Hard" };

// Scenarios Archipelago : nom interne -> nom affiche (anglais, texte en jeu).
struct ScenarioAP { const char* interne; const char* affiche; };
const ScenarioAP SCENARIOS_AP[] = {
    { "DEFAULT",    "A New Home" },
    { "TechHaven",  "The Arks" },
    { "REFUGEES",   "Refugees" },
    { "WINTERHOME", "The Fall of Winterhome" },
    { "BUILDERS",   "The Last Autumn" },
    { "Reconquest", "On The Edge" },
};

EndGame_t oEndGame = nullptr;
uintptr_t g_base = 0;

// Checks deja envoyes pendant cette session (le serveur dedoublonne aussi).
char g_envoyes[256][96];
int  g_nEnvoyes = 0;

struct Guid { uint64_t a, b; };
bool GuidEgal(const Guid& x, const Guid& y) { return x.a == y.a && x.b == y.b; }
bool GuidNul(const Guid& g) { return !g.a && !g.b; }

bool LireTexte(uintptr_t adr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adr, &p, 8) || p <= 0x10000) return false;
    return ReadAscii(p, out, taille) && out[0];
}

const ScenarioAP* ScenarioAPDe(const char* interne)
{
    for (const ScenarioAP& s : SCENARIOS_AP) if (strcmp(s.interne, interne) == 0) return &s;
    return nullptr;
}

// Scenario en cours : entree du scenario (ScenarioSystem +0x68), nom, GUID.
uintptr_t ScenarioEnCours(char* nom, int taille, Guid& guid)
{
    nom[0] = 0;
    guid = {};
    uintptr_t sys = SystemeParId(IdSysteme::Scenario), scn = 0;
    if (!sys || !SafeRead((void*)(sys + SCN_SYS_SCENARIO), &scn, 8) || scn <= 0x10000) return 0;
    LireTexte(scn, nom, taille);
    SafeRead((void*)(scn + 0x08), &guid, sizeof(guid));
    return scn;
}

// GUID des difficultes en vigueur (une par reglage).
int DifficultesEnVigueur(Guid* out, int max)
{
    uintptr_t sys = FindDifficultySystem(), tab = 0;
    uint32_t n = 0;
    if (!sys || !SafeRead((void*)(sys + 0x30), &tab, 8) || !SafeRead((void*)(sys + 0x38), &n, 4) ||
        tab <= 0x10000 || n > 64) return 0;
    int k = 0;
    for (uint32_t i = 0; i < n && k < max; ++i) {
        uintptr_t dif = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 0x10 + 8), &dif, 8) || dif <= 0x10000) continue;
        if (SafeRead((void*)(dif + 0x08), &out[k], sizeof(Guid)) && !GuidNul(out[k])) ++k;
    }
    return k;
}

bool ListeContient(uintptr_t champ, const Guid& g, bool* vide)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)champ, &tab, 8);
    SafeRead((void*)(champ + 8), &n, 4);
    *vide = (tab <= 0x10000 || n <= 0);
    if (*vide || n > 64) return false;
    for (int i = 0; i < n; ++i) {
        Guid e{};
        if (SafeRead((void*)(tab + (uintptr_t)i * 0x10), &e, sizeof(e)) && GuidEgal(e, g)) return true;
    }
    return false;
}

// Une ligne de sous-difficulte du scenario, avec le niveau joue.
struct Ligne {
    char id[64];        // dernier segment de la cle (stable, nom du check)
    char affiche[96];   // nom traduit (journal, popup)
    int  niveau;        // 0..3, -1 si aucun niveau ne correspond
};

int LignesDuScenario(const Guid& scn, Ligne* out, int max)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(g_base + OFF_UI_CONFIG + UI_CURSEURS), &tab, 8) ||
        !SafeRead((void*)(g_base + OFF_UI_CONFIG + UI_CURSEURS + 8), &n, 4) || tab <= 0x10000 ||
        n <= 0 || n > MAX_LIGNES) {
        printf("[FIN] curseurs de difficulte illisibles\n");
        return 0;
    }
    Guid enVigueur[64];
    int nv = DifficultesEnVigueur(enVigueur, 64);
    int k = 0;
    for (int i = 0; i < n && k < max; ++i) {
        uintptr_t c = tab + (uintptr_t)i * CURSEUR_TAILLE;
        bool vide = false;
        bool visible = ListeContient(c + CURSEUR_VISIBLE, scn, &vide) || vide;
        bool videInv = false;
        if (ListeContient(c + CURSEUR_INVISIBLE, scn, &videInv)) visible = false;
        if (!visible) continue;

        Ligne& l = out[k];
        char cle[128] = "";
        LireTexte(c + CURSEUR_NOM, cle, sizeof(cle));
        const char* seg = strrchr(cle, '/');
        strcpy_s(l.id, sizeof(l.id), seg ? seg + 1 : (cle[0] ? cle : "?"));
        TraduireCleJeu(c + CURSEUR_NOM, l.affiche, sizeof(l.affiche));
        l.niveau = -1;
        for (int j = 0; j < NB_NIVEAUX && l.niveau < 0; ++j) {
            Guid g{};
            if (!SafeRead((void*)(c + CURSEUR_NIVEAUX + j * NIVEAU_TAILLE), &g, sizeof(g)) || GuidNul(g)) continue;
            for (int m = 0; m < nv; ++m) if (GuidEgal(g, enVigueur[m])) { l.niveau = j; break; }
        }
        ++k;
    }
    return k;
}

void EnvoyerUneFois(const char* check)
{
    for (int i = 0; i < g_nEnvoyes; ++i)
        if (strcmp(g_envoyes[i], check) == 0) {
            printf("[FIN] check \"%s\" deja envoye dans cette session\n", check);
            return;
        }
    if (g_nEnvoyes < 256) strcpy_s(g_envoyes[g_nEnvoyes++], sizeof(g_envoyes[0]), check);
    SendVictoryCheck(check);
}

void Victoire(const ScenarioAP& s, const Guid& scn)
{
    Ligne lignes[MAX_LIGNES];
    int n = LignesDuScenario(scn, lignes, MAX_LIGNES);
    int general = n > 0 ? NB_NIVEAUX - 1 : -1;
    for (int i = 0; i < n; ++i) {
        printf("[FIN]   ligne \"%s\" (\"%s\") : niveau %s\n", lignes[i].id, lignes[i].affiche,
               lignes[i].niveau >= 0 ? NOMS_NIVEAUX[lignes[i].niveau] : "inconnu");
        if (lignes[i].niveau < general) general = lignes[i].niveau;
    }
    printf("[FIN]   %d ligne(s), niveau general = %s, mode %s\n", n,
           general >= 0 ? NOMS_NIVEAUX[general] : "inconnu",
           MODE_TEST == ModeVictoire::Complete ? "Complete" : MODE_TEST == ModeVictoire::General ? "General" : "Detail");

    char check[96];
    switch (VictoireMode()) {
    case ModeVictoire::Complete:
        sprintf_s(check, "%s - Victory", s.affiche);
        EnvoyerUneFois(check);
        break;
    case ModeVictoire::General:
        for (int j = 0; j <= general; ++j) {
            sprintf_s(check, "%s - Victory (%s)", s.affiche, NOMS_NIVEAUX[j]);
            EnvoyerUneFois(check);
        }
        break;
    case ModeVictoire::Detail:
        for (int i = 0; i < n; ++i)
            for (int j = 0; j <= lignes[i].niveau; ++j) {
                sprintf_s(check, "%s - Victory (%s: %s)", s.affiche, lignes[i].id, NOMS_NIVEAUX[j]);
                EnvoyerUneFois(check);
            }
        break;
    }
}

void hkEndGame(uintptr_t etat, uintptr_t fin, uintptr_t cinematique, uintptr_t p4)
{
    __try {
        int32_t phase = -1;
        uintptr_t enAttente = 0;
        SafeRead((void*)(etat + ETAT_PHASE), &phase, 4);
        SafeRead((void*)(etat + ETAT_FIN_EN_ATTENTE), &enAttente, 8);
        char nomFin[96] = "?", nomScn[64] = "?";
        uint8_t victoire = 0;
        if (fin > 0x10000) {
            LireTexte(fin, nomFin, sizeof(nomFin));
            SafeRead((void*)(fin + FIN_VICTOIRE), &victoire, 1);
        }
        Guid gScn{};
        ScenarioEnCours(nomScn, sizeof(nomScn), gScn);
        bool acceptee = fin > 0x10000 && phase == 3 && enAttente == 0;
        printf("[FIN] fin de partie \"%s\" : %s (scenario %s)%s\n", nomFin, victoire ? "VICTOIRE" : "DEFAITE",
               nomScn, acceptee ? "" : " -- refusee par le jeu (phase ou fin deja en attente)");
        if (acceptee && victoire) {
            const ScenarioAP* s = ScenarioAPDe(nomScn);
            if (s) Victoire(*s, gScn);
            else printf("[FIN]   scenario hors Archipelago : aucun check\n");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[FIN] EXCEPTION 0x%08lX a la lecture de la fin de partie\n", GetExceptionCode());
    }
    oEndGame(etat, fin, cinematique, p4);
}

} // namespace

ModeVictoire VictoireMode() { return MODE_TEST; }

// --- DEBUG : fin de partie de test ----------------------------------------------
namespace {
volatile LONG g_testDemande = 0;   // 0 rien, 1 defaite, 2 victoire
}

void VictoireDemanderTest(bool victoire)
{
    InterlockedExchange(&g_testDemande, victoire ? 2 : 1);
    printf("[FIN] test demande (%s) : il s'execute sur le thread du jeu (jeu hors pause)\n",
           victoire ? "victoire" : "defaite");
}

void VictoirePompe()
{
    LONG d = InterlockedExchange(&g_testDemande, 0);
    if (!d) return;
    bool victoire = d == 2;
    uintptr_t tab = 0, etat = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(g_base + OFF_FINS), &tab, 8) || !SafeRead((void*)(g_base + OFF_FINS + 8), &n, 4) ||
        tab <= 0x10000 || n <= 0 || n > 256) {
        printf("[FIN] test : fins de partie illisibles\n");
        return;
    }
    uintptr_t choisie = 0;
    printf("[FIN] test : %d fin(s) de partie dans la config :\n", n);
    for (int i = 0; i < n; ++i) {
        uintptr_t fin = 0;
        uint8_t v = 0;
        char nom[96] = "?";
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &fin, 8) || fin <= 0x10000) continue;
        LireTexte(fin, nom, sizeof(nom));
        SafeRead((void*)(fin + FIN_VICTOIRE), &v, 1);
        printf("[FIN]   %2d \"%s\" : %s\n", i, nom, v ? "victoire" : "defaite");
        if (!choisie && (v != 0) == victoire) choisie = fin;
    }
    if (!choisie) {
        printf("[FIN] test : aucune fin de type %s\n", victoire ? "victoire" : "defaite");
        return;
    }
    if (!SafeRead((void*)(g_base + OFF_ETAT_JEU), &etat, 8) || etat <= 0x10000) {
        printf("[FIN] test : etat de jeu illisible\n");
        return;
    }
    int32_t phase = -1;
    SafeRead((void*)(etat + ETAT_PHASE), &phase, 4);
    if (phase != 3) {
        printf("[FIN] test : partie hors phase GAMEPLAY (phase %d), rien\n", phase);
        return;
    }
    // Passe par le hook : meme chemin qu'une vraie fin.
    __try {
        ((EndGame_t)(g_base + OFF_END_GAME))(etat, choisie, 0, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[FIN] test : EXCEPTION 0x%08lX dans EndGame\n", GetExceptionCode());
    }
}

void InstallHooksVictoire(uintptr_t modBase)
{
    g_base = modBase;
    void* p = (void*)(modBase + OFF_END_GAME);
    bool ok = MH_CreateHook(p, (void*)&hkEndGame, (void**)&oEndGame) == MH_OK && MH_EnableHook(p) == MH_OK;
    printf("[FIN] hook de la fin de partie (+%llX) %s\n", (unsigned long long)OFF_END_GAME, ok ? "OK" : "KO");
}
