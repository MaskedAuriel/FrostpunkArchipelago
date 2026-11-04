#pragma once
#include <cstdint>
#include "MinHook.h"
#include "MessageManager.h"
// Nature du contenu d'un site : check envoye au serveur, ou item local.
struct SitePayload {
    const char* scenario;
    const char* site;
    int         locationId;      // remplace type + value
};

// Trace verbeuse (une ligne par expedition et par tour).
int ScootingListePortee(wchar_t* out, int maxChars);
// Liste de droite (groupes.cpp, hook du remplissage de l'inventaire) :
// expedition dont la case est en cours de remplissage, 0 apres.
void ScootingExpeditionAffichee(uintptr_t comp);
// Liste pour la traduction de l'item AP : expedition affichee si elle est
// posee (*exp = son indice, 0..15), sinon liste du tick (*exp = -1).
int ScootingListePourAffichage(wchar_t* out, int maxChars, int* exp);
int ExpeditionSelectionnee();

void ScootingInit();        // objets du scouting ; appele par le tick a chaque nouvelle partie
// Au demarrage : table des sites porteurs d'un check = toutes les locations
// de site de known_ap_ids.h (descriptions dans ap_data).
void ScootingPayloadsInit();
void ScootingTick();        // une fois par seconde, APRES PartieTick (mod_common)
int  ScootingLocationPourCleSite(const char* cle);

void ScootingAppliquerTresors();
void ScootingSubstituerContenuSites();
void ScootingChargerEnAttente();

// Thread du jeu, sortie de FrostpunkGame::LoadGame (hook de mod_common) :
// cargaisons a definition morte videes avant le 1er tick, items AP en
// attente reinjectes si le porteur existe deja.
void ScootingApresChargement();
void ScootingReparerCargaisons();
void ScootingSauverEnAttente();
bool InjecterPorteur(uintptr_t comp, uintptr_t def, int64_t quantite);
uint32_t CargaisonTotale(uintptr_t comp);
void MettreAJourListePortee();
bool EnJeu();
extern volatile bool g_apDonneesActif;
extern bool g_defEnregistree;
extern volatile bool g_pret;
void ScootingFournirContenuSites();
int ExpeditionVisee();
// F9 (thread du jeu) : une ligne "[SXLS]" par site d'exploration de la partie
// (noms internes et affiches, contenu), pour outils\quetes_excel.py.
void ScootingReleverSites();
// Nom interne d'un gabarit de site (cle de SITE_PAYLOADS), "" si illisible.
bool ScootingNomDuGabarit(uintptr_t tpl, char* out, int maxlen);
// Nom interne du gabarit d'un GUID (pool des gabarits, sans chargement).
bool ScootingNomGabaritParGuid(const uint8_t* guid, char* out, int taille);
// F9 (thread du jeu) : sites de la partie designes par le nom d'entite pointe
// par 'adrNom' (NameString, ex. "DefaultRandomizationPoint_037").
void ScootingSitesDuNom(uintptr_t adrNom, char* out, int taille);
// F9 (thread du jeu) : toutes les tables de tirage des sites (tous scenarios)
// et les sites possibles de chacune, lignes "[TXLS]".
void ScootingReleverTablesDeSites();
// Au demarrage : hooks des marqueurs de site, du tirage fixe des sites, de la
// repartition de leur contenu et des reglages du scenario.
void InstallHooksExpeditions(uintptr_t modBase);