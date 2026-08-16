#pragma once
#include <cstdint>

// Nature du contenu d'un site : check envoye au serveur, ou item local.
struct SitePayload {
    const char* scenario;
    const char* site;
    int         locationId;      // remplace type + value
};

// Trace verbeuse (une ligne par expedition et par tour).
extern volatile bool g_traceScout;
int ScootingListePortee(wchar_t* out, int maxChars);
int ExpeditionSelectionnee();

void ScootingInit();        // un seul scan ; appele automatiquement par le tick
void ScootingTick();        // a appeler une fois par seconde
void ScootingDumpCargo();   // diagnostic : contenu des cargaisons
void ScootingDumpSiteCargo();
void ScootingDumpTemplate(const char* nomCherche);
void ScootingDumpTreasure(const char* nomCherche);
void ScootingProbeTreasure(const char* nomCherche);
void ScootingDumpTreasureArrays(const char* nomCherche);
void ScootingResoudreTreasure(const char* nomCherche);
void ScootingTrouverParGuid(uint64_t g0, uint64_t g1);
bool ScootingSetTreasureResource(const char* nomSite, int element, const char* nomRessource);
int  ScootingLocationPourCleSite(const char* cle);

// Reapplique les substitutions de tresor (a rejouer a chaque session).
void ScootingAppliquerTresors();
void ScootingVerifierTresors();
void TracerCargaison(uintptr_t comp, const char* prefixe);
void ScootingDumpRessources();
void ScootingSubstituerContenuSites();
void ScootingChargerEnAttente();
void ScootingReparerCargaisons();
void ScootingSauverEnAttente();
bool InjecterPorteur(uintptr_t comp, uintptr_t def, int64_t quantite);
uint32_t CargaisonTotale(uintptr_t comp);
void ScootingProbeConteneur();
void ResoudreSelectionSystem();
void MettreAJourListePortee();
void ScootingFournirContenuSites();
void ScootingEtatApresInit();
void PatcherGabarits();
void CacherGabarits();

extern volatile bool g_apDonneesActif;
extern bool g_defEnregistree;