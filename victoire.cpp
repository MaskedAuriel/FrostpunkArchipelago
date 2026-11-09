// victoire.cpp
// ---------------------------------------------------------------------------
// Fin de partie et victoire Archipelago (2026-11-01, revu le 2026-11-04).
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
//  - Difficulte (lue jusqu'au 2026-11-04 pour les anciens checks de
//    victoire, plus utilisee) : FrostpunkUIConfig (base + 0x40615C0) +0xDC0
//    "DifficultySliders" (objets de 0xB8 : +0x08 4 niveaux de 0x20, +0x88 /
//    +0xA0 scenarios visibles / invisibles) ; FrostpunkDifficultySystem +0x30
//    difficultes en vigueur, +0x48 IronmanMode.
//  - Fins de partie : FrostpunkGameConfig (base + 0x2C38DE0, deduit de
//    PhotoModeCameraEffects +0x1E0 = DAT_142c38fc0 dans FUN_141ba79e0 ;
//    reflexion FUN_1412af2b0) +0xB0 "Game endings" = pointeurs de
//    FrostpunkGameEndingEntry (compte +0xB8). Collection des fins
//    (FUN_1412adde0) = DAT_142c38e70.
//  - Test (Maj / Ctrl + F9) : EndGame(*(base + 0x2C30158), fin, 0) sur le
//    thread du jeu ; cinematique nulle = journal du jeu « Game ended with no
//    ending provided », sans autre effet relu dans le decompile.
//
// Decision de l'auteur (2026-11-04) : la difficulte est imposee a la creation
// de la partie (scenarios_ap.cpp) et l'ironman ne change rien : toute victoire
// dans un scenario Archipelago compte pour le goal, sans lecture du niveau.
// ---------------------------------------------------------------------------
#include "victoire.h"
#include "archipelago.h"
#include "mod_common.h"
#include "MinHook.h"
#include <cstring>

namespace {

// Regle n. 5 : 4 arguments declares (le jeu en passe 3).
typedef void (*EndGame_t)(uintptr_t etat, uintptr_t fin, uintptr_t cinematique, uintptr_t p4);

static const uintptr_t OFF_END_GAME   = Adr(0x1301490);   // FUN_141301490
static const uintptr_t OFF_ETAT_JEU   = Adr(0x2C30158);   // DAT_142c30158 : FrostpunkGameStateGameplay*
static const uintptr_t OFF_FINS       = Adr(0x2C38E90);   // FrostpunkGameConfig +0xB0 "Game endings"
constexpr int ETAT_PHASE           = 0x50;
constexpr int ETAT_FIN_EN_ATTENTE  = 0x58;
constexpr int FIN_VICTOIRE         = 0x18;
constexpr int SCN_SYS_SCENARIO     = 0x68;        // ScenarioSystem "CurrentScenario"

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

// Nom interne du scenario en cours (ScenarioSystem +0x68 -> +0x00).
bool ScenarioEnCours(char* nom, int taille)
{
    nom[0] = 0;
    uintptr_t sys = SystemeParId(IdSysteme::Scenario), scn = 0;
    if (!sys || !SafeRead((void*)(sys + SCN_SYS_SCENARIO), &scn, 8) || scn <= 0x10000) return false;
    return LireTexte(scn, nom, taille);
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
        ScenarioEnCours(nomScn, sizeof(nomScn));
        bool acceptee = fin > 0x10000 && phase == 3 && enAttente == 0;
        printf("[FIN] fin de partie \"%s\" : %s (scenario %s)%s\n", nomFin, victoire ? "VICTOIRE" : "DEFAITE",
               nomScn, acceptee ? "" : " -- refusee par le jeu (phase ou fin deja en attente)");
        if (acceptee && victoire) {
            // Victoire = evenement du goal, sans item (apworld, Q7 c).
            const ScenarioAP* s = ScenarioAPDe(nomScn);
            if (s) SendScenarioVictory(s->interne, s->affiche);
            else printf("[FIN]   scenario hors Archipelago : rien\n");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[FIN] EXCEPTION 0x%08lX a la lecture de la fin de partie\n", GetExceptionCode());
    }
    oEndGame(etat, fin, cinematique, p4);
}

} // namespace

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
