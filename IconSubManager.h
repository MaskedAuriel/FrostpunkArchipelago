#pragma once
#include <cstdint>
#include "known_techs.h"
#include "mod_common.h"




struct SubDecl { unsigned long off; const wchar_t* fichier; const char* libelle; };
static const SubDecl SUBSTITUTIONS[] = {
    { 2680, L"ap_icons_ressources.gz", "atlas icones ressources" },
    { 3037, L"ap_buildings.gz", "atlas Batiments recherche" },
    { 1179, L"ap_icons_recherche.gz", "atlas icones de recherches" },
    // ui/tiles/icons.texture (UI\Tiles\Icons.dds) : atlas des bulles de
    // dilemme, DXT5 2048 x 2048, 16 x 16 cases de 128 px (2026-10-28).
    { 2350, L"ap_icons_bulles.gz", "atlas icones des bulles" },
};

struct TechIcons {
    const char* nom;        // "icon_vanilla", "icon_trap", ...
    int32_t batCol, batRow;   // case dans l'atlas des miniatures (#3037)
    int32_t techCol, techRow;  // case dans l'atlas des technos (#1179)
    // Case dans l'atlas des bulles (#2350, UI\Tiles\Icons.dds), cases libres
    // de la ligne 1. C'est aussi l'atlas des icones du livre des lois.
    int32_t bulleCol, bulleRow;
};

static const TechIcons ARCHIPELAGO_ICONS_TECH[] = {
    { "icon_vanilla",     15, 3,  0, 7,   7, 1 },
    { "icon_filler",      15, 2,  0, 3,   8, 1 },
    { "icon_important",   15, 1,  0, 2,   9, 1 },
    { "icon_progressive", 15, 4,  0, 5,  10, 1 },
    { "icon_trap",        15, 5,  0, 4,  11, 1 },
    { "icon_done",        15, 0,  0, 6,  12, 1 },
    // Variante pour l'icone des groupes (carte, liste a droite) : vanilla
    // reduite de 15 % (demande de l'auteur), case libre (8,4) de l'atlas des
    // bulles ; pas de case propre dans les autres atlas (celles de vanilla).
    { "icon_vanilla_groupe", 15, 3,  0, 7,   8, 4 },
};
struct TechnoAP {
    uint64_t         hash;
    const TechIcons* icone;
    uintptr_t        entree;      // resolue une fois par ResoudreTechnosAP
    bool             miniature;
    char             batiment[64];
};



void RafraichirIconeTechno(TechnoAP& t);
bool SetTechIconTile(uint64_t hash, const TechIcons* icone);
bool AppliquerIconeTechno(uint64_t hash, const TechIcons* icone);
void AppliquerIconeBatiment(const char* nomBat, int32_t col, int32_t row);
void RestaurerIconesBatiments();
void ApAjouterTechno(uint64_t hash, const char* nomIcone,
    const char* batiment = nullptr);
void ApChargerTechnosDeTest();
void ResoudreTechnosAP();
void RemplirCacheGabarits(uintptr_t* objs, int n);
uintptr_t PremierGabaritBat();   // 0 si le cache est vide
int NombreGabaritsBat();
bool GabaritBatEnCache(uintptr_t tpl);
void InstallCaptureGabarits(uintptr_t modBase);   // hook du ctor +0x1916F90
int  GabaritsBatCaptures(uintptr_t* out, int maxOut);   // gabarits vivants
void RapportCapturesGabarits();
bool EstMiniature(uintptr_t te, char* batOut, int maxlen);
static bool NomBatimentDepuisCle(const char* cle, char* out, int maxlen);
void DiagnostiquerTechno(const char* nomCherche);
void RafraichirToutesLesIcones();

extern TechnoAP g_technosAP[256];
extern int      g_nTechnosAP;