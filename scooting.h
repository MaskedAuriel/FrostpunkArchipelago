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
int ExpeditionSelectionnee();

void ScootingInit();        // objets du scouting ; appele par le tick a chaque nouvelle partie
void ScootingTick();        // une fois par seconde, APRES PartieTick (mod_common)
int  ScootingLocationPourCleSite(const char* cle);

void ScootingAppliquerTresors();
void ScootingSubstituerContenuSites();
void ScootingChargerEnAttente();
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