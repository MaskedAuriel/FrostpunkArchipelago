// mod_common.h
// ---------------------------------------------------------------------------
// Socle partage par tous les modules : includes, lecture/ecriture memoire sure,
// globals (les "self" captures), et les offsets du systeme de technologies.
//
// REGLE C++ : ce qui est PARTAGE entre fichiers est declare ici (extern pour
// les variables, prototype pour les fonctions) et DEFINI une seule fois dans
// mod_common.cpp. Ce qui est interne a un fichier reste "static" dans son .cpp.
// ---------------------------------------------------------------------------
#pragma once

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "versions_jeu.h"   // Adr() : adresses Epic traduites pour l'exe en cours
#include "scooting.h"
#include "known_techs.h"
#include "IconSubManager.h"

// --- Lecture / ecriture memoire protegees -----------------------------------
// Attention a l'ordre des parametres : (source, destination, taille).
bool SafeRead(const void* src, void* dst, size_t size);
bool SafeWrite(void* dst, const void* src, size_t n);
bool ReadAscii(uintptr_t addr, char* out, int maxlen);
void ScanForStrings(const char* label, uintptr_t base, int lenBytes);
void FindStaticPointersTo(uintptr_t value);
bool EnsureResSelf();
bool EnsureTechSelf();
bool ResolveSelf(uintptr_t& dest, const uintptr_t* offs, int n,
    bool (*valide)(uintptr_t), const char* label);
bool ResSelfValid(uintptr_t p);
bool TechSelfValid(uintptr_t p);

void ScanObjectsByClass(const char* filtre, int maxResults);
bool StrContains(const char* s, const char* sub);
uintptr_t FindFirstObjectByClass(const char* filtre);
void DumpObject(uintptr_t obj, int lenBytes);
void DumpVTable(uintptr_t obj, int count);
void DumpPropertyManager(const char* filtre, int maxEntries);
void DumpClassProps(const char* className, int lenBytes);

void DumpRecordProperties(uintptr_t rec);
uintptr_t FindClassRecord(const char* className);
void ListClassRecords(const char* filtre, int maxResults);
void FindPropertyByName(const char* filtre);
void DumpDynarray(uintptr_t owner, int off, int elemSize, int maxShow);
int CollectObjectsByClass(const char* filtre, uintptr_t* out, int maxOut);
void InstallNtHooks();
void InstallTraceChargement(uintptr_t modBase); // [CHARG] etapes du chargement d'une sauvegarde
void DumpIOMappings();
void DumpMappedRegions(SIZE_T tailleMin);
void DumpResourceEntries();
void DumpResourceIcons();
void SwapIconTest();
void DumpAtlasTexture();
void DumpBlobLog();
void ResetBlobLog();
bool AjouterSubstitution(unsigned long long off, const wchar_t* fichier);
void SetIconCell(const char* ressource, int col, int row);
void ChargerSubstitutions();
void RapportSubstitutions();   // tick : lectures servies par blob substitue
bool PartieRechargee();
void CollectObjectsByClasses(const char** noms, int nNoms, uintptr_t** sorties, int* compteurs, int maxParClasse);
void DumpGlobauxEconomie();
void SetTechIconTile(const char* nomTechno, int32_t col, int32_t row);
void BroyerToutesLesIcones();
void DumpBatimentsDesTechnos();
bool ClasseExacte(uintptr_t obj, const char* ref);
void SurveillerArbreTechno();
void DumpNomsGabarits();
bool StrEq(const char* a, const char* b);
bool GetClassNameFP(uintptr_t obj, char* out, int maxlen);
void TrouverDebutFonction(uintptr_t adresse);
void ChercherReferencesVtable(uintptr_t vtable, const char* nom);
// Debut (absolu) de chaque fonction qui charge 'vtable' (constructeurs...).
int  FonctionsChargeantVtable(uintptr_t vtable, uintptr_t* debuts, int maxOut);
// Dump annote (classes RTTI, chaines) des 'longueur' premiers octets.
void DecrireChamps(uintptr_t obj, int longueur, const char* nom);
// Adresses de retour de la pile courante (call E8 / FF /2), adresses Ghidra.
void TracerPile(const char* qui, uintptr_t obj, long numero);
// Nom de classe -> vtable(s), via RTTI, sans instance. Renvoie le nombre trouve.
int  VtablesParNomDeClasse(const char* nom, uintptr_t* out, int maxOut);
void InstallAllocHook(uintptr_t modBase);
void InitAllocateur(uintptr_t modBase);
void TesterAllocateur();
bool StrEqLoc(const char* a, const char* b);
bool ToucheAppuyee(int vk);
// Ligne de journal ecrite tout de suite sur disque (survit a un crash du jeu).
// Definie dans version.cpp.
void JournalImmediat(const char* fmt, ...);
// Dossier des journaux (version.cpp) : logs\ du projet (dev) ou <jeu>\mods\logs (release).
const wchar_t* DossierJournaux();
int  ChercherValeurDansTas(uintptr_t valeur, uintptr_t* out, int maxOut, SIZE_T borne = 0x1000000);
bool DebutObjet(uintptr_t adr, int reculMax, uintptr_t* debut, char* cls, int maxlen);


extern volatile bool g_traceIO;
extern volatile bool g_consolePrete;
extern volatile bool g_siteApAffiche;
extern volatile DWORD g_instantArbre;
typedef uintptr_t(*Alloc_t)(size_t);
extern Alloc_t MoteurAlloc;
// --- Les "self" captures par les hooks --------------------------------------
// g_selfRessource : renseigne par le hook du setter de ressources (0x173FE40)
// g_selfTech      : renseigne par les hooks technos (0x1329610 / 0x1327120)
// Ce sont DEUX objets differents : ne pas les confondre.
extern uintptr_t g_selfRessource;
extern uintptr_t g_selfTech;
extern uintptr_t g_setupSelf;
extern uintptr_t g_scenarioSelf;

// --- Offsets du systeme de technologies (relatifs a g_selfTech) -------------
constexpr int TECH_LIST_OFF   = 0x2248;  // -> base du tableau des technos
constexpr int TECH_COUNT_OFF  = 0x2250;  // -> nombre de technos (172)
constexpr int TECH_ACTIVE_OFF = 0x2260;  // -> techno en cours de recherche
constexpr int TECH_LOCKEY_OFF = 0x68;   // -> "@Technologies/<X>/Name"
constexpr int TECH_LOCDESC_OFF = 0x70;  // -> "@Technologies/<X>/Description"

// Une entree du tableau fait 0x10 octets : { ptr techno (+0x00), etat (+0x08) }
constexpr int TECH_ENTRY_SIZE = 0x10;
constexpr int TECH_ENTRY_PTR  = 0x00;
constexpr int TECH_ENTRY_STATE= 0x08;

// Offsets dans l'objet techno lui-meme
constexpr int TECH_DESC_OFF   = 0x18;    // -> descripteur (nom interne ASCII)
constexpr int TECH_HASH_OFF   = 0x20;    // -> hash 64 bits (identifiant stable)

// --- Offsets du systeme de ressources (relatifs a g_selfRessource) ----------
constexpr int RES_CONT_OFF    = 0x2240;  // -> conteneur des ressources
constexpr int RES_ENTRY_SIZE  = 0x70;
constexpr int RES_STOCK_OFF   = 0x08;

constexpr int TECH_LOCNAME_OFF = 0x68;
constexpr int TECH_LOCSHORT_OFF = 0x78;

// --- Adresses des fonctions du jeu (offsets depuis la base du module) -------
// Regroupees ici pour n'avoir qu'un seul endroit a mettre a jour si le jeu
// est patche.
namespace GameOffsets {
    inline const uintptr_t ModifyResource  = Adr(0x173FE40);  // setter de ressources
    inline const uintptr_t ResIdToIndex    = Adr(0x139E930);  // resId -> index
    inline const uintptr_t CanResearch     = Adr(0x1326A60);  // clic sur une techno
    inline const uintptr_t ResearchStart   = Adr(0x1329610);  // demarrer une recherche
    inline const uintptr_t ResearchComplete= Adr(0x1327120);  // completion (location check)
    inline const uintptr_t CancelResearch  = Adr(0x1326C80);  // annuler une recherche
    inline const uintptr_t Translate       = Adr(0x0FEB340);  // traduire(cle) -> valeur

    // Chaque systeme du jeu a son global dans l'image : SystemesGlobaux +
    // id * SystemesPas, ou id vient de l'annuaire des types (AnnuaireTypes,
    // entrees {ptr nom, id} au pas de 0x10). Ex. Technology 0x41 -> +0x2C30A20.
    inline const uintptr_t SystemesGlobaux = Adr(0x2C30200);
    constexpr uintptr_t SystemesPas     = 0x20;
    inline const uintptr_t AnnuaireTypes   = Adr(0x2C31B20);
}

// Ids des systemes dans l'annuaire des types (releves le 2026-10-15).
namespace IdSysteme {
    constexpr int Difficulty      = 0x03;
    constexpr int Scenario        = 0x0C;
    constexpr int MessageSystem   = 0x14;
    constexpr int MessageProducer = 0x15;
    constexpr int Economy         = 0x3F;
    constexpr int Technology      = 0x41;
    constexpr int Selection       = 0x4C;
    constexpr int GameEntity      = 0x58;
    constexpr int WorldMap        = 0x60;
    constexpr int City            = 0x64;
    constexpr int ExpeditionSite  = 0x65;
    constexpr int Expedition      = 0x68;
    constexpr int ExpeditionHost  = 0x69;
    constexpr int Building        = 0x8B;
}

// Instance d'un systeme par son id (0 si le global est vide : hors partie).
uintptr_t SystemeParId(int id);

// Composants d'un systeme : liste {entite, composant} a +0x2078 (pas 0x10),
// nombre a +0x2080. Ne garde que ceux de la classe 'classe'. Renvoie le nombre.
int ComposantsDuSysteme(int idSysteme, const char* classe, uintptr_t* out, int maxOut);

// --- Objets captures a leur construction (hook du constructeur) ------------
// Ajout depuis le hook (thread du jeu), lecture depuis le tick : protege par
// un verrou. Quand le tableau est plein, on retire les objets detruits
// (classe RTTI changee) avant d'ajouter.
struct ListeCaptures {
    SRWLOCK     verrou;
    uintptr_t*  objets;
    int         capacite;
    int         n;
    const char* classe;
    long        appels;      // nombre total d'appels du constructeur
};
void CaptureAjouter(ListeCaptures& l, uintptr_t obj);
// Copie les objets encore vivants (bonne classe), sans doublon.
int  CaptureLire(ListeCaptures& l, uintptr_t* out, int maxOut);

// --- Cycle de vie de la partie ----------------------------------------------
// PartieTick (une fois par seconde, avant les modules) detecte les chargements
// et resout les references globales : systemes, gabarits, entrees de message,
// technos Archipelago. Un module compare PartieGeneration() a la derniere
// valeur vue pour savoir qu'il doit refaire sa propre init.
void      PartieTick();
bool      PartiePrete();
unsigned  PartieGeneration();
uintptr_t PartieSelection();      // FrostpunkSelectionSystem (0 si absent)
bool      EconomiePresente();     // global de l'economie non nul : en partie

// --- Diagnostics d'ancrage (F8 au besoin) -----------------------------------
void DiagAncrages();              // gabarits / entrees : remontee de references
void DiagGabaritsPrecoces();      // templates presents des le menu ?
void DiagSystemesGroupes();       // F9 : groupes de survivants / refugies sur la carte
// Hooks des constructeurs de composants de la carte (expedition, groupe) :
// journalisent la pile d'appel des 12 premieres constructions de chacun.
void InstallTraceCreationEntites(uintptr_t modBase);

namespace SetupOffsets {
    inline const uintptr_t SelectScenario = Adr(0x1B2DCC0);  // lit l'index de scenario
    inline const uintptr_t ApplyConfig = Adr(0x1B2A9C0);  // applique les difficultes

    // Champs de l'objet de configuration (partage par les deux fonctions)
    constexpr int SCN_LIST = 0x198;   // tableau de pointeurs (stride 8)
    constexpr int SCN_COUNT = 0x1A0;   // nombre de scenarios
    constexpr int SCN_INDEX = 0x1B0;   // scenario choisi (0..5)
    constexpr int FLAG = 0x1E0;   // drapeau de branche
    constexpr int DIFF_LIST = 0x1C8;   // categories de difficulte (stride 0x10)
    constexpr int DIFF_COUNT = 0x1D0;   // leur nombre (4 attendu)

    // Dans une categorie de difficulte
    constexpr int CAT_TABLE = 0x00;    // -> variantes (stride 0x20)
    constexpr int CAT_OBJ = 0x08;    // -> objet portant le niveau choisi
    constexpr int CAT_LEVEL = 0x314;   // le niveau choisi
    constexpr int VAR_ENTRIES = 0x08;    // dans une variante : tableau d'entrees
    constexpr int VAR_COUNT = 0x10;
}

#define CHRONO_SEUIL_MS 5
#define CHRONO(expr)                                                      \
    do {                                                                  \
        DWORD _t0 = GetTickCount();                                       \
        expr;                                                             \
        DWORD _dt = GetTickCount() - _t0;                                 \
        if (_dt >= CHRONO_SEUIL_MS)                                       \
            printf("[CHRONO] %-34s %5lu ms\n", #expr, _dt);               \
    } while (0)