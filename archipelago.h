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

// --- Jeu -> serveur ----------------------------------------------------------

// Recherche terminee : location check de cette techno.
void SendResearchCheck(uint64_t techHash);

// Check porte par un scout et ramene en ville (id de SITE_PAYLOADS / ApCheck).
void SendLocationCheck(int locationId);
