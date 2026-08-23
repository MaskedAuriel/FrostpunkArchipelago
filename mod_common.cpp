// mod_common.cpp -- definitions du socle partage
#include "mod_common.h"
#include "scenario.h"
#include "MinHook.h"
#include <psapi.h>



uintptr_t g_selfRessource = 0;
uintptr_t g_selfTech      = 0;
uintptr_t g_setupSelf = 0;
uintptr_t g_scenarioSelf = 0;
extern volatile DWORD g_instantArbre = 0;

bool SafeRead(const void* src, void* dst, size_t size)
{
    __try { memcpy(dst, src, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool SafeWrite(void* dst, const void* src, size_t n)
{
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    return true;
}

bool ReadAscii(uintptr_t addr, char* out, int maxlen)
{
    if (addr <= 0x10000) return false;
    char tmp[80] = { 0 };
    if (!SafeRead((void*)addr, tmp, sizeof(tmp) - 1)) return false;
    if (tmp[0] < 0x20 || tmp[0] > 0x7E) return false;      // 1er car imprimable
    int n = 0;
    while (tmp[n] && n < maxlen - 1) {
        if (tmp[n] < 0x20 || tmp[n] > 0x7E) return false;  // caractere binaire
        out[n] = tmp[n]; n++;
    }
    if (n < 2) return false;                               // trop court
    out[n] = 0;
    return true;
}

// --- Explore une zone memoire et affiche les chaines pointees ---------------
void ScanForStrings(const char* label, uintptr_t base, int lenBytes)
{
    printf("  --- %s (0x%llX) ---\n", label, (unsigned long long)base);
    for (int off = 0; off < lenBytes; off += 8) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(base + off), &p, 8)) continue;

        char s[80];
        // cas 1 : le champ pointe directement sur une chaine
        if (ReadAscii(p, s, sizeof(s))) {
            printf("    +0x%03X -> \"%s\"\n", off, s);
            continue;
        }
        // cas 2 : le champ pointe sur une struct dont le 1er champ est la chaine
        if (p > 0x10000) {
            uintptr_t p2 = 0;
            if (SafeRead((void*)p, &p2, 8) && ReadAscii(p2, s, sizeof(s)))
                printf("    +0x%03X -> [ptr] -> \"%s\"\n", off, s);
        }
    }
}



void FindStaticPointersTo(uintptr_t value)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    if (!base || !value) { printf("[PTR] rien a chercher\n"); return; }

    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)base, &dos, sizeof(dos))) return;
    if (!SafeRead((void*)(base + dos.e_lfanew), &nt, sizeof(nt))) return;
    uintptr_t end = base + nt.OptionalHeader.SizeOfImage;

    printf("\n[PTR] recherche de 0x%llX dans l'image...\n", (unsigned long long)value);

    int found = 0;
    uintptr_t addr = base;
    while (addr < end && found < 64) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) break;
        if (mbi.RegionSize == 0) break;

        uintptr_t rstart = (uintptr_t)mbi.BaseAddress;
        uintptr_t rend = rstart + mbi.RegionSize;
        if (rend > end) rend = end;

        bool lisible = (mbi.State == MEM_COMMIT) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
        if (lisible) {
            for (uintptr_t p = (rstart + 7) & ~(uintptr_t)7; p + 8 <= rend; p += 8) {
                if (*(uintptr_t*)p == value) {
                    printf("   Frostpunk.exe+%llX\n", (unsigned long long)(p - base));
                    if (++found >= 64) break;
                }
            }
        }
        addr = rend;
    }
    printf("[PTR] %d emplacement(s)\n", found);
}

static const uintptr_t TECH_SELF_GLOBALS[] = { 0x2C30A20, 0x4062BA8 };
static const uintptr_t RES_SELF_GLOBALS[] = { 0x40916D0, 0x2C309E0 };



static bool TechSelfValid(uintptr_t p)
{
    if (p <= 0x10000) return false;
    uintptr_t list = 0; int count = 0;
    if (!SafeRead((void*)(p + TECH_LIST_OFF), &list, 8)) return false;
    if (!SafeRead((void*)(p + TECH_COUNT_OFF), &count, 4)) return false;
    return list > 0x10000 && count > 0 && count < 4096;
}


static bool ResSelfValid(uintptr_t p)
{
    if (p <= 0x10000) return false;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(p + RES_CONT_OFF), &cont, 8)) return false;
    return cont > 0x10000;
}

// Resout un self depuis une liste de globaux candidats.

static bool ResolveSelf(uintptr_t& dest, const uintptr_t* offs, int n,
    bool (*valide)(uintptr_t), const char* label)
{
    if (valide(dest)) return true;                 // deja bon

    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    for (int i = 0; i < n; ++i) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(base + offs[i]), &p, 8)) continue;
        if (!valide(p)) continue;
        dest = p;
        printf("[SELF] %s resolu via Frostpunk.exe+%llX -> 0x%llX\n",
            label, (unsigned long long)offs[i], (unsigned long long)p);
        return true;
    }
    return false;
}

bool EnsureTechSelf()
{
    return ResolveSelf(g_selfTech, TECH_SELF_GLOBALS,
        (int)(sizeof(TECH_SELF_GLOBALS) / sizeof(uintptr_t)),
        &TechSelfValid, "g_selfTech");
}

bool EnsureResSelf()
{
    return ResolveSelf(g_selfRessource, RES_SELF_GLOBALS,
        (int)(sizeof(RES_SELF_GLOBALS) / sizeof(uintptr_t)),
        &ResSelfValid, "g_selfRessource");
}

bool StrContains(const char* s, const char* sub)
{
    if (!sub || !*sub) return true;
    for (int i = 0; s[i]; ++i) {
        int j = 0;
        while (sub[j] && s[i + j] == sub[j]) j++;
        if (!sub[j]) return true;
    }
    return false;
}

// Enumere les objets C++ du tas dont le nom de classe contient 'filtre'.
// Un objet commence par sa vtable, qui pointe forcement dans l'image du
// module : ce test elimine presque tout avant meme de toucher au RTTI.
void ScanObjectsByClass(const char* filtre, int maxResults)
{
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)modBase, &dos, sizeof(dos))) return;
    if (!SafeRead((void*)(modBase + dos.e_lfanew), &nt, sizeof(nt))) return;
    uintptr_t modEnd = modBase + nt.OptionalHeader.SizeOfImage;

    printf("\n[OBJ] recherche de classes contenant \"%s\"\n", filtre);

    int found = 0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (found < maxResults && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));

        if (lisible && mbi.RegionSize <= 0x4000000) {
            uintptr_t s = (uintptr_t)mbi.BaseAddress;
            for (uintptr_t p = s; p + 8 <= next; p += 8) {
                uintptr_t vptr = 0;
                if (!SafeRead((void*)p, &vptr, 8)) continue;
                if (vptr < modBase || vptr >= modEnd) continue;

                char cls[128];
                if (!GetClassNameFP(p, cls, sizeof(cls))) continue;
                if (!StrContains(cls, filtre)) continue;

                printf("   0x%llX  %s\n", (unsigned long long)p, cls);
                if (++found >= maxResults) break;
            }
        }
        addr = next;
    }
    printf("[OBJ] %d objet(s)\n", found);
}

void DumpObject(uintptr_t obj, int lenBytes)
{
    if (obj <= 0x10000) { printf("[DUMP] objet invalide\n"); return; }

    char cls[128] = { 0 };
    GetClassNameFP(obj, cls, sizeof(cls));
    printf("\n[DUMP] 0x%llX  %s\n", (unsigned long long)obj, cls[0] ? cls : "(sans RTTI)");

    for (int off = 0; off + 8 <= lenBytes; off += 8) {
        uintptr_t v = 0;
        if (!SafeRead((void*)(obj + off), &v, 8)) continue;
        if (!v) continue;

        char info[192] = { 0 };

        if (v > 0x10000) {
            char c2[128] = { 0 };
            if (GetClassNameFP(v, c2, sizeof(c2)))
                _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> [%s]", c2);

            if (!info[0]) {
                char s[96] = { 0 };
                if (SafeRead((void*)v, s, sizeof(s) - 1) &&
                    s[0] >= 0x20 && s[0] < 0x7F && s[1]) {
                    bool ok = true;
                    for (int i = 0; s[i] && i < 64; ++i)
                        if (s[i] < 0x20 || s[i] > 0x7E) { ok = false; break; }
                    if (ok) _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> \"%s\"", s);
                }
            }
        }

        // Interpretations numeriques utiles quand ce n'est pas un pointeur.
        uint32_t lo = (uint32_t)v;
        float f; memcpy(&f, &lo, 4);
        printf("   +0x%03X = 0x%016llX  (i32=%d f=%.3f)%s\n",
            off, (unsigned long long)v, (int)lo, f, info);
    }
}

uintptr_t FindFirstObjectByClass(const char* filtre)
{
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)modBase, &dos, sizeof(dos))) return 0;
    if (!SafeRead((void*)(modBase + dos.e_lfanew), &nt, sizeof(nt))) return 0;
    uintptr_t modEnd = modBase + nt.OptionalHeader.SizeOfImage;

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
                if (StrContains(cls, filtre)) return p;
            }
        }
        addr = next;
    }
    return 0;
}

void DumpVTable(uintptr_t obj, int count)
{
    uintptr_t vptr = 0;
    if (!SafeRead((void*)obj, &vptr, 8) || vptr <= 0x10000) return;

    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    char cls[128] = { 0 };
    GetClassNameFP(obj, cls, sizeof(cls));
    printf("\n[VTBL] %s\n       vtable = Frostpunk.exe+%llX\n",
        cls[0] ? cls : "?", (unsigned long long)(vptr - base));

    for (int i = 0; i < count; ++i) {
        uintptr_t fn = 0;
        if (!SafeRead((void*)(vptr + (uintptr_t)i * 8), &fn, 8)) break;
        if (fn < base) break;                       // fin de table
        printf("   [%2d] Frostpunk.exe+%llX\n", i, (unsigned long long)(fn - base));
    }
}

void DumpPropertyManager(const char* filtre, int maxEntries)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), pm = 0;
    if (!SafeRead((void*)(base + 0x408B740), &pm, 8) || pm <= 0x10000) {
        printf("[PM] PropertyManager introuvable\n"); return;
    }

    const int ENTRY = 0xE0, FIRST = 0x20;
    const int NAME = 0x40, SIZE = 0x68, PROPS = 0x00;

    printf("\n[PM] 0x%llX  filtre \"%s\"\n", (unsigned long long)pm, filtre);
    int n = 0;
    for (int i = 0; i < maxEntries; ++i) {
        uintptr_t e = pm + FIRST + (uintptr_t)i * ENTRY;

        uintptr_t namePtr = 0;
        if (!SafeRead((void*)(e + NAME), &namePtr, 8) || namePtr <= 0x10000) continue;

        char nom[192] = { 0 };
        if (!SafeRead((void*)namePtr, nom, sizeof(nom) - 1)) continue;
        if (nom[0] < 0x20 || nom[0] > 0x7E || !nom[1]) continue;
        if (!StrContains(nom, filtre)) continue;

        uint64_t taille = 0; uintptr_t props = 0;
        SafeRead((void*)(e + SIZE), &taille, 8);
        SafeRead((void*)(e + PROPS), &props, 8);

        char cp[128] = { 0 };
        if (props > 0x10000) GetClassNameFP(props, cp, sizeof(cp));

        printf("   [%4d] %-58s taille=0x%llX  props=0x%llX %s\n",
            i, nom, (unsigned long long)taille,
            (unsigned long long)props, cp);
        n++;
    }
    printf("[PM] %d entree(s)\n", n);
}

void DumpClassProps(const char* className, int lenBytes)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), pm = 0;
    if (!SafeRead((void*)(base + 0x408B740), &pm, 8) || pm <= 0x10000) {
        printf("[PM] PropertyManager introuvable\n"); return;
    }

    const int ENTRY = 0xE0, FIRST = 0x20, NAME = 0x40, PROPS = 0x00;

    for (int i = 0; i < 200; ++i) {
        uintptr_t e = pm + FIRST + (uintptr_t)i * ENTRY;

        uintptr_t namePtr = 0;
        if (!SafeRead((void*)(e + NAME), &namePtr, 8) || namePtr <= 0x10000) continue;

        char nom[192] = { 0 };
        if (!SafeRead((void*)namePtr, nom, sizeof(nom) - 1)) continue;
        if (!StrContains(nom, className)) continue;

        uintptr_t props = 0;
        SafeRead((void*)(e + PROPS), &props, 8);
        printf("\n[PM] %s -> props = 0x%llX\n", nom, (unsigned long long)props);
        if (props > 0x10000) DumpObject(props, lenBytes);
        return;
    }
    printf("[PM] classe \"%s\" absente du premier bloc\n", className);
}
void DumpRecordProperties(uintptr_t rec)
{
    if (!rec) return;
    const int ARR[2] = { 0x10, 0x28 };

    for (int a = 0; a < 2; ++a) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        SafeRead((void*)(rec + ARR[a]), &arr, 8);
        SafeRead((void*)(rec + ARR[a] + 8), &cnt, 4);
        if (arr <= 0x10000 || cnt == 0 || cnt > 512) continue;

        printf("   -- tableau %d : %u element(s) --\n", a, cnt);
        for (uint32_t n = 0; n < cnt; ++n) {
            uintptr_t node = 0;
            if (!SafeRead((void*)(arr + (uintptr_t)n * 8), &node, 8)) continue;
            if (node <= 0x10000) continue;

            uintptr_t pName = 0; uint64_t off = 0;
            SafeRead((void*)(node + 0x08), &pName, 8);
            SafeRead((void*)(node + 0x28), &off, 8);

            char pn[128] = { 0 };
            if (pName > 0x10000) SafeRead((void*)pName, pn, sizeof(pn) - 1);
            char cls[96] = { 0 };
            GetClassNameFP(node, cls, sizeof(cls));

            printf("      +0x%03llX  %-40s [%s]\n",
                (unsigned long long)off, pn[0] ? pn : "(sans nom)", cls);
        }
    }
}

uintptr_t FindClassRecord(const char* className)
{
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)modBase, &dos, sizeof(dos))) return 0;
    if (!SafeRead((void*)(modBase + dos.e_lfanew), &nt, sizeof(nt))) return 0;

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
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; p + 8 <= next; p += 8) {
                uintptr_t s = *(uintptr_t*)p;
                if (s <= 0x10000) continue;

                char buf[192] = { 0 };
                if (!SafeRead((void*)s, buf, sizeof(buf) - 1)) continue;

                bool egal = true;                    // comparaison EXACTE
                for (int i = 0; ; ++i) {
                    if (buf[i] != className[i]) { egal = false; break; }
                    if (className[i] == 0) break;
                }
                if (!egal) continue;

                uintptr_t rec = p - 0x40;            // le nom est en +0x40
                uint64_t taille = 0;
                if (!SafeRead((void*)(rec + 0x68), &taille, 8)) continue;
                if (taille == 0 || taille > 0x10000) continue;   // controle

                printf("[REC] %s -> enregistrement 0x%llX (taille 0x%llX)\n",
                    className, (unsigned long long)rec,
                    (unsigned long long)taille);
                return rec;
            }
        }
        addr = next;
    }
    printf("[REC] \"%s\" introuvable\n", className);
    return 0;
}

void ListClassRecords(const char* filtre, int maxResults)
{
    printf("\n[REC] classes contenant \"%s\"\n", filtre);
    int found = 0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (found < maxResults && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        if (lisible && mbi.RegionSize <= 0x4000000) {
            uintptr_t deb = (uintptr_t)mbi.BaseAddress;
            for (uintptr_t p = deb + 0x40; p + 8 <= next; p += 8) {
                uintptr_t s = *(uintptr_t*)p;
                if (s <= 0x10000) continue;

                char nom[192] = { 0 };
                if (!SafeRead((void*)s, nom, sizeof(nom) - 1)) continue;
                if (nom[0] < 0x20 || nom[0] > 0x7E || !nom[1]) continue;
                if (!StrContains(nom, filtre)) continue;

                uintptr_t rec = p - 0x40;
                uint64_t taille = 0; uintptr_t arr = 0; uint32_t cnt = 0;
                if (!SafeRead((void*)(rec + 0x68), &taille, 8)) continue;
                if (taille == 0 || taille > 0x10000) continue;
                if (!SafeRead((void*)(rec + 0x10), &arr, 8)) continue;
                if (!SafeRead((void*)(rec + 0x18), &cnt, 4)) continue;
                if (arr <= 0x10000 || cnt == 0 || cnt > 512) continue;

                printf("   0x%llX  %-52s taille=0x%llX  %u prop.\n",
                    (unsigned long long)rec, nom,
                    (unsigned long long)taille, cnt);
                found++;
                if (found >= maxResults) break;
            }
        }
        addr = next;
    }
    printf("[REC] %d classe(s)\n", found);
}

void FindPropertyByName(const char* filtre)
{
    printf("\n[PROP] proprietes contenant \"%s\"\n", filtre);
    int found = 0;
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
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress + 0x40; p + 8 <= next; p += 8) {
                uintptr_t s = *(uintptr_t*)p;
                if (s <= 0x10000) continue;

                char nomCls[192] = { 0 };
                if (!SafeRead((void*)s, nomCls, sizeof(nomCls) - 1)) continue;
                if (nomCls[0] < 0x20 || nomCls[0] > 0x7E || !nomCls[1]) continue;

                uintptr_t rec = p - 0x40;
                uint64_t taille = 0; uintptr_t arr = 0; uint32_t cnt = 0;
                if (!SafeRead((void*)(rec + 0x68), &taille, 8)) continue;
                if (taille == 0 || taille > 0x10000) continue;
                if (!SafeRead((void*)(rec + 0x10), &arr, 8)) continue;
                if (!SafeRead((void*)(rec + 0x18), &cnt, 4)) continue;
                if (arr <= 0x10000 || cnt == 0 || cnt > 512) continue;

                for (uint32_t n = 0; n < cnt; ++n) {
                    uintptr_t node = 0;
                    if (!SafeRead((void*)(arr + (uintptr_t)n * 8), &node, 8)) continue;
                    if (node <= 0x10000) continue;

                    uintptr_t pName = 0; uint64_t off = 0;
                    SafeRead((void*)(node + 0x08), &pName, 8);
                    SafeRead((void*)(node + 0x28), &off, 8);
                    if (pName <= 0x10000) continue;

                    char pn[128] = { 0 };
                    if (!SafeRead((void*)pName, pn, sizeof(pn) - 1)) continue;
                    if (!StrContains(pn, filtre)) continue;

                    printf("   %-52s +0x%03llX  %s\n",
                        nomCls, (unsigned long long)off, pn);
                    found++;
                }
            }
        }
        addr = next;
    }
    printf("[PROP] %d resultat(s)\n", found);
}

void DumpDynarray(uintptr_t owner, int off, int elemSize, int maxShow)
{
    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
    SafeRead((void*)(owner + off), &arr, 8);
    SafeRead((void*)(owner + off + 8), &cnt, 4);
    SafeRead((void*)(owner + off + 12), &cap, 4);

    printf("\n[ARR] +0x%03X -> 0x%llX  %u element(s) (capacite %u), pas 0x%X\n",
        off, (unsigned long long)arr, cnt, cap, elemSize);
    if (arr <= 0x10000 || cnt == 0 || cnt > 4096) return;

    for (uint32_t i = 0; i < cnt && (int)i < maxShow; ++i) {
        uintptr_t e = arr + (uintptr_t)i * elemSize;

        uintptr_t v = 0;
        SafeRead((void*)e, &v, 8);

        char info[224] = { 0 };
        if (v > 0x10000) {
            char cls[128] = { 0 };
            if (GetClassNameFP(v, cls, sizeof(cls)))
                _snprintf_s(info, sizeof(info), _TRUNCATE, "  [%s]", cls);
            if (!info[0]) {
                char s[96] = { 0 };
                if (SafeRead((void*)v, s, sizeof(s) - 1) &&
                    s[0] >= 0x20 && s[0] < 0x7F && s[1])
                    _snprintf_s(info, sizeof(info), _TRUNCATE, "  \"%s\"", s);
            }
        }
        printf("   [%2u] 0x%016llX  (i32=%d)%s\n",
            i, (unsigned long long)v, (int)(uint32_t)v, info);
    }
}

int CollectObjectsByClass(const char* filtre, uintptr_t* out, int maxOut)
{
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
    if (!SafeRead((void*)modBase, &dos, sizeof(dos))) return 0;
    if (!SafeRead((void*)(modBase + dos.e_lfanew), &nt, sizeof(nt))) return 0;
    uintptr_t modEnd = modBase + nt.OptionalHeader.SizeOfImage;

    int n = 0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    while (n < maxOut && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
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
                out[n++] = p;
                if (n >= maxOut) break;
            }
        }
        addr = next;
    }
    return n;
}

typedef HANDLE(WINAPI* CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES,
    DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI* ReadFile_t)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef BOOL(WINAPI* SetFilePointerEx_t)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER, DWORD);

static CreateFileW_t      oCreateFileW = nullptr;
static ReadFile_t         oReadFile = nullptr;
static SetFilePointerEx_t oSetFilePointerEx = nullptr;

// Handles des archives qui nous interessent.
static HANDLE g_hArchives[8] = {};
static int    g_nArchives = 0;
volatile bool g_traceIO = false;
volatile bool g_consolePrete = false;    // mis a true apres AllocConsole

static bool EstArchiveSuivie(HANDLE h)
{
    for (int i = 0; i < g_nArchives; ++i) if (g_hArchives[i] == h) return true;
    return false;
}



static BOOL WINAPI hkSetFilePointerEx(HANDLE h, LARGE_INTEGER dist,
    PLARGE_INTEGER nouv, DWORD methode)
{
    BOOL r = oSetFilePointerEx(h, dist, nouv, methode);
    if (g_traceIO && EstArchiveSuivie(h) && r && nouv)
        printf("[IO] seek -> offset 0x%llX\n", (unsigned long long)nouv->QuadPart);
    return r;
}



static HANDLE WINAPI hkCreateFileW(LPCWSTR nom, DWORD acces, DWORD partage,
    LPSECURITY_ATTRIBUTES sa, DWORD creation,
    DWORD flags, HANDLE modele)
{
    HANDLE h = oCreateFileW(nom, acces, partage, sa, creation, flags, modele);
    if (g_traceIO && nom)
        wprintf(L"[IO] open %s -> %p\n", nom, h);
    if (h != INVALID_HANDLE_VALUE && nom && g_nArchives < 8) {
        bool dat = false;
        for (int i = 0; nom[i] && nom[i + 1] && nom[i + 2] && nom[i + 3]; ++i)
            if ((nom[i] == L'.') &&
                (nom[i + 1] == L'd' || nom[i + 1] == L'D') &&
                (nom[i + 2] == L'a' || nom[i + 2] == L'A') &&
                (nom[i + 3] == L't' || nom[i + 3] == L'T')) {
                dat = true; break;
            }
        if (dat) {
            g_hArchives[g_nArchives++] = h;
            wprintf(L"[IO] archive suivie : %s (handle %p)\n", nom, h);
        }
    }
    return h;
}

typedef HANDLE(WINAPI* CreateFileMappingW_t)(HANDLE, LPSECURITY_ATTRIBUTES, DWORD,
    DWORD, DWORD, LPCWSTR);
typedef LPVOID(WINAPI* MapViewOfFile_t)(HANDLE, DWORD, DWORD, DWORD, SIZE_T);

static CreateFileMappingW_t oCreateFileMappingW = nullptr;
static MapViewOfFile_t      oMapViewOfFile = nullptr;



static void NomDuHandle(HANDLE h, wchar_t* out, int maxlen)
{
    out[0] = 0;
    if (!h || h == INVALID_HANDLE_VALUE) return;
    DWORD r = GetFinalPathNameByHandleW(h, out, maxlen - 1, FILE_NAME_NORMALIZED);
    if (r == 0 || r >= (DWORD)maxlen) out[0] = 0;
}


static HANDLE  g_hCache[64];
static wchar_t g_nCache[64][260];
static int     g_nCacheCount = 0;





static bool   g_ioInstalles = false;
static thread_local bool t_dansHook = false;

// Nom d'un handle, avec cache. Robuste : on ecarte les handles invalides
// et tout ce qui n'est pas un fichier disque (pipes, consoles, sockets).
static const wchar_t* NomCache(HANDLE h)
{
    static HANDLE  hs[128];
    static wchar_t noms[128][128];
    static int     n = 0;

    if (!h || h == INVALID_HANDLE_VALUE) return L"(invalide)";

    for (int i = 0; i < n; ++i)
        if (hs[i] == h) return noms[i];
    if (n >= 128) return L"(cache plein)";

    int idx = n++;
    hs[idx] = h;
    noms[idx][0] = 0;

    if (GetFileType(h) != FILE_TYPE_DISK) { wcscpy_s(noms[idx], 128, L"(non-disque)"); return noms[idx]; }

    wchar_t buf[512] = {};
    DWORD r = GetFinalPathNameByHandleW(h, buf, 511, FILE_NAME_NORMALIZED);
    if (r == 0 || r >= 512) { wcscpy_s(noms[idx], 128, L"(inconnu)"); return noms[idx]; }

    const wchar_t* p = buf;
    for (const wchar_t* q = buf; *q; ++q) if (*q == L'\\') p = q + 1;
    wcsncpy_s(noms[idx], 128, p, 127);
    return noms[idx];
}

static BOOL WINAPI hkReadFile(HANDLE h, LPVOID buf, DWORD taille,
    LPDWORD lus, LPOVERLAPPED ov)
{
    if (t_dansHook || !g_traceIO) return oReadFile(h, buf, taille, lus, ov);

    t_dansHook = true;
    unsigned long long offset = 0;
    if (ov) offset = ((unsigned long long)ov->OffsetHigh << 32) | ov->Offset;
    else {
        LARGE_INTEGER z; z.QuadPart = 0;
        LARGE_INTEGER c; c.QuadPart = 0;
        if (oSetFilePointerEx(h, z, &c, FILE_CURRENT)) offset = c.QuadPart;
    }
    const wchar_t* nom = NomCache(h);
    t_dansHook = false;

    BOOL r = oReadFile(h, buf, taille, lus, ov);

    t_dansHook = true;
    static int total = 0;
    total++;
    // Sans filtre, mais borne : on veut juste savoir si le hook vit.
    if (total <= 40)
        wprintf(L"[IO] #%d read %-24s offset=0x%010llX taille=%u\n",
            total, nom, offset, taille);
    else if (total % 500 == 0)
        printf("[IO] %d lectures au total\n", total);
    t_dansHook = false;

    return r;
}

struct MapInfo { HANDLE hMap; wchar_t nom[128]; unsigned long long taille; };
static MapInfo g_maps[64];
static volatile long g_nMaps = 0;

struct VueInfo { HANDLE hMap; void* base; unsigned long long offset; SIZE_T taille; };
static VueInfo g_vues[256];
static volatile long g_nVues = 0;


static HANDLE WINAPI hkCreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES sa,
    DWORD prot, DWORD maxHigh, DWORD maxLow,
    LPCWSTR nom)
{
    HANDLE r = oCreateFileMappingW(hFile, sa, prot, maxHigh, maxLow, nom);

    if (r && !t_dansHook) {
        t_dansHook = true;
        LARGE_INTEGER t; t.QuadPart = 0;
        GetFileSizeEx(hFile, &t);                 // sans E/S, sur le handle

        // On ne retient que les grosses archives : le .dat fait 6,7 Go.
        if (t.QuadPart > 100000000LL) {
            long i = InterlockedIncrement(&g_nMaps) - 1;
            if (i < 64) {
                g_maps[i].hMap = r;
                g_maps[i].taille = (unsigned long long)t.QuadPart;
                g_maps[i].nom[0] = 0;
                GetFinalPathNameByHandleW(hFile, g_maps[i].nom, 127, FILE_NAME_NORMALIZED);
            }
        }
        t_dansHook = false;
    }
    return r;
}

static LPVOID WINAPI hkMapViewOfFile(HANDLE hMap, DWORD acces,
    DWORD offHigh, DWORD offLow, SIZE_T taille)
{
    LPVOID p = oMapViewOfFile(hMap, acces, offHigh, offLow, taille);

    if (p) {
        bool suivi = false;
        long n = g_nMaps; if (n > 64) n = 64;
        for (long i = 0; i < n; ++i) if (g_maps[i].hMap == hMap) { suivi = true; break; }
        if (suivi) {
            long i = InterlockedIncrement(&g_nVues) - 1;
            if (i < 256) {
                g_vues[i].hMap = hMap;
                g_vues[i].base = p;
                g_vues[i].offset = ((unsigned long long)offHigh << 32) | offLow;
                g_vues[i].taille = taille;
            }
        }
    }
    return p;
}




static void TracerLecture(HANDLE h, unsigned long long pos, ULONG len,
    PVOID buf, LONG st)
{
    static HANDLE             dernierH = nullptr;
    static unsigned long long dernierFin = 0;
    static int                n = 0;

    bool nouveauBlob = (h != dernierH || pos != dernierFin);
    dernierH = h;
    dernierFin = pos + len;

    if (!nouveauBlob) return;

    unsigned char t[4] = {};
    if (st == 0 && buf) memcpy(t, buf, 4);
    bool gzip = (t[0] == 0x1F && t[1] == 0x8B);

    printf("[BLOB] #%d offset=0x%010llX  %02X %02X %02X %02X%s\n",
        ++n, pos, t[0], t[1], t[2], t[3], gzip ? "  <-- GZIP" : "");
}



typedef struct _IO_STATUS_BLOCK_FP {
    union { LONG Status; PVOID Pointer; };
    ULONG_PTR Information;
} IO_STATUS_BLOCK_FP;

typedef LONG(NTAPI* NtReadFile_t)(HANDLE, HANDLE, void*, void*,
    IO_STATUS_BLOCK_FP*, PVOID, ULONG,
    PLARGE_INTEGER, PULONG);
static NtReadFile_t oNtReadFile = nullptr;


struct BlobLu { HANDLE h; unsigned long long off; };
static BlobLu g_blobs[8192];
static volatile long g_nBlobs = 0;


static bool g_ntInstalle = false;





void ResetBlobLog()
{
    g_nBlobs = 0;
    printf("[BLOB] journal vide\n");
}

static volatile LONG g_vus = 0, g_retenus = 0, g_sansPos = 0;

static void EnregistrerLecture(HANDLE h, unsigned long long pos, ULONG len)
{
    InterlockedIncrement(&g_vus);
    if (!pos) { InterlockedIncrement(&g_sansPos); return; }

    // Position attendue si la lecture prolongeait la precedente sur ce handle
    static HANDLE            handles[64] = {};
    static unsigned long long suivantes[64] = {};
    int slot = -1;
    for (int i = 0; i < 64; ++i) {
        if (handles[i] == h) { slot = i; break; }
        if (handles[i] == nullptr && slot < 0) slot = i;
    }
    if (slot < 0) return;

    bool nouveau = (handles[slot] != h) || (pos != suivantes[slot]);
    handles[slot] = h;
    suivantes[slot] = pos + len;

    if (!nouveau) return;

    if (g_nBlobs < 8192) {
        g_blobs[g_nBlobs].h = h;
        g_blobs[g_nBlobs].off = pos;
        g_nBlobs++;
        InterlockedIncrement(&g_retenus);
    }
}

void DumpBlobLog()
{
    printf("\n[BLOB] %ld lectures vues, %ld retenues, %ld sans position\n",
        g_vus, g_retenus, g_sansPos);
    printf("[BLOB] %d debut(s) enregistre(s)\n", g_nBlobs);
    for (int i = 0; i < g_nBlobs; ++i)
        printf("   textures-s3.dat 0x%010llX\n", (unsigned long long)g_blobs[i].off);
}
void DumpMappedRegions(SIZE_T tailleMin)
{
    printf("\n[MAP] regions MEM_MAPPED >= %llu octets\n",
        (unsigned long long)tailleMin);

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    int n = 0;

    while (VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_MAPPED &&
            mbi.RegionSize >= tailleMin &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {

            unsigned char t[16] = {};
            SafeRead(mbi.BaseAddress, t, 16);

            printf("   base=%p  taille=%llu  prot=0x%X\n     ",
                mbi.BaseAddress, (unsigned long long)mbi.RegionSize, mbi.Protect);
            for (int i = 0; i < 16; ++i) printf("%02X ", t[i]);
            printf("  ");
            for (int i = 0; i < 16; ++i)
                printf("%c", (t[i] >= 32 && t[i] < 127) ? t[i] : '.');
            printf("\n");
            n++;
        }
        addr = next;
    }
    printf("[MAP] %d region(s)\n", n);
}

void DumpResourceEntries()
{
    if (!EnsureResSelf()) { printf("[RES] self introuvable\n"); return; }

    uintptr_t cont = 0, base = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000) {
        printf("[RES] conteneur illisible\n"); return;
    }
    base = cont;                      // adapte si ton code deréférence une fois de plus

    printf("\n[RES] conteneur 0x%llX\n", (unsigned long long)base);

    for (int i = 0; i < 12; ++i) {
        uintptr_t e = base + (uintptr_t)i * 0x70;
        int32_t val = 0;
        if (!SafeRead((void*)(e + 0x08), &val, 4)) break;

        printf("\n--- entree %d @0x%llX  valeur=%d ---\n",
            i, (unsigned long long)e, val);

        for (int off = 0; off < 0x70; off += 8) {
            uintptr_t v = 0;
            if (!SafeRead((void*)(e + off), &v, 8)) break;
            if (!v) continue;

            char info[192] = { 0 };
            if (v > 0x10000) {
                // Une definition a son nom en +0x00 (pointeur vers ASCII).
                uintptr_t np = 0;
                if (SafeRead((void*)v, &np, 8) && np > 0x10000) {
                    char s[64] = { 0 };
                    if (SafeRead((void*)np, s, sizeof(s) - 1) &&
                        s[0] >= 0x20 && s[0] < 0x7F && s[1])
                        _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> [def] \"%s\"", s);
                }
                if (!info[0]) {
                    char s[64] = { 0 };
                    if (SafeRead((void*)v, s, sizeof(s) - 1) &&
                        s[0] >= 0x20 && s[0] < 0x7F && s[1])
                        _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> \"%s\"", s);
                }
            }
            printf("   +0x%02X = %016llX  (i32=%d)%s\n",
                off, (unsigned long long)v, (int)(uint32_t)v, info);
        }
    }
}

void DumpResourceIcons()
{
    if (!EnsureResSelf()) { printf("[RES] self introuvable\n"); return; }

    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000) {
        printf("[RES] conteneur illisible\n"); return;
    }

    printf("\n[ICON] Icon = def+0x098, 0x18 octets\n");

    for (int i = 0; i < 12; ++i) {
        uintptr_t e = cont + (uintptr_t)i * 0x70;

        uintptr_t def = 0;
        if (!SafeRead((void*)e, &def, 8) || def <= 0x10000) continue;

        uintptr_t np = 0; char nom[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000)
            SafeRead((void*)np, nom, sizeof(nom) - 1);
        if (nom[0] < 0x20 || nom[0] > 0x7E) continue;

        unsigned char ic[0x18] = {};
        if (!SafeRead((void*)(def + 0x98), ic, 0x18)) continue;

        printf("   %-20s def=0x%llX\n      ", nom, (unsigned long long)def);
        for (int k = 0; k < 0x18; ++k) printf("%02X ", ic[k]);
        printf("\n");
    }
}

void SwapIconTest()
{
    if (!EnsureResSelf()) return;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8)) return;

    uintptr_t defCoal = 0, defWood = 0;
    for (int i = 0; i < 12; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;
        uintptr_t np = 0; char nom[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000) SafeRead((void*)np, nom, 63);
        if (!strcmp(nom, "Wood")) defCoal = def;
        if (!strcmp(nom, "Augmentation parts")) defWood = def;
    }
    if (!defCoal || !defWood) { printf("[ICON] definitions introuvables\n"); return; }

    int32_t cw[2] = {};                       // colonne + ligne de Wood
    SafeRead((void*)(defWood + 0x98 + 8), cw, 8);
    SafeWrite((void*)(defCoal + 0x98 + 8), cw, 8);
    printf("[ICON] Coal pointe desormais sur la case (%d,%d)\n", cw[0], cw[1]);
}

void DumpAtlasTexture()
{
    if (!EnsureResSelf()) return;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8)) return;

    uintptr_t def = 0;
    if (!SafeRead((void*)cont, &def, 8) || def <= 0x10000) return;

    uintptr_t tex = 0;
    if (!SafeRead((void*)(def + 0x98), &tex, 8) || tex <= 0x10000) {
        printf("[ATLAS] pointeur illisible\n"); return;
    }

    char cls[128] = { 0 };
    GetClassNameFP(tex, cls, sizeof(cls));
    printf("\n[ATLAS] 0x%llX  classe=%s\n",
        (unsigned long long)tex, cls[0] ? cls : "(sans RTTI)");
    DumpObject(tex, 0x80);
    ScanForStrings("atlas", tex, 0x80);
}

// Enregistrement silencieux des debuts de blob : aucun printf, aucun appel
// Windows couteux -> installable des le chargement de la DLL.

// L'archive de textures est-elle celle-ci ? On compare le nom mis en cache,
// pour ne jamais substituer sur un autre fichier.
static bool EstArchiveTextures(HANDLE h)
{
    const wchar_t* nom = NomCache(h);
    if (!nom) return false;

    const wchar_t* ref = L"textures-s3.dat";
    for (int i = 0; ; ++i) {
        wchar_t a = nom[i];
        if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');   // insensible a la casse
        if (a != ref[i]) return false;
        if (ref[i] == 0) return true;
    }
}

// Fait afficher a une ressource l'icone d'une case donnee de l'atlas.
void SetIconCell(const char* ressource, int col, int row)
{
    if (!EnsureResSelf()) { printf("[ICON] self introuvable\n"); return; }

    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000) return;

    for (int i = 0; i < 12; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;

        uintptr_t np = 0; char nom[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000) SafeRead((void*)np, nom, sizeof(nom) - 1);
        if (strcmp(nom, ressource) != 0) continue;

        int32_t cr[2] = { col, row };                 // colonne puis ligne
        if (SafeWrite((void*)(def + 0x98 + 8), cr, 8))
            printf("[ICON] %s -> case (%d,%d)\n", nom, col, row);
        else
            printf("[ICON] ecriture refusee\n");
        return;
    }
    printf("[ICON] ressource \"%s\" introuvable\n", ressource);
}

// --- Substitution de blob a la lecture ---------------------------------
struct Substitution {
    unsigned long long off;      // offset du blob dans textures-s3.dat
    unsigned char* data;     // notre flux gzip complet
    size_t             len;
};
static Substitution g_subs[8];
static int          g_nSubs = 0;

// Charge un flux gzip prepare hors ligne et l'associe a un offset.
bool AjouterSubstitution(unsigned long long off, const wchar_t* fichier)
{
    if (g_nSubs >= 8) return false;
    HANDLE h = CreateFileW(fichier, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        wprintf(L"[SUB] ECHEC : %s introuvable (erreur %lu)\n", fichier, GetLastError());
        return false;
    }

    LARGE_INTEGER t; GetFileSizeEx(h, &t);
    unsigned char* buf = (unsigned char*)malloc((size_t)t.QuadPart);
    DWORD lus = 0;
    ReadFile(h, buf, (DWORD)t.QuadPart, &lus, nullptr);
    CloseHandle(h);

    g_subs[g_nSubs].off = off;
    g_subs[g_nSubs].data = buf;
    g_subs[g_nSubs].len = lus;
    g_nSubs++;
    printf("[SUB] blob 0x%llX -> %u octets\n", off, lus);
    return true;
}

static Substitution* SubPour(unsigned long long pos, size_t* dedans)
{
    for (int i = 0; i < g_nSubs; ++i) {
        if (pos >= g_subs[i].off && pos < g_subs[i].off + g_subs[i].len) {
            *dedans = (size_t)(pos - g_subs[i].off);
            return &g_subs[i];
        }
    }
    return nullptr;
}


static unsigned long long OffsetDuBlob(int index)
{
    wchar_t chemin[MAX_PATH];
    GetModuleFileNameW(NULL, chemin, MAX_PATH);
    for (int i = (int)wcslen(chemin) - 1; i >= 0; --i)
        if (chemin[i] == L'\\') { chemin[i + 1] = 0; break; }
    wcscat_s(chemin, MAX_PATH, L"textures-s3.idx");

    HANDLE h = CreateFileW(chemin, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER pos; pos.QuadPart = 11 + (long long)index * 29 + 20;
    SetFilePointerEx(h, pos, nullptr, FILE_BEGIN);
    unsigned long long off = 0; DWORD lus = 0;
    ReadFile(h, &off, 8, &lus, nullptr);
    CloseHandle(h);
    return (lus == 8) ? off : 0;
}

void ChargerSubstitutions()
{
    // Chemin du dossier de l'executable, pour ne pas dependre du repertoire courant.
    wchar_t base[MAX_PATH] = {};
    GetModuleFileNameW(NULL, base, MAX_PATH);
    for (int i = (int)wcslen(base) - 1; i >= 0; --i) {
        if (base[i] == L'\\') { base[i + 1] = 0; break; }
    }

    int ok = 0;
    for (const auto& s : SUBSTITUTIONS) {
        wchar_t chemin[MAX_PATH];
        swprintf_s(chemin, MAX_PATH, L"%smods\\%s", base, s.fichier);
        if (AjouterSubstitution(OffsetDuBlob(s.off), chemin)) ok++;
        else printf("[SUB] %s : non charge\n", s.libelle);
    }
    printf("[SUB] %d/%d substitution(s) active(s)\n", ok, (int)(sizeof(SUBSTITUTIONS) / sizeof(SUBSTITUTIONS[0])));
}



static LONG NTAPI hkNtReadFile(HANDLE h, HANDLE ev, void* apc, void* apcCtx,
    IO_STATUS_BLOCK_FP* iosb, PVOID buf, ULONG len,
    PLARGE_INTEGER off, PULONG key)
{
    // Reentrance : NomCache et SetFilePointerEx font des E/S.
    if (t_dansHook)
        return oNtReadFile(h, ev, apc, apcCtx, iosb, buf, len, off, key);

    t_dansHook = true;
    bool tex = EstArchiveTextures(h);

    // Position : ByteOffset est nul ou vaut la sentinelle -2 pour ces lectures,
    // la position reelle vient du pointeur de fichier interne.
    unsigned long long pos = 0;
    bool posOk = false;
    if (tex) {
        if (off) {
            long long v = off->QuadPart;
            if (v >= 0) { pos = (unsigned long long)v; posOk = true; }
        }
        if (!posOk) {
            LARGE_INTEGER z; z.QuadPart = 0;
            LARGE_INTEGER cur; cur.QuadPart = 0;
            if (SetFilePointerEx(h, z, &cur, FILE_CURRENT)) {
                pos = (unsigned long long)cur.QuadPart;
                posOk = true;
            }
        }
    }
    t_dansHook = false;

    // --- Substitution : on sert nos octets sans appeler l'original ---
    if (tex && posOk && buf) {
        size_t dedans = 0;
        Substitution* s = SubPour(pos, &dedans);
        if (s) {
            size_t dispo = s->len - dedans;
            size_t n = (len < dispo) ? len : dispo;
            memcpy(buf, s->data + dedans, n);
            if (iosb) { iosb->Status = 0; iosb->Information = n; }
            LARGE_INTEGER d; d.QuadPart = (long long)n;
            SetFilePointerEx(h, d, nullptr, FILE_CURRENT);
            return 0;
        }
    }

    // --- Enregistrement des debuts de blob ---
    if (tex && posOk) EnregistrerLecture(h, pos, len);

    return oNtReadFile(h, ev, apc, apcCtx, iosb, buf, len, off, key);
}

typedef LONG(NTAPI* NtWriteFile_t)(HANDLE, HANDLE, void*, void*,
    IO_STATUS_BLOCK_FP*, PVOID, ULONG,
    PLARGE_INTEGER, PULONG);
static NtWriteFile_t oNtWriteFile = nullptr;

volatile bool g_traceEcriture = true;

static LONG NTAPI hkNtWriteFile(HANDLE h, HANDLE ev, void* apc, void* apcCtx,
    IO_STATUS_BLOCK_FP* iosb, PVOID buf, ULONG len,
    PLARGE_INTEGER off, PULONG key)
{
    if (g_traceEcriture && !t_dansHook) {
        t_dansHook = true;
        const wchar_t* nom = NomCache(h);
        // On ignore la console et les journaux du jeu.
        bool interessant = true;
        if (nom) {
            for (int i = 0; nom[i] && nom[i + 3]; ++i)
                if (nom[i] == L'.' && nom[i + 1] == L'l' && nom[i + 2] == L'o' && nom[i + 3] == L'g')
                {
                    interessant = false; break;
                }
        }
        if (interessant) {
            static int n = 0;
            if (++n <= 40)
                wprintf(L"[WRITE] #%d %-28s len=%u\n", n, nom ? nom : L"?", len);
        }
        t_dansHook = false;
    }
    return oNtWriteFile(h, ev, apc, apcCtx, iosb, buf, len, off, key);
}

// A appeler depuis une touche, jamais au demarrage : hooker les E/S pendant
// l'initialisation du processus fait crasher le jeu.
void InstallNtHooks()
{
    if (g_ntInstalle) return;

    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) { printf("[NT] ntdll introuvable\n"); return; }

    // --- NtReadFile : substitution de textures -----------------------------
    void* c = (void*)GetProcAddress(nt, "NtReadFile");
    if (!c) {
        printf("[NT] NtReadFile introuvable\n");
    }
    else if (MH_CreateHook(c, (void*)&hkNtReadFile, (void**)&oNtReadFile) == MH_OK &&
        MH_EnableHook(c) == MH_OK) {
        printf("[NT] hook NtReadFile OK\n");
    }
    else {
        printf("[NT] hook NtReadFile KO\n");
    }

    g_ntInstalle = true;
}

bool PartieRechargee()
{
    static uintptr_t derniere = 0;
    uintptr_t modBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));
    uintptr_t eco = 0;
    if (!SafeRead((void*)(modBase + 0x40916D0), &eco, 8)) return false;
    if (eco <= 0x10000) return false;                 // pas encore en partie

    if (eco == derniere) return false;                // rien de neuf
    bool premiere = (derniere == 0);
    derniere = eco;

    printf("[LOADING] %s : economie a 0x%llX\n",
        premiere ? "partie chargee" : "PARTIE RECHARGEE",
        (unsigned long long)eco);
    return true;
}

static uintptr_t TailleModule(uintptr_t base)
{
    IMAGE_DOS_HEADER dos = {};
    if (!SafeRead((void*)base, &dos, sizeof(dos))) return 0;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 nt = {};
    if (!SafeRead((void*)(base + dos.e_lfanew), &nt, sizeof(nt))) return 0;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    return nt.OptionalHeader.SizeOfImage;
}

void CollectObjectsByClasses(const char** noms, int nNoms,
    uintptr_t** sorties, int* compteurs, int maxParClasse)
{
    for (int i = 0; i < nNoms; ++i) compteurs[i] = 0;

    uintptr_t modBase = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t modEnd = modBase + TailleModule(modBase);
    if (modEnd == modBase) return;

    static const int TAILLE = 32768;
    static const int SONDAGE = 16;
    static uintptr_t cleVptr[TAILLE] = {};
    static int       verdict[TAILLE] = {};
    static int       occupe = 0;

    // >>> LE BLOC VA ICI <
    // La table memorise un INDICE dans `noms` : si la liste change entre deux
    // appels, les anciens verdicts designent les mauvaises classes.
    static const char* dernieresNoms[16] = {};
    static int dernierNb = 0;
    bool memeListe = (dernierNb == nNoms);
    for (int i = 0; memeListe && i < nNoms; ++i)
        if (dernieresNoms[i] != noms[i]) memeListe = false;

    if (!memeListe) {
        for (int i = 0; i < TAILLE; ++i) { cleVptr[i] = 0; verdict[i] = 0; }
        occupe = 0;
        dernierNb = (nNoms < 16) ? nNoms : 16;
        for (int i = 0; i < dernierNb; ++i) dernieresNoms[i] = noms[i];
        printf("[SCAN] liste de classes changee -- table videe\n");
    }

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));

        if (lisible && mbi.RegionSize <= 0x4000000) {
            __try {
                uintptr_t* d = (uintptr_t*)mbi.BaseAddress;
                uintptr_t* fin = (uintptr_t*)next;
                for (; d < fin; ++d) {
                    uintptr_t vptr = *d;
                    if (vptr < modBase || vptr >= modEnd) continue;

                    // Sondage borne : on cherche la cle ou un emplacement libre.
                    int h = (int)((vptr >> 4) & (TAILLE - 1));
                    int trouve = -1, libre = -1;
                    for (int s = 0; s < SONDAGE; ++s) {
                        int k = (h + s) & (TAILLE - 1);
                        if (cleVptr[k] == vptr) { trouve = k; break; }
                        if (cleVptr[k] == 0) { libre = k; break; }
                    }

                    int c;
                    if (trouve >= 0) {
                        c = verdict[trouve];
                    }
                    else {
                        // Verdict a calculer. On ne memorise que s'il reste
                        // de la place, sinon on recalcule (rare).
                        c = -1;
                        char cls[128] = { 0 };
                        if (GetClassNameFP((uintptr_t)d, cls, sizeof(cls))) {
                            for (int i = 0; i < nNoms; ++i) {
                                bool eq = true;
                                for (int j = 0; ; ++j) {
                                    if (cls[j] != noms[i][j]) { eq = false; break; }
                                    if (noms[i][j] == 0) break;
                                }
                                if (eq) { c = i; break; }
                            }
                        }
                        if (libre >= 0 && occupe < (TAILLE * 3) / 4) {
                            cleVptr[libre] = vptr;
                            verdict[libre] = c;
                            occupe++;
                        }
                    }

                    if (c >= 0 && compteurs[c] < maxParClasse)
                        sorties[c][compteurs[c]++] = (uintptr_t)d;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        addr = next;
    }
}

bool ClasseExacte(uintptr_t obj, const char* ref)
{
    char cls[128] = { 0 };
    if (!GetClassNameFP(obj, cls, sizeof(cls))) return false;
    for (int j = 0; ; ++j) {
        if (cls[j] != ref[j]) return false;
        if (ref[j] == 0) return true;
    }
}

bool StrEq(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}


void SurveillerArbreTechno()
{
    static bool  ouvertAvant = false;
    static DWORD debutSignal = 0;

    bool signal = (g_instantArbre != 0) &&
        (GetTickCount() - g_instantArbre < 2000);

    if (signal && debutSignal == 0) debutSignal = GetTickCount();
    if (!signal) debutSignal = 0;

    // Ouvert = signal present depuis plus d'une seconde
    bool ouvert = signal && (GetTickCount() - debutSignal > 1000);

    if (!g_pret) { ouvertAvant = false; return; }   // pas encore en partie

    if (ouvert && !ouvertAvant) {
        for (int i = 0; i < g_nTechnosAP; ++i) {
            TechnoAP& t = g_technosAP[i];
            if (!t.entree) continue;
            if (t.miniature) {
                if (t.batiment[0])
                    AppliquerIconeBatiment(t.batiment, t.icone->batCol, t.icone->batRow);
            }
            else {
                int32_t cr[2] = { t.icone->techCol, t.icone->techRow };
                SafeWrite((void*)(t.entree + 0x80), cr, 8);
            }
        }
    }
    else if (!ouvert && ouvertAvant) {
        printf("[ARBRE] ferme\n");
        RestaurerIconesBatiments();
    }
    ouvertAvant = ouvert;
}

