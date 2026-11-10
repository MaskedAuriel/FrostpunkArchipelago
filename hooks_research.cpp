// hooks_research.cpp
#include "hooks_research.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "tech_brain.h"
#include "hooks_loc.h"
#include "known_techs.h"   // TechByHash (journal [TECHPRE])
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
// Prerequis lus sur la RECHERCHE, pas sur la reception (demande de l'auteur,
// 2026-11-10). L'etat du jeu d'une techno AP vaut 2 seulement si elle est
// recue (SyncGameState) ; or le jeu lit cet etat pour les prerequis de ses
// enfants (parent, palier TierUnlockN) : le check suivant d'un arbre n'etait
// accessible qu'apres l'item. Pendant l'evaluation (CanResearch, et
// ResearchStart qui peut la refaire), les technos AP recherchees mais pas
// recues (etat 0) passent a l'etat 2, puis reviennent a 0. Jamais la techno
// evaluee elle-meme (le jeu plante en evaluant une techno a l'etat 2).
// Liste des pointeurs recalculee au plus toutes les 500 ms (CanResearch est
// appelee pour chaque noeud de l'arbre) ; l'etat est reverifie avant ecriture.
// ---------------------------------------------------------------------------
static const int MAX_ELEVEES = 256;

static int EleverRecherchees(uintptr_t self, uintptr_t cible, uintptr_t* elevees)
{
    static uintptr_t cache[MAX_ELEVEES];
    static int nCache = 0;
    static DWORD prochain = 0;
    if (GetTickCount() >= prochain) {
        prochain = GetTickCount() + 500;
        nCache = 0;
        for (int i = 0; i < RT_Count() && nCache < MAX_ELEVEES; ++i) {
            TechRuntime* r = RT_At(i);
            if (!r || !r->researched || r->received || !TechEstUnCheck(r->hash)) continue;
            uintptr_t t = FindTechPtrByHash(r->hash);
            if (t) cache[nCache++] = t;
        }
    }
    int n = 0;
    for (int i = 0; i < nCache; ++i) {
        uintptr_t t = cache[i];
        if (t == cible || TechStateByPtr(self, t) != 0) continue;
        if (WriteTechStateByPtr(self, t, 2)) elevees[n++] = t;
    }
    return n;
}

// Bug du 2026-11-10 (The Arks : Steam WallDrill et Improved Heaters grises,
// debloques au rechargement) : un item peut etre RECU pendant l'appel du jeu
// (le jeu modifie une ressource -> hook du setter -> pompe des items ->
// OnItemReceived -> etat 2). Le rabaissement remettait alors a 0 une techno
// recue (journal : WallDrill recue l. 5815, etat 0 l. 5879). Une techno
// recue entre-temps garde son etat 2.
static void RabaisserRecherchees(uintptr_t self, const uintptr_t* elevees, int n)
{
    for (int i = 0; i < n; ++i) {
        uint64_t h = HashOfTechPtr(elevees[i]);
        TechRuntime* r = h ? RT_Get(h) : nullptr;
        if (r && r->received) {
            printf("[AP] %s recue pendant l'evaluation : etat 2 garde\n", TechByHash(h) ? TechByHash(h)->name : "?");
            continue;
        }
        WriteTechStateByPtr(self, elevees[i], 0);
    }
}

// DIAG (2026-11-10, Steam Sawmill non recherchable, Sawmill recherchee mais
// pas recue) : une ligne par techno AP quand la reponse du jeu change :
// reponse, parents eleves, parent (table) recherche / recu / etat du jeu.
static void JournalCanResearch(uintptr_t self, uint64_t hash, char vanilla, int nElevees)
{
    static uint64_t vus[128];
    static char reponses[128];
    static int nVus = 0;
    int k = 0;
    for (; k < nVus; ++k) if (vus[k] == hash) break;
    if (k < nVus && reponses[k] == vanilla) return;
    if (k == nVus) { if (nVus >= 128) return; vus[nVus++] = hash; }
    reponses[k] = vanilla;
    const TechInfo* t = TechByHash(hash);
    const TechInfo* p = (t && t->parent) ? TechByHash(t->parent) : nullptr;
    TechRuntime* rp = p ? RT_Get(p->hash) : nullptr;
    uintptr_t pp = p ? FindTechPtrByHash(p->hash) : 0;
    printf("[TECHPRE] %s : jeu %d, %d techno(s) recherchee(s) elevee(s) ; parent %s (recherche %d, recu %d, etat %d), palier %d\n",
        t ? t->name : "?", (int)vanilla, nElevees, p ? p->name : "-", rp && rp->researched ? 1 : 0,
        rp && rp->received ? 1 : 0, pp ? TechStateByPtr(self, pp) : -1, t ? t->tier : -1);
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

    // Prerequis : parents recherches comptes comme faits (2026-11-10).
    uintptr_t elevees[MAX_ELEVEES];
    int nElevees = EleverRecherchees(self, tech, elevees);
    char vanilla = oCanResearch(self, tech, ctx);
    RabaisserRecherchees(self, elevees, nElevees);

    if (restore2) WriteTechStateByPtr(self, tech, 2);

    uint64_t hash = HashOfTechPtr(tech);
    if (!hash) return vanilla;
    JournalCanResearch(self, hash, vanilla, nElevees);

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

    // Prerequis : parents recherches comptes comme faits (2026-11-10).
    uintptr_t elevees[MAX_ELEVEES];
    int nElevees = EleverRecherchees(self, tech, elevees);
    oResearchStart(self, tech);
    RabaisserRecherchees(self, elevees, nElevees);

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
// Arbre des technos : un noeud n'est SELECTIONNABLE que si FUN_141bc3cb0 le
// juge disponible, par FUN_141bc5100(panneau, techno, 1) (prerequis lus sur
// l'etat du jeu) ; CanResearch n'est appelee qu'une fois le noeud
// selectionne (bouton du panneau, FUN_141bc9b30). Test de l'auteur du
// 2026-11-10 (Steam Sawmill non selectionnable, Sawmill recherchee mais pas
// recue ; aucune ligne [TECHPRE]) -> meme elevation autour de
// FUN_141bc5100 et du rafraichissement d'un noeud FUN_141bca5a0(panneau,
// GUID de la techno) (liens du noeud : "NOT_AVAILABLE" si le parent n'est
// pas a l'etat 2). La techno du noeud elle-meme n'est jamais elevee.
// Decompiles du 2026-11-10 (Epic) ; Steam : 0x1A65E40, 0x1A6B2E0.
typedef uintptr_t (*Fonction4R_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static const uintptr_t OFF_NOEUD_DISPONIBLE = Adr(0x1BC5100), OFF_RAFRAICHIR_NOEUD = Adr(0x1BCA5A0);
static Fonction4R_t oNoeudDisponible = nullptr, oRafraichirNoeud = nullptr;

// DIAG (2026-11-10, "Generator Upgrade - less coal" grise alors que
// CanResearch l'accepte) : pour l'appel de profondeur 1 (FUN_141bc3cb0),
// une ligne par techno AP quand la reponse change : reponse, technos
// elevees, et chaque prerequis "techno" (tech +0x38, compte +0x40 ; type
// vtable +0xE0 == 4 ; lien GUID +0x30, hash = 8 premiers octets) avec son
// etat PENDANT l'appel (apres elevation).
// 2026-11-10 (The Arks, "Boiler Upgrade" grise, Boilers et TierUnlock2 recus) :
// une seule ligne au depart (indisponible, prerequis a 0), puis plus rien
// alors que l'arbre a ete reconstruit -> on journalise aussi quand la
// signature des prerequis (hash, pointeur, etat pendant l'appel) change, et
// le hash et le pointeur de chaque prerequis (doublon de techno ?).
static void JournalNoeudDisponible(uintptr_t self, uintptr_t tech, uintptr_t r, int nElevees)
{
    uint64_t hash = HashOfTechPtr(tech);
    if (!hash || !TechEstUnCheck(hash)) return;
    static uint64_t vus[256];
    static uint64_t signatures[256];
    static int nVus = 0;
    char rep = (char)(r & 0xFF);
    const TechInfo* t = TechByHash(hash);
    char reqs[768] = "";
    uint64_t sig = 1469598103934665603ULL ^ (uint64_t)(unsigned char)rep;
    uintptr_t tab = 0;
    int32_t nReq = 0;
    SafeRead((void*)(tech + 0x38), &tab, 8);
    SafeRead((void*)(tech + 0x40), &nReq, 4);
    for (int i = 0; tab > 0x10000 && i < nReq && i < 8; ++i) {
        uintptr_t req = 0, vt = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &req, 8) || req <= 0x10000 || !SafeRead((void*)req, &vt, 8)) continue;
        int type = -1;
        __try { type = ((int(*)(uintptr_t))(*(uintptr_t*)(vt + 0xE0)))(req); }
        __except (EXCEPTION_EXECUTE_HANDLER) { type = -2; }
        uint64_t h = 0;
        SafeRead((void*)(req + 0x30), &h, 8);
        const TechInfo* rt = (type == 4 && h) ? TechByHash(h) : nullptr;
        uintptr_t rp = (type == 4 && h) ? FindTechPtrByHash(h) : 0;
        int etat = rp ? TechStateByPtr(self, rp) : -1;
        sig = (sig ^ h) * 1099511628211ULL;
        sig = (sig ^ (uint64_t)(etat + 2)) * 1099511628211ULL;
        char un[160];
        _snprintf_s(un, sizeof(un), _TRUNCATE, "%s[type %d] %s %016llX (0x%llX) etat %d", i ? " ; " : "", type,
            rt ? rt->name : (type == 4 ? "?" : "-"), (unsigned long long)h, (unsigned long long)rp, etat);
        strcat_s(reqs, un);
    }
    int k = 0;
    for (; k < nVus; ++k) if (vus[k] == hash) break;
    if (k < nVus && signatures[k] == sig) return;
    if (k == nVus) { if (nVus >= 256) return; vus[nVus++] = hash; }
    signatures[k] = sig;
    printf("[TECHUI] %s : disponible %d, %d elevee(s) ; prerequis : %s\n", t ? t->name : "?", (int)rep, nElevees,
        reqs[0] ? reqs : "(aucun)");
}

static uintptr_t hkNoeudDisponible(uintptr_t panneau, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    uintptr_t self = g_selfTech;
    if (!self) return oNoeudDisponible(panneau, tech, p3, p4);
    uintptr_t elevees[MAX_ELEVEES];
    int n = EleverRecherchees(self, tech, elevees);
    uintptr_t r = oNoeudDisponible(panneau, tech, p3, p4);
    if ((int)p3 == 1) JournalNoeudDisponible(self, tech, r, n);
    RabaisserRecherchees(self, elevees, n);
    return r;
}

static uintptr_t hkRafraichirNoeud(uintptr_t panneau, uintptr_t guid, uintptr_t p3, uintptr_t p4)
{
    uintptr_t self = g_selfTech;
    uint64_t h = 0;
    if (!self || !guid || !SafeRead((void*)guid, &h, 8)) return oRafraichirNoeud(panneau, guid, p3, p4);
    uintptr_t cible = h ? FindTechPtrByHash(h) : 0;
    uintptr_t elevees[MAX_ELEVEES];
    int n = EleverRecherchees(self, cible, elevees);
    uintptr_t r = oRafraichirNoeud(panneau, guid, p3, p4);
    RabaisserRecherchees(self, elevees, n);
    return r;
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
        { OFF_NOEUD_DISPONIBLE,          (void*)&hkNoeudDisponible,  (void**)&oNoeudDisponible,  "noeud disponible (arbre)" },
        { OFF_RAFRAICHIR_NOEUD,          (void*)&hkRafraichirNoeud,  (void**)&oRafraichirNoeud,  "rafraichissement d'un noeud (arbre)" },
    };
    for (auto& h : H) {
        void* target = (void*)(modBase + h.off);
        if (MH_CreateHook(target, h.detour, h.orig) != MH_OK || MH_EnableHook(target) != MH_OK)
            printf("[!] hook %s KO\n", h.name);
    }
    InstallHooksIconesTechnos(modBase);
}
