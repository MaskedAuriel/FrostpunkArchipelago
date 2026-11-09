// archipelago.h
// ---------------------------------------------------------------------------
// Surface de communication entre la couche Archipelago (le serveur, plus tard
// la classe reseau) et le jeu. Chaque fonction est un enchainement lisible :
// verifier que la partie est prete, appliquer l'effet, afficher la popup.
//
// La couche reseau n'appellera QUE ces fonctions : elle ne connait ni les
// hooks, ni les hash, ni les etats memoire du jeu.
//
//   Serveur -> jeu : Receive*   (un item arrive)
//   Jeu -> serveur : Send*      (un check est valide en jeu)
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// --- Serveur -> jeu ----------------------------------------------------------

// Item recu par son id Archipelago (AP_ITEMS de known_ap_ids.h).
// - Arbre, loi, montee de temperature (items d'UN scenario) : notes dans
//   mods\ap_items_recus.txt, appliques par ApItemsPompe dans toute partie de
//   leur scenario (tout de suite si c'est le scenario en cours).
// - Deblocage de scenario : ReceiveScenarioUnlock.
// - Contenu de site / filler (ressources + survivants) et piege : notes de
//   meme, livres UNE fois par ApItemsPompe dans la partie en cours, quel que
//   soit son scenario (un groupe AP par contenu, une bulle par piege).
// Tout thread. false si l'id est inconnu ou l'item refuse.
bool ReceiveItem(int64_t itemId, const char* fromPlayer);

// Thread du jeu (hook du setter de ressources, apres LoisAPPompe et
// TemperaturePompe) : applique a la partie en cours les items de son
// scenario recus et pas encore appliques (au changement de partie, le
// rattrapage est silencieux : une seule popup de synthese), et livre les
// items communs en attente, un par passage.
void ApItemsPompe();

// Duree du trajet des groupes de livraison des items communs, en heures de
// jeu (option delivery_hours du slot_data ; 24 h par defaut).
void ApReglerHeuresLivraison(int heures);

// TEST (touches F5, F7, F8) : id de l'item du scenario en cours, genre
// "tree" (cle = numero d'arbre), "law" (cle = nom de l'arbre de lois) ou
// "temperature" (cle nullptr). 0 si absent.
int64_t ApItemIdCourant(const char* genre, const char* cle);
// TEST (Maj+F7) : un item d'arbre d'un AUTRE scenario, au hasard. 0 si aucun.
int64_t ApItemIdAutreScenarioDeTest();
// TEST (F3) : un item commun, a tour de role (contenu a 3 classes de
// survivants, enfants seuls, ressources + automate, filler, piege).
int64_t ApItemCommunDeTest();

// "Progressive Tree <tree>" : debloque la techno suivante de l'arbre. Toute
// techno recue passe par la : un arbre a un seul niveau reste un arbre.
// false si la partie n'est pas prete, l'arbre hors bornes ou epuise.
bool ReceiveProgressive(int tree, const char* fromPlayer);

// Ressource ("coal", "wood", "steel", "steamcore", "rawfood", "ration"...).
// Le credit est applique quelques instants plus tard, sur le thread du jeu.
// false si la partie n'est pas prete ou la ressource inconnue / jamais vue.
bool ReceiveResource(const char* resource, int amount, const char* fromPlayer);

// "Progressive Law <tree>" : l'arbre de lois 'tree' (nom du journal [LOIAP])
// recoit un item ; l'effet de la loi signee au rang correspondant est
// applique quelques instants plus tard, sur le thread du jeu.
// false si la partie n'est pas prete ou l'arbre inconnu.
bool ReceiveLawProgressive(const char* tree, const char* fromPlayer);

// "Temperature Rise" : rend 'levels' niveaux de temperature (10 degres
// chacun), les montees scriptees etant retirees. Applique quelques instants
// plus tard, sur le thread du jeu. false si la partie n'est pas prete.
bool ReceiveTemperatureRise(int levels, const char* fromPlayer);

// "Scenario Unlock <scenario>" : le scenario (nom interne : DEFAULT,
// TechHaven, REFUGEES, WINTERHOME, BUILDERS, Reconquest) devient lancable au
// menu principal (scenarios_ap.cpp). Valable hors partie ; popup seulement si
// une partie est prete. false si le nom est inconnu.
bool ReceiveScenarioUnlock(const char* scenario, const char* fromPlayer);

// --- Jeu -> serveur ----------------------------------------------------------

// Recherche terminee : location check de cette techno.
void SendResearchCheck(uint64_t techHash);

// Check porte par un scout et ramene en ville : 'site' = nom interne du site
// (cle de la location AP), 'locationId' = id de test de SITE_PAYLOADS / ApCheck.
// Chaque Send* retrouve l'id de sa location AP dans le scenario en cours
// (known_ap_ids.h) ; sans location, rien n'est envoye (journal seulement).
void SendLocationCheck(int locationId, const char* site);
// Cle de location d'un site : son nom interne, sans le suffixe "-dead" des
// paires exclusives de Refugees ([ref]X / [ref]X-dead = un seul check).
void ApCleDeSite(const char* site, char* out, int taille);
// Locations AP des sites, tous scenarios (AP_LOCATIONS de known_ap_ids.h) :
// scenario, cle (nom interne du site), nom de la location, id. Chaines
// statiques. Renvoie le nombre ecrit.
struct ApSiteLoc { const char* scenario; const char* cle; const char* nom; int64_t id; };
int ApSitesLocations(ApSiteLoc* out, int max);

// Loi signee : check de rang 'rank' de l'arbre de lois 'tree'.
// 'effectPending' : l'effet de la loi attend l'item de l'arbre.
void SendLawCheck(const char* tree, int rank, const char* lawName, bool effectPending);

// Refugies de l'apres-005 (A New Home, 2026-11-05) : le groupe 'groupe'
// (1..3) arrive a la porte (dilemme "[Refugees] Refugees at the door k") :
// check "A New Home - Refugees Arrival k". Thread du jeu.
void SendRefugeesCheck(int groupe);

// Rang(s) FIXE(s) du check de la loi 'loi' (nom interne) de l'arbre 'arbre',
// dans le scenario en cours (AP_LOIS_RANGS de known_ap_ids.h, d'apres
// outils\lois_table.tsv). Seules les lois exclusives partagent un rang ; une
// loi de poids 2 en a deux. Renvoie le nombre de rangs ecrits (0 : absente).
int ApRangsDeLoi(const char* arbre, const char* loi, int* rangs, int max);

// Texte affiche en jeu : "Progressive Tree " -> "Progressive " (demande de
// l'auteur, 2026-11-04 : plus de mot "Tree" a l'ecran). Sur place. Couvre
// les noms venus du serveur pour les graines generees avant le renommage.
void ApTexteSansTree(char* s);
// Quete de l'histoire du scenario accomplie (quetes.cpp, hook de la fin de
// quete FUN_1416d22a0 ; known_quests.h). Popup "Check sent".
void SendQuestCheck(const char* questName, const char* displayName);
// Id de la location AP de la quete 'questName' (nom interne) dans le
// scenario en cours ; les quetes paralleles partagent le meme id. 0 si la
// quete n'est pas un check.
int64_t ApQueteLocation(const char* questName);
// Montee de temperature scriptee retiree (temperature.cpp, thread du jeu) :
// check "Temperature Rise k" du scenario, un par NIVEAU retire, k = rang
// chronologique du niveau parmi les niveaux retires de la partie.
void SendTemperatureCheck(int rank);
// Victoire d'un scenario au niveau cible (victoire.cpp, hook de EndGame
// FUN_141301490) : EVENEMENT du goal, pas une location (aucun item). Notee
// dans mods\ap_victoires.txt ; goal atteint quand tous les scenarios du
// slot sont gagnes. Popup "Scenario completed", puis "Goal reached".
void SendScenarioVictory(const char* scenario, const char* displayName);
// Scenarios a gagner pour le goal (option "scenarios" du slot_data, noms
// internes). Defaut TEMPORAIRE : A New Home seul.
void ApReglerScenariosGoal(const char* const* scenarios, int n);

// --- slot_data (connexion) -----------------------------------------------------

// Options du slot envoyees par le serveur a la connexion (fill_slot_data de
// l'apworld). Noms internes des scenarios ; goal_difficulty = "easy",
// "medium", "hard" ou "very_hard".
struct ApSlotData {
    const char* scenarios[6];
    int         nScenarios;
    const char* startingScenario;
    const char* goalDifficulty;
    bool        excludePurposePolitics;
    int         deliveryHours;
};
// Applique le slot_data : scenarios du goal, difficulte imposee, duree de
// livraison des groupes. Le scenario de depart arrive comme item (Scenario
// Unlock) ; exclude_purpose_politics ne change que la generation (locations
// EXCLUDED) : notes seulement. Tout thread, hors partie compris.
void ApAppliquerSlotData(const ApSlotData& d);
bool ApExclurePurposePolitics();

// --- Session (couche reseau, reseau_ap.cpp) -----------------------------------

// Session = graine + slot : ses fichiers vivent dans mods\<dossier>\ (items
// recus, items livres, victoires, et ceux du reseau). Change de session :
// compteurs relus, scenarios reverrouilles puis debloques selon les items de
// la session (sans popup). "" = mods\ lui-meme (aucune session connue).
// Tout thread.
void ApDefinirSession(const char* dossier);
// Chemin complet de 'fichier' dans le dossier de la session courante (cree
// au besoin). Tout thread.
bool ApCheminSession(char* out, size_t taille, const char* fichier);
// Goal atteint : tous les scenarios du slot sont gagnes (ap_victoires.txt de
// la session).
bool ApGoalAtteint();
// Message d'etat du reseau (connecte, deconnecte, refuse, item envoye...),
// texte en anglais, en Latin-1. Mis en file ; affiche en partie seulement.
void ApMessageReseau(const char* titre, const char* texte);

// --- Scouts (contenu des locations du slot) -------------------------------------

// Couche reseau : la location 'location' (id AP) contient 'item' (id dans le
// jeu du destinataire), nomme 'nomItem', pour le joueur 'nomJoueur' (Latin-1,
// copies). flags : 1 progression, 2 useful, 4 trap. 'pourMoi' : destinataire =
// ce slot. Tout thread.
void ApNoterScout(int64_t location, int64_t item, const char* nomItem, const char* nomJoueur,
                  int flags, bool pourMoi);

struct ApScoutInfo {
    int64_t item;
    int     flags;
    bool    pourMoi;
    char    nomItem[128];
    char    nomJoueur[64];
};
// Contenu de la location AP du scenario en cours : loi (arbre de lois, rang
// du check), techno (hash). false si la location n'existe pas ou si son
// contenu n'est pas encore connu (jamais connecte). Tout thread.
bool ApScoutLoi(const char* arbre, int rang, ApScoutInfo* out);
bool ApScoutTechno(uint64_t hash, ApScoutInfo* out);
// Nom affiche d'un check (noeuds des arbres) : "<joueur> - <item>", le
// destinataire en tete, meme pour ce slot (auteur, 2026-11-04 ; '|' coupe
// le texte en jeu). Latin-1.
void ApNomDuScout(const ApScoutInfo& s, char* out, size_t taille);

// Etat de la connexion, texte court en anglais (Latin-1) pour le libelle du
// menu principal : "Not connected", "Connecting...", "Connected",
// "Refused: InvalidSlot"... Tenu a jour par RequestConnection et
// ApMessageReseau. Copie dans 'out' ; renvoie un compteur qui change a chaque
// nouvel etat. Tout thread.
long ApEtatConnexion(char* out, size_t taille);
// Connecte au serveur Archipelago (Connected recu, connexion pas perdue
// depuis). Au menu principal, sans connexion : choix du scenario, Continue
// et Load game desactives (menu_ap.cpp). Tout thread.
bool ApEstConnecte();

// Bouton "Connect" du menu principal (menu_ap.cpp) : chaines UTF-8 saisies.
// Appele sur le thread du jeu, hors partie : ReseauConnecter (reseau_ap.h).
void RequestConnection(const char* server, const char* slot, const char* password);

// Icone du type de check d'une location du scenario en cours, d'apres son
// scout : "icon_trap", "icon_progressive", "icon_important" ou "icon_filler"
// (chaine litterale, comparable par pointeur). genre = "techno" (cle = hash
// en hexadecimal, %016llX), "law" ("<arbre>|<rang>"), "site" (nom interne,
// "-dead" retire ici), "quest" (nom interne), "temperature" (rang).
// nullptr si la location n'existe pas ou n'est pas encore scoutee : garder
// alors l'icone de test. Tout thread.
const char* ApIconeDuCheck(const char* genre, const char* cle);
// Nom du check d'une location AP par son id (scout) : "<joueur> - <item>"
// (ApNomDuScout). false si pas encore scoutee. Tout thread.
bool ApNomDuCheckLocation(int64_t locationId, char* out, size_t taille);
