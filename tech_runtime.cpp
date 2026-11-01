// tech_runtime.cpp
#include "tech_runtime.h"

// 256 emplacements pour 172 technos connues : marge confortable.
// A augmenter si des technos supplementaires apparaissent (DLC).
static const int RT_MAX = 256;
static TechRuntime g_rt[RT_MAX];
static int         g_rtCount = 0;

TechRuntime* RT_Get(uint64_t hash)
{
    for (int i = 0; i < g_rtCount; ++i)
        if (g_rt[i].used && g_rt[i].hash == hash) return &g_rt[i];

    if (g_rtCount >= RT_MAX) {
        printf("[!] tech_runtime plein (%d entrees) : augmente RT_MAX\n", RT_MAX);
        return nullptr;
    }
    TechRuntime* r = &g_rt[g_rtCount++];
    r->hash = hash;
    r->researched = false;
    r->received   = false;
    r->used       = true;
    return r;
}

bool RT_CanResearch(uint64_t hash)
{
    TechRuntime* r = RT_Get(hash);
    return r ? !r->researched : true;
}

int RT_Count() { return g_rtCount; }

TechRuntime* RT_At(int i) { return (i >= 0 && i < g_rtCount && g_rt[i].used) ? &g_rt[i] : nullptr; }

void RT_Reset()
{
    memset(g_rt, 0, sizeof(g_rt));
    g_rtCount = 0;
}
