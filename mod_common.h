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

extern volatile bool g_traceIO;
extern volatile bool g_consolePrete;
extern volatile bool g_siteApAffiche;

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
    constexpr uintptr_t ModifyResource  = 0x173FE40;  // setter de ressources
    constexpr uintptr_t ResIdToIndex    = 0x139E930;  // resId -> index
    constexpr uintptr_t CanResearch     = 0x1326A60;  // clic sur une techno
    constexpr uintptr_t ResearchStart   = 0x1329610;  // demarrer une recherche
    constexpr uintptr_t ResearchComplete= 0x1327120;  // completion (location check)
    constexpr uintptr_t CancelResearch  = 0x1326C80;  // annuler une recherche
    constexpr uintptr_t Translate       = 0x0FEB340;  // traduire(cle) -> valeur
}

namespace SetupOffsets {
    constexpr uintptr_t SelectScenario = 0x1B2DCC0;  // lit l'index de scenario
    constexpr uintptr_t ApplyConfig = 0x1B2A9C0;  // applique les difficultes

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
