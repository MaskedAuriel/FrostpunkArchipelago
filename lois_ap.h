#pragma once
// ---- lois_ap.h ----
// Lois Archipelago : signer une loi envoie son (ses) check(s) ; son EFFET
// (modificateurs + effets de la loi) attend l'item progressif de son arbre.
// Tout le reste de la signature (etat, frustration, delai, espoir,
// alignements, commentaires) reste immediat (decision de l'auteur, option A).
#include <cstdint>

// Hooks de la signature (+0x16FD350) et des effets (+0x16FDBC0).
void InstallHooksLoisAP(uintptr_t modBase);

// Thread du jeu (hook du setter de ressources, apres LoisPompe) : relecture
// de la sauvegarde locale au changement de partie, effets en attente
// appliques quand leur item est la, test F5.
void LoisAPPompe();

// Sortie de LoadGame, lecture reussie (thread du jeu) : la sauvegarde locale
// sera relue et comparee a la date de la sauvegarde du jeu.
void LoisAPApresChargement();
// Date de jeu courante (horloge du jeu), 0 si illisible.
uint64_t LoisDateDeJeu();

// Un item progressif de l'arbre 'arbre' (nom, voir le journal [LOIAP]).
// Tout thread ; l'effet est applique par LoisAPPompe. 'rang' recoit le
// nombre d'items de l'arbre apres reception. false si l'arbre est inconnu
// ou la partie pas encore relue.
bool LoisAPRecevoir(const char* arbre, int* rang);

// Items deja recus par l'arbre 'arbre' dans la partie 'gid' (GameId). -1 si
// la sauvegarde locale de cette partie n'est pas relue ou l'arbre inconnu.
// Thread du jeu (apres LoisAPPompe).
int LoisAPItemsRecus(const char* arbre, const uint64_t gid[2]);

// Arbre AP (nom, comme ReceiveLawProgressive) et poids (nombre de checks)
// de la loi 'loi' (FrostpunkSociotechEntry). Thread du jeu (construit les
// arbres au besoin). false pour une loi desactivee ou inconnue.
bool LoisAPArbreDeLoi(uintptr_t loi, char* arbre, int taille, int* poids);

// F5 : un item pour l'arbre de la loi en attente la plus ancienne (sinon de
// la derniere loi signee), sur le thread du jeu.
void LoisAPDemanderTest();

// --- Outils d'icones AP partages (thread du jeu) ---------------------------------
// TEMPORAIRE : type de check stable (cle, rang) en attendant le serveur.
const char* IconeCheckDeTest(const char* cle, int rang);
// Copie de l'image 'original' (instanciee depuis son element +0x1E8 / +0x1F0,
// animateur d'etat mis a 0 : aucun etat ne la retouche), ajoutee en dernier
// a 'parent', avec la case 'icone' de l'atlas des bulles (UI/Tiles/Icons.dds),
// montree, blanche. Garde la geometrie de la recette. 0 si echec.
uintptr_t IconeAPSurCopieDe(uintptr_t original, uintptr_t parent, const char* icone);
// Pose la case 'icone' de l'atlas des bulles sur une image deja creee.
bool IconeAPPoser(uintptr_t image, const char* icone);
// Recette (+0x1E8) et ressource (+0x1F0) de "circle_inside" (plume des lois
// signees), relevees sur un noeud du livre a sa construction ; false avant.
bool LoisModeleSigne(uintptr_t* recette, uintptr_t* ressource);
// Taille nulle (FUN_14102b170) : cache une image sans toucher a son etat.
void ElementTailleNulle(uintptr_t el);