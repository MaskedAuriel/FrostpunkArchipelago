// menu_ap.h
// ---------------------------------------------------------------------------
// Ecran de connexion Archipelago dans le menu principal : trois champs de
// saisie (serveur, slot, mot de passe) et un bouton "Connect", ajoutes au
// panneau principal du menu (FrostpunkUIMainMenuMainPanel) a son init.
//
// Les elements sont des CLONES d'elements du jeu, instancies depuis leur
// recette d'interface : le bouton depuis EXIT_BUTTON du menu, les champs
// depuis un UITextInput d'un panneau en jeu (sauvegarde, nom de la ville).
// Valeurs gardees dans mods\ap_connexion.txt. Le clic sur "Connect" passe les
// trois chaines a RequestConnection (archipelago.h).
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

void InstallMenuAP(uintptr_t modBase);
