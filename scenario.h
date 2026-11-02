#pragma once

#include "mod_common.h"

void InstallScenarioHooks(uintptr_t modBase);
static void hkSelectScenario(uintptr_t self, uintptr_t a2, uintptr_t a3, uintptr_t a4);
static void hkApplyConfig(uintptr_t self, uintptr_t a2, uintptr_t a3, uintptr_t a4);
bool ScenarioUsesGroup(const char* groupLabel);
const char* ScenarioVictoryId();
void FindScenarioGlobals();
void DumpScenarioGlobal();
void DumpGlobalBlock(uintptr_t from, uintptr_t to);
void ProbeForScenario(uintptr_t obj, int lenBytes);
uintptr_t FindSceneSystem(const char* name);
int SceneNameOffset(uintptr_t scene, const char* name);
void DumpSceneSystems();
void VerifierSystemesParId();
void FindValueInObject(uintptr_t obj, int lenBytes, uintptr_t needle, const char* label);
static void hkSetScenario(uintptr_t self, uintptr_t scenario, bool flag);
static uintptr_t hkCtor(uintptr_t self);
void DumpDifficultes(uintptr_t meta);
void LireDifficultes();
void ScanClassesDistinct(const char* filtre, int maxRegionScan);
void CaptureRunState();
static bool GuidEq(const uint64_t* a, const uint64_t* b);
uintptr_t FindDifficultySystem();
static int NiveauRetenu(uintptr_t defObj, uintptr_t* niveauxRetenus, int nRetenus);
bool FindSettingName(const uint64_t* guid, char* out, int maxlen);
void LireDifficultesRuntime();
uintptr_t FindScenarioSystem();
void ScenarioSetCaches(uintptr_t scenarioSys, uintptr_t difficultySys);
void ScenarioLireCaches(uintptr_t* scenarioSys, uintptr_t* difficultySys);
void DumpIconTile();
struct RunState {
    bool     valid;             // une partie est en cours et lisible
    char     scenario[64];      // nom interne : "REFUGEES", ...
    uint64_t gameId[2];         // identifie la partie (detecte un changement)

    // Difficultes : un couple (nom du reglage, index du niveau) par dimension.
    struct { char name[40]; int level; int levelCount; } diff[16];
    int      diffCount;
};

extern RunState g_run;

void CaptureRunState();

struct ScenarioConfig {
    char      name[64];
    int       index;
    int       difficulty[8];
    uintptr_t diffObj[8];      // objets de categorie (+0x314 = niveau)
    uintptr_t scenarioObj;     // objet du scenario
    int       diffCount;
    bool      valid;
};
extern ScenarioConfig g_scenario;

