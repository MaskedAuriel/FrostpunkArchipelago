// tech_brain.h
// ---------------------------------------------------------------------------
// La logique Archipelago : traduit les deux booleens de tech_runtime en etat
// du jeu, et reagit aux evenements (recherche terminee, item recu).
//
// MAPPING (l'etat 3 n'existe plus) :   etat jeu = received ? 2 : 0
//
//   researched | received | etat | effet | [Researched] | recherchable
//        F     |    F     |  0   | non   |     non      |    oui
//        F     |    T     |  2   | oui   |     non      |    oui   (scenario C)
//        T     |    F     |  0   | non   |     oui      |    non
//        T     |    T     |  2   | oui   |     oui      |    non
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"
#include "tech_runtime.h"
extern bool g_showApInfo;
// Applique l'etat jeu correspondant aux booleens de r.
void SyncGameState(TechRuntime* r);

// Le joueur vient de TERMINER une recherche (appele APRES l'original du jeu).
void OnResearchComplete(uint64_t hash);

// Le serveur Archipelago envoie l'item correspondant a cette techno.
void OnItemReceived(uint64_t hash);

// Envoie le location check au serveur (pour l'instant : trace console).
void AP_SendCheck(uint64_t hash);

// Test : simule la reception de TOUTES les technos.
void ReceiveAll();

void AP_LocationText(uint64_t hash, char* out, int maxlen);
// Etat de la progression, pour verifier apres plusieurs tirages.
void ReceiveRandomProgressive();
void DumpProgression(); 
bool OnProgressiveReceived(int tree);
