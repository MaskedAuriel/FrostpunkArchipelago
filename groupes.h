// groupes.h
// ---------------------------------------------------------------------------
// Groupes de survivants sur la carte du monde : creation a la demande (pour
// livrer en ville les items Archipelago), quel que soit le scenario.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// Thread du mod : demande la creation d'un groupe (test F3), template obtenu
// par son chemin dans l'arbre des templates du moteur. Executee au prochain
// passage de GroupesPompe.
void GroupesDemanderCreation();

// Thread du mod : demande de ramener a 30 minutes de jeu le temps de trajet
// restant du dernier groupe Archipelago cree, sinon du premier groupe (F8).
void GroupesDemanderRapprochement();

// Ressource portee par un groupe : affichee dans son inventaire (liste a
// droite, comme la cargaison d'un scout) et livree a son arrivee en ville.
struct CargaisonAP {
    const char* ressource;   // nom de resources.cpp ("coal", "steamcore"...) ; nullptr = fin
    int         quantite;
};
constexpr int MAX_CARGAISON_AP = 8;

// Survivants d'une classe sociale ("workers", "engineers", "children",
// "automaton"...), classe et statistiques prises dans les sites de la carte.
struct SurvivantsAP {
    const char* classe;      // nullptr = fin
    int         nombre;
};
constexpr int MAX_CLASSES_AP = 4;

// Groupe de survivants personnalise, cree sur un site et marchant vers la ville.
struct GroupeAP {
    const char* cheminTemplate;  // relatif a la racine de l'arbre des templates,
                                 // ex. "GameEntities/WorldMap/SurvivorGroups/SurvivorsFromExpeditionSite"
    const char* nom;             // affiche sur la carte (ASCII)
    const char* description;
    int         survivants;      // -1 : contenu du template ; sinon tout dans la 1re entree
    float       heures;          // duree du trajet en heures de jeu ; <= 0 : duree native
    bool        sansMorts;       // taux de mort du groupe a 0
    const char* cheminSurvivants;  // template dont on prend la classe sociale des
                                   // survivants (sa 1re entree) ; nullptr : liste par defaut
    void      (*surArrivee)(int slot);  // thread du jeu, juste apres l'arrivee en ville
    // Bulle de l'arrivee en ville (copie du dilemme "[Beacon]S: Survivors
    // group") : nullptr -> nom, description, "Accueillir".
    const char* dilemmeTitre;
    const char* dilemmeTexte;
    const char* dilemmeChoix;
    // Icone du groupe (carte du monde, liste a droite) : nom dans
    // ARCHIPELAGO_ICONS_TECH, case de l'atlas des bulles ; nullptr ->
    // "icon_vanilla_groupe" (vanilla reduite de 15 %).
    const char* icone;
    // Ressources portees (inventaire + livraison a l'arrivee, avant surArrivee).
    CargaisonAP cargaison[MAX_CARGAISON_AP];
    // Survivants par classe : si classes[0].classe est renseigne, une entree
    // par classe ; 'survivants' et 'cheminSurvivants' sont alors ignores.
    SurvivantsAP classes[MAX_CLASSES_AP];
    // Joueur Archipelago qui envoie le contenu (popups de l'arrivee :
    // "(from <joueur>)") ; nullptr -> le nom du groupe. Copie.
    const char* expediteur;
};

// Thread du jeu uniquement (pompe). Renvoie le slot du groupe (0..15) ou -1.
int GroupeCreer(const GroupeAP& g);

// Thread du jeu : heures de jeu avant l'arrivee du DERNIER groupe AP encore
// en route (0 si aucun). Sert a echelonner les livraisons : un nouveau
// groupe arrive 'duree' heures apres celui-la, jamais en meme temps.
float GroupesHeuresAvantDerniereArrivee();

// Textes des cles @AP/Groupe/<slot>/<champ>, pour hkTranslate : 'N' nom,
// 'D' description, 'T' titre du dilemme d'arrivee, 'X' son texte, 'C' son
// choix. nullptr si le slot est libre.
const char* GroupeTexte(int slot, char champ);

// Cle 8 bits du moteur ("@AP/..."), en-tete de comptage clone de 'modele'
// (une cle valide du jeu), allouee par l'allocateur du moteur, jamais liberee.
uintptr_t CleMoteur(const char* texte, uintptr_t modele);

// Au demarrage (avec les autres captures) : hook des fonctions qui
// construisent FrostpunkSurvivorGroupComponentTemplate, retrouvees par RTTI.
void InstallCaptureTemplatesGroupes(uintptr_t modBase);

// F9 : templates de groupe captures, arbre des templates du moteur (stubs
// sous SurvivorGroups), puis instantane du premier groupe et
// differences avec l'appui precedent (pour trouver le temps de trajet).
void DiagGroupes();

// Thread du jeu, hors evenement (hook du setter de ressources) : execute les
// creations en attente.
void GroupesPompe();

// Thread du jeu, entree et sortie de FrostpunkGame::LoadGame (hook de
// mod_common). Entre les deux, chaque groupe ajoute a son systeme est
// compare a notre sauvegarde (mods\ap_groupes.txt, identifiant de l'entite)
// et recoit sa copie du template (nom, description, icone), sa cargaison et
// ses textes de bulle. A la sortie : finalisation et releve [SAV].
void GroupesAvantChargement();
void GroupesComposantConstruit(uintptr_t comp);   // hook du constructeur (mod_common)
void GroupesLectureFaite();      // fin de la lecture de la sauvegarde, avec succes
void GroupesApresChargement();

// Thread du jeu, meme endroit : rend la carte du monde accessible sans
// balise (deverrouillage + compte de batiments d'acces au moins a 1).
void CarteAssurerAcces();
