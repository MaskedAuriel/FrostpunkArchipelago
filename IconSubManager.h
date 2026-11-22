#pragma once
#include <cstdint>
#include "known_techs.h"
#include "mod_common.h"




// Atlas substitues. Reperes par le HASH DE NOM de textures-s3.idx (u32 en
// tete d'entree, MurmurHash2 du nom en .texture), stable d'une version du jeu
// a l'autre ; les numeros d'entree (Epic 2680 / 3037 / 1179 / 2350) ne le
// sont pas (Steam : 2692 / 3055 / 1186 / 2361). Le fichier est choisi par
// FichierAtlasVersion (Steam : ap_buildings_steam.gz, atlas des batiments
// different ; les trois autres blobs sont identiques). 2026-11-07.
struct SubDecl { uint32_t hashNom; unsigned long entreeEpic; const wchar_t* fichier; const char* libelle; };
static const SubDecl SUBSTITUTIONS[] = {
    { 0xA26CE979, 2680, L"ap_icons_ressources.gz", "atlas icones ressources" },
    { 0xB97BA46D, 3037, L"ap_buildings.gz", "atlas Batiments recherche" },
    { 0x476DCA10, 1179, L"ap_icons_recherche.gz", "atlas icones de recherches" },
    // ui/tiles/icons.texture (UI\Tiles\Icons.dds) : atlas des bulles de
    // dilemme, DXT5 2048 x 2048, 16 x 16 cases de 128 px (2026-10-28).
    { 0x8F231C35, 2350, L"ap_icons_bulles.gz", "atlas icones des bulles" },
};

struct TechIcons {
    const char* nom;        // "icon_vanilla", "icon_trap", ...
    int32_t batCol, batRow;   // case dans l'atlas des miniatures (#3037)
    int32_t techCol, techRow;  // case dans l'atlas des technos (#1179)
    // Case dans l'atlas des bulles (#2350, UI\Tiles\Icons.dds), cases libres
    // de la ligne 1. C'est aussi l'atlas des icones du livre des lois.
    int32_t bulleCol, bulleRow;
};

// Cases de l'atlas de l'arbre (#1179) verifiees le 2026-11-01 sur le blob
// deploye (mods\ap_icons_recherche.gz) : (0,2) important, (0,3) filler,
// (0,4) done (vert), (0,5) trap (orange), (0,6) progressive (violet),
// (0,7) vanilla. Avant : progressive / trap / done en (0,5) / (0,4) / (0,6),
// d'ou un « done » violet signale par l'auteur.
static const TechIcons ARCHIPELAGO_ICONS_TECH[] = {
    { "icon_vanilla",     15, 3,  0, 7,   7, 1 },
    { "icon_filler",      15, 2,  0, 3,   8, 1 },
    { "icon_important",   15, 1,  0, 2,   9, 1 },
    { "icon_progressive", 15, 4,  0, 6,  10, 1 },
    { "icon_trap",        15, 5,  0, 5,  11, 1 },
    { "icon_done",        15, 0,  0, 4,  12, 1 },
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
// Icones AP de l'arbre des technos, posees pendant la construction de
// l'interface seulement (hooks +1BC5810, +1BE67F0, +1BB2B60, +1BB2D30).
void InstallHooksIconesTechnos(uintptr_t modBase);
// Fin de recherche (thread du jeu) : icone et nom du noeud de cette techno
// dans l'arbre deja construit, sans attendre sa reconstruction.
void IconesTechnoRafraichirNoeud(uintptr_t tech);
// Panneau de l'arbre des technos (vu au dernier noeud construit), 0 sinon :
// noeuds en +0x1A8 (enregistrements de 0x80, poignee du noeud en +0x10),
// nombre en +0x1B0. Legende de l'arbre (historique_ap.cpp).
uintptr_t IconesTechnoPanneau();
// Rafraichit tous les noeuds de l'arbre deja construit (etat AVAILABLE /
// NOT_AVAILABLE) : apres la reception d'une techno deja recherchee (2026-11-10).
void IconesTechnoRafraichirArbre();
extern TechnoAP g_technosAP[256];
extern int      g_nTechnosAP;