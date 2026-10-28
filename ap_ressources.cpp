#include "ap_ressources.h"
#include "mod_common.h"
#include "scooting.h"
#include "scenario.h"

// GUID invente, volontairement reconnaissable dans un dump.
static const uint64_t AP_GUID0 = 0x4150204954454D31ULL;   // "AP ITEM1"
static const uint64_t AP_GUID1 = 0x4152434849504F31ULL;   // "ARCHIPO1"

static const char  AP_NOM[] = "AP_Item";
static uint8_t     g_def[0x100];         // notre definition, jamais liberee
static bool        g_pret_ressources = false;

volatile bool g_apPorteurActif = true;

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
    if (g_pret_ressources) return true;

    if (!EconomieValide()) {
        static DWORD dernier = 0;
        if (GetTickCount() - dernier > 10000) {
            dernier = GetTickCount();
            printf("[APRES] economie pas encore prete\n");
        }
        return false;
    }

    uintptr_t src = DefinitionSource();
    if (!src) return false;

    if (!SafeRead((void*)src, g_def, AP_DEF_TAILLE)) return false;

    uintptr_t pnom = (uintptr_t)AP_NOM;
    memcpy(g_def + 0x00, &pnom, 8);
    memcpy(g_def + 0x08, &AP_GUID0, 8);
    memcpy(g_def + 0x10, &AP_GUID1, 8);

    uintptr_t pcle = (uintptr_t)AP_CLE;
    memcpy(g_def + 0x28, &pcle, 8);

    int32_t cr[2] = { 0, 1 };
    memcpy(g_def + 0x98 + 8, cr, 8);

    g_pret_ressources = true;
    printf("[APRES] definition fabriquee a 0x%llX\n",
        (unsigned long long)(uintptr_t)g_def);
    return true;
}
uintptr_t ApPorteurDef()
{
    return g_pret_ressources ? (uintptr_t)g_def : 0;
}

// La cle passee au setter de ressources est soit la definition, soit son GUID
// (definition + 8) : la convention exacte est deduite dans resources.cpp. On
// reconnait notre porteur dans les deux cas : cle == notre definition, ou GUID
// lu a la cle ou a cle + 8.
bool ApPorteurEstNotreGuid(uintptr_t guidPtr)
{
    if (guidPtr <= 0x10000) return false;
    if (g_pret_ressources && (guidPtr == (uintptr_t)g_def || guidPtr == (uintptr_t)g_def + 8))
        return true;
    for (int d = 0; d <= 8; d += 8) {
        uint64_t a = 0, b = 0;
        if (!SafeRead((void*)(guidPtr + d), &a, 8)) continue;
        if (!SafeRead((void*)(guidPtr + d + 8), &b, 8)) continue;
        if (a == AP_GUID0 && b == AP_GUID1) return true;
    }
    return false;
}

const char* ApPorteurNomInterne() { return AP_NOM; }


static bool EconomieValide()
{
    if (!EnsureResSelf()) return false;

    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
    if (!SafeRead((void*)(g_selfRessource + 0x2240), &arr, 8)) return false;
    if (!SafeRead((void*)(g_selfRessource + 0x2248), &cnt, 4)) return false;
    if (!SafeRead((void*)(g_selfRessource + 0x224C), &cap, 4)) return false;
    if (arr <= 0x10000 || cnt == 0 || cnt > 64 || cap > 64) return false;

    // Toutes les entrees doivent porter un nom lisible.
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) return false;
        uintptr_t np = 0; char n[64] = { 0 };
        if (!SafeRead((void*)def, &np, 8) || np <= 0x10000) return false;
        if (!SafeRead((void*)np, n, sizeof(n) - 1)) return false;
        if (n[0] < 0x20 || n[0] > 0x7E) return false;
    }
    return true;
}