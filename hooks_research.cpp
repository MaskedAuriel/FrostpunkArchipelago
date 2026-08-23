// hooks_research.cpp
#include "hooks_research.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "tech_brain.h"
#include "hooks_loc.h"
#include "MinHook.h"


// --- Pointeurs vers les originaux (internes a ce fichier) -------------------
typedef char (*CanResearch_t)(uintptr_t self, uintptr_t tech, uintptr_t ctx);
typedef void (*ResearchStart_t)(uintptr_t self, uintptr_t tech);
typedef void (*ResearchComplete_t)(uintptr_t self);
typedef char (*Cancel_t)(uintptr_t self);

static CanResearch_t      oCanResearch      = nullptr;
static ResearchStart_t    oResearchStart    = nullptr;
static ResearchComplete_t oResearchComplete = nullptr;
static Cancel_t           oCancel           = nullptr;

// ---------------------------------------------------------------------------
// CanResearch : decide si le clic sur une techno est accepte.
//
// Contrainte du jeu : il PLANTE si on evalue une techno a l'etat 2 (le chemin
// "TechnologyAlreadyResearched" n'est pas prevu pour etre re-parcouru). On
// abaisse donc l'etat le temps de l'appel original, puis on le restaure.
//
// Cas particulier : une techno a l'etat 2 ET en cours de recherche correspond
// a un clic d'ANNULATION. Cette combinaison fait planter le jeu quoi qu'on
// fasse -> on refuse l'annulation (decision assumee, cas rare).
// ---------------------------------------------------------------------------
static char hkCanResearch(uintptr_t self, uintptr_t tech, uintptr_t ctx)
{
    int32_t st = TechStateByPtr(self, tech);

    // Annulation d'une recherche portant sur une techno recue : interdite.
    if (st == 2 && IsInProgress(self, tech))
        return 0;                      // le clic ne fait rien

    // Demarrage sur une techno a l'etat 2 : abaisser le temps de l'evaluation.
    bool restore2 = false;
    if (st == 2) { WriteTechStateByPtr(self, tech, 0); restore2 = true; }

    char vanilla = oCanResearch(self, tech, ctx);

    if (restore2) WriteTechStateByPtr(self, tech, 2);

    uint64_t hash = HashOfTechPtr(tech);
    if (!hash) return vanilla;

    if (!RT_CanResearch(hash)) return 0;      // deja recherchee -> refus ferme
    if (restore2 && vanilla == 0) return 1;   // contourne le refus "etat 2"
    return vanilla;                           // sinon le jeu decide (prerequis)
}

// ---------------------------------------------------------------------------
// ResearchStart : le demarrage natif plante si la techno est a l'etat 2.
// On l'abaisse a 0 avant, on la restaure a 2 apres.
// ---------------------------------------------------------------------------
static void hkResearchStart(uintptr_t self, uintptr_t tech)
{
    if (self) g_selfTech = self;

    bool was2 = (TechStateByPtr(self, tech) == 2);
    if (was2) WriteTechStateByPtr(self, tech, 0);

    oResearchStart(self, tech);

    if (was2) WriteTechStateByPtr(self, tech, 2);
}

// ---------------------------------------------------------------------------
// ResearchComplete : c'est NOTRE location check.
// Ordre critique :
//   1. lire le hash AVANT l'original (il remet self+0x2260 a 0)
//   2. laisser le jeu terminer sa completion
//   3. APRES seulement, appliquer notre logique -> dernier mot sur l'etat
// ---------------------------------------------------------------------------
static void hkResearchComplete(uintptr_t self)
{
    if (self) g_selfTech = self;
    uintptr_t tech = 0; uint64_t hash = 0;
    if (SafeRead((void*)(self + TECH_ACTIVE_OFF), &tech, 8))
        hash = HashOfTechPtr(tech);

    OpenKeyLogWindow(10000);        // <-- AVANT, pas apres
    oResearchComplete(self);

    if (hash)
    {
        OnResearchComplete(hash);
        for (int i = 0; i < g_nTechnosAP; ++i) {
            if (g_technosAP[i].hash != hash) continue;
            RafraichirIconeTechno(g_technosAP[i]);
            break;
        }
    }

}

// ---------------------------------------------------------------------------
// CancelResearch : annulation normale (techno a l'etat 1). Le cas "etat 2 en
// cours" n'arrive plus, il est refuse en amont par hkCanResearch.
// ---------------------------------------------------------------------------
static char hkCancel(uintptr_t self)
{
    if (self) g_selfTech = self;

    uintptr_t tech = 0;
    if (self) SafeRead((void*)(self + TECH_ACTIVE_OFF), &tech, 8);
    uint64_t hash = HashOfTechPtr(tech);

    char ret = oCancel(self);

    // L'annulation native remet l'etat a 0 : on resynchronise selon received.
    if (hash) SyncGameState(RT_Get(hash));
    return ret;
}

// ---------------------------------------------------------------------------
void InstallResearchHooks(uintptr_t modBase)
{
    struct { uintptr_t off; void* detour; void** orig; const char* name; } H[] = {
        { GameOffsets::CanResearch,      (void*)&hkCanResearch,      (void**)&oCanResearch,      "CanResearch" },
        { GameOffsets::ResearchStart,    (void*)&hkResearchStart,    (void**)&oResearchStart,    "ResearchStart" },
        { GameOffsets::ResearchComplete, (void*)&hkResearchComplete, (void**)&oResearchComplete, "ResearchComplete" },
        { GameOffsets::CancelResearch,   (void*)&hkCancel,           (void**)&oCancel,           "CancelResearch" },
    };
    for (auto& h : H) {
        void* target = (void*)(modBase + h.off);
        if (MH_CreateHook(target, h.detour, h.orig) != MH_OK || MH_EnableHook(target) != MH_OK)
            printf("[!] hook %s KO\n", h.name);
    }
}
