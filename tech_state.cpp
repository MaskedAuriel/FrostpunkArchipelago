// tech_state.cpp
#include "tech_state.h"
#include "mod_common.h"

// Recupere base + count du tableau des technos. false si indisponible.
static bool TechTable(uintptr_t& base, int& count)
{
    if (!g_selfTech) return false;
    base = 0; count = 0;
    if (!SafeRead((void*)(g_selfTech + TECH_LIST_OFF),  &base,  sizeof(base)))  return false;
    if (!SafeRead((void*)(g_selfTech + TECH_COUNT_OFF), &count, sizeof(count))) return false;
    return base > 0x10000 && count > 0 && count <= 4096;
}

uint64_t HashOfTechPtr(uintptr_t techPtr)
{
    if (techPtr <= 0x10000) return 0;
    uint64_t h = 0;
    return SafeRead((void*)(techPtr + TECH_HASH_OFF), &h, 8) ? h : 0;
}

// Adresse de l'ENTREE (bloc de 0x10) correspondant au hash. 0 si absent.
static uintptr_t FindEntryByHash(uint64_t hash)
{
    uintptr_t base; int count;
    if (!hash || !TechTable(base, count)) return 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8)) continue;
        if (HashOfTechPtr(tech) == hash) return entry;
    }
    return 0;
}

int32_t TechStateByHash(uint64_t hash)
{
    uintptr_t entry = FindEntryByHash(hash);
    if (!entry) return -1;
    int32_t st = -1;
    SafeRead((void*)(entry + TECH_ENTRY_STATE), &st, 4);
    return st;
}

bool WriteTechStateByHash(uint64_t hash, int32_t state)
{
    uintptr_t entry = FindEntryByHash(hash);
    if (!entry) return false;
    return SafeWrite((void*)(entry + TECH_ENTRY_STATE), &state, 4);
}

uintptr_t FindTechPtrByHash(uint64_t hash)
{
    uintptr_t entry = FindEntryByHash(hash);
    if (!entry) return 0;
    uintptr_t tech = 0;
    SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8);
    return tech;
}

// --- Acces par pointeur d'objet techno --------------------------------------
// On passe self explicitement : dans les hooks il arrive en parametre et peut
// etre plus fiable que le global au moment de l'appel.
static uintptr_t FindEntryByPtr(uintptr_t self, uintptr_t techPtr)
{
    uintptr_t base = 0; int count = 0;
    if (!SafeRead((void*)(self + TECH_LIST_OFF),  &base,  sizeof(base)))  return 0;
    if (!SafeRead((void*)(self + TECH_COUNT_OFF), &count, sizeof(count))) return 0;
    if (base <= 0x10000 || count <= 0 || count > 4096) return 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, t = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &t, 8)) continue;
        if (t == techPtr) return entry;
    }
    return 0;
}

int32_t TechStateByPtr(uintptr_t self, uintptr_t techPtr)
{
    uintptr_t entry = (self && techPtr > 0x10000) ? FindEntryByPtr(self, techPtr) : 0;
    if (!entry) return -1;
    int32_t st = -1;
    SafeRead((void*)(entry + TECH_ENTRY_STATE), &st, 4);
    return st;
}

bool WriteTechStateByPtr(uintptr_t self, uintptr_t techPtr, int32_t state)
{
    uintptr_t entry = (self && techPtr > 0x10000) ? FindEntryByPtr(self, techPtr) : 0;
    if (!entry) return false;
    return SafeWrite((void*)(entry + TECH_ENTRY_STATE), &state, 4);
}

bool IsInProgress(uintptr_t self, uintptr_t techPtr)
{
    if (!self) return false;
    uintptr_t cur = 0;
    if (!SafeRead((void*)(self + TECH_ACTIVE_OFF), &cur, 8)) return false;
    return cur != 0 && cur == techPtr;
}


void InternalToSeg(const char* internal, char* out, int maxlen)
{
    if (!internal || maxlen < 1) { if (maxlen > 0) out[0] = 0; return; }
    const char* p = internal;

    // Prefixe de campagne "[Builders] ", "[Reconquest] ", "[Winterhome] "
    if (*p == '[') {
        const char* close = p;
        while (*close && *close != ']') ++close;
        if (*close == ']') { p = close + 1; while (*p == ' ') ++p; }
    }

    // Format habituel "<LETTRES> <num>_<Nom>" : tout ce qui suit le PREMIER '_'
    const char* us = p;
    while (*us && *us != '_') ++us;
    if (*us == '_') {
        p = us + 1;
    }
    else {
        // Pas de '_' : "H 5 Generator Upgrade..." -> retirer lettres puis chiffres
        const char* q = p;
        while (*q >= 'A' && *q <= 'z' && *q != ' ') ++q;   // jeton de lettres
        if (q != p && *q == ' ') {
            const char* r = q + 1;
            const char* d = r;
            while (*d >= '0' && *d <= '9') ++d;            // jeton de chiffres
            p = (d != r && *d == ' ') ? d + 1 : r;
        }
    }

    int j = 0;
    for (; p[j] && j < maxlen - 1; ++j) out[j] = p[j];
    out[j] = 0;
}

bool StrEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

// --- Parcours ---------------------------------------------------------------
int ForEachTech(void (*fn)(uint64_t hash))
{
    uintptr_t base; int count;
    if (!fn || !TechTable(base, count)) return 0;
    int n = 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8)) continue;
        uint64_t h = HashOfTechPtr(tech);
        if (h) { fn(h); n++; }
    }
    return n;
}

void DumpAllStates()
{
    uintptr_t base; int count;
    if (!TechTable(base, count)) { printf("[DUMP] tableau technos indisponible\n"); return; }
    int c0 = 0, c1 = 0, c2 = 0, cx = 0;
    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE;
        int32_t st = -1;
        SafeRead((void*)(entry + TECH_ENTRY_STATE), &st, 4);
        if (st == 0) c0++; else if (st == 1) c1++; else if (st == 2) c2++; else cx++;
    }
    printf("[DUMP] %d technos : etat0=%d etat1=%d etat2=%d autre=%d\n",
           count, c0, c1, c2, cx);
}

struct SegRow { char seg[48]; uint64_t hash; };

static bool SegEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

void DumpTechNames()
{
    if (!g_selfTech) { printf("[NAMES] g_selfTech = 0 : lance une recherche d'abord\n"); return; }

    uintptr_t base = 0; int count = 0;
    if (!SafeRead((void*)(g_selfTech + TECH_LIST_OFF), &base, sizeof(base)) ||
        !SafeRead((void*)(g_selfTech + TECH_COUNT_OFF), &count, sizeof(count)) ||
        base <= 0x10000 || count <= 0 || count > 4096) {
        printf("[NAMES] tableau technos illisible\n"); return;
    }

    static SegRow rows[512];
    int n = 0, broken = 0;

    for (int i = 0; i < count && n < 512; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8) || tech <= 0x10000) continue;

        uintptr_t desc = 0;
        char internal[64] = { 0 };
        if (!SafeRead((void*)(tech + TECH_DESC_OFF), &desc, 8) || desc <= 0x10000 ||
            !SafeRead((void*)desc, internal, sizeof(internal) - 1)) {
            printf("  [CASSE] [%3d] nom interne illisible\n", i);
            broken++;
            continue;
        }

        char seg[48];
        InternalToSeg(internal, seg, sizeof(seg));
        if (!seg[0]) {
            printf("  [CASSE] [%3d] \"%s\" -> segment vide\n", i, internal);
            broken++;
            continue;
        }

        SegRow* r = &rows[n++];
        int k = 0; for (; seg[k] && k < 47; ++k) r->seg[k] = seg[k]; r->seg[k] = 0;
        r->hash = HashOfTechPtr(tech);
    }

    // Doublons : segments portes par plusieurs technos (hash differents).
    int dupGroups = 0;
    printf("\n[NAMES] %d technos analysees\n", n);
    for (int i = 0; i < n; ++i) {
        bool firstOfGroup = true;
        for (int j = 0; j < i; ++j)
            if (SegEq(rows[j].seg, rows[i].seg)) { firstOfGroup = false; break; }
        if (!firstOfGroup) continue;

        int c = 0;
        for (int j = 0; j < n; ++j) if (SegEq(rows[j].seg, rows[i].seg)) c++;
        if (c > 1) {
            if (dupGroups == 0) printf("[NAMES] segments partages (normal, variantes de campagne) :\n");
            printf("   %-32s x%d\n", rows[i].seg, c);
            dupGroups++;
        }
    }

    printf("[NAMES] conversions cassees : %d\n", broken);
    printf("[NAMES] segments partages   : %d groupe(s)\n", dupGroups);
    if (broken == 0)
        printf("[NAMES] OK : tous les noms se convertissent. Les doublons sont\n"
            "        sans effet, SegIsResearched teste toutes les variantes.\n");
}


// --- Explore la techno dont le nom interne contient 'motif' -----------------
void DumpTechStrings(const char* motif)
{
    if (!g_selfTech) { printf("[STR] g_selfTech = 0\n"); return; }

    uintptr_t base = 0; int count = 0;
    if (!SafeRead((void*)(g_selfTech + TECH_LIST_OFF), &base, sizeof(base)) ||
        !SafeRead((void*)(g_selfTech + TECH_COUNT_OFF), &count, sizeof(count)) ||
        base <= 0x10000 || count <= 0 || count > 4096) {
        printf("[STR] tableau technos illisible\n"); return;
    }

    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8) || tech <= 0x10000) continue;

        uintptr_t desc = 0;
        char internal[64] = { 0 };
        if (!SafeRead((void*)(tech + TECH_DESC_OFF), &desc, 8) || desc <= 0x10000) continue;
        if (!SafeRead((void*)desc, internal, sizeof(internal) - 1)) continue;

        // le nom interne contient-il le motif ?
        bool found = false;
        for (int a = 0; internal[a] && !found; ++a) {
            int b = 0;
            while (motif[b] && internal[a + b] == motif[b]) b++;
            if (!motif[b]) found = true;
        }
        if (!found) continue;

        printf("\n[STR] techno \"%s\"  (objet 0x%llX, desc 0x%llX)\n",
            internal, (unsigned long long)tech, (unsigned long long)desc);
        ScanForStrings("objet techno", tech, 0x180);
        ScanForStrings("descripteur", desc, 0x100);
        return;                      // une seule suffit
    }
    printf("[STR] aucune techno dont le nom contient \"%s\"\n", motif);
}

bool TechLocKey(uintptr_t techPtr, char* out, int maxlen)
{
    if (techPtr <= 0x10000 || maxlen < 2) return false;
    uintptr_t p = 0;
    if (!SafeRead((void*)(techPtr + TECH_LOCKEY_OFF), &p, 8) || p <= 0x10000)
        return false;

    char raw[96] = { 0 };
    if (!SafeRead((void*)p, raw, sizeof(raw) - 1)) return false;

    const char* s = raw;
    if (*s == '@') ++s;                     // retire le prefixe
    if (!*s) return false;

    int j = 0;
    for (; s[j] && j < maxlen - 1; ++j) out[j] = s[j];
    out[j] = 0;
    return j > 0;
}





// Lit la cle stockee a un offset donne de l'objet techno, sans le '@'.
bool TechLocKeyAt(uintptr_t techPtr, int off, char* out, int maxlen)
{
    if (techPtr <= 0x10000 || maxlen < 2) return false;
    uintptr_t p = 0;
    if (!SafeRead((void*)(techPtr + off), &p, 8) || p <= 0x10000) return false;
    char raw[96] = { 0 };
    if (!SafeRead((void*)p, raw, sizeof(raw) - 1)) return false;
    const char* s = (raw[0] == '@') ? raw + 1 : raw;
    if (!*s) return false;
    int j = 0; for (; s[j] && j < maxlen - 1; ++j) out[j] = s[j];
    out[j] = 0;
    return j > 0;
}

// --- Table cle -> (techno, emplacement) ------------------------------------
// Construite une fois par partie. Les comparaisons passent d'abord par un
// hash 32 bits, la chaine n'est comparee qu'en cas de collision.
struct LocKeyEntry { uint32_t h32; uint64_t techHash; int slot; char key[96]; };

static LocKeyEntry g_locKeys[600];
static int         g_locKeyCount = 0;
static uintptr_t   g_locKeyBuiltFor = 0;

static uint32_t Fnv32(const char* s)
{
    uint32_t h = 2166136261u;
    for (int i = 0; s[i]; ++i) { h ^= (unsigned char)s[i]; h *= 16777619u; }
    return h;
}

static void BuildLocKeyTable()
{
    g_locKeyCount = 0;
    g_locKeyBuiltFor = g_selfTech;
    if (!g_selfTech) return;

    uintptr_t base = 0; int count = 0;
    if (!SafeRead((void*)(g_selfTech + TECH_LIST_OFF), &base, sizeof(base)) ||
        !SafeRead((void*)(g_selfTech + TECH_COUNT_OFF), &count, sizeof(count)) ||
        base <= 0x10000 || count <= 0 || count > 4096) return;

    const int offs[3] = { TECH_LOCNAME_OFF, TECH_LOCDESC_OFF, TECH_LOCSHORT_OFF };

    for (int i = 0; i < count; ++i) {
        uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
        if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8) || tech <= 0x10000) continue;
        uint64_t th = HashOfTechPtr(tech);
        if (!th) continue;

        for (int s = 0; s < 3; ++s) {
            if (g_locKeyCount >= 600) break;
            char k[96];
            if (!TechLocKeyAt(tech, offs[s], k, sizeof(k))) continue;
            LocKeyEntry* e = &g_locKeys[g_locKeyCount++];
            int j = 0; for (; k[j] && j < 95; ++j) e->key[j] = k[j]; e->key[j] = 0;
            e->h32 = Fnv32(e->key);
            e->techHash = th;
            e->slot = s;                    // 0 = Name, 1 = Description, 2 = Short
        }
    }
    printf("[LOC] table construite : %d cles pour %d technos\n", g_locKeyCount, count);
}

// La cle appartient-elle a une techno ? Renvoie son hash et l'emplacement.
bool LookupLocKey(const char* localKey, uint64_t* outHash, int* outSlot)
{
    if (!localKey || !*localKey || !g_selfTech) return false;
    if (g_locKeyBuiltFor != g_selfTech) BuildLocKeyTable();   // nouvelle partie

    uint32_t h = Fnv32(localKey);
    for (int i = 0; i < g_locKeyCount; ++i) {
        if (g_locKeys[i].h32 != h) continue;
        bool eq = true;
        const char* a = g_locKeys[i].key;
        for (int j = 0; ; ++j) { if (a[j] != localKey[j]) { eq = false; break; } if (a[j] == 0) break; }
        if (!eq) continue;
        if (outHash) *outHash = g_locKeys[i].techHash;
        if (outSlot) *outSlot = g_locKeys[i].slot;
        return true;
    }
    return false;
}