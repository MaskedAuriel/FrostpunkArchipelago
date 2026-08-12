// ---- ap_data.h ----
#pragma once
#include <cstdint>

// Ce que le serveur nous apprend sur une location, a la connexion.
struct ApCheck {
    int  id;                  // identifiant de location, fixe cote mod
    char joueur[64];          // a qui appartient l'objet trouve ici
    char description[160];    // ce qu'on affiche, ex "[AP] Envoi : Coal Mine"
    int  arbreTest;           // TEMPORAIRE : progressive livree localement, -1 si aucune
};

void            ApSetCheck(int id, const char* joueur, const char* description, int arbreTest = -1);
const ApCheck* ApCheckById(int id);
const wchar_t* ApDescriptionW(int id);   // version UTF-16 pour hkTranslate
void            ApDumpChecks();
void            ApChargerChecksDeTest();  // TEMPORAIRE, en attendant le serveur
const wchar_t* ApJoueurW(int id);