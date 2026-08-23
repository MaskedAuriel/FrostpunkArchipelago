#pragma once
#include <cstdint>

// Nature du contenu d'un site : check envoye au serveur, ou item local.
struct SitePayload {
    const char* scenario;
    const char* site;
    int         locationId;      // remplace type + value
};

// Trace verbeuse (une ligne par expedition et par tour).
int ScootingListePortee(wchar_t* out, int maxChars);
int ExpeditionSelectionnee();

void ScootingInit();        // un seul scan ; appele automatiquement par le tick
void ScootingTick();        // a appeler une fois par seconde
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