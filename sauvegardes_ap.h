// sauvegardes_ap.h
// ---------------------------------------------------------------------------
// Sauvegardes accessibles en Archipelago (bug du 2026-11-04) : avec le mod
// actif, seules les sauvegardes ECRITES pendant la session Archipelago en
// cours (graine + slot) apparaissent dans la liste du jeu ("Load game",
// "Continue"). Decision de l'auteur : masquer (pas refuser le chargement),
// sauvegardes de la session seulement.
//
//  - FUN_1412892f0(profil, nom, p3, p4) = ecriture d'une sauvegarde (thread
//    principal, "[GameSave] Game save created...") : le nom (sans ".save")
//    est note dans <session>\ap_sauvegardes.txt.
//  - FUN_141cdee60(profil, &liste, L"saves", L"*.save") = enumeration des
//    fichiers, appelee seulement par la liste des sauvegardes FUN_141284eb0 :
//    les noms absents de ap_sauvegardes.txt sont retires du tableau rendu.
//  - Bug du 2026-11-10 (The Arks) : la liste n'est construite qu'UNE fois, au
//    chargement du profil (FUN_1412878d0 -> cache profil +0x1B0), donc avant
//    la connexion, avec la session precedente : les sauvegardes d'une autre
//    graine restaient visibles. SauvegardesSuivreSession reconstruit ce cache
//    quand le dossier de session a change depuis la derniere liste.
// Journal [SAV].
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

void InstallHooksSauvegardes(uintptr_t modBase);

// Thread de l'interface (minuterie du menu, menu_ap.cpp), apres la connexion :
// si la session (graine + slot) differe de celle de la derniere liste, la
// liste en cache du profil est reconstruite. true si elle l'a ete.
bool SauvegardesSuivreSession();
