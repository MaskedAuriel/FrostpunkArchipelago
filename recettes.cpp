// ---- recettes.cpp ----
// Recettes de production (2026-11-03, demande de la session apworld : un item
// de contenu de site de Last Autumn recu dans un autre scenario est converti
// en ressources de base, au cout de fabrication de la ressource).
//
// Decompiles du 2026-11-03 (adresses Ghidra) :
//   FrostpunkBlueprintsConfig (200 octets, reflexion FUN_1414af330), instance
//     statique base + 0x407BE20 (FUN_1401e6d20) : +0x78 Production blueprints
//     (Dynarray de pointeurs), +0xB0 Extraction blueprints.
//   FrostpunkBlueprintEntry (FUN_1414aef40) : +0x00 nom, +0x08 GUID, +0x18
//     Requirements, +0x30 Display name, +0x38 Description.
//   FrostpunkProductionBlueprintEntry (0xF0, FUN_1414af8f0) : +0x70 Processing
//     resources (bool), +0x78 Products (FrostpunkProductEntry polymorphes),
//     +0x90 Production effects, +0xA8 Production cost
//     (Dynarray<FrostpunkResourceAmount>, 0x14 : +0x00 lien, +0x10 Amount),
//     +0xC0 Base production time (FrostpunkGameTimeInterval), +0xC8 Upgrades
//     blueprints (liens), +0xE0 Resource upfront consumption mode.
//   FrostpunkResourceProduct (0x38, FUN_141a20020) : +0x20 lien de ressource,
//     +0x30 Amount. Autres produits : GatherableResource, FoodRation, Agent.
//
// Lignes (separateur tabulation) :
//   [RECETTE] B <scenario> <recette> <nom affiche> <duree brute> <traitement> <nb ameliorations>
//   [RECETTE] O <scenario> <recette> <classe du produit> <ressource> <quantite>
//   [RECETTE] I <scenario> <recette> <ressource> <quantite>
//   [RECETTE] U <scenario> <recette> <recette amelioree>
#include "recettes.h"
#include "mod_common.h"
#include "scenario.h"      // g_run
#include "quetes.h"        // TraduireCleJeu
#include <cstring>

namespace {

constexpr uintptr_t OFF_CONFIG_RECETTES = 0x407BE20;
constexpr int CFG_RECETTES   = 0x78;
constexpr int REC_TRAITEMENT = 0x70;
constexpr int REC_PRODUITS   = 0x78;
constexpr int REC_COUT       = 0xA8;
constexpr int REC_DUREE      = 0xC0;
constexpr int REC_AMELIOR    = 0xC8;
constexpr int PROD_RESSOURCE = 0x20;
constexpr int PROD_QUANTITE  = 0x30;

constexpr uintptr_t OFF_ECONOMIE   = 0x40916D0;
constexpr int       ECO_RESSOURCES = 0x2240;   // entrees de 0x70, compte +8
constexpr int       ECO_PAS        = 0x70;

const char* g_scn = "?";

bool LireTab(uintptr_t adr, uintptr_t& tab, uint32_t& n, uint32_t max)
{
    tab = 0; n = 0;
    if (!SafeRead((void*)adr, &tab, 8) || !SafeRead((void*)(adr + 8), &n, 4)) { n = 0; return false; }
    if (n == 0) return true;
    if (tab <= 0x10000 || n > max) { tab = 0; n = 0; return false; }
    return true;
}

bool LireNom(uintptr_t adrPtr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adrPtr, &p, 8) || p <= 0x10000) return false;
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        if (c < 0x20 || c >= 0x7F) { out[0] = 0; return false; }
        out[i] = c;
    }
    out[i] = 0;
    return i >= 2;
}

// Nom interne d'une ressource par son GUID (conteneur de l'economie).
void NomRessource(const uint8_t* guid, char* out, int taille)
{
    uintptr_t eco = 0, arr = 0;
    uint32_t n = 0;
    if (SafeRead((void*)((uintptr_t)GetModuleHandleW(NULL) + OFF_ECONOMIE), &eco, 8) && eco > 0x10000 &&
        LireTab(eco + ECO_RESSOURCES, arr, n, 64)) {
        for (uint32_t i = 0; i < n; ++i) {
            uintptr_t def = 0;
            uint8_t g[16] = {};
            if (!SafeRead((void*)(arr + (uintptr_t)i * ECO_PAS), &def, 8) || def <= 0x10000) continue;
            if (SafeRead((void*)(def + 8), g, 16) && memcmp(g, guid, 16) == 0 && LireNom(def, out, taille)) return;
        }
    }
    int o = sprintf_s(out, taille, "?");
    for (int i = 0; i < 16 && o + 2 < taille; ++i) o += sprintf_s(out + o, taille - o, "%02X", guid[i]);
}

// Nom d'une recette par son GUID, parmi celles de la config.
void NomRecette(uintptr_t tab, uint32_t n, const uint8_t* guid, char* out, int taille)
{
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t r = 0;
        uint8_t g[16] = {};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &r, 8) || r <= 0x10000) continue;
        if (SafeRead((void*)(r + 8), g, 16) && memcmp(g, guid, 16) == 0 && LireNom(r, out, taille)) return;
    }
    int o = sprintf_s(out, taille, "?");
    for (int i = 0; i < 16 && o + 2 < taille; ++i) o += sprintf_s(out + o, taille - o, "%02X", guid[i]);
}

void ReleverRecette(uintptr_t r, uintptr_t tabRec, uint32_t nRec)
{
    char nom[128] = "?", aff[256] = "-", cls[96];
    LireNom(r, nom, sizeof(nom));
    TraduireCleJeu(r + 0x30, aff, sizeof(aff));
    uint32_t duree[2] = {};
    uint8_t traitement = 0;
    SafeRead((void*)(r + REC_DUREE), duree, 8);
    SafeRead((void*)(r + REC_TRAITEMENT), &traitement, 1);
    uintptr_t t = 0; uint32_t n = 0;
    LireTab(r + REC_AMELIOR, t, n, 64);
    float f = 0;
    memcpy(&f, &duree[0], 4);
    printf("[RECETTE]\tB\t%s\t%s\t%s\t%u/%u(%.2f)\t%u\t%u\n", g_scn, nom, aff, duree[0], duree[1], f,
        traitement, n);

    // Ameliorations : liens (0x10) vers d'autres recettes.
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t g[16] = {};
        char u[128];
        SafeRead((void*)(t + (uintptr_t)i * 0x10), g, 16);
        NomRecette(tabRec, nRec, g, u, sizeof(u));
        printf("[RECETTE]\tU\t%s\t%s\t%s\n", g_scn, nom, u);
    }

    // Produits (polymorphes) : seule la classe ressource est decodee.
    if (LireTab(r + REC_PRODUITS, t, n, 32))
        for (uint32_t i = 0; i < n; ++i) {
            uintptr_t p = 0;
            if (!SafeRead((void*)(t + (uintptr_t)i * 8), &p, 8) || p <= 0x10000) continue;
            if (!GetClassNameFP(p, cls, sizeof(cls))) strcpy_s(cls, "?");
            uint8_t g[16] = {};
            int32_t q = 0;
            char res[64] = "-";
            if (strstr(cls, "ResourceProduct")) {
                SafeRead((void*)(p + PROD_RESSOURCE), g, 16);
                SafeRead((void*)(p + PROD_QUANTITE), &q, 4);
                NomRessource(g, res, sizeof(res));
            }
            printf("[RECETTE]\tO\t%s\t%s\t%s\t%s\t%d\n", g_scn, nom, cls, res, q);
        }

    // Cout : Dynarray<FrostpunkResourceAmount>.
    if (LireTab(r + REC_COUT, t, n, 32))
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t g[16] = {};
            int32_t q = 0;
            char res[64];
            SafeRead((void*)(t + (uintptr_t)i * 0x14), g, 16);
            SafeRead((void*)(t + (uintptr_t)i * 0x14 + 0x10), &q, 4);
            NomRessource(g, res, sizeof(res));
            printf("[RECETTE]\tI\t%s\t%s\t%s\t%d\n", g_scn, nom, res, q);
        }
}

} // namespace

void RecettesReleve()
{
    g_scn = g_run.valid ? g_run.scenario : "?";
    uintptr_t cfg = (uintptr_t)GetModuleHandleW(NULL) + OFF_CONFIG_RECETTES;
    char cls[96] = "?";
    GetClassNameFP(cfg, cls, sizeof(cls));
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(cfg + CFG_RECETTES, tab, n, 1024)) {
        printf("[RECETTE] config illisible (classe %s)\n", cls);
        return;
    }
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t r = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &r, 8) && r > 0x10000) ReleverRecette(r, tab, n);
    }
    printf("[RECETTE] %u recette(s) relevee(s) (config %s, scenario %s)\n", n, cls, g_scn);
}
