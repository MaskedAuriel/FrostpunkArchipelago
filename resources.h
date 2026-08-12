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
bool Res_Add(const char* name, int delta);
