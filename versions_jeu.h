#pragma once
// versions_jeu.h -- Versions de Frostpunk.exe prises en charge (2026-11-07).
//
// Toutes les adresses du mod (fonctions, globaux) ont ete relevees sur l'exe
// Epic (Ghidra, base 0x140000000) et restent ECRITES EN RVA EPIC dans le code.
// Adr(rvaEpic) les traduit pour l'exe en cours :
//   - Epic  (lie le 18/08/2021, 0x611CFD2D) : identite ;
//   - Steam (lie le 08/10/2025, 0x68E665D9) : table VERSION_STEAM
//     (versions_jeu.cpp), etablie par appariement automatique des deux exes
//     (fonctions identiques via .pdata, references exactes aux globaux) et
//     verifiee dans Ghidra pour les cas douteux ;
//   - exe inconnu : le mod ne pose AUCUN hook (PayloadThread), le jeu tourne
//     en vanilla ; une ligne au journal.
// Toute NOUVELLE adresse ajoutee au mod doit aussi entrer dans VERSION_STEAM,
// sinon Adr renvoie 0 sur Steam (journal [VERSION] au demarrage).
//
// Les offsets DANS les structures du jeu (+0x2248, +0x2A0...) ne passent pas
// par Adr : identiques dans les deux versions pour tout ce que le mod lit.

#include <cstdint>

enum class VersionJeu { Inconnue, Epic, Steam };

// Detection paresseuse (en-tete PE de l'exe charge), utilisable des
// l'initialisation statique de la DLL.
VersionJeu  VersionDuJeu();
const char* NomVersionJeu();          // "Epic", "Steam", "inconnue"
bool        VersionJeuSupportee();

// RVA Epic -> RVA de l'exe en cours. 0 si la version est inconnue ou si
// l'adresse manque a la table (note pour le journal).
uintptr_t Adr(uintptr_t rvaEpic);

// Journal : version detectee et adresses manquantes demandees jusqu'ici
// (appele par PayloadThread, une fois la console prete).
void JournalVersionJeu();

// Fichier de substitution d'un atlas pour la version en cours (Steam : atlas
// des batiments propre, son contenu differe de celui d'Epic).
const wchar_t* FichierAtlasVersion(const wchar_t* fichierEpic);
