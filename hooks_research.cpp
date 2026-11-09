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

// --- AddResearchedTechnology (FUN_141325e40, decompile le 2026-11-04) -------
// (systeme, techno, p3) : si l'etat n'est pas deja 2, etat = 2, effets de la
// techno (FUN_141326650 : modificateurs...) puis ecouteurs de l'evenement
// "techno recherchee" (systeme +0x2108, compteur +0x2120). ResearchComplete
// (FUN_141327120) remet l'etat a 0 puis l'appelle.
// Bug du 2026-11-04 (ScoutsLimit recue, pas recherchee : pas de 2e equipe) :
// le mod ecrivait l'etat 2 a la reception, ce qui debloque les batiments
// (lus sur l'etat) mais n'applique NI les effets NI les ecouteurs. Desormais
// la reception passe par cette fonction (TechAppliquerEffets), et une
// recherche terminee d'une techno deja recue ne la rappelle pas (effets
// appliques une seule fois).
typedef uintptr_t (*AjouterRecherchee_t)(uintptr_t self, uintptr_t tech, uintptr_t p3, uintptr_t p4);
static AjouterRecherchee_t oAjouterRecherchee = nullptr;
static const uintptr_t OFF_AJOUTER_RECHERCHEE = Adr(0x1325E40);
static thread_local uintptr_t t_completionTech = 0;   // techno de la ResearchComplete en cours

static uintptr_t hkAjouterRecherchee(uintptr_t self, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    if (tech && tech == t_completionTech) {
        uint64_t h = HashOfTechPtr(tech);
        TechRuntime* r = h ? RT_Get(h) : nullptr;
        if (r && r->received && TechEstUnCheck(h)) {
            // Effets deja appliques a la reception : pas une 2e fois. L'etat
            // final (2) est pose par OnResearchComplete (SyncGameState).
            printf("[AP] recherche d'une techno deja recue : effets non reappliques\n");
            return 1;                  // "AddResearchedTechnology should succeed"
        }
        // Recherche d'une techno AP pas encore recue (2026-11-07, auteur) : le
        // menu des batiments ne doit PAS passer au bleu a la recherche, mais a
        // la reception -> notification (3e argument) coupee ici.
        if (r && TechEstUnCheck(h)) return oAjouterRecherchee(self, tech, 0, p4);
    }
    return oAjouterRecherchee(self, tech, p3, p4);
}

bool TechAppliquerEffets(uint64_t hash)
{
    uintptr_t self = g_selfTech, tech = FindTechPtrByHash(hash);
    if (!self || !tech || !oAjouterRecherchee) return false;
    if (TechStateByPtr(self, tech) == 2) return true;     // deja faite (sauvegarde)
    bool ok = true;
    // 3e argument : transmis aux ecouteurs "techno recherchee"
    // (ResearchComplete passe 1). 2026-11-06 : essai a 0 -> plus de bleu du
    // tout. Voulu par l'auteur (2026-11-07) : bleu a la RECEPTION, pas a la
    // recherche -> 1 ici, 0 pour la recherche (hkAjouterRecherchee).
    __try { oAjouterRecherchee(self, tech, 1, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    if (!ok) printf("[AP] EXCEPTION en appliquant les effets de la techno %016llX\n", (unsigned long long)hash);
    return ok && TechStateByPtr(self, tech) == 2;
}

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

    // Techno sans check dans ce scenario (techno de depart de Winterhome ou
    // On the Edge, decision de l'auteur du 2026-11-03) : le jeu decide seul ;
    // a l'etat 2 elle est deja recherchee, donc refusee.
    uint64_t h0 = HashOfTechPtr(tech);
    if (h0 && !TechEstUnCheck(h0))
        return st == 2 ? 0 : oCanResearch(self, tech, ctx);

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

    t_completionTech = tech;            // lu par hkAjouterRecherchee
    oResearchComplete(self);
    t_completionTech = 0;

    // Icone "icon_done" et [Researched] : posees sur le noeud de l'arbre deja
    // construit (sinon seulement a sa prochaine reconstruction).
    if (hash) {
        OnResearchComplete(hash);
        IconesTechnoRafraichirNoeud(tech);
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
        { OFF_AJOUTER_RECHERCHEE,        (void*)&hkAjouterRecherchee, (void**)&oAjouterRecherchee, "AddResearchedTechnology" },
    };
    for (auto& h : H) {
        void* target = (void*)(modBase + h.off);
        if (MH_CreateHook(target, h.detour, h.orig) != MH_OK || MH_EnableHook(target) != MH_OK)
            printf("[!] hook %s KO\n", h.name);
    }
    InstallHooksIconesTechnos(modBase);
}
