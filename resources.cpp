// resources.cpp
#include "resources.h"
#include "MinHook.h"
#include "ap_ressources.h"

volatile bool g_traceRes = false;

typedef int32_t(*ModifyResource_t)(uintptr_t self, uintptr_t guidPtr,
    int32_t amount, uintptr_t p4,
    int32_t p5, bool p6);

typedef int     (*ResIdToIndex_t)(uintptr_t container, uintptr_t* pResId);

static ModifyResource_t oModifyResource = nullptr;
static ResIdToIndex_t   ResIdToIndex    = nullptr;

// --- Table des GUID connus (16 octets a resId+0x08, en ordre memoire) --------
struct KnownResource { const char* name; uint8_t guid[16]; };
static const KnownResource KNOWN[] = {
    { "ration",             { 0xFE,0xBC,0x57,0xFB, 0x94,0x6B,0xF4,0x90, 0x7B,0x48,0xB9,0x8B, 0x42,0x1B,0x7E,0x06 } },
    { "coal",               { 0x0B,0xCF,0x0B,0x94, 0xB9,0x43,0x37,0x85, 0x9C,0x44,0xBC,0xDF, 0x67,0xF9,0xBE,0xE1 } },
    { "wood",               { 0x66,0x05,0xA1,0xA7, 0x75,0x94,0xC3,0xB8, 0x0A,0x43,0xE6,0x27, 0x70,0xEB,0x23,0x42 } },
    { "steamcore",          { 0xD4,0xD2,0x23,0x36, 0x7A,0x20,0x42,0xB4, 0x4D,0x46,0x51,0x41, 0xB7,0xC4,0x47,0x2B } },
    { "steel",              { 0xD3,0xF7,0x9D,0x03, 0xA4,0xED,0x94,0x91, 0x8C,0x4B,0xFA,0xE9, 0x06,0x90,0xBA,0x67 } },
    { "rawfood",            { 0x75,0xB6,0x78,0xB1, 0x89,0xC7,0x33,0x98, 0x01,0x46,0x92,0x0C, 0x17,0x98,0x52,0x16 } },
    { "prosthesis",         { 0x35,0x8B,0xA1,0xC0, 0x33,0xF8,0xFA,0x91, 0x2E,0x4D,0x27,0xE0, 0xEF,0x42,0xD1,0xD3 } },
    { "structuralprofiles", { 0x07,0xB7,0x2F,0xF5, 0x5B,0xA8,0x02,0x84, 0x7C,0x44,0x06,0x25, 0xA8,0x33,0x13,0xB9 } },
    { "steelcomposite",     { 0x70,0xD2,0x93,0xB0, 0x13,0xAB,0xFA,0x91, 0x77,0x40,0xE9,0x28, 0x8F,0xAC,0x3F,0x55 } },
    { "steamexchanger",     { 0x8A,0x6C,0x63,0x63, 0x16,0xBF,0x21,0x86, 0xD4,0x46,0x71,0xD9, 0xF1,0xAE,0x48,0x4C } },
    { "corpse",             { 0x8D,0x2C,0x2A,0x33, 0xF6,0xBF,0x1D,0x8B, 0x30,0x4F,0xB0,0x97, 0x47,0x15,0xF7,0xDF } },
};
static const int KNOWN_COUNT = sizeof(KNOWN) / sizeof(KNOWN[0]);

// --- Contexte capture par ressource -----------------------------------------
struct ResCtx {
    uint8_t   guid[16];
    uintptr_t self;
    uintptr_t resId;
    uintptr_t arg4;
    bool      valid;
};
static ResCtx g_res[32];
static int    g_resCount = 0;

// Le detour tourne sur le thread du jeu, Res_* sur le notre : ils partagent g_res.
static CRITICAL_SECTION g_lock;
static bool g_lockReady = false;

static ResCtx* FindByGuid(const uint8_t* g)
{
    for (int i = 0; i < g_resCount; ++i)
        if (memcmp(g_res[i].guid, g, 16) == 0) return &g_res[i];
    return nullptr;
}

static const KnownResource* FindKnown(const char* name)
{
    for (int i = 0; i < KNOWN_COUNT; ++i)
        if (strcmp(KNOWN[i].name, name) == 0) return &KNOWN[i];
    return nullptr;
}

// Adresse de la valeur : base = *(self+0x2240) ; addr = base + index*0x70 + 8
static bool ResolveAddr(ResCtx* c, uintptr_t& outAddr)
{
    if (!c || !c->valid || !ResIdToIndex) return false;
    uintptr_t cont = c->self + RES_CONT_OFF;
    uintptr_t base = 0;
    if (!SafeRead((void*)cont, &base, sizeof(base)) || !base) return false;

    uintptr_t resIdCopy = c->resId;      // ResIdToIndex attend un POINTEUR
    int index = ResIdToIndex(cont, &resIdCopy);
    if (index < 0) return false;

    outAddr = base + (uintptr_t)index * RES_ENTRY_SIZE + RES_STOCK_OFF;
    return true;
}



static int32_t hkModifyResource(uintptr_t self, uintptr_t guidPtr,
    int32_t amount, uintptr_t p4,
    int32_t p5, bool p6)
{
    // --- Garde-fou : notre ressource fictive ne doit JAMAIS atteindre le
    //     stock de la ville. Le jeu irait la chercher dans son conteneur,
    //     ou elle ne figure pas.
    if (ApPorteurEstNotreGuid(guidPtr)) {
        printf("[APRES] credit de %d intercepte et annule\n", amount);
        return 0;                    // zero unite appliquee : retour legitime
    }

    // --- Tracage optionnel des variations de stock ------------------------
    if (g_traceRes && guidPtr > 0x10000) {
        char nom[64] = { 0 };
        uintptr_t np = 0;
        // La definition commence 8 octets avant son GUID.
        if (SafeRead((void*)(guidPtr - 8), &np, 8) && np > 0x10000)
            SafeRead((void*)np, nom, sizeof(nom) - 1);
        if (nom[0] < 0x20 || nom[0] > 0x7E) nom[0] = 0;

        printf("[RES] %-20s %+6d\n", nom[0] ? nom : "?", amount);
    }

    return oModifyResource(self, guidPtr, amount, p4, p5, p6);
}

int Res_Get(const char* name)
{
    const KnownResource* k = FindKnown(name);
    if (!k || !g_lockReady) return -1;

    int result = -1;
    EnterCriticalSection(&g_lock);
    ResCtx* c = FindByGuid(k->guid);
    uintptr_t addr = 0;
    if (ResolveAddr(c, addr)) {
        int v = -1;
        if (SafeRead((void*)addr, &v, sizeof(v))) result = v;
    }
    LeaveCriticalSection(&g_lock);
    return result;
}

/*bool Res_Add(const char* name, int delta)
{
    const KnownResource* k = FindKnown(name);
    if (!k || !g_lockReady) return false;

    bool ok = false;
    EnterCriticalSection(&g_lock);
    ResCtx* c = FindByGuid(k->guid);
    if (c && c->valid && oModifyResource) {
        oModifyResource(c->self, c->resId, delta, c->arg4);
        ok = true;
    }
    LeaveCriticalSection(&g_lock);
    return ok;
}*/

void InstallResourceHooks(uintptr_t modBase)
{
    if (!g_lockReady) { InitializeCriticalSection(&g_lock); g_lockReady = true; }

    ResIdToIndex = reinterpret_cast<ResIdToIndex_t>(modBase + GameOffsets::ResIdToIndex);

    void* target = (void*)(modBase + GameOffsets::ModifyResource);
    if (MH_CreateHook(target, (void*)&hkModifyResource, (void**)&oModifyResource) != MH_OK
        || MH_EnableHook(target) != MH_OK)
        printf("[!] hook ModifyResource KO\n");
}
