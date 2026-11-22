// mod_common.cpp -- definitions du socle partage
#include "mod_common.h"
#include "scenario.h"
#include "MessageManager.h"
#include "resources.h"
#include "groupes.h"   // GroupesApresChargement (sortie de LoadGame)
#include "evenements.h" // DilemmesAPChargement
#include "scooting.h"   // ScootingApresChargement
#include "lois_ap.h"    // LoisAPApresChargement
#include "archipelago.h" // ApItemsApresChargement (livraisons de la sauvegarde)
#include "MinHook.h"
#include <psapi.h>

typedef uintptr_t(*Fabrique_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static Fabrique_t oFabrique = nullptr;

Alloc_t MoteurAlloc = nullptr;

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

static const uintptr_t TECH_SELF_GLOBALS[] = { Adr(0x2C30A20), Adr(0x4062BA8) };
static const uintptr_t RES_SELF_GLOBALS[] = { Adr(0x40916D0), Adr(0x2C309E0) };



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
    if (!SafeRead((void*)(base + Adr(0x408B740)), &pm, 8) || pm <= 0x10000) {
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
    if (!SafeRead((void*)(base + Adr(0x408B740)), &pm, 8) || pm <= 0x10000) {
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
    base = cont;                      // adapte si ton code der�f�rence une fois de plus

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
    const char*        libelle;
    // Diagnostic (2026-11-03, icones des lois) : lectures servies depuis le
    // debut du blob, et nombre deja journalise par RapportSubstitutions.
    volatile long      lectures;
    long               rapportees;
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


// Decalage dans textures-s3.dat du blob dont le hash de nom (u32 en tete
// d'entree de l'index : en-tete 11 octets, entrees de 29 octets triees par
// hash, decalage en +20) vaut 'hashNom'. Independant de la version du jeu
// (2026-11-07 : les numeros d'entree different entre Epic et Steam).
// 'entree' recoit le numero d'entree trouve (journal). 0 si absent.
static unsigned long long OffsetDuBlob(uint32_t hashNom, int* entree)
{
    *entree = -1;
    wchar_t chemin[MAX_PATH];
    GetModuleFileNameW(NULL, chemin, MAX_PATH);
    for (int i = (int)wcslen(chemin) - 1; i >= 0; --i)
        if (chemin[i] == L'\\') { chemin[i + 1] = 0; break; }
    wcscat_s(chemin, MAX_PATH, L"textures-s3.idx");

    HANDLE h = CreateFileW(chemin, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER taille = {};
    GetFileSizeEx(h, &taille);
    if (taille.QuadPart < 11 || taille.QuadPart > (64 << 20)) { CloseHandle(h); return 0; }
    unsigned char* idx = (unsigned char*)malloc((size_t)taille.QuadPart);
    DWORD lus = 0;
    BOOL lu = idx && ReadFile(h, idx, (DWORD)taille.QuadPart, &lus, nullptr);
    CloseHandle(h);
    if (!lu || lus != (DWORD)taille.QuadPart) { free(idx); return 0; }

    unsigned long long off = 0;
    int n = (int)((lus - 11) / 29);
    for (int i = 0; i < n; ++i) {
        const unsigned char* e = idx + 11 + (size_t)i * 29;
        uint32_t hs; memcpy(&hs, e, 4);
        if (hs == hashNom) { memcpy(&off, e + 20, 8); *entree = i; break; }
    }
    free(idx);
    return off;
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
        int entree = -1;
        unsigned long long off = OffsetDuBlob(s.hashNom, &entree);
        if (!off) {
            printf("[SUB] %s : hash 0x%08X absent de textures-s3.idx (Epic : entree %lu) -- non charge\n",
                s.libelle, s.hashNom, s.entreeEpic);
            continue;
        }
        const wchar_t* fichier = FichierAtlasVersion(s.fichier);
        wchar_t chemin[MAX_PATH];
        swprintf_s(chemin, MAX_PATH, L"%smods\\%s", base, fichier);
        if (AjouterSubstitution(off, chemin)) {
            g_subs[g_nSubs - 1].libelle = s.libelle; ok++;
            wprintf(L"[SUB]   entree %d (hash 0x%08X) <- %s\n", entree, s.hashNom, fichier);
        }
        else printf("[SUB] %s : non charge\n", s.libelle);
    }
    printf("[SUB] %d/%d substitution(s) active(s)\n", ok, (int)(sizeof(SUBSTITUTIONS) / sizeof(SUBSTITUTIONS[0])));
}

// Tick d'une seconde (version.cpp) : une ligne par blob substitue des qu'il a
// ete relu depuis le debut. Sert a savoir QUAND l'atlas des bulles / des lois
// (#2350) est charge (menu, partie) et s'il passe bien par notre substitution.
void RapportSubstitutions()
{
    for (int i = 0; i < g_nSubs; ++i) {
        long n = g_subs[i].lectures;
        if (n == g_subs[i].rapportees) continue;
        g_subs[i].rapportees = n;
        printf("[SUB] %s : servi depuis le debut %ld fois (partie prete : %s)\n",
            g_subs[i].libelle ? g_subs[i].libelle : "?", n, PartiePrete() ? "oui" : "non");
    }
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
            if (dedans == 0) InterlockedIncrement(&s->lectures);   // pas de printf ici
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

// --- Recherche d'un fichier dans une archive (.dat / .idx) -------------------
// FUN_140e51fa0(lecteur, sortie, nom) (containerfilereadercore, decompile du
// 2026-10-28) : hash MurmurHash2 FUN_140e53a40(nom, longueur, graine =
// lecteur[0xC], m = lecteur[0xE] = 0x5bd1e995). Sa trace [ARCH] (hook de
// journal seulement) a ete retiree pour la 1.0 : l'atlas cherche est trouve.

// --- Trace du chargement d'une sauvegarde (2026-10-29) -----------------------
// But : trouver un point, sur le thread du jeu, ou la sauvegarde est
// entierement chargee (groupes et mouvements recrees) mais ou la partie n'a
// pas encore tourne, pour y recoller nos donnees AP. Decompiles :
//  - FUN_141265df0(jeu, nom) = FrostpunkGame::LoadGame (frostpunkgame.cpp,
//    drapeau "LoadingGame" jeu +0x489) : FUN_141286840 (lecture), puis si
//    succes FUN_141264b10(jeu, 1, ...) (etat "InFrostpunkGame") et
//    FUN_141266390(jeu) ;
//  - FUN_141286840(?, nom, p3) : "Loading game save..." / "...successfully
//    loaded", lit le .save (FUN_141286580) et le deserialise
//    (FUN_14101f110(gestionnaire d'entites, donnees, ...)) ;
//  - FUN_14126c9f0(systeme de temps) : verification "The game should not be
//    paused after load" (ecouteur d'un evenement de fin de chargement ?).
// A chaque point : thread, nombre de groupes de survivants deja la, temps.
typedef uintptr_t(*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static Fonction4_t oLoadGame = nullptr, oLectureSauvegarde = nullptr, oApresChargementTemps = nullptr;
static bool g_lectureReussie = false;   // sauvegarde lue pendant ce LoadGame (thread du jeu)

static void NoterPointChargement(const char* point)
{
    static ULONGLONG debut = 0;
    ULONGLONG t = GetTickCount64();
    if (!strcmp(point, "LoadGame : entree")) debut = t;
    uintptr_t comps[32];
    int n = ComposantsDuSysteme(0x6D, "FrostpunkSurvivorGroupComponent", comps, 32);
    JournalImmediat("[CHARG] %-36s thread %lu, %d groupe(s) de survivants, t+%llu ms\n", point,
        GetCurrentThreadId(), n, (unsigned long long)(debut ? t - debut : 0));
}

static uintptr_t hkLoadGame(uintptr_t jeu, uintptr_t nom, uintptr_t p3, uintptr_t p4)
{
    NoterPointChargement("LoadGame : entree");
    GroupesAvantChargement();
    // Reperes immediats (crash c0000374 du 2026-11-10 pendant un chargement :
    // le journal s'arretait a l'entree).
    JournalImmediat("[CHARG] groupes : avant chargement fait\n");
    DilemmesAPChargement(0);
    MessagesAvantChargement();      // controles des messages AP detruits par le chargement
    JournalImmediat("[CHARG] dilemmes AP : avant chargement fait, appel du LoadGame du jeu\n");
    uintptr_t r = oLoadGame(jeu, nom, p3, p4);
    NoterPointChargement("LoadGame : sortie");
    // Test du 2026-10-29 : chargement synchrone sur le thread du jeu, groupes
    // deja inscrits dans leur systeme a ce point (4/4) : point de recollage.
    GroupesApresChargement();
    DilemmesAPChargement(2);
    MessagesApresChargement();
    if (g_lectureReussie) ScootingApresChargement();   // pas pour le 2e appel de LoadGame, a vide
    if (g_lectureReussie) LoisAPApresChargement();     // date de la sauvegarde : effets des lois deja presents ?
    if (g_lectureReussie) ApItemsApresChargement();    // livraisons d'items de cette sauvegarde (2026-11-10)
    g_lectureReussie = false;
    return r;
}

static uintptr_t hkLectureSauvegarde(uintptr_t p1, uintptr_t nom, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oLectureSauvegarde(p1, nom, p3, p4);
    NoterPointChargement((r & 0xFF) ? "lecture de la sauvegarde : succes" : "lecture de la sauvegarde : ECHEC");
    if (r & 0xFF) { g_lectureReussie = true; GroupesLectureFaite(); DilemmesAPChargement(1); }
    return r;
}

static uintptr_t hkApresChargementTemps(uintptr_t sys, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    NoterPointChargement("verification apres chargement");
    return oApresChargementTemps(sys, p2, p3, p4);
}

void InstallTraceChargement(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; Fonction4_t* orig; const char* nom; } h[] = {
        { Adr(0x1265DF0), (void*)&hkLoadGame,             &oLoadGame,             "LoadGame" },
        { Adr(0x1286840), (void*)&hkLectureSauvegarde,    &oLectureSauvegarde,    "lecture de la sauvegarde" },
        { Adr(0x126C9F0), (void*)&hkApresChargementTemps, &oApresChargementTemps, "verification apres chargement" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, (void**)x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[CHARG] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}

// Detecte un (re)chargement de partie. L'economie (+0x40916D0) seule ne
// suffit pas : en passant a un autre scenario, elle a ete reallouee a la meme
// adresse (2026-10-15). Le systeme de technologies (+0x2C30A20), lui, a
// change d'adresse a chaque chargement observe : on surveille les deux.
bool PartieRechargee()
{
    static uintptr_t derniereEco = 0, derniereTech = 0;
    uintptr_t modBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));
    uintptr_t eco = 0, tech = 0;
    if (!SafeRead((void*)(modBase + Adr(0x40916D0)), &eco, 8)) return false;
    if (eco <= 0x10000) return false;                 // pas encore en partie
    tech = SystemeParId(IdSysteme::Technology);
    if (!tech) return false;                          // chargement en cours

    if (eco == derniereEco && tech == derniereTech) return false;   // rien de neuf
    bool premiere = (derniereEco == 0);
    const char* cause = (eco != derniereEco && tech != derniereTech) ? "economie et technologies"
        : (eco != derniereEco) ? "economie" : "technologies seules";
    derniereEco = eco;
    derniereTech = tech;

    printf("[LOADING] %s : economie a 0x%llX, technologies a 0x%llX (%s)\n",
        premiere ? "partie chargee" : "PARTIE RECHARGEE",
        (unsigned long long)eco, (unsigned long long)tech, cause);
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

    static const int TAILLE = 131072;
    static const int SONDAGE = 32;
    static uintptr_t cleVptr[TAILLE] = {};
    static int       verdict[TAILLE] = {};
    static int       occupe = 0;

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
    }

    // Compteurs de diagnostic
    long nRegions = 0, nCandidats = 0, nRtti = 0, nOctets = 0;

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));

        // Les objets du moteur vivent dans des regions de taille modeste.
        // Au-dela, ce sont des tampons de donnees : les parcourir coute des
        // minutes pour rien. 8 Mo ne suffit pas (init incomplete) : 16 Mo.
        if (lisible && mbi.RegionSize > 0x1000000) lisible = false;

        if (lisible && mbi.RegionSize <= 0x4000000) {
            nRegions++;
            nOctets += (long)(mbi.RegionSize / 1024);
            __try {
                uintptr_t* d = (uintptr_t*)mbi.BaseAddress;
                uintptr_t* fin = (uintptr_t*)next;
                for (; d < fin; ++d) {
                    uintptr_t vptr = *d;
                    if (vptr < modBase || vptr >= modEnd) continue;
                    nCandidats++;

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
                        nRtti++;
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

    printf("[SCAN] %ld regions, %ld Ko, %ld candidats, %ld resolutions RTTI, table %d/%d\n",
        nRegions, nOctets, nCandidats, nRtti, occupe, TAILLE);
}

uintptr_t SystemeParId(int id)
{
    if (id < 0 || id > 0x200) return 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), sys = 0;
    if (!SafeRead((void*)(base + GameOffsets::SystemesGlobaux +
        (uintptr_t)id * GameOffsets::SystemesPas), &sys, 8)) return 0;
    return sys > 0x10000 ? sys : 0;
}

int ComposantsDuSysteme(int idSysteme, const char* classe, uintptr_t* out, int maxOut)
{
    uintptr_t sys = SystemeParId(idSysteme);
    if (!sys) return 0;
    uintptr_t arr = 0; uint32_t cnt = 0;
    if (!SafeRead((void*)(sys + 0x2078), &arr, 8)) return 0;
    if (!SafeRead((void*)(sys + 0x2080), &cnt, 4)) return 0;
    if (arr <= 0x10000 || cnt > 4096) return 0;

    int n = 0;
    for (uint32_t k = 0; k < cnt && n < maxOut; ++k) {
        uintptr_t comp = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &comp, 8)) continue;
        if (comp <= 0x10000 || !ClasseExacte(comp, classe)) continue;
        out[n++] = comp;
    }
    return n;
}

void CaptureAjouter(ListeCaptures& l, uintptr_t obj)
{
    AcquireSRWLockExclusive(&l.verrou);
    l.appels++;
    if (l.n >= l.capacite) {
        int garde = 0;
        for (int i = 0; i < l.n; ++i)
            if (ClasseExacte(l.objets[i], l.classe)) l.objets[garde++] = l.objets[i];
        l.n = garde;
    }
    if (l.n < l.capacite) l.objets[l.n++] = obj;
    ReleaseSRWLockExclusive(&l.verrou);
}

int CaptureLire(ListeCaptures& l, uintptr_t* out, int maxOut)
{
    AcquireSRWLockShared(&l.verrou);
    int n = 0;
    for (int i = 0; i < l.n && n < maxOut; ++i) {
        uintptr_t o = l.objets[i];
        if (!ClasseExacte(o, l.classe)) continue;
        bool deja = false;
        for (int j = 0; j < n; ++j) if (out[j] == o) { deja = true; break; }
        if (!deja) out[n++] = o;
    }
    ReleaseSRWLockShared(&l.verrou);
    return n;
}

// Cherche les emplacements du tas (memoire privee RW, regions <= 'borne'
// octets, 0 = sans borne) qui contiennent exactement 'valeur'. Renvoie le
// nombre trouve (borne a maxOut).
int ChercherValeurDansTas(uintptr_t valeur, uintptr_t* out, int maxOut, SIZE_T borne)
{
    int n = 0;
    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (n < maxOut && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) && (mbi.Type == MEM_PRIVATE) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) &&
            (borne == 0 || mbi.RegionSize <= borne);

        if (lisible) {
            __try {
                uintptr_t* d = (uintptr_t*)mbi.BaseAddress;
                uintptr_t* fin = (uintptr_t*)next;
                for (; d < fin && n < maxOut; ++d)
                    if (*d == valeur) out[n++] = (uintptr_t)d;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        addr = next;
    }
    return n;
}

// Remonte depuis 'adr' (pas de 8, au plus 'reculMax' octets) jusqu'au premier
// mot qui est un vptr resolu par le RTTI : donne l'objet qui contient 'adr'.
bool DebutObjet(uintptr_t adr, int reculMax, uintptr_t* debut, char* cls, int maxlen)
{
    for (int r = 0; r <= reculMax; r += 8) {
        uintptr_t cand = (adr & ~(uintptr_t)7) - (uintptr_t)r;
        if (GetClassNameFP(cand, cls, maxlen)) { *debut = cand; return true; }
    }
    return false;
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



// Ancienne pose des icones AP a l'ouverture de l'arbre : la detection
// (g_instantArbre, pose par TexteArbreTechno) ne s'est jamais declenchee,
// TexteArbreTechno n'etant appelee nulle part. Remplacee le 2026-11-01 par
// la pose pendant la construction de l'interface (InstallHooksIconesTechnos,
// IconSubManager), sans aucune ecriture permanente. Il ne reste qu'a rendre
// d'eventuelles icones de batiment encore sauvegardees.
void SurveillerArbreTechno()
{
    RestaurerIconesBatiments();
}

// Cherche dans le code de l'image les "lea reg, [rip+disp32]" qui chargent
// l'adresse 'vtable' : ce sont les constructeurs (et destructeurs) de la
// classe, qui y ecrivent le vptr. Le debut exact de chaque fonction vient de
// la table .pdata (RtlLookupFunctionEntry).
void ChercherReferencesVtable(uintptr_t vtable, const char* nom)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    printf("\n[VTREF] %s : vtable Frostpunk.exe+%llX\n", nom,
        (unsigned long long)(vtable - base));
    if (vtable <= base) { printf("   vtable invalide\n"); return; }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

    int n = 0;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const unsigned char* d = (const unsigned char*)(base + sec[s].VirtualAddress);
        size_t taille = sec[s].Misc.VirtualSize;
        __try {
            for (size_t i = 0; i + 7 <= taille && n < 32; ++i) {
                if ((d[i] != 0x48 && d[i] != 0x4C) || d[i + 1] != 0x8D) continue;
                if ((d[i + 2] & 0xC7) != 0x05) continue;          // mod 00, rm 101
                int32_t disp = *(const int32_t*)(d + i + 3);
                uintptr_t cible = (uintptr_t)(d + i + 7) + (intptr_t)disp;
                if (cible != vtable) continue;

                uintptr_t ici = (uintptr_t)(d + i);
                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(ici, &imageBase, nullptr);
                if (f)
                    printf("   lea a +%llX  dans la fonction +%X (taille 0x%X, lea a +0x%llX)\n",
                        (unsigned long long)(ici - base), f->BeginAddress,
                        f->EndAddress - f->BeginAddress,
                        (unsigned long long)(ici - base - f->BeginAddress));
                else
                    printf("   lea a +%llX  (fonction inconnue de .pdata)\n",
                        (unsigned long long)(ici - base));
                n++;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    printf("[VTREF] %d reference(s)\n", n);
}

// Meme recherche que ChercherReferencesVtable, sans log : renvoie le debut
// (adresse absolue) de chaque fonction distincte qui charge 'vtable'.
int FonctionsChargeantVtable(uintptr_t vtable, uintptr_t* debuts, int maxOut)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    if (vtable <= base) return 0;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);

    int n = 0;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const unsigned char* d = (const unsigned char*)(base + sec[s].VirtualAddress);
        size_t taille = sec[s].Misc.VirtualSize;
        __try {
            for (size_t i = 0; i + 7 <= taille && n < maxOut; ++i) {
                if ((d[i] != 0x48 && d[i] != 0x4C) || d[i + 1] != 0x8D) continue;
                if ((d[i + 2] & 0xC7) != 0x05) continue;
                int32_t disp = *(const int32_t*)(d + i + 3);
                if ((uintptr_t)(d + i + 7) + (intptr_t)disp != vtable) continue;

                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION f = RtlLookupFunctionEntry((uintptr_t)(d + i), &imageBase, nullptr);
                if (!f) continue;
                uintptr_t debut = base + f->BeginAddress;
                bool connu = false;
                for (int k = 0; k < n; ++k) if (debuts[k] == debut) connu = true;
                if (!connu) debuts[n++] = debut;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    return n;
}

// Chemin inverse de GetClassNameFP : nom de classe -> vtable(s), sans
// instance. Chaine ".?AV<nom>@@" -> TypeDescriptor (chaine - 0x10) ->
// CompleteObjectLocator (signature 1, RVA du TypeDescriptor a +0x0C) ->
// vtable (le mot qui precede la vtable pointe sur le COL). Plusieurs vtables
// possibles (heritage multiple). Ne parcourt que l'image : rapide.
int VtablesParNomDeClasse(const char* nom, uintptr_t* out, int maxOut)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS64* nt = (IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    int nSec = nt->FileHeader.NumberOfSections;

    char cherche[192];
    int nCols = 0, n = 0;
    uintptr_t cols[8] = {};
    for (int forme = 0; forme < 2 && !nCols; ++forme) {
        sprintf_s(cherche, forme ? ".?AU%s@@" : ".?AV%s@@", nom);
        size_t lc = strlen(cherche);

        // 1. TypeDescriptor : la chaine exacte (terminee par 0).
        uintptr_t td = 0;
        for (int s = 0; s < nSec && !td; ++s) {
            if (sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            const char* d = (const char*)(base + sec[s].VirtualAddress);
            size_t taille = sec[s].Misc.VirtualSize;
            __try {
                for (size_t i = 0; i + lc + 1 <= taille; ++i)
                    if (d[i] == '.' && memcmp(d + i, cherche, lc) == 0 && d[i + lc] == 0) {
                        td = (uintptr_t)(d + i) - 0x10;
                        break;
                    }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
        if (!td) continue;
        uint32_t tdRva = (uint32_t)(td - base);

        // 2. CompleteObjectLocator(s) : signature 1 et RVA du TD a +0x0C.
        for (int s = 0; s < nSec && nCols < 8; ++s) {
            if (sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            const uint8_t* d = (const uint8_t*)(base + sec[s].VirtualAddress);
            size_t taille = sec[s].Misc.VirtualSize;
            __try {
                for (size_t i = 0; i + 0x18 <= taille && nCols < 8; i += 4)
                    if (*(const uint32_t*)(d + i) == 1 && *(const uint32_t*)(d + i + 0x0C) == tdRva)
                        cols[nCols++] = (uintptr_t)(d + i);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    // 3. vtables : le mot juste avant la vtable pointe sur le COL.
    for (int c = 0; c < nCols && n < maxOut; ++c) {
        for (int s = 0; s < nSec && n < maxOut; ++s) {
            if (sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE) continue;
            const uint8_t* d = (const uint8_t*)(base + sec[s].VirtualAddress);
            size_t taille = sec[s].Misc.VirtualSize;
            __try {
                for (size_t i = 0; i + 16 <= taille && n < maxOut; i += 8)
                    if (*(const uintptr_t*)(d + i) == cols[c]) out[n++] = (uintptr_t)(d + i + 8);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    return n;
}

void TrouverDebutFonction(uintptr_t adresse)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    printf("\n[FN] remontee depuis Frostpunk.exe+%llX\n",
        (unsigned long long)(adresse - base));

    int trouves = 0;
    for (uintptr_t p = adresse; p > adresse - 0x600 && trouves < 6; --p) {
        unsigned char o[8] = {};
        if (!SafeRead((void*)p, o, 8)) continue;

        bool prologue =
            (o[0] == 0x48 && o[1] == 0x89 && (o[2] == 0x5C || o[2] == 0x54 ||
                o[2] == 0x4C || o[2] == 0x74) && o[3] == 0x24) ||
            (o[0] == 0x40 && (o[1] == 0x53 || o[1] == 0x55 || o[1] == 0x56 || o[1] == 0x57)) ||
            (o[0] == 0x48 && o[1] == 0x83 && o[2] == 0xEC) ||
            (o[0] == 0x48 && o[1] == 0x81 && o[2] == 0xEC) ||
            (o[0] == 0x4C && o[1] == 0x8B && o[2] == 0xDC) ||
            (o[0] == 0x55 && o[1] == 0x48 && o[2] == 0x8B && o[3] == 0xEC);

        if (!prologue) continue;

        unsigned char av = 0;
        SafeRead((void*)(p - 1), &av, 1);
        const char* qualite = (av == 0xC3 || av == 0xCC || av == 0x90)
            ? "sur" : "possible";

        printf("   +%llX  (%d octets avant, %s)\n",
            (unsigned long long)(p - base), (int)(adresse - p), qualite);
        trouves++;
    }
    if (!trouves) printf("   aucun candidat\n");
}




void InitAllocateur(uintptr_t modBase)
{
    MoteurAlloc = (Alloc_t)(modBase + Adr(0xE27BF0));
    printf("[ALLOC] allocateur du moteur a Frostpunk.exe+E27BF0\n");
}

// Test prudent : on demande un petit bloc et on verifie qu'il est ecrivable.
void TesterAllocateur()
{
    if (!MoteurAlloc) return;
    uintptr_t p = MoteurAlloc(0x60);
    printf("[ALLOC] demande de 96 octets -> 0x%llX\n", (unsigned long long)p);
    if (p > 0x10000) {
        uint64_t motif = 0x4141414141414141ULL;
        bool ok = SafeWrite((void*)p, &motif, 8);
        uint64_t relu = 0;
        SafeRead((void*)p, &relu, 8);
        printf("[ALLOC] ecriture=%d  relu=%016llX\n", (int)ok, (unsigned long long)relu);
    }
}

bool StrEqLoc(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

bool ToucheAppuyee(int vk)
{
    static bool avant[256] = {};
    if (vk < 0 || vk > 255) return false;

    bool maintenant = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool front = maintenant && !avant[vk];
    avant[vk] = maintenant;
    return front;
}

// ============================================================================
//  Cycle de vie de la partie
// ============================================================================
// Detection des chargements et resolution des references globales partagees
// par tous les modules : systemes (scenario, difficulte, selection,
// producteur de messages), gabarits de batiment, entrees de message, technos
// Archipelago. Chaque init reussie incremente PartieGeneration() : un module
// qui voit la generation changer refait sa propre init (ex. ScootingInit).

static bool      g_partiePrete = false;
static unsigned  g_partieGeneration = 0;
static uintptr_t g_partieSelection = 0;
static uintptr_t g_partieListeSites = 0;   // liste +0x2078 d'ExpeditionSite a l'init

bool      PartiePrete()      { return g_partiePrete; }
unsigned  PartieGeneration() { return g_partieGeneration; }
uintptr_t PartieSelection()  { return g_partieSelection; }

bool EconomiePresente()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), eco = 0;
    if (!SafeRead((void*)(base + Adr(0x40916D0)), &eco, 8)) return false;
    return eco > 0x10000;
}

// Tableau {entite, composant} d'un systeme (+0x2078), 0 si illisible.
static uintptr_t ListeDuSysteme(int idSysteme)
{
    uintptr_t sys = SystemeParId(idSysteme), arr = 0;
    if (!sys || !SafeRead((void*)(sys + 0x2078), &arr, 8)) return 0;
    return arr;
}

static void PartieOublier(const char* raison)
{
    if (g_partiePrete) printf("[PARTIE] references oubliees : %s\n", raison);
    Res_OublierContextes();      // self et definitions de ressources changent
    g_partiePrete = false;
    g_partieSelection = 0;
    g_partieListeSites = 0;
}

static bool PartieInit()
{
    DWORD t0 = GetTickCount();
    static uintptr_t bufBat[1024], bufMsgEnt[2048];
    int nBat = GabaritsBatCaptures(bufBat, 1024);
    int nMsgEnt = EntreesMessageCaptures(bufMsgEnt, 2048);

    // --- systemes : global fixe, verifie par la classe et un champ plausible ---
    uintptr_t scnSys = SystemeParId(IdSysteme::Scenario);
    if (scnSys && !ClasseExacte(scnSys, "FrostpunkScenarioSystem")) scnSys = 0;
    if (scnSys) {
        uintptr_t s = 0;
        if (!SafeRead((void*)(scnSys + 0x68), &s, 8)) scnSys = 0;
    }

    uintptr_t difSys = SystemeParId(IdSysteme::Difficulty);
    if (difSys && !ClasseExacte(difSys, "FrostpunkDifficultySystem")) difSys = 0;
    if (difSys) {
        uintptr_t a = 0; uint32_t c = 0;
        if (!SafeRead((void*)(difSys + 0x30), &a, 8) ||
            !SafeRead((void*)(difSys + 0x38), &c, 4) ||
            a <= 0x10000 || c == 0 || c > 64) difSys = 0;
    }

    uintptr_t selSys = SystemeParId(IdSysteme::Selection);
    if (selSys && !ClasseExacte(selSys, "FrostpunkSelectionSystem")) selSys = 0;
    if (selSys) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(selSys + 0x2250), &arr, 8) ||
            !SafeRead((void*)(selSys + 0x2258), &cnt, 4) || cnt > 64) selSys = 0;
    }

    uintptr_t msgSys = SystemeParId(IdSysteme::MessageProducer);
    if (msgSys && !ClasseExacte(msgSys, "FrostpunkMessageProducerSystem")) msgSys = 0;

    printf("[PARTIE] scn=%d dif=%d sel=%d msgSys=%d bat=%d msgEnt=%d (%lu ms)\n",
        scnSys ? 1 : 0, difSys ? 1 : 0, selSys ? 1 : 0, msgSys ? 1 : 0,
        nBat, nMsgEnt, GetTickCount() - t0);

    ScenarioSetCaches(scnSys, difSys);

    // --- gabarits de batiment et technos Archipelago ---
    RemplirCacheGabarits(bufBat, nBat);
    ResoudreTechnosAP();

    // --- messages : references de fabrication ---
    uintptr_t producteurs[1] = { msgSys };
    MessageSetReferences(producteurs, msgSys ? 1 : 0, bufMsgEnt, nMsgEnt);

    // --- ressources : self et GUID de chacune, depuis le conteneur ---
    Res_InitDepuisEconomie();

    // Manques non bloquants, mais qui privent d'une fonctionnalite.
    if (!difSys) printf("  [PARTIE] systeme de difficulte absent ou tableau +0x30 implausible\n");
    if (!selSys) printf("  [PARTIE] systeme de selection absent : pas de survol des scouts\n");
    if (!msgSys) printf("  [PARTIE] producteur de messages absent : pas de notifications\n");
    if (!nBat)   printf("  [PARTIE] aucun gabarit de batiment capture (hook +1916F90) : pas d'icones de batiment\n");
    if (!MessageEntreeRef())
        printf("  [PARTIE] entree CitizensDied absente parmi %d entree(s) capturee(s) (hook +19E68B0)\n", nMsgEnt);

    g_partieSelection = selSys;
    g_partieListeSites = ListeDuSysteme(IdSysteme::ExpeditionSite);

    // Le systeme de scenario porte g_run et le GameId (cle de persistance) :
    // sans lui, rien ne peut fonctionner.
    g_partiePrete = (scnSys != 0);
    if (g_partiePrete) {
        g_partieGeneration++;
        printf("[PARTIE] prete (generation %u)\n", g_partieGeneration);
    }
    else {
        printf("[PARTIE] init incomplete : systeme de scenario absent (global +0x%llX) -- nouvel essai dans 5 s\n",
            (unsigned long long)(GameOffsets::SystemesGlobaux + IdSysteme::Scenario * GameOffsets::SystemesPas));
    }
    return g_partiePrete;
}

void PartieTick()
{
    // --- changement de partie : economie ou systeme de technos recrees ---
    if (PartieRechargee()) PartieOublier("chargement detecte");

    // --- garde-fou : tant que le chargement n'est pas detecte, les pointeurs
    //     des modules peuvent viser des objets detruits (vu le 2026-10-15).
    //     La liste des sites est reallouee a chaque partie, mais pas en cours
    //     de partie (contrairement a celle d'envoi). ---
    if (g_partiePrete) {
        uintptr_t sites = ListeDuSysteme(IdSysteme::ExpeditionSite);
        if (sites != g_partieListeSites) {
            printf("[PARTIE] liste des sites changee (0x%llX -> 0x%llX) -- objets perimes\n",
                (unsigned long long)g_partieListeSites, (unsigned long long)sites);
            PartieOublier("liste des sites changee");
        }
    }
    if (g_partiePrete) return;

    // --- init : 5 s apres que l'economie soit apparue et restee presente ---
    static DWORD stableDepuis = 0;
    if (!EconomiePresente()) { stableDepuis = 0; return; }
    if (stableDepuis == 0) { stableDepuis = GetTickCount(); return; }
    if (GetTickCount() - stableDepuis < 5000) return;
    stableDepuis = 0;
    if (!PartieInit()) stableDepuis = GetTickCount();
}

// ============================================================================
//  Diagnostic F8 : recherche d'ancrages fixes pour remplacer le balayage
// ============================================================================
// La table de la scene (+0x2C31B20) donne {nom, id} et non {nom, objet}. On
// cherche le tableau "id -> systeme" a partir de systemes deja trouves par le
// balayage, puis qui pointe sur ce tableau, puis ou les systemes d'expedition
// rangent les sites et le composant d'envoi.

static void AfficherConteneur(uintptr_t adr)
{
    uintptr_t debut = 0; char cls[128] = { 0 };
    if (DebutObjet(adr, 0x800, &debut, cls, sizeof(cls)))
        printf("      0x%llX  dans %s +0x%llX\n", (unsigned long long)adr, cls,
            (unsigned long long)(adr - debut));
    else
        printf("      0x%llX  (objet englobant inconnu)\n", (unsigned long long)adr);
}

// Cherche dans 'sys' (0x4000 octets) un pointeur direct vers chaque cible, ou
// un pointeur vers un tableau (512 premiers mots) qui la contient.
// 'trouves[c]' est incremente a chaque trace de la cible c.
static void ChercherDansSysteme(uintptr_t sys, const char* nomSys,
    const uintptr_t* cibles, const char* const* noms, int nCibles, int* trouves)
{
    if (sys <= 0x10000) { printf("   %s : global vide\n", nomSys); return; }
    static uintptr_t bloc[0x4000 / 8];
    memset(bloc, 0, sizeof(bloc));
    if (!SafeRead((void*)sys, bloc, sizeof(bloc))) {
        // Objet plus court que 0x4000 : on se contente de 0x800.
        memset(bloc, 0, sizeof(bloc));
        if (!SafeRead((void*)sys, bloc, 0x800)) return;
    }
    static uintptr_t tab[512];
    for (int i = 0; i < (int)(sizeof(bloc) / 8); ++i) {
        uintptr_t v = bloc[i];
        bool direct = false;
        for (int c = 0; c < nCibles; ++c) {
            if (cibles[c] <= 0x10000 || v != cibles[c]) continue;
            printf("   %s +0x%04X = %s (direct)\n", nomSys, i * 8, noms[c]);
            trouves[c]++;
            direct = true;
        }
        if (direct || v <= 0x10000 || (v & 7)) continue;
        if (!SafeRead((void*)v, tab, sizeof(tab))) {
            if (!SafeRead((void*)v, tab, 64 * 8)) continue;
            for (int k = 64; k < 512; ++k) tab[k] = 0;
        }
        for (int c = 0; c < nCibles; ++c) {
            if (cibles[c] <= 0x10000) continue;
            for (int k = 0; k < 512; ++k) {
                if (tab[k] != cibles[c]) continue;
                uint32_t cnt = 0;
                SafeRead((void*)(sys + i * 8 + 8), &cnt, 4);
                printf("   %s +0x%04X -> [%d] = %s (tableau, mot suivant = %u)\n",
                    nomSys, i * 8, k, noms[c], cnt);
                trouves[c]++;
                break;
            }
        }
    }
}

// Qui, dans le tas, pointe sur 'cible' (16 premiers), avec l'objet englobant
// et les deux dwords qui suivent (compte / capacite d'un tableau dynamique).
// borne : taille max des regions parcourues (0 = toutes).
static void QuiPointeSur(uintptr_t cible, const char* nomCible, SIZE_T borne = 0x1000000)
{
    if (cible <= 0x10000) { printf("[ANCRE] %s : inconnu\n", nomCible); return; }
    static uintptr_t hits[16];
    DWORD t0 = GetTickCount();
    int n = ChercherValeurDansTas(cible, hits, 16, borne);
    printf("[ANCRE] %s = 0x%llX : %d emplacement(s) du tas pointent dessus (%lu ms, borne %s)\n",
        nomCible, (unsigned long long)cible, n, GetTickCount() - t0,
        borne ? "16 Mo" : "aucune");
    for (int h = 0; h < n; ++h) {
        uint32_t suite[2] = {};
        SafeRead((void*)(hits[h] + 8), suite, 8);
        AfficherConteneur(hits[h]);
        printf("         dwords suivants : %u, %u\n", suite[0], suite[1]);
    }
    FindStaticPointersTo(cible);
}

// Liste {entite, composant} d'un systeme : tableau a +0x2078 (pas de 0x10),
// nombre a +0x2080. Affiche le nombre, les 3 premieres paires et verifie que
// tous les composants sont de la meme classe.
static void DecrireListeSysteme(int id, const char* nomSys, const char* attendu)
{
    uintptr_t sys = SystemeParId(id);
    if (!sys) { printf("   %s : global vide\n", nomSys); return; }
    uintptr_t arr = 0; uint32_t cnt = 0, cnt2060 = 0;
    SafeRead((void*)(sys + 0x2078), &arr, 8);
    SafeRead((void*)(sys + 0x2080), &cnt, 4);
    SafeRead((void*)(sys + 0x2068), &cnt2060, 4);
    printf("   %-22s +0x2078 : %u paire(s) (+0x2068 : %u) -- attendu %s\n",
        nomSys, cnt, cnt2060, attendu);
    if (arr <= 0x10000 || cnt == 0 || cnt > 4096) return;

    char ref[128] = { 0 };
    int memeClasse = 0;
    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t paire[2] = {};
        if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10), paire, 16)) continue;
        char clsComp[128] = { 0 }, clsEnt[128] = { 0 };
        if (!GetClassNameFP(paire[1], clsComp, sizeof(clsComp))) strcpy_s(clsComp, "?");
        if (!GetClassNameFP(paire[0], clsEnt, sizeof(clsEnt))) strcpy_s(clsEnt, "?");
        if (k == 0) strcpy_s(ref, clsComp);
        if (StrEq(clsComp, ref)) memeClasse++;
        if (k < 3)
            printf("      [%u] entite 0x%llX (%s)  comp 0x%llX (%s)\n", k,
                (unsigned long long)paire[0], clsEnt, (unsigned long long)paire[1], clsComp);
    }
    printf("      %d/%u composant(s) de classe %s\n", memeClasse, cnt, ref);
}

// Decrit les pointeurs des 0x80 premiers octets d'un objet, l'objet qui
// l'englobe eventuellement, et qui pointe sur cet englobant.
static void DecrireEtRemonter(uintptr_t obj, const char* nom)
{
    if (obj <= 0x10000) { printf("[ANCRE] %s : inconnu\n", nom); return; }
    char cls[128] = { 0 };
    if (!GetClassNameFP(obj, cls, sizeof(cls))) strcpy_s(cls, "?");
    printf("\n[ANCRE] %s = 0x%llX (%s)\n", nom, (unsigned long long)obj, cls);

    uintptr_t mots[0x80 / 8] = {};
    SafeRead((void*)obj, mots, sizeof(mots));
    for (int i = 1; i < (int)(sizeof(mots) / 8); ++i) {
        uintptr_t v = mots[i];
        if (v <= 0x10000 || (v & 7)) continue;
        char c[128] = { 0 };
        if (GetClassNameFP(v, c, sizeof(c))) {
            printf("   +0x%02X -> 0x%llX  %s\n", i * 8, (unsigned long long)v, c);
            continue;
        }
        char s[64] = { 0 };
        if (!SafeRead((void*)v, s, sizeof(s) - 1)) continue;
        int len = 0;
        while (len < 63 && s[len] >= 0x20 && s[len] < 0x7F) len++;
        if (len >= 3 && s[len] == 0)
            printf("   +0x%02X -> 0x%llX  \"%s\"\n", i * 8, (unsigned long long)v, s);
    }

    uintptr_t debut = 0; char c2[128] = { 0 };
    if (DebutObjet(obj - 8, 0x2000, &debut, c2, sizeof(c2))) {
        printf("   englobe par %s a 0x%llX (objet a +0x%llX dedans)\n", c2,
            (unsigned long long)debut, (unsigned long long)(obj - debut));
        QuiPointeSur(debut, "englobant");
    }
    else {
        printf("   pas d'englobant RTTI dans les 0x2000 octets precedents\n");
    }
}

// Diagnostic F8 : les gabarits de batiment et les entrees de message
// existent-ils des le menu principal, et gardent-ils leur adresse d'une
// partie a l'autre ? Chaque appui balaye ces deux classes seulement et
// compare au releve de l'appui precedent.
void DiagGabaritsPrecoces()
{
    static const char* noms[2] = {
        "FrostpunkBuildingComponentTemplate",
        "FrostpunkMessageEntry",
    };
    static uintptr_t gab[1024], ent[1024];          // releve courant
    static uintptr_t gabAvant[1024], entAvant[1024]; // releve precedent
    static int nGabAvant = -1, nEntAvant = 0, passe = 0;
    passe++;

    uintptr_t* sorties[2] = { gab, ent };
    int compteurs[2] = {};
    DWORD t0 = GetTickCount();
    CollectObjectsByClasses(noms, 2, sorties, compteurs, 1024);
    DWORD dt = GetTickCount() - t0;

    printf("\n[GABARIT] passe %d (%s) : %d gabarit(s) de batiment, %d entree(s) de message en %lu ms\n",
        passe, EconomiePresente() ? "en partie" : "hors partie (menu ?)",
        compteurs[0], compteurs[1], dt);

    if (PartiePrete()) {
        int dansCache = 0;
        for (int i = 0; i < compteurs[0]; ++i) if (GabaritBatEnCache(gab[i])) dansCache++;
        bool entreeVue = false;
        for (int i = 0; i < compteurs[1]; ++i) if (ent[i] == MessageEntreeRef()) { entreeVue = true; break; }
        printf("[GABARIT] compare a l'init de la partie : %d/%d gabarit(s) du cache retrouve(s), "
            "entree CitizensDied %s\n", dansCache, NombreGabaritsBat(), entreeVue ? "retrouvee" : "ABSENTE");
    }

    if (nGabAvant >= 0) {
        int gOk = 0, eOk = 0;
        for (int i = 0; i < nGabAvant; ++i)
            for (int j = 0; j < compteurs[0]; ++j) if (gabAvant[i] == gab[j]) { gOk++; break; }
        for (int i = 0; i < nEntAvant; ++i)
            for (int j = 0; j < compteurs[1]; ++j) if (entAvant[i] == ent[j]) { eOk++; break; }
        printf("[GABARIT] compare a la passe %d : gabarits %d/%d a la meme adresse, entrees %d/%d\n",
            passe - 1, gOk, nGabAvant, eOk, nEntAvant);
    }

    for (int i = 0; i < compteurs[0]; ++i) gabAvant[i] = gab[i];
    for (int i = 0; i < compteurs[1]; ++i) entAvant[i] = ent[i];
    nGabAvant = compteurs[0];
    nEntAvant = compteurs[1];
}

void DiagAncrages()
{
    if (!PartiePrete()) { printf("[ANCRE] init pas terminee\n"); return; }

    // Les listes {entite, composant} a +0x2078 et la regle des globaux sont
    // confirmees (2026-10-15, voir CLAUDE.md). Reste : gabarits et entrees.

    // --- A. composant de batiment -> gabarit ? ---
    printf("\n[ANCRE] ===== A. gabarits atteignables par les batiments =====\n");
    uintptr_t bsys = SystemeParId(IdSysteme::Building), barr = 0;
    uint32_t bcnt = 0;
    if (bsys) {
        SafeRead((void*)(bsys + 0x2078), &barr, 8);
        SafeRead((void*)(bsys + 0x2080), &bcnt, 4);
    }
    if (barr > 0x10000 && bcnt > 0 && bcnt <= 4096) {
        uintptr_t comp0 = 0;
        SafeRead((void*)(barr + 8), &comp0, 8);
        DecrireEtRemonter(comp0, "BuildingComponent[0]");

        // Pour chaque composant : premier pointeur (0x100 octets) vers un
        // FrostpunkBuildingComponentTemplate. On compte les gabarits distincts.
        static uintptr_t distincts[512];
        int nDist = 0, enCache = 0, offTrouve = -1;
        for (uint32_t k = 0; k < bcnt; ++k) {
            uintptr_t comp = 0;
            if (!SafeRead((void*)(barr + (uintptr_t)k * 0x10 + 8), &comp, 8)) continue;
            uintptr_t mots[0x100 / 8] = {};
            if (!SafeRead((void*)comp, mots, sizeof(mots))) continue;
            for (int i = 1; i < (int)(sizeof(mots) / 8); ++i) {
                if (mots[i] <= 0x10000 || (mots[i] & 7)) continue;
                if (!ClasseExacte(mots[i], "FrostpunkBuildingComponentTemplate")) continue;
                if (offTrouve < 0) offTrouve = i * 8;
                bool deja = false;
                for (int d = 0; d < nDist; ++d) if (distincts[d] == mots[i]) { deja = true; break; }
                if (!deja && nDist < 512) {
                    distincts[nDist++] = mots[i];
                    if (GabaritBatEnCache(mots[i])) enCache++;
                }
                break;
            }
        }
        printf("[ANCRE] gabarit lu a +0x%X du composant : %d gabarit(s) distinct(s) pour %u batiment(s), "
            "%d dans le cache du scan (%d)\n",
            offTrouve, nDist, bcnt, enCache, NombreGabaritsBat());
    }
    else {
        printf("[ANCRE] liste des batiments illisible\n");
    }

    // --- B. tableau des entrees de message (pas de 0xE0) ---
    printf("\n[ANCRE] ===== B. tableau des entrees de message =====\n");
    uintptr_t entree = MessageEntreeRef();
    if (entree) {
        const uintptr_t PAS = 0xE0;
        uintptr_t debut = entree;
        while (ClasseExacte(debut - PAS, "FrostpunkMessageEntry")) debut -= PAS;
        int n = 0;
        while (ClasseExacte(debut + (uintptr_t)n * PAS, "FrostpunkMessageEntry")) n++;
        printf("[ANCRE] tableau contigu : debut 0x%llX, %d entree(s), CitizensDied a l'indice %d\n",
            (unsigned long long)debut, n, (int)((entree - debut) / PAS));
        QuiPointeSur(debut, "debut du tableau d'entrees", 0);

        const uintptr_t cibles[] = { debut };
        const char* const noms[] = { "debut tableau entrees" };
        int trouves[1] = {};
        ChercherDansSysteme(SystemeParId(IdSysteme::MessageProducer), "MessageProducerSystem",
            cibles, noms, 1, trouves);
        ChercherDansSysteme(SystemeParId(IdSysteme::MessageSystem), "MessageSystem",
            cibles, noms, 1, trouves);
        if (!trouves[0]) printf("   debut du tableau : aucune trace dans les 2 systemes de message\n");
    }
    else {
        printf("[ANCRE] entree CitizensDied inconnue\n");
    }

    // --- C. gabarit : references dans TOUT le tas (sans borne de region) ---
    printf("\n[ANCRE] ===== C. gabarit bat[0], tas complet =====\n");
    QuiPointeSur(PremierGabaritBat(), "gabarit bat[0]", 0);

    printf("[ANCRE] ===== fin =====\n");
}

// Champs non nuls des 'longueur' premiers octets d'un objet : pointeur vers
// objet RTTI (classe), pointeur vers chaine ASCII, sinon valeur brute avec
// ses deux int32 et le float du dword bas.
void DecrireChamps(uintptr_t obj, int longueur, const char* nom)
{
    char cls[128] = { 0 };
    if (obj <= 0x10000 || !GetClassNameFP(obj, cls, sizeof(cls))) strcpy_s(cls, "?");
    printf("\n[GROUPE] %s = 0x%llX (%s), 0x%X premiers octets :\n",
        nom, (unsigned long long)obj, cls, longueur);
    if (obj <= 0x10000) return;

    static uint8_t bloc[0x400];
    if (longueur > (int)sizeof(bloc)) longueur = (int)sizeof(bloc);
    memset(bloc, 0, sizeof(bloc));
    if (!SafeRead((void*)obj, bloc, longueur)) { printf("   illisible\n"); return; }

    for (int off = 8; off + 8 <= longueur; off += 8) {
        uint64_t v = *(uint64_t*)(bloc + off);
        if (!v) continue;
        char c[128] = { 0 }, s[64] = { 0 };
        if (v > 0x10000 && !(v & 7) && GetClassNameFP((uintptr_t)v, c, sizeof(c))) {
            printf("   +0x%03X  0x%llX  -> %s\n", off, (unsigned long long)v, c);
            continue;
        }
        if (v > 0x10000 && SafeRead((void*)v, s, sizeof(s) - 1)) {
            int len = 0;
            while (len < 63 && s[len] >= 0x20 && s[len] < 0x7F) len++;
            if (len >= 3 && s[len] == 0) {
                printf("   +0x%03X  0x%llX  -> \"%s\"\n", off, (unsigned long long)v, s);
                continue;
            }
        }
        int32_t bas = (int32_t)(v & 0xFFFFFFFF), haut = (int32_t)(v >> 32);
        float f = 0; memcpy(&f, bloc + off, 4);
        printf("   +0x%03X  0x%016llX  (i32 %d / %d, f %.3g)\n",
            off, (unsigned long long)v, bas, haut, f);
    }
}

// Diagnostic F9 (2026-10-28) : comment le jeu represente les groupes de
// survivants / refugies qui marchent vers la ville ? Liste les systemes
// candidats (liste {entite, composant} a +0x2078) et detaille le premier
// composant et la premiere entite de chacun. A lancer pendant qu'un groupe
// est visible sur la carte (scenario Refugees, par exemple).
void DiagSystemesGroupes()
{
    if (!PartiePrete()) { printf("[GROUPE] partie pas prete\n"); return; }

    struct Candidat { int id; const char* nom; };
    const Candidat candidats[] = {
        { 0x6D, "SurvivorGroupSystem" },   { 0x6C, "SettlementSystem" },
        { 0x6A, "OutpostSystem" },         { 0x6B, "OutpostTransportSystem" },
        { 0x69, "ExpeditionHostSystem" },  { 0x68, "ExpeditionSystem" },
        { 0xB7, "AbsentCitizenSystem" },   { 0x61, "WorldMapMarkerSystem" },
    };
    printf("\n[GROUPE] ===== systemes candidats =====\n");
    for (int i = 0; i < (int)(sizeof(candidats) / sizeof(candidats[0])); ++i) {
        uintptr_t sys = SystemeParId(candidats[i].id);
        char cls[128] = { 0 };
        if (!sys || !GetClassNameFP(sys, cls, sizeof(cls))) strcpy_s(cls, "?");
        printf("[GROUPE] id 0x%02X %-24s = 0x%llX (%s)\n", candidats[i].id, candidats[i].nom,
            (unsigned long long)sys, cls);
        DecrireListeSysteme(candidats[i].id, candidats[i].nom, "-");
    }

    // Detail : premier composant et premiere entite des systemes de groupes.
    const int detail[] = { 0x6D, 0x6C, 0x6B };
    for (int i = 0; i < (int)(sizeof(detail) / sizeof(detail[0])); ++i) {
        uintptr_t sys = SystemeParId(detail[i]), arr = 0; uint32_t cnt = 0;
        if (!sys) continue;
        SafeRead((void*)(sys + 0x2078), &arr, 8);
        SafeRead((void*)(sys + 0x2080), &cnt, 4);
        if (arr <= 0x10000 || cnt == 0 || cnt > 4096) continue;
        uintptr_t paire[2] = {};
        SafeRead((void*)arr, paire, 16);
        char nom[64];
        sprintf_s(nom, "systeme 0x%02X : composant[0]", detail[i]);
        DecrireChamps(paire[1], 0x200, nom);
        sprintf_s(nom, "systeme 0x%02X : entite[0]", detail[i]);
        DecrireChamps(paire[0], 0x100, nom);
    }

    // --- Groupe de survivants : template du composant, puis noms et offsets
    //     des proprietes (systeme de reflexion du jeu). Lent (parcours de la
    //     memoire), une seule fois par appui. ---
    uintptr_t sysGr = SystemeParId(0x6D), arrGr = 0;
    uint32_t cntGr = 0;
    if (sysGr) {
        SafeRead((void*)(sysGr + 0x2078), &arrGr, 8);
        SafeRead((void*)(sysGr + 0x2080), &cntGr, 4);
    }
    if (arrGr > 0x10000 && cntGr > 0 && cntGr <= 4096) {
        uintptr_t comp0 = 0, tpl = 0;
        SafeRead((void*)(arrGr + 8), &comp0, 8);
        SafeRead((void*)(comp0 + 0x20), &tpl, 8);
        DecrireChamps(tpl, 0x200, "template du groupe[0] (composant +0x20)");
    }

    // Proprietes du template obtenues le 2026-10-28 (FindClassRecord, 81 s) :
    // consignees dans CLAUDE.md, recherche retiree.

    // --- vtables des classes liees aux groupes (et aux expeditions, creees
    //     en cours de partie : leur constructeur mene a la creation
    //     d'entites sur la carte). Pour chacune : premieres methodes
    //     virtuelles et fonctions qui chargent la vtable (constructeurs /
    //     destructeurs), adresses a ouvrir dans Ghidra. ---
    const char* classes[] = {
        "FrostpunkSurvivorGroupComponent",
        "FrostpunkMakeSurvivorGroupKnownEffect",
        "FrostpunkSurvivorGroupReachedCityEventTrigger",
        "FrostpunkSurvivorGroupSystem",
        "FrostpunkSurvivorsEntry",
        "FrostpunkExpeditionComponent",
    };
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    for (int i = 0; i < (int)(sizeof(classes) / sizeof(classes[0])); ++i) {
        uintptr_t vt[4] = {};
        int nvt = VtablesParNomDeClasse(classes[i], vt, 4);
        printf("\n[GROUPE] %s : %d vtable(s)\n", classes[i], nvt);
        for (int v = 0; v < nvt; ++v) {
            printf("   vtable Frostpunk.exe+%llX (Ghidra 0x%llX), methodes :\n",
                (unsigned long long)(vt[v] - modBase), (unsigned long long)(vt[v] - modBase + 0x140000000ULL));
            for (int m = 0; m < 12; ++m) {
                uintptr_t f = 0;
                if (!SafeRead((void*)(vt[v] + (uintptr_t)m * 8), &f, 8)) break;
                if (f < modBase || f >= modBase + 0x10000000) break;   // fin de la vtable
                printf("      [%2d] Frostpunk.exe+%llX\n", m, (unsigned long long)(f - modBase));
            }
            ChercherReferencesVtable(vt[v], classes[i]);
        }
    }
    printf("[GROUPE] ===== fin =====\n");
}

// --- Trace de la chaine de creation des entites de la carte -----------------
// Les constructeurs des composants (expedition +0x19E9560, groupe de
// survivants +0x1A0EB70) ne sont appeles que par leur fabrique de reflexion
// et par un clonage : la fonction generique "template -> entite" est plus
// haut dans la pile. A chaque construction, on balaye la pile a la recherche
// d'adresses de retour dans l'image (precedees d'un call direct E8 ou d'un
// call indirect FF /2), qu'on journalise avec leur adresse Ghidra et le debut
// de la fonction qui les contient (.pdata). _ReturnAddress() est inutilisable
// sous MinHook, d'ou le balayage.
typedef uintptr_t(*CtorComposant_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static CtorComposant_t oCtorExpedition = nullptr;
static CtorComposant_t oCtorGroupe     = nullptr;
static volatile long   g_traceExpedition = 0;
static volatile long   g_traceGroupe     = 0;
static const long      TRACE_MAX_APPELS  = 12;   // par constructeur

// Vrai si 'ret' suit une instruction call (E8 rel32, FF 15 [rip], FF /2 reg
// ou memoire avec deplacement 0/8/32 bits).
static bool SuitUnCall(uintptr_t ret)
{
    unsigned char o[7] = {};
    if (!SafeRead((void*)(ret - 7), o, 7)) return false;
    // o[6] = ret-1, o[5] = ret-2, ..., o[0] = ret-7
    if (o[2] == 0xE8) return true;                                       // E8 rel32
    if (o[1] == 0xFF && o[2] == 0x15) return true;                       // FF 15 disp32
    if (o[5] == 0xFF && (o[6] & 0x38) == 0x10 && (o[6] >> 6) == 3) return true;   // FF D0+r
    if (o[5] == 0xFF && (o[6] & 0x38) == 0x10 && (o[6] >> 6) == 0) return true;   // FF 10+r
    if (o[4] == 0xFF && (o[5] & 0x38) == 0x10 && (o[5] >> 6) == 1) return true;   // FF 50+r d8
    if (o[3] == 0xFF && (o[4] & 0x38) == 0x10 && (o[4] >> 6) == 1) return true;   // avec SIB
    if (o[1] == 0xFF && (o[2] & 0x38) == 0x10 && (o[2] >> 6) == 2) return true;   // FF 90+r d32
    if (o[0] == 0xFF && (o[1] & 0x38) == 0x10 && (o[1] >> 6) == 2) return true;   // avec SIB
    return false;
}

void TracerPile(const char* qui, uintptr_t obj, long numero)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
    uintptr_t fin = base + nt->OptionalHeader.SizeOfImage;

    printf("[TRACE] %s #%ld : objet 0x%llX, thread %lu\n", qui, numero,
        (unsigned long long)obj, GetCurrentThreadId());

    // Pile exacte par deroulage (.pdata) : le balayage brut ci-dessous ramasse
    // aussi des adresses de retour perimees (vu le 2026-10-28 : zlib et flux
    // de fichier au-dessus du vrai appelant).
    void* cadres[24] = {};
    USHORT nc = RtlCaptureStackBackTrace(0, 24, cadres, nullptr);
    int dansImage = 0;
    for (USHORT k = 0; k < nc; ++k) {
        uintptr_t v = (uintptr_t)cadres[k];
        if (v <= base || v >= fin) continue;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(v, &imageBase, nullptr);
        printf("   cadre %2u  retour Ghidra 0x%llX  dans FUN_%llx\n", k,
            (unsigned long long)(v - base + 0x140000000ULL),
            (unsigned long long)((f ? f->BeginAddress : 0) + 0x140000000ULL));
        ++dansImage;
    }
    if (dansImage > 0) return;
    printf("   (deroulage sans resultat : balayage brut)\n");

    volatile uintptr_t ancre = 0;
    uintptr_t* p = (uintptr_t*)&ancre;
    int trouves = 0;
    for (int i = 0; i < 1024 && trouves < 16; ++i) {
        uintptr_t v = 0;
        if (!SafeRead(p + i, &v, 8)) break;
        if (v <= base + 0x1000 || v >= fin) continue;
        if (!SuitUnCall(v)) continue;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(v, &imageBase, nullptr);
        if (f)
            printf("   pile+0x%03X  retour Ghidra 0x%llX  dans FUN_%llx\n", i * 8,
                (unsigned long long)(v - base + 0x140000000ULL),
                (unsigned long long)(f->BeginAddress + 0x140000000ULL));
        else
            printf("   pile+0x%03X  retour Ghidra 0x%llX  (fonction inconnue)\n", i * 8,
                (unsigned long long)(v - base + 0x140000000ULL));
        ++trouves;
    }
}

static uintptr_t hkCtorGroupe(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    uintptr_t r = oCtorGroupe(a, b, c, d);
    GroupesComposantConstruit(a);   // pendant LoadGame : groupes a recoller
    return r;
}

// Constructeur de FrostpunkSurvivorGroupComponent (+0x1A0EB70) : groupes a
// recoller pendant LoadGame. (La trace de pile des constructeurs a ete
// retiree pour la 1.0.)
void InstallTraceCreationEntites(uintptr_t modBase)
{
    void* p2 = (void*)(modBase + Adr(0x1A0EB70));
    if (MH_CreateHook(p2, (void*)&hkCtorGroupe, (void**)&oCtorGroupe) == MH_OK &&
        MH_EnableHook(p2) == MH_OK) printf("[TRACE] hook +1A0EB70 (groupe) OK\n");
    else printf("[TRACE] hook +1A0EB70 (groupe) KO\n");
}
