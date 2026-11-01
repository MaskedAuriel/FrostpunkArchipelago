// archipelago.h
// ---------------------------------------------------------------------------
// Surface de communication entre la couche Archipelago (le serveur, plus tard
// la classe reseau) et le jeu. Chaque fonction est un enchainement lisible :
// verifier que la partie est prete, appliquer l'effet, afficher la popup.
//
// La couche reseau n'appellera QUE ces fonctions : elle ne connait ni les
// hooks, ni les hash, ni les etats memoire du jeu.
//
//   Serveur -> jeu : Receive*   (un item arrive)
//   Jeu -> serveur : Send*      (un check est valide en jeu)
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// --- Serveur -> jeu ----------------------------------------------------------

// "Progressive Tree <tree>" : debloque la techno suivante de l'arbre. Toute
// techno recue passe par la : un arbre a un seul niveau reste un arbre.
// false si la partie n'est pas prete, l'arbre hors bornes ou epuise.
bool ReceiveProgressive(int tree, const char* fromPlayer);

// Ressource ("coal", "wood", "steel", "steamcore", "rawfood", "ration"...).
// Le credit est applique quelques instants plus tard, sur le thread du jeu.
// false si la partie n'est pas prete ou la ressource inconnue / jamais vue.
bool ReceiveResource(const char* resource, int amount, const char* fromPlayer);

// "Progressive Law <tree>" : l'arbre de lois 'tree' (nom du journal [LOIAP])
// recoit un item ; l'effet de la loi signee au rang correspondant est
// applique quelques instants plus tard, sur le thread du jeu.
// false si la partie n'est pas prete ou l'arbre inconnu.
bool ReceiveLawProgressive(const char* tree, const char* fromPlayer);

// "Temperature Rise" : rend 'levels' niveaux de temperature (10 degres
// chacun), les montees scriptees etant retirees. Applique quelques instants
// plus tard, sur le thread du jeu. false si la partie n'est pas prete.
bool ReceiveTemperatureRise(int levels, const char* fromPlayer);

// "Scenario Unlock <scenario>" : le scenario (nom interne : DEFAULT,
// TechHaven, REFUGEES, WINTERHOME, BUILDERS, Reconquest) devient lancable au
// menu principal (scenarios_ap.cpp). Valable hors partie ; popup seulement si
// une partie est prete. false si le nom est inconnu.
bool ReceiveScenarioUnlock(const char* scenario, const char* fromPlayer);

// --- Jeu -> serveur ----------------------------------------------------------

// Recherche terminee : location check de cette techno.
void SendResearchCheck(uint64_t techHash);

// Check porte par un scout et ramene en ville (id de SITE_PAYLOADS / ApCheck).
void SendLocationCheck(int locationId);

// Loi signee : check de rang 'rank' de l'arbre de lois 'tree'.
// 'effectPending' : l'effet de la loi attend l'item de l'arbre.
void SendLawCheck(const char* tree, int rank, const char* lawName, bool effectPending);
// Quete de l'histoire du scenario accomplie (quetes.cpp, hook de la fin de
// quete FUN_1416d22a0 ; known_quests.h). Popup "Check sent".
void SendQuestCheck(const char* questName, const char* displayName);

// Bouton "Connect" du menu principal (menu_ap.cpp) : chaines UTF-8 saisies.
// Appele sur le thread du jeu, hors partie. TODO reseau : ouvrir la connexion.
void RequestConnection(const char* server, const char* slot, const char* password);
