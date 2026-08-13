#pragma once
#include <cstdint>

// Definition de ressource fabriquee par le mod, servant de support visuel
// aux items Archipelago. Clonee depuis une definition existante pour heriter
// de tous les champs qu'on ne connait pas.
bool       ApPorteurInit();                    // une fois par session
uintptr_t  ApPorteurDef();                     // 0 tant que non initialisee
bool       ApPorteurEstNotreGuid(uintptr_t guidPtr);
const char* ApPorteurNomInterne();
bool ApPorteurEnregistrer();

extern volatile bool g_apPorteurActif;
bool EconomieValide();