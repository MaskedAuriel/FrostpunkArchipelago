#pragma once
// ---- lois_ap.h ----
// Lois Archipelago : signer une loi envoie son (ses) check(s) ; son EFFET
// (modificateurs + effets de la loi) attend l'item progressif de son arbre.
// Tout le reste de la signature (etat, frustration, delai, espoir,
// alignements, commentaires) reste immediat (decision de l'auteur, option A).
#include <cstdint>

// Hooks de la signature (+0x16FD350) et des effets (+0x16FDBC0).
void InstallHooksLoisAP(uintptr_t modBase);

// Thread du jeu (hook du setter de ressources, apres LoisPompe) : relecture
// de la sauvegarde locale au changement de partie, effets en attente
// appliques quand leur item est la, test F5.
void LoisAPPompe();

// Sortie de LoadGame, lecture reussie (thread du jeu) : la sauvegarde locale
// sera relue et comparee a la date de la sauvegarde du jeu.
void LoisAPApresChargement();

// Un item progressif de l'arbre 'arbre' (nom, voir le journal [LOIAP]).
// Tout thread ; l'effet est applique par LoisAPPompe. 'rang' recoit le
// nombre d'items de l'arbre apres reception. false si l'arbre est inconnu
// ou la partie pas encore relue.
bool LoisAPRecevoir(const char* arbre, int* rang);

// F5 : un item pour l'arbre de la loi en attente la plus ancienne (sinon de
// la derniere loi signee), sur le thread du jeu.
void LoisAPDemanderTest();
