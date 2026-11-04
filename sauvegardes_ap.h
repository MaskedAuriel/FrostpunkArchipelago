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
// Journal [SAV].
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

void InstallHooksSauvegardes(uintptr_t modBase);
