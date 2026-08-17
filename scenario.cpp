
#include "scenario.h"
#include "mod_common.h"
#include "MinHook.h"
#include <intrin.h>   // _ReturnAddress


typedef void (*SelectScenario_t)(uintptr_t self, uintptr_t a2, uintptr_t a3, uintptr_t a4);
static SelectScenario_t oSelectScenario = nullptr;
typedef void (*ApplyConfig_t)(uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4);
static ApplyConfig_t oApplyConfig = nullptr;
typedef void (*SetScenario_t)(uintptr_t self, uintptr_t scenario, bool flag);
static SetScenario_t oSetScenario = nullptr;
typedef uintptr_t(*Ctor_t)(uintptr_t self);
static Ctor_t oCtor = nullptr;

uintptr_t g_diffObj = 0;      // instance capturee
      // extern dans le .h si tu veux y acceder ailleurs

struct ScenarioTechs { const char* scenario; const char* groups[6]; };

static const ScenarioTechs SCENARIO_TECHS[] = {
    { "DEFAULT",    { "Main/Tier","Main/Heater","Main/Exploration","Main/Ressources","Main/Food", nullptr } },
    { "TechHaven",  { "Main/Tier","Main/Heater","Arks/Exploration","Main/Ressources","Main/Food", nullptr } },
    { "REFUGEES",   { "Main/Tier","Main/Heater","Main/Exploration","Main/Ressources","Main/Food", nullptr } },
    { "WINTERHOME", { "Main/Tier","Winterhome/Heater", "Winterhome/Exploration", "Main/Ressources","Main/Food", nullptr} },
    { "BUILDERS",   { "Main/Tier","Builders", nullptr } },
    { "Reconquest", { "Main/Tier","Edge/Heater","Edge/Exploration","Edge/Ressources","Edge/Food", nullptr } },
};





static void hkApplyConfig(uintptr_t self, uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
    if (self) g_scenarioSelf = self;

    // La fonction peut etre appelee plusieurs fois : on ne trace qu'au premier
    // passage sur un objet donne.
    static uintptr_t dernier = 0;
    bool tracer = (self && self != dernier);
    if (tracer) dernier = self;

    if (tracer) {
        using namespace SetupOffsets;
        uint8_t flag = 0xFF;
        SafeRead((void*)(self + FLAG), &flag, 1);

        uintptr_t base = 0; int count = 0;
        SafeRead((void*)(self + DIFF_LIST), &base, 8);
        SafeRead((void*)(self + DIFF_COUNT), &count, 4);

        printf("\n[SCN] self=0x%llX flag=%d liste=0x%llX count=%d\n",
            (unsigned long long)self, flag, (unsigned long long)base, count);

        if (flag == 0 && base > 0x10000 && count > 0 && count < 512) {
            for (int i = 0; i < count; ++i) {
                uintptr_t item = base + (uintptr_t)i * 0x10;
                uintptr_t table = 0, obj = 0;
                if (!SafeRead((void*)(item + CAT_TABLE), &table, 8)) continue;
                if (!SafeRead((void*)(item + CAT_OBJ), &obj, 8)) continue;
                if (obj <= 0x10000) continue;

                int32_t idx = -1;
                SafeRead((void*)(obj + CAT_LEVEL), &idx, 4);

                uintptr_t entries = 0; int nent = 0;
                if (table > 0x10000 && idx >= 0) {
                    uintptr_t var = table + (uintptr_t)idx * 0x20;
                    SafeRead((void*)(var + VAR_ENTRIES), &entries, 8);
                    SafeRead((void*)(var + VAR_COUNT), &nent, 4);
                }
                printf("   [%2d] obj=0x%llX index=%d  entrees=%d\n",
                    i, (unsigned long long)obj, idx, nent);
            }
        }
    }

    oApplyConfig(self, a2, a3, a4);
}


void InstallScenarioHooks(uintptr_t modBase)
{
    {
        void* t = (void*)(modBase + SetupOffsets::SelectScenario);
        if (MH_CreateHook(t, (void*)&hkSelectScenario, (void**)&oSelectScenario) != MH_OK
            || MH_EnableHook(t) != MH_OK)
            printf("[!] hook SElectSCenario KO\n");
    }
    {
        void* t = (void*)(modBase + SetupOffsets::ApplyConfig);
        if (MH_CreateHook(t, (void*)&hkApplyConfig, (void**)&oApplyConfig) != MH_OK
            || MH_EnableHook(t) != MH_OK)
            printf("[!] hook ApplyConfig KO\n");
    }
    {
        void* t = (void*)(modBase + 0x1267C50);
        if (MH_CreateHook(t, (void*)&hkSetScenario, (void**)&oSetScenario) != MH_OK
            || MH_EnableHook(t) != MH_OK)
            printf("[!] hook SetScenario KO\n");
    }
    /* {
        void* t = (void*)(modBase + 0x1A46EA0);
        if (MH_CreateHook(t, (void*)&hkCtor, (void**)&oCtor) != MH_OK
            || MH_EnableHook(t) != MH_OK)
            printf("[!] hook hkCtor KO\n");
    }*/
    
}


static bool StrEqual(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

bool ScenarioUsesGroup(const char* groupLabel)
{
    if (!g_scenario.valid) return true;        // scenario inconnu -> tout accepter
    for (auto& s : SCENARIO_TECHS) {
        if (!StrEqual(s.scenario, g_scenario.name)) continue;
        for (int i = 0; i < 6 && s.groups[i]; ++i)
            if (StrEqual(s.groups[i], groupLabel)) return true;
        return false;                          // scenario connu, groupe absent
    }
    return true;                               // scenario non repertorie
}

const char* ScenarioVictoryId()
{
    return g_scenario.valid ? g_scenario.name : "";
}

ScenarioConfig g_scenario = {};

static bool CaptureSetup(uintptr_t self)
{
    using namespace SetupOffsets;
    ScenarioConfig c = {};

    int32_t idx = -1, nscn = 0;
    uintptr_t list = 0;
    SafeRead((void*)(self + SCN_INDEX), &idx, 4);
    SafeRead((void*)(self + SCN_COUNT), &nscn, 4);
    SafeRead((void*)(self + SCN_LIST), &list, 8);
    c.index = idx;

    // Objet du scenario selectionne, puis son nom interne.
    // Attention : +0x000 contient un POINTEUR vers la chaine.
    uintptr_t scn = 0;
    if (list > 0x10000 && idx >= 0 && idx < nscn)
        SafeRead((void*)(list + (uintptr_t)idx * 8), &scn, 8);
    c.scenarioObj = scn;

    if (scn > 0x10000) {
        uintptr_t p = 0;
        if (SafeRead((void*)(scn + 0x00), &p, 8) && p > 0x10000)
            SafeRead((void*)p, c.name, sizeof(c.name) - 1);
    }

    // Categories de difficulte : on garde l'objet ET le niveau choisi.
    uintptr_t dlist = 0; int32_t ndiff = 0;
    SafeRead((void*)(self + DIFF_LIST), &dlist, 8);
    SafeRead((void*)(self + DIFF_COUNT), &ndiff, 4);

    if (dlist > 0x10000 && ndiff > 0 && ndiff <= 8) {
        c.diffCount = ndiff;
        for (int i = 0; i < ndiff; ++i) {
            uintptr_t cat = dlist + (uintptr_t)i * 0x10, obj = 0;
            if (!SafeRead((void*)(cat + CAT_OBJ), &obj, 8) || obj <= 0x10000) continue;
            c.diffObj[i] = obj;
            SafeRead((void*)(obj + CAT_LEVEL), &c.difficulty[i], 4);
        }
    }

    c.valid = (c.name[0] != 0);
    if (!c.valid) return false;      // on garde la capture precedente

    g_scenario = c;
    return true;
}

static void hkSelectScenario(uintptr_t self, uintptr_t a2, uintptr_t a3, uintptr_t a4)
{
    using namespace SetupOffsets;
    if (self) g_setupSelf = self;

    if (self) {
        // Cette fonction est appelee a chaque interaction du menu : on ne
        // recapture que si la selection a reellement change.
        int32_t idx = -1;
        SafeRead((void*)(self + SCN_INDEX), &idx, 4);

        static uintptr_t dernierSelf = 0;
        static int32_t   dernierIdx = -999;
        bool change = (self != dernierSelf || idx != dernierIdx);

        if (change) {
            dernierSelf = self;
            dernierIdx = idx;

            if (CaptureSetup(self)) {
                printf("[SETUP] %s (index %d, obj 0x%llX) difficultes :",
                    g_scenario.name, g_scenario.index,
                    (unsigned long long)g_scenario.scenarioObj);
                for (int i = 0; i < g_scenario.diffCount; ++i)
                    printf(" %d", g_scenario.difficulty[i]);
                printf("\n");
            }
            else {
                printf("[SETUP] capture impossible (index %d)\n", idx);
            }
        }
    }

    oSelectScenario(self, a2, a3, a4);
}

// Un pointeur designe-t-il un objet scenario ? On reconnait la signature
// relevee au scan : +0x238 -> "@Generated/ScenariosConfig/Scenarios/..."
static bool LooksLikeScenario(uintptr_t p, char* nameOut, int nameLen)
{
    if (p <= 0x10000) return false;

    uintptr_t k = 0;
    if (!SafeRead((void*)(p + 0x238), &k, 8) || k <= 0x10000) return false;

    char key[64] = { 0 };
    if (!SafeRead((void*)k, key, sizeof(key) - 1)) return false;

    const char* pfx = "@Generated/ScenariosConfig/Scenarios/";
    for (int i = 0; pfx[i]; ++i) if (key[i] != pfx[i]) return false;

    uintptr_t n = 0;
    if (nameOut && SafeRead((void*)(p + 0x00), &n, 8) && n > 0x10000)
        SafeRead((void*)n, nameOut, nameLen - 1);
    return true;
}

// Cherche dans l'image les globaux pointant vers un objet scenario.
void FindScenarioGlobals()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)base, &dos, sizeof(dos))) return;
    if (!SafeRead((void*)(base + dos.e_lfanew), &nt, sizeof(nt))) return;
    uintptr_t end = base + nt.OptionalHeader.SizeOfImage;

    printf("\n[SCNPTR] recherche de globaux vers un objet scenario...\n");
    int found = 0;
    uintptr_t addr = base;

    while (addr < end && found < 32) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) break;
        if (mbi.RegionSize == 0) break;

        uintptr_t rs = (uintptr_t)mbi.BaseAddress, re = rs + mbi.RegionSize;
        if (re > end) re = end;

        if (mbi.State == MEM_COMMIT &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            for (uintptr_t p = (rs + 7) & ~(uintptr_t)7; p + 8 <= re; p += 8) {
                uintptr_t v = *(uintptr_t*)p;
                char nom[64] = { 0 };
                if (LooksLikeScenario(v, nom, sizeof(nom))) {
                    printf("   Frostpunk.exe+%llX -> %s\n",
                        (unsigned long long)(p - base), nom);
                    if (++found >= 32) break;
                }
            }
        }
        addr = re;
    }
    printf("[SCNPTR] %d candidat(s)\n", found);
}

void DumpScenarioGlobal()
{
    const uintptr_t OFF = 0x2C31B48;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);

    uintptr_t p = 0;
    if (!SafeRead((void*)(base + OFF), &p, 8)) {
        printf("[SCNG] global illisible\n");
        return;
    }
    printf("\n[SCNG] Frostpunk.exe+%llX = 0x%llX\n",
        (unsigned long long)OFF, (unsigned long long)p);

    if (p <= 0x10000) { printf("       -> nul / invalide\n"); return; }

    uintptr_t k = 0;
    if (!SafeRead((void*)(p + 0x238), &k, 8)) {
        printf("       -> +0x238 illisible (objet detruit ?)\n"); return;
    }
    printf("       +0x238 = 0x%llX\n", (unsigned long long)k);

    if (k > 0x10000) {
        char key[96] = { 0 };
        if (SafeRead((void*)k, key, sizeof(key) - 1))
            printf("       cle = \"%s\"\n", key);
    }

    uintptr_t n = 0;
    if (SafeRead((void*)(p + 0x00), &n, 8) && n > 0x10000) {
        char nom[64] = { 0 };
        if (SafeRead((void*)n, nom, sizeof(nom) - 1))
            printf("       nom = \"%s\"\n", nom);
    }

    // Les premiers champs, au cas ou la structure serait differente.
    for (int off = 0; off < 0x40; off += 8) {
        uintptr_t v = 0;
        if (SafeRead((void*)(p + off), &v, 8))
            printf("       +0x%02X = 0x%llX\n", off, (unsigned long long)v);
    }
}

void DumpGlobalBlock(uintptr_t from, uintptr_t to)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    printf("\n[BLOC] Frostpunk.exe+%llX .. +%llX\n",
        (unsigned long long)from, (unsigned long long)to);

    for (uintptr_t off = from; off + 8 <= to; off += 8) {
        uintptr_t v = 0;
        if (!SafeRead((void*)(base + off), &v, 8) || v <= 0x10000) continue;

        char info[128] = { 0 };

        // Cle de scenario ?
        uintptr_t k = 0;
        if (SafeRead((void*)(v + 0x238), &k, 8) && k > 0x10000) {
            char key[96] = { 0 };
            if (SafeRead((void*)k, key, sizeof(key) - 1)) {
                const char* pfx = "@Generated/";
                bool ok = true;
                for (int i = 0; pfx[i] && ok; ++i) if (key[i] != pfx[i]) ok = false;
                if (ok) _snprintf_s(info, sizeof(info), _TRUNCATE, "  cle=%s", key);
            }
        }

        // Chaine pointee par +0x00 ?
        if (!info[0]) {
            uintptr_t n = 0;
            if (SafeRead((void*)(v + 0x00), &n, 8) && n > 0x10000) {
                char nom[48] = { 0 };
                if (SafeRead((void*)n, nom, sizeof(nom) - 1)) {
                    bool imprimable = (nom[0] >= 0x20 && nom[0] < 0x7F);
                    for (int i = 0; imprimable && nom[i] && i < 32; ++i)
                        if (nom[i] < 0x20 || nom[i] > 0x7E) imprimable = false;
                    if (imprimable && nom[1])
                        _snprintf_s(info, sizeof(info), _TRUNCATE, "  \"%s\"", nom);
                }
            }
        }

        printf("   +%07llX = 0x%llX%s\n",
            (unsigned long long)off, (unsigned long long)v, info);
    }
}

void ProbeForScenario(uintptr_t obj, int lenBytes)
{
    if (obj <= 0x10000) { printf("[PROBE] objet invalide\n"); return; }
    printf("\n[PROBE] 0x%llX sur 0x%X octets\n",
        (unsigned long long)obj, lenBytes);

    int trouve = 0;
    for (int off = 0; off + 8 <= lenBytes; off += 8) {
        uintptr_t v = 0;
        if (!SafeRead((void*)(obj + off), &v, 8) || v <= 0x10000) continue;

        char nom[64] = { 0 };
        if (LooksLikeScenario(v, nom, sizeof(nom))) {
            printf("   +0x%03X -> SCENARIO \"%s\"\n", off, nom);
            trouve++;
            continue;
        }

        // Un cran plus bas, sur une fenetre reduite pour rester leger.
        for (int off2 = 0; off2 + 8 <= 0x200; off2 += 8) {
            uintptr_t v2 = 0;
            if (!SafeRead((void*)(v + off2), &v2, 8) || v2 <= 0x10000) continue;
            char nom2[64] = { 0 };
            if (LooksLikeScenario(v2, nom2, sizeof(nom2))) {
                printf("   +0x%03X -> +0x%03X -> SCENARIO \"%s\"\n", off, off2, nom2);
                trouve++;
                break;
            }
        }
    }
    printf("[PROBE] %d reference(s)\n", trouve);
}

uintptr_t FindSceneSystem(const char* name)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), scene = 0;
    if (!SafeRead((void*)(base + 0x2C31B20), &scene, 8) || scene <= 0x10000) return 0;

    for (int off = 0; off < 0x1000; off += 0x10) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(scene + off), &p, 8) || p <= 0x10000) continue;

        char n[64] = { 0 };
        if (!SafeRead((void*)p, n, sizeof(n) - 1)) continue;

        bool eq = true;
        for (int i = 0; ; ++i) {
            if (n[i] != name[i]) { eq = false; break; }
            if (n[i] == 0) break;
        }
        if (!eq) continue;

        uintptr_t obj = 0;
        SafeRead((void*)(scene + off + 8), &obj, 8);
        printf("[SCENE] %-32s nom a +0x%03X, objet = 0x%llX\n",
            name, off, (unsigned long long)obj);
        return obj;
    }
    printf("[SCENE] %s introuvable\n", name);
    return 0;
}

void FindValueInObject(uintptr_t obj, int lenBytes, uintptr_t needle, const char* label)
{
    if (obj <= 0x10000 || !needle) return;
    printf("[FIND] %s (0x%llX) dans 0x%llX :\n",
        label, (unsigned long long)needle, (unsigned long long)obj);
    int n = 0;
    for (int off = 0; off + 8 <= lenBytes; off += 8) {
        uintptr_t v = 0;
        if (SafeRead((void*)(obj + off), &v, 8) && v == needle) {
            printf("   +0x%04X\n", off);
            n++;
        }
    }
    if (!n) printf("   absent\n");
}

// Offset du NOM d'un systeme dans la scene (-1 si absent).
int SceneNameOffset(uintptr_t scene, const char* name)
{
    for (int off = 0; off < 0x4000; off += 8) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(scene + off), &p, 8) || p <= 0x10000) continue;
        char n[64] = { 0 };
        if (!SafeRead((void*)p, n, sizeof(n) - 1)) continue;
        bool eq = true;
        for (int i = 0; ; ++i) {
            if (n[i] != name[i]) { eq = false; break; }
            if (n[i] == 0) break;
        }
        if (eq) return off;
    }
    return -1;
}





static void hkSetScenario(uintptr_t self, uintptr_t scenario, bool flag)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t retAdr = (uintptr_t)_ReturnAddress();

    char nom[64] = { 0 };
    bool estScenario = LooksLikeScenario(scenario, nom, sizeof(nom));

    printf("\n[SETSCN] self=0x%llX  valeur=0x%llX  flag=%d\n",
        (unsigned long long)self, (unsigned long long)scenario, flag);
    printf("         appele depuis Frostpunk.exe+%llX\n",
        (unsigned long long)(retAdr - base));
    if (estScenario) {
        printf("         -> SCENARIO \"%s\"\n", nom);
        int n = 0; for (; nom[n] && n < 63; ++n) g_scenario.name[n] = nom[n];
        g_scenario.name[n] = 0;
        g_scenario.valid = true;
    }

    oSetScenario(self, scenario, flag);
}

bool GetClassNameFP(uintptr_t obj, char* out, int maxlen)
{
    if (obj <= 0x10000 || maxlen < 4) return false;

    uintptr_t vptr = 0;
    if (!SafeRead((void*)obj, &vptr, 8) || vptr <= 0x10000) return false;

    uintptr_t col = 0;                       // CompleteObjectLocator
    if (!SafeRead((void*)(vptr - 8), &col, 8) || col <= 0x10000) return false;

    uint32_t sig = 0xFFFFFFFF, tdRva = 0;
    SafeRead((void*)col, &sig, 4);
    if (sig != 1) return false;              // 1 = format x64
    if (!SafeRead((void*)(col + 0x0C), &tdRva, 4) || !tdRva) return false;

    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    char raw[160] = { 0 };
    if (!SafeRead((void*)(base + tdRva + 0x10), raw, sizeof(raw) - 1)) return false;

    // ".?AVFrostpunkTruc@@" -> "FrostpunkTruc"
    const char* s = raw;
    if (s[0] == '.' && s[1] == '?' && s[2] == 'A' && (s[3] == 'V' || s[3] == 'U'))
        s += 4;
    int j = 0;
    for (; s[j] && s[j] != '@' && j < maxlen - 1; ++j) out[j] = s[j];
    out[j] = 0;
    return j > 0;
}

void DumpIconTile()
{
    if (!EnsureTechSelf()) return;

    uintptr_t base = 0; uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);

    for (uint32_t k = 0; k < cnt && k < 4; ++k) {
        uintptr_t te = 0;
        SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8);
        if (te <= 0x10000) continue;

        uintptr_t dsc = 0; char nom[64] = { 0 };
        SafeRead((void*)(te + 0x18), &dsc, 8);
        if (dsc > 0x10000) SafeRead((void*)dsc, nom, sizeof(nom) - 1);

        uint64_t tile = 0, grid = 0;
        SafeRead((void*)(te + 0x80), &tile, 8);
        SafeRead((void*)(te + 0x88), &grid, 8);

        printf("[TILE] %-24s  IconTile=%016llX  Grid=%016llX\n",
            nom, (unsigned long long)tile, (unsigned long long)grid);

        // Si c'est un pointeur, que trouve-t-on derriere ?
        if (tile > 0x10000 && tile < 0x7FFFFFFFFFFF) {
            char s[128] = { 0 };
            if (SafeRead((void*)tile, s, sizeof(s) - 1) && s[0] >= 0x20 && s[0] < 0x7F)
                printf("        -> \"%s\"\n", s);
            char cls[96] = { 0 };
            if (GetClassNameFP(tile, cls, sizeof(cls)))
                printf("        [%s]\n", cls);
        }
    }
}


static uintptr_t hkCtor(uintptr_t self)
{
    uintptr_t r = oCtor(self);
    if (self) {
        g_diffObj = self;
        char cls[96] = { 0 };
        GetClassNameFP(self, cls, sizeof(cls));
        printf("[CTOR] %s cree a 0x%llX\n",
            cls[0] ? cls : "(classe inconnue)", (unsigned long long)self);
    }
    return r;
}

static bool GuidEq(const uint64_t* a, const uint64_t* b)
{
    return a[0] == b[0] && a[1] == b[1];
}


void LireDifficultes()
{
    uintptr_t sys = FindFirstObjectByClass("FrostpunkDifficultySystem");
    if (!sys) { printf("[DIF] systeme absent\n"); return; }

    uintptr_t arr = 0; uint32_t cnt = 0;
    SafeRead((void*)(sys + 0x30), &arr, 8);
    SafeRead((void*)(sys + 0x38), &cnt, 4);

    // 1) definitions nommees (elements pairs) : GUID du reglage -> nom + niveaux
    struct Def { uint64_t g[2]; char nom[64]; uintptr_t lvls; uint32_t nlvl; };
    Def defs[16]; int nd = 0;

    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t e = 0;
        SafeRead((void*)(arr + (uintptr_t)i * 8), &e, 8);
        if (e <= 0x10000) continue;

        // Une definition a un pointeur Difficulties non nul en +0x18.
        uintptr_t lst = 0;
        SafeRead((void*)(e + 0x18), &lst, 8);
        if (lst <= 0x10000) continue;                 // c'est un objet "niveau"

        if (nd >= 16) break;
        Def& d = defs[nd];
        SafeRead((void*)(e + 0x08), &d.g[0], 8);
        SafeRead((void*)(e + 0x10), &d.g[1], 8);
        d.lvls = lst;
        SafeRead((void*)(e + 0x20), &d.nlvl, 4);
        uintptr_t p = 0; d.nom[0] = 0;
        if (SafeRead((void*)(e + 0x00), &p, 8) && p > 0x10000)
            SafeRead((void*)p, d.nom, sizeof(d.nom) - 1);
        nd++;
    }

    // 2) objets "niveau" (Difficulties nul) : leur GUID +0x08 est celui du
    //    niveau retenu ; on cherche dans quel reglage il figure, et a quel rang.
    printf("\n[DIF] niveaux actifs :\n");
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t e = 0;
        SafeRead((void*)(arr + (uintptr_t)i * 8), &e, 8);
        if (e <= 0x10000) continue;
        uintptr_t lst = 0;
        SafeRead((void*)(e + 0x18), &lst, 8);
        if (lst > 0x10000) continue;                  // c'est une definition

        uint64_t gl[2] = {};
        SafeRead((void*)(e + 0x08), &gl[0], 8);
        SafeRead((void*)(e + 0x10), &gl[1], 8);

        // Chercher ce GUID de niveau dans les listes Difficulties.
        for (int k = 0; k < nd; ++k) {
            for (uint32_t j = 0; j < defs[k].nlvl && j < 32; ++j) {
                uintptr_t lv = 0;
                SafeRead((void*)(defs[k].lvls + (uintptr_t)j * 8), &lv, 8);
                if (lv <= 0x10000) continue;
                uint64_t g[2] = {};
                SafeRead((void*)(lv + 0x08), &g[0], 8);
                SafeRead((void*)(lv + 0x10), &g[1], 8);
                if (g[0] == gl[0] && g[1] == gl[1]) {
                    printf("   %-20s niveau %u / %u\n",
                        defs[k].nom[0] ? defs[k].nom : "(inconnu)",
                        j, defs[k].nlvl);
                }
            }
        }
    }
}

void ScanClassesDistinct(const char* filtre, int maxRegionScan)
{
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)modBase, &dos, sizeof(dos))) return;
    if (!SafeRead((void*)(modBase + dos.e_lfanew), &nt, sizeof(nt))) return;
    uintptr_t modEnd = modBase + nt.OptionalHeader.SizeOfImage;

    static char noms[256][128];
    static int  compte[256];
    int nd = 0;

    printf("\n[CLS] classes distinctes contenant \"%s\"\n", filtre);

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        if (lisible && mbi.RegionSize <= 0x4000000) {
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; p + 8 <= next; p += 8) {
                uintptr_t vptr = 0;
                if (!SafeRead((void*)p, &vptr, 8)) continue;
                if (vptr < modBase || vptr >= modEnd) continue;
                char cls[128];
                if (!GetClassNameFP(p, cls, sizeof(cls))) continue;
                if (!StrContains(cls, filtre)) continue;

                int idx = -1;
                for (int k = 0; k < nd; ++k) {
                    bool eq = true;
                    for (int j = 0; ; ++j) {
                        if (noms[k][j] != cls[j]) { eq = false; break; }
                        if (cls[j] == 0) break;
                    }
                    if (eq) { idx = k; break; }
                }
                if (idx >= 0) { compte[idx]++; continue; }

                if (nd < 256) {
                    int j = 0; for (; cls[j] && j < 127; ++j) noms[nd][j] = cls[j];
                    noms[nd][j] = 0; compte[nd] = 1; nd++;
                }
            }
        }
        addr = next;
    }

    for (int k = 0; k < nd; ++k)
        printf("   %-56s x%d\n", noms[k], compte[k]);
    printf("[CLS] %d classe(s) distincte(s)\n", nd);
}

void CheckScenarioVictory()
{
    static uintptr_t sysCache = 0;
    static bool      dejaSignale = false;

    // Retrouver le systeme, avec cache tant qu'il reste valide.
    if (sysCache) {
        char cls[64] = { 0 };
        if (!GetClassNameFP(sysCache, cls, sizeof(cls)) ||
            !StrContains(cls, "ScenarioSystem"))
            sysCache = 0;                     // invalide -> rescanner
    }
    if (!sysCache) {
        sysCache = FindFirstObjectByClass("FrostpunkScenarioSystem");
        dejaSignale = false;                  // nouvelle partie
    }
    if (!sysCache) return;

    uint8_t vic = 0;
    if (!SafeRead((void*)(sysCache + 0x78), &vic, 1)) return;

    if (vic && !dejaSignale) {
        dejaSignale = true;

        // Identite du scenario gagne, pour le check.
        uintptr_t scn = 0; char nom[64] = { 0 };
        if (SafeRead((void*)(sysCache + 0x68), &scn, 8) && scn > 0x10000) {
            uintptr_t p = 0;
            if (SafeRead((void*)(scn + 0x00), &p, 8) && p > 0x10000)
                SafeRead((void*)p, nom, sizeof(nom) - 1);
        }
        printf("[VICTOIRE] scenario %s remporte !\n", nom[0] ? nom : "?");
    }
    if (!vic) dejaSignale = false;            // reset pour la prochaine partie
}

RunState g_run = {};

static uintptr_t g_scnSysCache = 0;
static uintptr_t g_difSysCache = 0;

// Revalide un cache d'instance ; le vide s'il n'est plus du bon type.
static uintptr_t CachedSystem(uintptr_t& cache, const char* nom)
{
    if (cache) {
        char cls[96] = { 0 };
        if (!GetClassNameFP(cache, cls, sizeof(cls)) || !StrContains(cls, nom))
            cache = 0;
    }
    if (!cache) cache = FindFirstObjectByClass(nom);
    return cache;
}



static int NiveauRetenu(uintptr_t defObj, uintptr_t* retenus, int nr)
{
    uintptr_t lst = 0; uint32_t nlvl = 0;
    SafeRead((void*)(defObj + 0x18), &lst, 8);
    SafeRead((void*)(defObj + 0x20), &nlvl, 4);
    if (lst <= 0x10000 || nlvl > 32) return -1;

    for (uint32_t j = 0; j < nlvl; ++j) {
        uintptr_t niv = 0;
        SafeRead((void*)(lst + (uintptr_t)j * 8), &niv, 8);
        if (niv <= 0x10000) continue;

        uint64_t g[2] = {};
        SafeRead((void*)(niv + 0x08), &g[0], 8);
        SafeRead((void*)(niv + 0x10), &g[1], 8);

        for (int m = 0; m < nr; ++m) {
            uint64_t gr[2] = {};
            SafeRead((void*)(retenus[m] + 0x08), &gr[0], 8);
            SafeRead((void*)(retenus[m] + 0x10), &gr[1], 8);
            if (GuidEq(gr, g)) return (int)j;
        }
    }
    return -1;
}


void CaptureRunState()
{
    // Plus aucun balayage ici : les instances viennent du scan unique.
    if (!g_scnSysCache) { g_run.valid = false; return; }

    uintptr_t scn = 0;
    if (!SafeRead((void*)(g_scnSysCache + 0x68), &scn, 8)) {
        g_scnSysCache = 0;          // lecture impossible : l'instance est morte
        g_run.valid = false;
        return;
    }
    if (scn <= 0x10000) return;

    char nom[64] = { 0 };
    uintptr_t p = 0;
    if (SafeRead((void*)(scn + 0x00), &p, 8) && p > 0x10000)
        SafeRead((void*)p, nom, sizeof(nom) - 1);
    if (nom[0] == 0) return;

    RunState r = {};
    int i = 0; for (; nom[i] && i < 63; ++i) r.scenario[i] = nom[i];
    r.scenario[i] = 0;
    SafeRead((void*)(g_scnSysCache + 0x110), &r.gameId[0], 8);
    SafeRead((void*)(g_scnSysCache + 0x118), &r.gameId[1], 8);

    // --- difficultes : 13 elements de 0x10 = {definition, niveau choisi} ---





    uintptr_t difSys = FindDifficultySystem();
    if (difSys) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        SafeRead((void*)(difSys + 0x30), &arr, 8);
        SafeRead((void*)(difSys + 0x38), &cnt, 4);

        if (arr > 0x10000 && cnt > 0 && cnt <= 32) {
            for (uint32_t k = 0; k < cnt && r.diffCount < 16; ++k) {
                uintptr_t e = arr + (uintptr_t)k * 0x10;
                uintptr_t def = 0, lvl = 0;
                SafeRead((void*)(e + 0x00), &def, 8);
                SafeRead((void*)(e + 0x08), &lvl, 8);
                if (def <= 0x10000 || lvl <= 0x10000) continue;

                char rn[40] = { 0 };
                uintptr_t pn = 0;
                if (SafeRead((void*)(def + 0x00), &pn, 8) && pn > 0x10000)
                    SafeRead((void*)pn, rn, sizeof(rn) - 1);
                if (rn[0] == 0) continue;

                uintptr_t lst = 0; uint32_t nlvl = 0;
                SafeRead((void*)(def + 0x18), &lst, 8);
                SafeRead((void*)(def + 0x20), &nlvl, 4);

                uint64_t g[2] = {};
                SafeRead((void*)(lvl + 0x08), &g[0], 8);
                SafeRead((void*)(lvl + 0x10), &g[1], 8);

                int idx = -1;
                if (lst > 0x10000 && nlvl && nlvl <= 32) {
                    for (uint32_t j = 0; j < nlvl; ++j) {
                        uintptr_t lv = 0;
                        SafeRead((void*)(lst + (uintptr_t)j * 8), &lv, 8);
                        if (lv <= 0x10000) continue;
                        uint64_t g2[2] = {};
                        SafeRead((void*)(lv + 0x08), &g2[0], 8);
                        SafeRead((void*)(lv + 0x10), &g2[1], 8);
                        if (g2[0] == g[0] && g2[1] == g[1]) { idx = (int)j; break; }
                    }
                }

                auto& d = r.diff[r.diffCount++];
                int j = 0; for (; rn[j] && j < 39; ++j) d.name[j] = rn[j];
                d.name[j] = 0;
                d.levelCount = (int)nlvl;
                d.level = idx;
            }
        }
    }

    r.valid = true;
    g_run = r;
}

bool ScenarioJustWon()
{
    if (!g_scnSysCache) return false;
    uint8_t vic = 0;
    if (!SafeRead((void*)(g_scnSysCache + 0x78), &vic, 1)) return false;
    return vic != 0;
}

// Instance valide du systeme de difficulte, avec cache et temporisation :
// le scan complet est couteux et risque pendant un chargement, on n'en tente
// qu'un toutes les 5 secondes tant qu'on ne trouve rien.
uintptr_t FindDifficultySystem()
{
    // Revalidation du cache : le tableau +0x30 doit rester plausible.
    if (g_difSysCache) {
        uintptr_t a = 0; uint32_t c = 0;
        if (!SafeRead((void*)(g_difSysCache + 0x30), &a, 8) ||
            !SafeRead((void*)(g_difSysCache + 0x38), &c, 4) ||
            a <= 0x10000 || c == 0 || c > 64)
            g_difSysCache = 0;
    }
    if (g_difSysCache) return g_difSysCache;

    static DWORD prochainScan = 0;
    if (GetTickCount() < prochainScan) return 0;
    prochainScan = GetTickCount() + 5000;

    static uintptr_t objs[64];
    int n = CollectObjectsByClass("FrostpunkDifficultySystem", objs, 64);
    for (int i = 0; i < n; ++i) {
        uintptr_t a = 0; uint32_t c = 0;
        if (!SafeRead((void*)(objs[i] + 0x30), &a, 8)) continue;
        if (!SafeRead((void*)(objs[i] + 0x38), &c, 4)) continue;
        if (a > 0x10000 && c > 0 && c <= 64) { g_difSysCache = objs[i]; break; }
    }
    return g_difSysCache;
}




bool FindSettingName(const uint64_t* guid, char* out, int maxlen)
{
    out[0] = 0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        if (lisible && mbi.RegionSize <= 0x4000000) {
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; p + 16 <= next; p += 8) {
                if (*(uint64_t*)p != guid[0]) continue;
                if (*(uint64_t*)(p + 8) != guid[1]) continue;

                // Le GUID est en +0x08 de la definition -> objet a p-8.
                uintptr_t obj = p - 8;
                uintptr_t np = 0;
                if (!SafeRead((void*)obj, &np, 8) || np <= 0x10000) continue;

                char nom[64] = { 0 };
                if (!SafeRead((void*)np, nom, sizeof(nom) - 1)) continue;
                if (nom[0] < 0x20 || nom[0] > 0x7E || !nom[1]) continue;

                // Une definition a aussi une liste Difficulties en +0x18.
                uintptr_t lst = 0; uint32_t nl = 0;
                SafeRead((void*)(obj + 0x18), &lst, 8);
                SafeRead((void*)(obj + 0x20), &nl, 4);
                if (lst <= 0x10000 || nl == 0 || nl > 32) continue;

                int j = 0; for (; nom[j] && j < maxlen - 1; ++j) out[j] = nom[j];
                out[j] = 0;
                return true;
            }
        }
        addr = next;
    }
    return false;
}

// Renvoie l'objet definition portant ce GUID en +0x08, ou 0.
void LireDifficultesRuntime()
{
    uintptr_t sys = FindDifficultySystem();
    if (!sys) { printf("[DIF] systeme absent\n"); return; }

    uintptr_t arr = 0; uint32_t cnt = 0;
    SafeRead((void*)(sys + 0x30), &arr, 8);
    SafeRead((void*)(sys + 0x38), &cnt, 4);
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) { printf("[DIF] tableau invalide\n"); return; }

    printf("\n[DIF] %u reglage(s)\n", cnt);
    for (uint32_t i = 0; i < cnt; ++i) {
        // Element de 0x10 : definition en +0x00, niveau choisi en +0x08.
        uintptr_t e = arr + (uintptr_t)i * 0x10;
        uintptr_t def = 0, lvl = 0;
        SafeRead((void*)(e + 0x00), &def, 8);
        SafeRead((void*)(e + 0x08), &lvl, 8);
        if (def <= 0x10000 || lvl <= 0x10000) continue;

        char dn[48] = { 0 }, ln[48] = { 0 };
        uintptr_t p = 0;
        if (SafeRead((void*)def, &p, 8) && p > 0x10000) SafeRead((void*)p, dn, sizeof(dn) - 1);
        p = 0;
        if (SafeRead((void*)lvl, &p, 8) && p > 0x10000) SafeRead((void*)p, ln, sizeof(ln) - 1);

        // Position du niveau retenu dans la liste des niveaux du reglage.
        uintptr_t lst = 0; uint32_t nl = 0;
        SafeRead((void*)(def + 0x18), &lst, 8);
        SafeRead((void*)(def + 0x20), &nl, 4);

        uint64_t g[2] = {};
        SafeRead((void*)(lvl + 0x08), &g[0], 8);
        SafeRead((void*)(lvl + 0x10), &g[1], 8);

        int idx = -1;
        if (lst > 0x10000 && nl && nl <= 32) {
            for (uint32_t j = 0; j < nl; ++j) {
                uintptr_t lv = 0;
                SafeRead((void*)(lst + (uintptr_t)j * 8), &lv, 8);
                if (lv <= 0x10000) continue;
                uint64_t g2[2] = {};
                SafeRead((void*)(lv + 0x08), &g2[0], 8);
                SafeRead((void*)(lv + 0x10), &g2[1], 8);
                if (g2[0] == g[0] && g2[1] == g[1]) { idx = (int)j; break; }
            }
        }

        printf("   %-28s %-12s (index %d / %u)\n", dn, ln, idx +1, nl);
    }
}

uintptr_t FindScenarioSystem()
{
    static uintptr_t objs[64];
    int n = CollectObjectsByClass("FrostpunkScenarioSystem", objs, 64);
    for (int i = 0; i < n; ++i) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(objs[i] + 0x68), &scn, 8)) continue;
        if (scn <= 0x10000) continue;
        uintptr_t p = 0;
        if (!SafeRead((void*)(scn + 0x00), &p, 8) || p <= 0x10000) continue;
        char nom[64] = { 0 };
        if (!SafeRead((void*)p, nom, sizeof(nom) - 1)) continue;
        if (nom[0] < 0x20 || nom[0] > 0x7E || !nom[1]) continue;
        return objs[i];
    }
    return 0;
}

void ScenarioSetCaches(uintptr_t scenarioSys, uintptr_t difficultySys)
{
    g_scnSysCache = scenarioSys;
    g_difSysCache = difficultySys;
    printf("[SCEN] caches poses : scenario=0x%llX difficulte=0x%llX\n",
        (unsigned long long)scenarioSys, (unsigned long long)difficultySys);
}