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
// Nom du check porte par cette techno (TEMPORAIRE : nom interne de la techno).
void AP_NomDuCheck(uint64_t hash, char* out, int maxlen);
// Etat de la progression, pour verifier apres plusieurs tirages.
void ReceiveRandomProgressive();
void DumpProgression(); 
// Debloque la techno suivante de l'arbre. nomTech (optionnel) recoit son nom.
bool OnProgressiveReceived(int tree, const char** nomTech = nullptr);
int  ArbreProgressifAuHasard();   // -1 si tous les arbres sont epuises

// Sauvegarde locale des technos (mods\ap_technos_<GameId>.txt), prioritaire
// sur le serveur. Boucle du mod, tick d'une seconde, apres CaptureRunState :
// relit et reapplique l'etat a chaque nouvelle partie.
void TechPersistanceTick();
