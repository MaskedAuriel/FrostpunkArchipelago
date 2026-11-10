// evenements.h
// ---------------------------------------------------------------------------
// Evenements du jeu (FrostpunkGameEventSystem) : bulles en ville, dilemmes.
// Etape 1 : journal de chaque evenement invoque ; pour ceux dont le contexte
// est un groupe Archipelago arrive en ville (bulle gardee, decision de
// l'auteur), releve detaille de la chaine d'effets jusqu'au dilemme.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// Au demarrage : hook de l'invocation d'un evenement (FUN_1417b9b70).
void InstallHooksEvenements(uintptr_t modBase);

// Nom de l'evenement en cours d'execution (vide hors evenement ; thread du jeu).
extern char g_nomEvtEnCours[128];

// Thread du jeu : les evenements invoques avec cette entite pour contexte
// sont releves en detail (effets, dilemme, choix). 16 entites au plus.
void EvenementsSuivreEntite(uintptr_t entite);

// Thread du jeu : pendant un evenement dont le contexte est 'entite', la
// creation du dilemme 'orig' cree 'remplacant' a la place (hook de
// FUN_1417cde10). Une regle par entite (la derniere gagne).
void EvenementsRemplacerDilemme(uintptr_t entite, uintptr_t orig, uintptr_t remplacant);

// --- Dilemmes Archipelago (catalogue : dilemmes_ap.h) -----------------------
// Bulle en ville (copie de "[Beacon]S: Survivors group", sans aucun effet du
// jeu) dont chaque choix applique l'effet declare. Ignorer la bulle applique
// le choix 'choixSiIgnore' : un piege est inevitable.
struct DilemmeAP;

// Thread du jeu uniquement (DilemmesPompe). Renvoie l'identifiant du dilemme
// cree, ou -1. 'sousTitre' (copie) remplace celui du catalogue s'il est non
// vide ; il est garde dans ap_dilemmes.txt pour le rechargement.
int DilemmeAPCreer(const DilemmeAP& def, const char* sousTitre = nullptr);

// Textes des cles @AP/Dil/<slot>/T (titre), /X (texte), /C<i> (choix i),
// /S (sous-titre d'arc), pour hkTranslate ; nullptr si le slot est libre.
const char* DilemmePiegeTexte(int slot, char champ, int indice);

// Tuile (UITextureTile, 0x18 octets) de l'icone 'icone' (nom de
// ARCHIPELAGO_ICONS_TECH) dans l'atlas des bulles (UI\Tiles\Icons.dds,
// entree 2350) : texture et nombre de cases repris du marqueur de
// "Survivors group". false si indisponible.
bool TuileAtlasBulles(const char* icone, uint8_t* tuile);

// Sauvegarde AP des bulles (mods\ap_dilemmes.txt), thread du jeu, appelee par
// les hooks du chargement (mod_common) : 0 = entree de LoadGame, 1 = fin de
// la lecture de la sauvegarde, 2 = sortie de LoadGame. Les bulles AP en
// attente recoivent de nouveau notre copie (textes, icone, effets AP).
void DilemmesAPChargement(int etape);

// Dilemme du jeu par son lien (GUID de 16 octets), ou 0.
uintptr_t DilemmeParGuid(const uint8_t* guid);

// Copie d'un FrostpunkDilemmaEntry (et de ses choix) dont le titre
// (+0xD8), la description (+0xE8) et le texte de chaque choix (+0xA8) sont
// remplaces par les cles donnees (nullptr : champ d'origine garde).
// Memoire du moteur, jamais liberee. 0 en cas d'echec.
uintptr_t DilemmeCloner(uintptr_t orig, const char* cleTitre, const char* cleDescription, const char* cleChoix);

// Releve unique (journal [REFUG]) des dilemmes du jeu dont le nom contient
// 'motif' : textes, choix, effets initiaux et effets / resultats de chaque
// choix. Lecture seule de la configuration, tout thread. Sert a retrouver
// l'arrivee des refugies de l'apres-005 d'A New Home (2026-11-04).
void ReleverDilemmesParNom(const char* motif);
// Tous les dilemmes a 2 choix ou plus, textes traduits : lignes [DILTAB] (2026-11-10).
void ReleverDilemmesAChoix();
// Crise du charbon (2026-11-10) : bulle "[coal crisis] Fear over coal supply" creee d'office
// au debut de "Prepare for the storm" (sauf check de Build Coal Thumpers deja fait).
void EvenementsForcerCriseCharbon();

// Classe de l'entite et chemin de son CompoundTemplate (journal).
void DecrireEntiteContexte(uintptr_t entite, char* out, int maxlen);

// Detail d'un dilemme du jeu au journal [REFUG] (textes, choix, effets et
// resultats de chaque choix). Lecture seule, tout thread.
void ReleverDilemmeDetail(uintptr_t dilemme);

// Arrivee en ville d'un convoi AP en cours (groupes.cpp) : les bulles du jeu
// creees pendant ce temps sont refusees (2026-11-07).
void EvenementsArriveeConvoiAP(bool enCours);

// Resout (choix 0) les bulles de la porte des refugies en attente. Thread du
// jeu, appele par DilemmesPompe.
void DilemmesResoudreEnAttente();

// Une bulle de la porte des refugies a-t-elle ete creee (depuis la derniere
// remise a zero) ?
bool EvenementsBullePorteCreee(bool remettreAZero);

// Invoque l'evenement du jeu 'nom' (contexte par defaut, force). Thread du
// jeu. false si introuvable ou systeme des evenements pas encore vu.
bool EvenementsInvoquerParNom(const char* nom);
