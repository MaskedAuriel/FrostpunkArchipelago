#pragma once
// ---- lois.h ----
#include <cstdint>
// Lois du jeu (en interne : "sociotechs").
//   FrostpunkSocietyConfig (instance statique base + 0x4070460) :
//     +0x78 categories (arbres), +0xB0 lois (tableaux de pointeurs, compte +8)
//   FrostpunkScenariosConfig (instance statique base + 0x2C34490) :
//     +0x88 scenarios, +0x210 reglages de lois par scenario
//   Systeme des lois : *(base + 0x408FBD0), etat par loi en +0xD8

// Releve complet (F4) : arbres, lois, prerequis, reglages des scenarios et
// etat courant. Lecture seule.
void DiagLois();

// F4 : demande le releve, execute par LoisPompe sur le thread du jeu (le
// parcours des evenements resout des liens de dilemme par une fonction du
// jeu). Jeu hors pause.
void LoisDemanderReleve();

// Thread du jeu (hook du setter de ressources, a cote des autres pompes) :
// execute le releve demande (LoisDemanderReleve).
void LoisPompe();

// Appelee par hkTranslate pour chaque traduction (cle sans '@') : memorise le
// texte affiche des cles "Generated/SocietyConfig/Sociotechs/.../Display name",
// pour que F4 donne le nom des lois tel que le jeu l'affiche.
void LoisNoterTraduction(const char* cle, uintptr_t valeur);

// Nom affiche de la loi 'loi' (FrostpunkSociotechEntry), s'il a deja ete
// traduit par le jeu dans cette session. false sinon.
bool LoisNomAffiche(uintptr_t loi, char* out, int taille);

// Parcours de tous les evenements (repertoires base + 0x406DDA8, effets +0x58,
// effets imbriques, dilemmes et leurs choix), comme le releve des unlocks de
// F4 : 'visiteur' est appele sur chaque objet "Frostpunk..." atteint, avec
// l'evenement (FrostpunkGameEventEntry) qui le porte. Thread du jeu (resout
// les liens de dilemme par une fonction du jeu).
typedef void (*VisiteurEffet_t)(uintptr_t obj, const char* classe, uintptr_t evt,
    const char* nomEvt, const char* nomRep);
void LoisParcourirEvenements(VisiteurEffet_t visiteur);

// Meme parcours sur un seul tableau d'effets {ptr, compte} (ex. effets d'une
// quete) ; 'visiteur' recoit evt = 0, nomEvt = 'nom', nomRep = 'rep'.
void LoisParcourirEffets(uintptr_t adrTableau, VisiteurEffet_t visiteur, const char* nom, const char* rep);

// Declencheurs et conditions d'un evenement, en lignes prefixees par 'prefixe'.
void LoisDecrireEvenement(uintptr_t evt, const char* prefixe);

// Liste d'exigences / de conditions {ptr, compte} decrite en une ligne.
void LoisDecrireExigences(uintptr_t adrTableau, char* out, int taille);
