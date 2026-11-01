// resources.h
// ---------------------------------------------------------------------------
// Lecture / modification des ressources du jeu, via le hook du setter
// (0x173FE40) qui capture en continu le contexte de chaque ressource.
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"

void InstallResourceHooks(uintptr_t modBase);

// Valeur actuelle d'une ressource par nom ("coal", "wood", ...). -1 si inconnue.
int  Res_Get(const char* name);

// Ajoute delta a une ressource par nom. false si le contexte n'est pas capture.
// Le credit est applique par le hook, sur le thread du jeu, au prochain appel
// de ModifyResource (quelques instants plus tard).
bool Res_Add(const char* name, int delta);

// Definition (entree) d'une ressource par nom, ancree a l'init de la partie :
// +0x28 cle du nom, +0x98 tuile d'icone (comme la cargaison d'un scout). 0 si
// inconnue.
uintptr_t Res_Definition(const char* name);

// GUID (16 octets, ordre memoire) d'une ressource connue. false si inconnue.
bool Res_Guid(const char* name, uint8_t* out16);

// A appeler a chaque chargement de partie : les contextes captures deviennent faux.
void Res_OublierContextes();

// Ancre toutes les ressources connues depuis le conteneur de l'economie (a
// l'init de la partie). Renvoie le nombre ancre.
int  Res_InitDepuisEconomie();
