// ---- ap_data.h ----
#pragma once
#include <cstdint>

// Check porte par un scout (sites d'exploration) : id local (100000 + numero
// de la location AP 7747300000 + n, scooting.cpp) et texte affiche.
struct ApCheck {
    int  id;                  // identifiant de location, fixe cote mod
    char joueur[64];          // a qui appartient l'objet trouve ici
    char description[160];    // nom du check : "<joueur> - <item>" (scout), sinon nom de la location
};

void            ApSetCheck(int id, const char* joueur, const char* description);
// Le texte est repris du scout (ApNomDuCheckLocation, archipelago.h) des
// qu'il est connu : il suit la session et la connexion.
const ApCheck* ApCheckById(int id);
const wchar_t* ApDescriptionW(int id);   // version UTF-16 pour hkTranslate
const wchar_t* ApJoueurW(int id);
