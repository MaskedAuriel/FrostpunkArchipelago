#include "ap_ressources.h"
#include "mod_common.h"

// GUID invente, volontairement reconnaissable dans un dump.
static const uint64_t AP_GUID0 = 0x4150204954454D31ULL;   // "AP ITEM1"
static const uint64_t AP_GUID1 = 0x4152434849504F31ULL;   // "ARCHIPO1"

static const char  AP_NOM[] = "AP_Item";
static uint8_t     g_def[0x100];         // notre definition, jamais liberee
static bool        g_pret = false;

volatile bool g_apPorteurActif = true;

static bool StrEqLoc(const char* a, const char* b)
{
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

// Definition source : n'importe laquelle du conteneur fait l'affaire.
static uintptr_t PremiereDefinition()
{
    if (!EnsureResSelf()) return 0;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000)
        return 0;
    for (int i = 0; i < 12; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;
        uintptr_t np = 0; char n[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000) SafeRead((void*)np, n, sizeof(n) - 1);
        if (n[0] >= 0x20 && n[0] < 0x7F) return def;
    }
    return 0;
}

static uintptr_t DefinitionSource()
{
    if (!EnsureResSelf()) return 0;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000)
        return 0;

    for (int i = 0; i < 12; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;
        uintptr_t np = 0; char n[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000) SafeRead((void*)np, n, sizeof(n) - 1);
        if (StrEqLoc(n, "Wood")) return def;      // presente dans tous les scenarios
    }
    return 0;
}

// Cle de localisation de notre item, posee dans le champ "Display name".
static const char AP_CLE[] = "@Resources/Names/AP_Item";

static const size_t AP_DEF_TAILLE = 0xD8;   // taille reelle de FrostpunkResourceEntry


bool ApPorteurInit()
{
    if (!g_apPorteurActif) return false;
    if (g_pret) return true;

    uintptr_t src = DefinitionSource();
    if (!src) { printf("[APRES] definition source introuvable\n"); return false; }

    if (!SafeRead((void*)src, g_def, AP_DEF_TAILLE)) {
        printf("[APRES] copie impossible\n"); return false;
    }

    uintptr_t pnom = (uintptr_t)AP_NOM;
    memcpy(g_def + 0x00, &pnom, 8);
    memcpy(g_def + 0x08, &AP_GUID0, 8);
    memcpy(g_def + 0x10, &AP_GUID1, 8);

    uintptr_t pcle = (uintptr_t)AP_CLE;
    memcpy(g_def + 0x28, &pcle, 8);

    int32_t cr[2] = { 0, 1 };
    memcpy(g_def + 0x98 + 8, cr, 8);

    g_pret = true;
    printf("[APRES] definition fabriquee a 0x%llX (%zu octets, clonee de 0x%llX)\n",
        (unsigned long long)(uintptr_t)g_def, AP_DEF_TAILLE, (unsigned long long)src);
    return true;
}

uintptr_t ApPorteurDef()
{
    return g_pret ? (uintptr_t)g_def : 0;
}

bool ApPorteurEstNotreGuid(uintptr_t guidPtr)
{
    if (guidPtr <= 0x10000) return false;
    uint64_t a = 0, b = 0;
    if (!SafeRead((void*)guidPtr, &a, 8)) return false;
    if (!SafeRead((void*)(guidPtr + 8), &b, 8)) return false;
    return a == AP_GUID0 && b == AP_GUID1;
}

const char* ApPorteurNomInterne() { return AP_NOM; }

bool ApPorteurEnregistrer()
{
    if (!g_apPorteurActif) return false;
    static bool fait = false;
    if (fait) return true;

    uintptr_t def = ApPorteurDef();
    if (!def) { printf("[CONT] definition non fabriquee\n"); return false; }
    if (!EnsureResSelf()) return false;

    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
    if (!SafeRead((void*)(g_selfRessource + 0x2240), &arr, 8)) return false;
    if (!SafeRead((void*)(g_selfRessource + 0x2248), &cnt, 4)) return false;
    if (!SafeRead((void*)(g_selfRessource + 0x224C), &cap, 4)) return false;
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) { printf("[CONT] conteneur illisible\n"); return false; }

    // Deja enregistree ? (rechargement sans quitter le jeu)
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t d = 0;
        SafeRead((void*)(arr + (uintptr_t)i * 0x70), &d, 8);
        if (d == def) { printf("[CONT] deja enregistree en position %u\n", i); fait = true; return true; }
    }

    if (cnt >= cap) { printf("[CONT] plus de place (%u/%u)\n", cnt, cap); return false; }

    // On clone l'entree 0 pour heriter de tous les champs inconnus,
    // puis on remplace la definition et on remet le stock a zero.
    uint8_t modele[0x70];
    if (!SafeRead((void*)arr, modele, sizeof(modele))) return false;

    uintptr_t e = arr + (uintptr_t)cnt * 0x70;
    if (!SafeWrite((void*)e, modele, sizeof(modele))) return false;
    if (!SafeWrite((void*)e, &def, 8)) return false;

    uint64_t zero = 0;
    SafeWrite((void*)(e + 0x08), &zero, 8);      // stock
    SafeWrite((void*)(e + 0x30), &zero, 8);      // cumul / historique

    uint32_t neuf = cnt + 1;
    if (!SafeWrite((void*)(g_selfRessource + 0x2248), &neuf, 4)) return false;

    fait = true;
    printf("[CONT] definition enregistree en position %u (%u/%u)\n", cnt, neuf, cap);
    return true;
}