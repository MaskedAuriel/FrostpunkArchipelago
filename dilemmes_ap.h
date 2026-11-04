// dilemmes_ap.h
// ---------------------------------------------------------------------------
// Catalogue des dilemmes Archipelago : bulles en ville (pieges, evenements)
// avec plusieurs choix. La mecanique (copie du dilemme du jeu, creation,
// resolution) est dans evenements.cpp ; ICI on ne fait que DECLARER.
//
// POUR AJOUTER UN DILEMME : une entree dans DILEMMES_AP (dilemmes_ap.cpp),
// puis DilemmeAPDemander("son_id") depuis n'importe ou (touche, reseau...).
// Tous les textes sont affiches EN JEU : en anglais, ASCII.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

constexpr int MAX_CHOIX_DILEMME_AP = 4;

struct ChoixDilemmeAP {
    const char* texte;          // texte du bouton ; nullptr = fin de la liste des choix
    const char* ressource;      // "coal", "steel", "wood", "rawfood", "ration", "steamcore"... ; nullptr : aucune
    int         quantite;       // SIGNEE : -50 = la ville perd 50, +20 = elle gagne 20
    void      (*effetSpecial)(); // effet en plus (citoyens, technos...) ; nullptr : aucun
};

struct DilemmeAP {
    const char*    id;             // identifiant unique, ex. "trap_coal"
    const char*    titre;          // titre du panneau
    const char*    sousTitre;      // petite ligne au-dessus du titre ("New Arrivals" chez le jeu)
    const char*    texte;          // description
    const char*    icone;          // icone de la bulle : nom dans ARCHIPELAGO_ICONS_TECH ("icon_trap"...)
    const char*    titrePopup;     // titre de la notification affichee apres le choix
    int            choixSiIgnore;  // choix applique si la bulle expire sans reponse (inevitable)
    ChoixDilemmeAP choix[MAX_CHOIX_DILEMME_AP];
};

// Definition par identifiant, ou nullptr.
const DilemmeAP* DilemmeAPParId(const char* id);

// N'importe quel thread : met le dilemme en file ; il est cree sur le thread
// du jeu au prochain passage de DilemmesPompe (jeu hors pause, partie prete).
bool DilemmeAPDemander(const char* id);

// Thread du jeu (hook du setter de ressources) : cree les dilemmes en file.
void DilemmesPompe();
