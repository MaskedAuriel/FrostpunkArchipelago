// historique_ap.cpp
// ---------------------------------------------------------------------------
// Historique de la session Archipelago et legende des icones AP en jeu
// (docs/chantiers/historique.md).
//
// Decisions de l'auteur (2026-11-21) :
//  - historique : bouton dans le HUD, PANNEAU DEDIE (pas un message), items
//    recus + checks envoyes, toute la session (graine + slot), tout
//    l'historique (defilement) ;
//  - legende : panneau dedie a cote du livre des lois, de l'arbre des
//    technos et sur la carte du monde, icones dessinees, sens court (anglais).
//
// Releve du 2026-11-21 (journal [HIST], RELEVE_INTERFACE) :
//  - FUN_14196b2d0 = entree 37 de la vtable de FrostpunkUIGameplayController :
//    FUN_141024ba0("UI/MainScreen", "FRAME") -> racine FRAME du HUD (1280 x
//    720, coordonnees absolues). HUD > TOP_LEFT_static > TOP_LEFT_BUTTONS >
//    INGAME_MENU_BUTTON (UIButton 37 x 36, image "icon" 32 x 32) : modele du
//    bouton. HUD > WORLD_MAP_HUD (visible sur la carte) : parent de la
//    legende de la carte.
//  - UI/Panels/City/TutorialSummaryPanel : FRAME > ANIM > cadre, en-tete
//    (COLUMN_HEADER : TEXT, DESCRIPTION_TITLE), liste (LIST_SCROLL_PANE >
//    SUMMARY_LAYOUT > SUMMARY_ROW, texte "TEXT"), texte qui defile
//    (DESCRIPTION_SCROLL_PANE > DESCRIPTION_LAYOUT > DESCRIPTION_TEXT, barre
//    DESCRIPTION_SCROLL_BAR) : modele du panneau d'historique, et des
//    elements de la legende (fond "CENTER", titre, texte, image).
//  - FUN_141bcf2e0 (init du panneau des tutoriels) relie la barre a sa zone
//    par FUN_141516ec0(barre, &poignee_zone, 0, &nom_vide).
//  - FUN_141bbdf60 = init du livre des lois (FrostpunkUISociotechPanel) :
//    racine **(panneau +0xA8), TREE_VIEW = **(panneau +0x190).
//  - FUN_141bc6800 = init de l'arbre des technos : racine **(panneau +0xA8).
// Offsets d'element et de recette : voir menu_ap.cpp et
// docs/chantiers/menu_connexion.md.
//
// Tout s'execute sur le thread du jeu : hooks d'init des panneaux, rappels
// de clic, HistoriquePompe (setter de ressources).
// ---------------------------------------------------------------------------
#include "historique_ap.h"
#include "mod_common.h"
#include "archipelago.h"
#include "lois_ap.h"       // IconeAPPoser : case AP de l'atlas des bulles sur une image
#include "IconSubManager.h"   // IconesTechnoPanneau : noeuds de l'arbre des technos
#include "MinHook.h"
#include <cstring>
#include <string>
#include <vector>

namespace {

typedef uintptr_t (*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef uintptr_t (*Instancier_t)(uintptr_t recette, uintptr_t ressource, uint32_t drapeaux, uintptr_t ctx);
typedef uintptr_t (*ObtenirRecette_t)(uintptr_t pool, const char* chemin, uintptr_t charger);
typedef uintptr_t (*RacineRecette_t)(uintptr_t ressource);
typedef uintptr_t (*Poignee_t)(uintptr_t objet);
typedef void      (*PoigneeRef_t)(uintptr_t poignee);
typedef void      (*AjouterEnfant_t)(uintptr_t parent, uintptr_t enfant);
typedef void      (*BrancherClic_t)(uintptr_t bouton, uintptr_t objet, uintptr_t fonction, int p4,
                                    uintptr_t p5, int p6);
typedef void      (*PoserTexte_t)(uintptr_t texte, const wchar_t* chaine, uintptr_t cfg, uintptr_t p4);
typedef void      (*Montrer_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void      (*PoserTaille_t)(uintptr_t, float, float);
typedef void      (*PoserPosition_t)(uintptr_t, const float*);
typedef void      (*Relayout_t)(uintptr_t layout);
typedef void      (*LierBarre_t)(uintptr_t barre, uintptr_t* poigneeZone, int axe, uintptr_t* nom);
typedef void      (*NomDetruire_t)(uintptr_t* nom);
typedef void      (*InitPanneau_t)(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4);

static const uintptr_t OFF_CREER_ECRAN_PRINCIPAL = Adr(0x196B2D0);   // FUN_14196b2d0
static const uintptr_t OFF_INIT_LIVRE      = Adr(0x1BBDF60);   // FUN_141bbdf60 : init du livre des lois
static const uintptr_t OFF_INIT_TECHNOS    = Adr(0x1BC6800);   // FUN_141bc6800 : init de l'arbre des technos
static const uintptr_t OFF_LIER_BARRE      = Adr(0x1516EC0);   // FUN_141516ec0 : barre <- zone a defilement
static const uintptr_t OFF_INSTANCIER      = Adr(0x110C780);
static const uintptr_t OFF_OBTENIR_RECETTE = Adr(0x1035C80);
static const uintptr_t OFF_RACINE_RECETTE  = Adr(0x1119140);
static const uintptr_t OFF_POIGNEE         = Adr(0xE1A120);
static const uintptr_t OFF_POIGNEE_PLUS    = Adr(0xE1A110);
static const uintptr_t OFF_POIGNEE_MOINS   = Adr(0xE1A260);
static const uintptr_t OFF_AJOUTER_ENFANT  = Adr(0x1023D70);
static const uintptr_t OFF_BRANCHER_CLIC   = Adr(0x105A1C0);
static const uintptr_t OFF_POSER_TEXTE     = Adr(0x111BEF0);
static const uintptr_t OFF_MONTRER         = Adr(0x105AC90);
static const uintptr_t OFF_POSER_TAILLE    = Adr(0x102B170);
static const uintptr_t OFF_POSER_POSITION  = Adr(0x102A3E0);
static const uintptr_t OFF_RELAYOUT        = Adr(0x11FD800);
static const uintptr_t OFF_NOM_DETRUIRE    = Adr(0xE4EAB0);
static const uintptr_t OFF_POOL_TEMPLATES  = Adr(0x2B2E710);
static const uintptr_t OFF_CTX_INSTANCIER  = Adr(0x2BE0438);
static const uintptr_t OFF_CFG_TEXTE       = Adr(0x2BFD628);

constexpr int EL_POSITION  = 0x80;     // translation locale (2 flottants)
constexpr int EL_NOM       = 0x1D8;
constexpr int EL_RECETTE   = 0x1E8;    // recette / animateur d'etat
constexpr int EL_TAILLE    = 0x210;    // largeur, hauteur (flottants)
constexpr int EL_ENFANT    = 0x220;
constexpr int EL_PARENT    = 0x230;
constexpr int EL_FRERE     = 0x240;
constexpr int EL_DRAPEAUX  = 0x270;    // bit 0 : visible
constexpr int REC_ENFANTS  = 0x30;
constexpr int REC_COMPTE   = 0x38;
constexpr int REC_NOM      = 0x48;
constexpr int REC_POSITION = 0x80;
constexpr int REC_ANCRE    = 0x90;
constexpr int REC_MODE     = 0x114;
constexpr int PAN_RACINE   = 0xA8;     // panneau : poignee de la racine
constexpr int LIVRE_ARBRE  = 0x190;    // livre des lois : poignee de TREE_VIEW

constexpr bool RELEVE_INTERFACE = false;   // releve [HIST] (diagnostic, 2026-11-21)

const char* const RECETTE_TUTORIEL = "UI/Panels/City/TutorialSummaryPanel";

// --- Reglages d'aspect (premier essai : a ajuster d'apres le rendu) ------------
// Bouton : meme parent que le bouton du menu en jeu (TOP_LEFT_BUTTONS),
// position = position de recette du bouton du menu + (DX, DY). 1er essai
// (2026-11-21) : avec son animateur, la copie se placait EXACTEMENT sur le
// bouton du menu (l'animation d'etat impose la translation) ; animateur
// retire depuis, position (-42, -35) + (45, 0) dans TOP_LEFT_BUTTONS (10, 94).
// 2e essai : (45, 0) chevauchait encore le rond du bouton du menu -> dessous.
// 5e essai : decalage du CONTENEUR de la copie (voir ConstruireBouton).
// 7e test : "bouton trop haut" -> 52 puis 72 ; 8e test : encore plus bas -> 100.
// 9e test : le bouton etait colle en haut a gauche : le conteneur "LEFT"
// (ancre au bord par sa recette) ignorait notre position -> retour au
// conteneur DESCRIPTION_VIDEO (position respectee au 6e test : bouton sous
// celui du menu), de taille nulle (plus de rectangle blanc ni de clics
// captes) ; decalage de 60.
constexpr float BOUTON_DX = 0.f, BOUTON_DY = 60.f;
constexpr float BOUTON_ECHELLE = 2.f;     // 6e test : "doubler sa taille"
constexpr int   REC_ECHELLE = 0xC0;       // UIElementRecipe : Scale (2 flottants)
const char* const BOUTON_ICONE = "icon_progressive";
// Legende : coin haut-gauche dans le parent de chaque ecran.
struct PlaceLegende { const char* ecran; float x, y; };
const PlaceLegende LEG_LIVRE   = { "livre",   30.f, 430.f };    // dans TREE_VIEW (0, 0) du livre
const PlaceLegende LEG_TECHNOS = { "technos", -610.f, 90.f };   // dans la racine du panneau (640, 360)
const PlaceLegende LEG_CARTE   = { "carte",   20.f, 420.f };    // dans WORLD_MAP_HUD (-1, 0), 1280 x 720
constexpr float LEG_LARGEUR = 270.f, LEG_ENTETE = 38.f, LEG_PAS = 32.f, LEG_ICONE = 26.f;
// Decision de l'auteur (2026-11-21, 2e test) : plus de legende permanente en
// bas a gauche ; arbre des technos = deux entrees dans le vide en haut a
// droite (a gauche de CLOSE) : icone "fait" + "Check Sended", anneau bleu des
// technos + "Tech Received". Livre et carte : contenu a redefinir.
constexpr bool LEGENDE_LIVRE = false, LEGENDE_CARTE = false;
// 3e test (capture) : ligne bien placee a gauche de CLOSE, mais textes ~14
// plus bas que les icones, "Tech Received" mordant sur CLOSE, anneau
// thin_stroke presque invisible -> decalage de 30 a gauche, textes remontes,
// anneau = copie de l'anneau d'un vrai noeud de l'arbre (LegendeTechnosAnneau).
constexpr float LEGT_LARGEUR = 330.f;     // de la gauche de la legende a la gauche de CLOSE
constexpr float LEGT_TEXTE_DY = -24.f;    // haut du texte par rapport au centre des icones
constexpr float LEGT_ENTREE  = 150.f;     // pas entre les deux entrees
constexpr float LEGT_TEXTE   = 112.f;     // largeur d'un texte
const float LEGT_TEINTE[4] = { 0.13f, 0.78f, 0.84f, 1.f };   // bleu des noeuds (teinte d'etat, lois_ap.cpp)
// Livre des lois (8e test) : meme ligne, a gauche de CUSTOM_CLOSE_BUTTON
// ((1184, 60) dans ANIM, recette) ; premier essai, a regler sur capture.
// 10e test : plume encore plus grosse, texte "Signed and effective" (plus
// long : ligne decalee a gauche, texte plus large).
constexpr float LEGL_LARGEUR = 400.f;
constexpr float LEGL_DY      = 0.f;
constexpr float LEGL_PLUME   = 70.f;      // taille de la plume "Signed and effective"
constexpr float LEGL_TEXTE2  = 200.f;

struct LigneLegende { const char* icone; const wchar_t* sens; };
const LigneLegende LEGENDE[] = {
    { "icon_progressive", L"Progression item" },
    { "icon_important",   L"Useful item" },
    { "icon_filler",      L"Filler item" },
    { "icon_trap",        L"Trap" },
    { "icon_done",        L"Already checked" },
    // 11e test : ligne "Check (contents unknown)" retiree (auteur : inutile).
};

// 11e test (demande de l'auteur) : paragraphes sous les icones de l'onglet
// "Legends", fonctionnement des technos, des lois et de l'exploration.
const wchar_t* const LEGENDE_PARAGRAPHES =
    L"Technologies: researching a technology sends its check, whoever the item belongs to. "
    L"Its effect only applies once you have also received the Progressive item of its tree, "
    L"which unlocks the technologies of that tree in order.\n\n"
    // 12e test (auteur) : preciser les doubles lois (paire exclusive = un seul check).
    L"Laws: signing a law sends its check. A double law (two mutually exclusive laws, where signing "
    L"one locks the other) holds a single check: both sides show the same check and signing either "
    L"one sends it. The law takes effect (modifiers, buildings) only once the Progressive item of "
    L"its tree has been received: it is then shown as signed and effective.\n\n"
    L"Exploration: world map sites showing an Archipelago icon hold checks. Scouts pick them up "
    L"when exploring the site; the checks are sent when the scouts get back to the city.";
// 11e test : legende descendue encore un peu (+12).
constexpr float LEGP_X = 52.f, LEGP_Y = 54.f;
constexpr float LEGP_LARGEUR_TEXTE = 640.f;
constexpr float LEGP_HORS_ECRAN = -20000.f;   // decalage des pieces cachees
constexpr int NB_LEGENDE = (int)(sizeof(LEGENDE) / sizeof(LEGENDE[0]));

Fonction4_t   oCreerEcranPrincipal = nullptr;
InitPanneau_t oInitLivre = nullptr;
InitPanneau_t oInitTechnos = nullptr;

uintptr_t g_racineHud = 0;        // poignee sur la racine FRAME du HUD
ULONGLONG g_instantCreation = 0;
bool      g_releveFait = true;
bool      g_hudEquipe = true;     // bouton + legende de la carte poses pour ce HUD
int       g_lignes = 0;           // garde du releve

// Panneau d'historique (poignees : les elements peuvent mourir avec le HUD).
uintptr_t g_panneau = 0, g_texteDesc = 0, g_layoutDesc = 0, g_zoneDesc = 0;
uintptr_t g_lignesListe[3] = {};
int       g_onglet = 0;           // 0 recus, 1 envoyes, 2 legendes
std::vector<uintptr_t> g_piecesLegendePanneau;   // poignees : legende de l'onglet "Legends"
std::vector<float>     g_posLegendePanneau;      // position (x, y) de chaque piece, 2 par piece

uintptr_t Base()
{
    static uintptr_t b = (uintptr_t)GetModuleHandleW(nullptr);
    return b;
}

// --- Petits outils ---------------------------------------------------------------
void LireNom(uintptr_t obj, int off, char* out, int maxlen)
{
    uintptr_t p = 0;
    out[0] = 0;
    if (SafeRead((void*)(obj + off), &p, 8) && p > 0x10000) ReadAscii(p, out, maxlen);
}

uintptr_t ObjetDePoignee(uintptr_t poignee)
{
    uintptr_t o = 0;
    if (poignee > 0x10000) SafeRead((void*)poignee, &o, 8);
    return o;
}

uintptr_t PrendrePoignee(uintptr_t obj)
{
    uintptr_t h = 0;
    __try {
        h = ((Poignee_t)(Base() + OFF_POIGNEE))(obj);
        if (h) ((PoigneeRef_t)(Base() + OFF_POIGNEE_PLUS))(h);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { h = 0; }
    return h;
}

void LacherPoignee(uintptr_t& h)
{
    if (!h) return;
    __try { ((PoigneeRef_t)(Base() + OFF_POIGNEE_MOINS))(h); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    h = 0;
}

bool ClasseEst(uintptr_t obj, const char* nom)
{
    char cls[96] = "";
    return obj > 0x10000 && GetClassNameFP(obj, cls, sizeof(cls)) && StrEq(cls, nom);
}

bool EstTexte(uintptr_t obj) { return ClasseEst(obj, "UIText") || ClasseEst(obj, "UITextBox"); }

// Element vivant nomme 'nom' sous 'el' (profondeur d'abord, 'el' exclu).
uintptr_t TrouverParNom(uintptr_t el, const char* nom, int prof = 0)
{
    if (el < 0x10000 || prof > 16) return 0;
    uintptr_t c = 0;
    SafeRead((void*)(el + EL_ENFANT), &c, 8);
    for (int garde = 0; c > 0x10000 && garde < 512; ++garde) {
        char n[64];
        LireNom(c, EL_NOM, n, sizeof(n));
        if (StrEq(n, nom)) return c;
        uintptr_t r = TrouverParNom(c, nom, prof + 1);
        if (r) return r;
        if (!SafeRead((void*)(c + EL_FRERE), &c, 8)) break;
    }
    return 0;
}

// Tous les textes sous 'el' ('el' compris) recoivent 'texte'.
void PoserTexte(uintptr_t t, const wchar_t* chaine);
void PoserTousLesTextes(uintptr_t el, const wchar_t* texte, int prof = 0)
{
    if (el < 0x10000 || prof > 16) return;
    if (EstTexte(el)) PoserTexte(el, texte);
    uintptr_t c = 0;
    SafeRead((void*)(el + EL_ENFANT), &c, 8);
    for (int garde = 0; c > 0x10000 && garde < 512; ++garde) {
        PoserTousLesTextes(c, texte, prof + 1);
        if (!SafeRead((void*)(c + EL_FRERE), &c, 8)) break;
    }
}

bool EstVisible(uintptr_t el)
{
    uint32_t d = 0;
    return el > 0x10000 && SafeRead((void*)(el + EL_DRAPEAUX), &d, 4) && (d & 1);
}

void Montrer(uintptr_t el, bool visible)
{
    if (el < 0x10000) return;
    __try { ((Montrer_t)(Base() + OFF_MONTRER))(el, visible ? 1 : 0, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a l'affichage de 0x%llX\n", GetExceptionCode(), (unsigned long long)el);
    }
}

void PoserTexte(uintptr_t t, const wchar_t* chaine)
{
    if (t < 0x10000) return;
    __try { ((PoserTexte_t)(Base() + OFF_POSER_TEXTE))(t, chaine, Base() + OFF_CFG_TEXTE, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a la pose d'un texte\n", GetExceptionCode());
    }
}

void Placer(uintptr_t el, float x, float y)
{
    if (el < 0x10000) return;
    float pos[4] = { x, y, 0.f, 1.f };
    __try { ((PoserPosition_t)(Base() + OFF_POSER_POSITION))(el, pos); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void Dimensionner(uintptr_t el, float l, float h)
{
    if (el < 0x10000) return;
    __try { ((PoserTaille_t)(Base() + OFF_POSER_TAILLE))(el, l, h); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

bool AjouterEnfant(uintptr_t parent, uintptr_t el)
{
    if (parent < 0x10000 || el < 0x10000) return false;
    __try { ((AjouterEnfant_t)(Base() + OFF_AJOUTER_ENFANT))(parent, el); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a l'ajout de 0x%llX\n", GetExceptionCode(), (unsigned long long)el);
        return false;
    }
    return true;
}

// Objet des ecouteurs de clic : jamais nul. 2e essai (2026-11-21) : avec un
// objet nul, le clic du bouton d'historique ne faisait rien (le menu
// principal passe toujours son panneau, menu_ap.cpp). FUN_14105a1c0 garde
// l'objet sous forme de POIGNEE du moteur (FUN_140e1a120) : il faut un objet
// du jeu -> le bouton lui-meme (le rappel ignore l'objet).
bool BrancherClic(uintptr_t bouton, void (*fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))
{
    if (bouton < 0x10000) return false;
    __try { ((BrancherClic_t)(Base() + OFF_BRANCHER_CLIC))(bouton, bouton, (uintptr_t)fn, 0, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX au branchement d'un clic\n", GetExceptionCode());
        return false;
    }
    return true;
}

void Relayout(uintptr_t layout)
{
    if (layout < 0x10000) return;
    __try { ((Relayout_t)(Base() + OFF_RELAYOUT))(layout); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Definies plus bas (legende de l'arbre des technos).
uintptr_t TrouverNomContenant(uintptr_t el, const char* sous, int prof = 0);
void PoserCouleurEl(uintptr_t el, const float* c);

void DecrireElement(const char* qui, uintptr_t el)
{
    char nom[64] = "", cls[96] = "?";
    float pos[2] = {}, taille[2] = {};
    LireNom(el, EL_NOM, nom, sizeof(nom));
    GetClassNameFP(el, cls, sizeof(cls));
    SafeRead((void*)(el + EL_POSITION), pos, 8);
    SafeRead((void*)(el + EL_TAILLE), taille, 8);
    printf("[HIST] %s : \"%s\" %s 0x%llX pos (%.0f, %.0f) taille %.0f x %.0f %s\n", qui, nom, cls,
        (unsigned long long)el, pos[0], pos[1], taille[0], taille[1], EstVisible(el) ? "visible" : "cache");
}

// --- Recettes ---------------------------------------------------------------------
uintptr_t RessourceRecette(const char* chemin, uintptr_t* racine)
{
    char tampon[260];
    strcpy_s(tampon, chemin);
    uintptr_t res = 0;
    *racine = 0;
    __try {
        res = ((ObtenirRecette_t)(Base() + OFF_OBTENIR_RECETTE))(Base() + OFF_POOL_TEMPLATES, tampon, 1);
        if (res) *racine = ((RacineRecette_t)(Base() + OFF_RACINE_RECETTE))(res);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[HIST] EXCEPTION 0x%08lX a l'obtention de \"%s\"\n", GetExceptionCode(), tampon);
        res = 0;
    }
    return res;
}

uintptr_t ChercherNoeud(uintptr_t noeud, const char* nom, int prof = 0)
{
    if (noeud < 0x10000 || prof > 24) return 0;
    char n[96];
    LireNom(noeud, REC_NOM, n, sizeof(n));
    if (StrEq(n, nom)) return noeud;
    uintptr_t tab = 0;
    int32_t c = 0;
    if (!SafeRead((void*)(noeud + REC_ENFANTS), &tab, 8) || !SafeRead((void*)(noeud + REC_COMPTE), &c, 4) ||
        tab < 0x10000 || c <= 0 || c > 4096) return 0;
    for (int i = 0; i < c; ++i) {
        uintptr_t e = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8)) continue;
        uintptr_t r = ChercherNoeud(e, nom, prof + 1);
        if (r) return r;
    }
    return 0;
}

uintptr_t InstancierBrut(uintptr_t noeud, uintptr_t ressource)
{
    uintptr_t el = 0;
    __try { el = ((Instancier_t)(Base() + OFF_INSTANCIER))(noeud, ressource, 1, Base() + OFF_CTX_INSTANCIER); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a l'instanciation de 0x%llX\n", GetExceptionCode(),
            (unsigned long long)noeud);
        el = 0;
    }
    return el;
}

// Instancie un noeud ; un noeud en mode 1 ("jamais") est leve le temps de
// l'appel (les recettes sont des donnees partagees : on remet la valeur).
uintptr_t InstancierNoeud(uintptr_t noeud, uintptr_t ressource)
{
    if (noeud < 0x10000) return 0;
    int32_t mode = 0;
    SafeRead((void*)(noeud + REC_MODE), &mode, 4);
    if (mode == 1) { int32_t zero = 0; SafeWrite((void*)(noeud + REC_MODE), &zero, 4); }
    uintptr_t el = InstancierBrut(noeud, ressource);
    if (mode == 1) SafeWrite((void*)(noeud + REC_MODE), &mode, 4);
    return el;
}

uintptr_t Cloner(uintptr_t el)
{
    uintptr_t rec = 0, res = 0;
    if (!SafeRead((void*)(el + EL_RECETTE), &rec, 8) || rec < 0x10000 ||
        !SafeRead((void*)(el + 0x1F0), &res, 8)) return 0;
    return InstancierNoeud(rec, res);
}

// Recette du panneau des tutoriels (modele de l'historique et de la legende).
struct ModeleTuto { uintptr_t res, racine, frame, centre, titre, texteLigne, texteCorps, image; bool ok; };
ModeleTuto g_tuto = {};

bool ModeleTutoriel()
{
    if (g_tuto.ok) return true;
    g_tuto.res = RessourceRecette(RECETTE_TUTORIEL, &g_tuto.racine);
    if (!g_tuto.res || !g_tuto.racine) { printf("[HIST] recette \"%s\" introuvable\n", RECETTE_TUTORIEL); return false; }
    g_tuto.frame = ChercherNoeud(g_tuto.racine, "FRAME");
    g_tuto.centre = ChercherNoeud(g_tuto.racine, "CENTER");
    g_tuto.titre = ChercherNoeud(g_tuto.racine, "DESCRIPTION_TITLE");
    uintptr_t ligne = ChercherNoeud(g_tuto.racine, "SUMMARY_ROW");
    g_tuto.texteLigne = ligne ? ChercherNoeud(ligne, "TEXT") : 0;
    g_tuto.texteCorps = ChercherNoeud(g_tuto.racine, "DESCRIPTION_TEXT");
    g_tuto.image = ChercherNoeud(g_tuto.racine, "small_deco_arrow");
    printf("[HIST] modele des tutoriels : FRAME 0x%llX, CENTER 0x%llX, titre 0x%llX, texte 0x%llX, image 0x%llX\n",
        (unsigned long long)g_tuto.frame, (unsigned long long)g_tuto.centre, (unsigned long long)g_tuto.titre,
        (unsigned long long)g_tuto.texteLigne, (unsigned long long)g_tuto.image);
    g_tuto.ok = g_tuto.frame != 0;
    return g_tuto.ok;
}

// --- Texte de l'historique ------------------------------------------------------------
// Fichiers de la session (graine + slot) : ap_items_recus.txt ("<id>\t<joueur>",
// ordre d'arrivee), checks.txt (une location par ligne, ordre d'envoi).
std::vector<std::string> LireLignes(const char* fichier)
{
    std::vector<std::string> l;
    char chemin[MAX_PATH], ligne[512];
    FILE* f = nullptr;
    if (!ApCheminSession(chemin, sizeof(chemin), fichier) || fopen_s(&f, chemin, "r") != 0 || !f) return l;
    while (fgets(ligne, sizeof(ligne), f)) {
        ligne[strcspn(ligne, "\r\n")] = 0;
        if (ligne[0]) l.push_back(ligne);
    }
    fclose(f);
    return l;
}

// UTF-8 si valide, sinon Latin-1 (noms des joueurs et des items des autres jeux).
std::wstring Large(const char* s)
{
    if (!s || !s[0]) return L"";
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    UINT cp = n > 0 ? CP_UTF8 : 1252;
    if (n <= 0) n = MultiByteToWideChar(cp, 0, s, -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 1, L'\0');
    if (n > 0) MultiByteToWideChar(cp, cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, s, -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

int NombreRecus()   { return (int)LireLignes("ap_items_recus.txt").size(); }
int NombreEnvoyes() { return (int)LireLignes("checks.txt").size(); }

std::wstring TexteRecus()
{
    std::vector<std::string> l = LireLignes("ap_items_recus.txt");
    std::wstring t;
    if (l.empty()) return L"No item received yet in this session.";
    for (int i = (int)l.size() - 1; i >= 0; --i) {
        const char* s = l[i].c_str();
        long long id = _strtoi64(s, nullptr, 10);
        const char* tab = strchr(s, '\t');
        const char* nom = ApNomItemParId(id);
        wchar_t num[16];
        swprintf_s(num, L"%d. ", i + 1);
        t += num;
        if (nom) t += Large(nom);
        else { wchar_t b[40]; swprintf_s(b, L"Item %lld", id); t += b; }
        if (tab && tab[1]) { t += L"  -  from "; t += Large(tab + 1); }
        t += L"\n";
    }
    return t;
}

std::wstring TexteEnvoyes()
{
    std::vector<std::string> l = LireLignes("checks.txt");
    std::wstring t;
    if (l.empty()) return L"No check sent yet in this session.";
    for (int i = (int)l.size() - 1; i >= 0; --i) {
        long long id = _strtoi64(l[i].c_str(), nullptr, 10);
        const char* nom = ApNomLocationParId(id);
        char scout[256];
        wchar_t num[16];
        swprintf_s(num, L"%d. ", i + 1);
        t += num;
        if (nom) t += Large(nom);
        else { wchar_t b[40]; swprintf_s(b, L"Location %lld", id); t += b; }
        if (ApNomDuCheckLocation(id, scout, sizeof(scout))) { t += L"\n      "; t += Large(scout); }
        t += L"\n";
    }
    return t;
}

// --- Panneau d'historique -----------------------------------------------------------
void CbRecus(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void CbEnvoyes(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void CbLegendes(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void CbFermer(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void ConstruireLegendeListe(uintptr_t parent, float x, float y, std::vector<uintptr_t>& pieces);   // plus bas

void LibellesListe()
{
    wchar_t b[64];
    swprintf_s(b, L"Received items (%d)", NombreRecus());
    PoserTousLesTextes(ObjetDePoignee(g_lignesListe[0]), b);
    swprintf_s(b, L"Sent checks (%d)", NombreEnvoyes());
    PoserTousLesTextes(ObjetDePoignee(g_lignesListe[1]), b);
    PoserTousLesTextes(ObjetDePoignee(g_lignesListe[2]), L"Legends");
}

// Pause du jeu pendant que le panneau est ouvert (10e test, demande de
// l'auteur). Decompile : les panneaux du jeu posent la raison "UIPanelPause"
// (FUN_14196c7e0) par FUN_14100ec50(minuteur, &raison) et la retirent par
// FUN_14100f1a0 ; minuteur = *(base + 0x2C30130) ; le minuteur garde un
// ensemble trie de POINTEURS de raisons (simple comparaison) : notre propre
// raison, une structure statique { nom } comme celles du jeu, ne se melange
// pas a celles des panneaux du jeu. Steam : 0x2B8D390, 0xEA8840, 0xEA8D90
// (retrouves par la reference a "UIPanelPause", script
// appel_avec_ptr_chaine.py, identiques sur Epic).
typedef void (*RaisonPause_t)(uintptr_t minuteur, uintptr_t raison);
static const uintptr_t OFF_MINUTEUR_JEU = Adr(0x2C30130);
static const uintptr_t OFF_POSER_PAUSE  = Adr(0x100EC50);
static const uintptr_t OFF_RETIRER_PAUSE = Adr(0x100F1A0);
struct RaisonPause { const char* nom; uint64_t reserve[3]; };
RaisonPause g_raisonPause = { "ArchipelagoHistoryPause", {} };
uintptr_t   g_minuteurPause = 0;     // minuteur sur lequel notre raison est posee (0 : aucune)

void PauseHistorique(bool pause)
{
    if (pause == (g_minuteurPause != 0)) return;
    uintptr_t minuteur = g_minuteurPause;
    if (pause) SafeRead((void*)(Base() + OFF_MINUTEUR_JEU), &minuteur, 8);
    if (minuteur < 0x10000) { printf("[HIST] minuteur du jeu introuvable : pas de pause\n"); return; }
    __try {
        ((RaisonPause_t)(Base() + (pause ? OFF_POSER_PAUSE : OFF_RETIRER_PAUSE)))(minuteur, (uintptr_t)&g_raisonPause);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a la %s\n", GetExceptionCode(), pause ? "pause" : "reprise");
        return;
    }
    g_minuteurPause = pause ? minuteur : 0;
    printf("[HIST] jeu %s (minuteur 0x%llX)\n", pause ? "mis en pause" : "repris", (unsigned long long)minuteur);
}

// Verrou de la camera (12e test : la molette faisait bouger la vue sous le
// panneau). Decompile : les panneaux du jeu incrementent a l'ouverture
// (FUN_14196c7e0, drapeau virtuel +0x68) et decrementent a la fermeture
// (FUN_14196c6e0) le compteur *(DAT_144065ae0 + 0x1F0) ; le controleur de la
// camera ne zoome / ne bouge que s'il vaut 0 (FUN_1413b9f50, FUN_1413b5ef0).
// Steam : 0x3FC2998 (4 sites "mov rax, [G] ; add [rax+0x1F0]" identiques,
// script verrou_camera.py).
static const uintptr_t OFF_CAMERA_JEU = Adr(0x4065AE0);
constexpr uintptr_t CAMERA_VERROU = 0x1F0;
uintptr_t g_cameraVerrouillee = 0;   // controleur sur lequel notre verrou est pose (0 : aucun)

void VerrouCamera(bool verrou)
{
    if (verrou == (g_cameraVerrouillee != 0)) return;
    uintptr_t cam = g_cameraVerrouillee;
    if (verrou) SafeRead((void*)(Base() + OFF_CAMERA_JEU), &cam, 8);
    int32_t n = 0;
    if (cam < 0x10000 || !SafeRead((void*)(cam + CAMERA_VERROU), &n, 4)) {
        printf("[HIST] camera du jeu introuvable : pas de verrou\n");
        g_cameraVerrouillee = 0;
        return;
    }
    n += verrou ? 1 : -1;
    SafeWrite((void*)(cam + CAMERA_VERROU), &n, 4);
    g_cameraVerrouillee = verrou ? cam : 0;
    printf("[HIST] camera %s (compteur %d)\n", verrou ? "verrouillee" : "liberee", n);
}

// Molette dans la liste (12e test) : la zone a defilement copiee ne recoit
// pas la molette (sa barre n'est pas une FrostpunkUIScrollBar). Crochet
// WH_GETMESSAGE sur le thread de l'interface (celui qui traite les messages
// Windows : minuterie du menu, menu_ap.cpp) : WM_MOUSEWHEEL est avale et
// converti en defilement.
// 13e-15e tests : la zone a defilement du jeu ne defile pas (tache 0x4F de
// FUN_1411fc420 envoyee, position et maximum toujours a 0) : le texte garde
// une hauteur de 1 (704 x 1, il deborde sans agrandir son contenu), donc pour
// le rendu tout tient dans la zone.
// 16e test : defilement fait main. Le layout du texte (enfant de la zone, qui
// decoupe son contenu) est deplace vers le haut de 'g_decalage' ; la borne
// vient du nombre de lignes du texte (on l'ecrit nous-memes) x hauteur de
// ligne (taille de police de la recette du texte x 1,25, 26 a defaut).
constexpr float MOLETTE_PAS = 60.f;          // unites d'interface par cran
constexpr float INTERLIGNE = 1.25f;          // hauteur de ligne / taille de police
constexpr float LIGNE_DEFAUT = 26.f;
constexpr float DEFIL_MARGE = 40.f;          // marge sous la derniere ligne
HHOOK g_crochetMolette = nullptr;
float g_decalage = 0.f;                      // defilement courant (0 = haut)
int   g_lignesTexte = 0;                     // lignes du texte de l'onglet
float g_posLayout[2] = {};                   // position d'origine du layout
bool  g_posLayoutLue = false;

float HauteurLigne()
{
    uintptr_t texte = ObjetDePoignee(g_texteDesc), rec = 0;
    int32_t police = 0;
    if (texte && SafeRead((void*)(texte + 0x1E8), &rec, 8) && rec > 0x10000)
        SafeRead((void*)(rec + 0x210), &police, 4);
    return (police >= 8 && police <= 64) ? police * INTERLIGNE : LIGNE_DEFAUT;
}

// Pose le layout au decalage 'd' borne a [0, hauteur du texte - zone].
void PoserDefilement(float d)
{
    uintptr_t layout = ObjetDePoignee(g_layoutDesc), zone = ObjetDePoignee(g_zoneDesc);
    if (!layout || !zone) return;
    if (!g_posLayoutLue) {
        SafeRead((void*)(layout + EL_POSITION), g_posLayout, 8);
        g_posLayoutLue = true;
    }
    float tailleZone[2] = {};
    SafeRead((void*)(zone + EL_TAILLE), tailleZone, 8);
    float maxi = g_lignesTexte * HauteurLigne() + DEFIL_MARGE - tailleZone[1];
    if (maxi < 0.f) maxi = 0.f;
    if (d > maxi) d = maxi;
    if (d < 0.f) d = 0.f;
    g_decalage = d;
    Placer(layout, g_posLayout[0], g_posLayout[1] - d);
    static int journal = 0;
    if (journal++ < 6)
        printf("[HIST] defilement %.0f / %.0f (%d ligne(s), ligne %.1f, zone %.0f)\n", d, maxi, g_lignesTexte,
            HauteurLigne(), tailleZone[1]);
}

// 'crans' > 0 : molette vers le haut (remonter la liste).
void DefilerListe(float crans)
{
    if (g_onglet == 2) return;
    PoserDefilement(g_decalage - crans * MOLETTE_PAS);
}

LRESULT CALLBACK CrochetMolette(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION && wp == PM_REMOVE && lp) {
        MSG* m = (MSG*)lp;
        if (m->message == WM_MOUSEWHEEL) {
            short crans = (short)HIWORD(m->wParam);
            DefilerListe((float)crans / WHEEL_DELTA);
            m->message = WM_NULL;   // ni le jeu ni la camera ne la voient
        }
    }
    return CallNextHookEx(g_crochetMolette, code, wp, lp);
}

void CrochetMoletteActif(bool actif)
{
    if (actif == (g_crochetMolette != nullptr)) return;
    if (actif) {
        g_crochetMolette = SetWindowsHookExW(WH_GETMESSAGE, &CrochetMolette, nullptr, GetCurrentThreadId());
        printf("[HIST] crochet de la molette %s (thread %lu)\n", g_crochetMolette ? "pose" : "KO",
            GetCurrentThreadId());
    } else {
        UnhookWindowsHookEx(g_crochetMolette);
        g_crochetMolette = nullptr;
        printf("[HIST] crochet de la molette retire\n");
    }
}

// Panneau ouvert = comme un panneau du jeu : pause, camera verrouillee,
// molette pour la liste.
void ModalHistorique(bool ouvert)
{
    PauseHistorique(ouvert);
    VerrouCamera(ouvert);
    CrochetMoletteActif(ouvert);
}

// Montrer AVEC animation (FUN_14105ac90(el, 1, anime = 1, forcer = 1)) : 6e
// test, panneau ouvert (journal) mais invisible avec l'affichage sans
// animation -> on laisse jouer l'apparition (OnShow) de la recette.
void MontrerAnime(uintptr_t el, bool visible)
{
    if (el < 0x10000) return;
    __try { ((Montrer_t)(Base() + OFF_MONTRER))(el, visible ? 1 : 0, 1, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a l'affichage anime de 0x%llX\n", GetExceptionCode(), (unsigned long long)el);
    }
}

// Releve du panneau construit : visibilite et couleur (+0x120, RGBA) des
// premiers niveaux, pour comprendre un panneau invisible.
void DecrirePanneau(uintptr_t el, int prof)
{
    if (el < 0x10000 || prof > 4 || g_lignes >= 120) return;
    char nom[64] = "", cls[96] = "?";
    float pos[2] = {}, taille[2] = {}, coul[4] = {};
    LireNom(el, EL_NOM, nom, sizeof(nom));
    GetClassNameFP(el, cls, sizeof(cls));
    SafeRead((void*)(el + EL_POSITION), pos, 8);
    SafeRead((void*)(el + EL_TAILLE), taille, 8);
    SafeRead((void*)(el + 0x120), coul, 16);
    printf("[HIST] %*s%s %s pos (%.0f, %.0f) taille %.0f x %.0f %s couleur (%.2f %.2f %.2f %.2f)\n", prof * 2, "",
        nom[0] ? nom : "-", cls, pos[0], pos[1], taille[0], taille[1], EstVisible(el) ? "visible" : "cache",
        coul[0], coul[1], coul[2], coul[3]);
    ++g_lignes;
    uintptr_t c = 0;
    SafeRead((void*)(el + EL_ENFANT), &c, 8);
    for (int garde = 0; c > 0x10000 && garde < 64; ++garde) {
        DecrirePanneau(c, prof + 1);
        if (!SafeRead((void*)(c + EL_FRERE), &c, 8)) break;
    }
}

// Onglet 2 (8e test, demande de l'auteur) : "Legends" = la legende complete
// des icones (celle d'abord posee en bas a gauche) : texte vide, pieces de la
// legende montrees ; cachees dans les autres onglets.
// 11e test : apres deux autres onglets, il fallait cliquer deux fois sur
// "Legends" (bit visible leve mais rien a l'ecran : l'affichage du moteur
// cumule les masquages). Les pieces restent donc toujours visibles et sont
// cachees en les placant hors de l'ecran.
void MontrerLegendePanneau(bool montrer)
{
    for (size_t i = 0; i < g_piecesLegendePanneau.size() && 2 * i + 1 < g_posLegendePanneau.size(); ++i) {
        uintptr_t el = ObjetDePoignee(g_piecesLegendePanneau[i]);
        if (!el) continue;
        Placer(el, g_posLegendePanneau[2 * i] + (montrer ? 0.f : LEGP_HORS_ECRAN), g_posLegendePanneau[2 * i + 1]);
    }
}

void AfficherOnglet(int onglet)
{
    g_onglet = onglet;
    uintptr_t texte = ObjetDePoignee(g_texteDesc);
    if (!texte) return;
    std::wstring t = onglet == 0 ? TexteRecus() : onglet == 1 ? TexteEnvoyes() : std::wstring(L" ");
    PoserTexte(texte, t.c_str());
    // 16e test : nouvel onglet = liste remontee en haut, bornes du defilement.
    g_lignesTexte = 1;
    for (wchar_t c : t) if (c == L'\n') ++g_lignesTexte;
    PoserDefilement(0.f);
    MontrerLegendePanneau(onglet == 2);
    LibellesListe();
    uintptr_t titre = TrouverParNom(ObjetDePoignee(g_panneau), "DESCRIPTION_TITLE");
    PoserTexte(titre, onglet == 0 ? L"Received items" : onglet == 1 ? L"Sent checks" : L"Archipelago icons");
    Relayout(ObjetDePoignee(g_layoutDesc));
    printf("[HIST] onglet %s : %d caractere(s)\n", onglet == 0 ? "recus" : onglet == 1 ? "envoyes" : "legendes",
        (int)t.size());
}

void OublierPanneau()
{
    LacherPoignee(g_panneau);
    LacherPoignee(g_texteDesc);
    LacherPoignee(g_layoutDesc);
    LacherPoignee(g_zoneDesc);
    for (uintptr_t& h : g_lignesListe) LacherPoignee(h);
    for (uintptr_t& h : g_piecesLegendePanneau) LacherPoignee(h);
    g_piecesLegendePanneau.clear();
    g_posLegendePanneau.clear();
    g_posLayoutLue = false;
    g_decalage = 0.f;
}

// Barre de defilement reliee a la zone du texte, comme le fait l'init du
// panneau des tutoriels (FUN_141bcf2e0) : FUN_141516ec0(barre, &poignee, 0,
// &nom vide). La barre garde sa propre reference sur la poignee.
void LierBarre(uintptr_t barre)
{
    uintptr_t vide = 0;
    __try { ((LierBarre_t)(Base() + OFF_LIER_BARRE))(barre, &g_zoneDesc, 0, &vide); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[HIST] EXCEPTION 0x%08lX a la liaison de la barre\n", GetExceptionCode());
        return;
    }
    __try { ((NomDetruire_t)(Base() + OFF_NOM_DETRUIRE))(&vide); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    printf("[HIST] barre de defilement reliee a la zone du texte\n");
}

// Copie du FRAME du panneau des tutoriels, ajoutee a la racine du HUD (par-
// dessus le reste). Les enfants de DESCRIPTION_LAYOUT autres que le texte
// (images, videos, schemas des tutoriels) ne sont pas instancies : mode 1
// le temps de l'appel.
bool ConstruirePanneau()
{
    uintptr_t racine = ObjetDePoignee(g_racineHud);
    if (!racine || !ModeleTutoriel()) return false;

    uintptr_t layoutRec = ChercherNoeud(g_tuto.frame, "DESCRIPTION_LAYOUT");
    struct Mode { uintptr_t noeud; int32_t mode; };
    std::vector<Mode> modes;
    uintptr_t tab = 0;
    int32_t c = 0;
    if (layoutRec && SafeRead((void*)(layoutRec + REC_ENFANTS), &tab, 8) &&
        SafeRead((void*)(layoutRec + REC_COMPTE), &c, 4) && tab > 0x10000 && c > 0 && c < 4096) {
        for (int i = 0; i < c; ++i) {
            uintptr_t e = 0;
            char n[96];
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e < 0x10000) continue;
            LireNom(e, REC_NOM, n, sizeof(n));
            if (StrEq(n, "DESCRIPTION_TEXT")) continue;
            Mode m = { e, 0 };
            SafeRead((void*)(e + REC_MODE), &m.mode, 4);
            modes.push_back(m);
            int32_t un = 1;
            SafeWrite((void*)(e + REC_MODE), &un, 4);
        }
    }
    JournalImmediat("[HIST] instanciation du panneau d'historique (%d enfant(s) de description ecarte(s)) ...\n",
        (int)modes.size());
    uintptr_t el = InstancierBrut(g_tuto.frame, g_tuto.res);
    for (const Mode& m : modes) SafeWrite((void*)(m.noeud + REC_MODE), &m.mode, 4);
    if (!el) { printf("[HIST] instanciation du panneau KO\n"); return false; }
    if (!AjouterEnfant(racine, el)) return false;
    OublierPanneau();
    g_panneau = PrendrePoignee(el);
    DecrireElement("panneau", el);

    // En-tete.
    uintptr_t entete = TrouverParNom(el, "COLUMN_HEADER");
    PoserTexte(entete ? TrouverParNom(entete, "TEXT") : 0, L"ARCHIPELAGO");
    Montrer(TrouverParNom(el, "DESCRIPTION_TITLE"), true);

    // Liste : la ligne de la recette + une copie (recus, envoyes). Plus de
    // ligne "Close" (auteur, 6e test : il y a deja une fermeture native) ;
    // tout element "CLOSE" du panneau ferme le panneau.
    uintptr_t ligne = TrouverParNom(el, "SUMMARY_ROW");
    uintptr_t layoutListe = 0;
    if (ligne) SafeRead((void*)(ligne + EL_PARENT), &layoutListe, 8);
    void (*rappels[3])(uintptr_t, uintptr_t, uintptr_t, uintptr_t) = { &CbRecus, &CbEnvoyes, &CbLegendes };
    for (int i = 0; i < 3 && ligne; ++i) {
        uintptr_t l = i == 0 ? ligne : Cloner(ligne);
        if (!l) { printf("[HIST] copie de la ligne %d KO\n", i); continue; }
        if (i > 0) AjouterEnfant(layoutListe, l);
        BrancherClic(l, rappels[i]);
        Montrer(l, true);
        g_lignesListe[i] = PrendrePoignee(l);
    }
    Relayout(layoutListe);
    if (!ligne) printf("[HIST] pas de SUMMARY_ROW : liste vide\n");
    uintptr_t fermer = TrouverNomContenant(el, "CLOSE");
    if (fermer) { BrancherClic(fermer, &CbFermer); DecrireElement("fermeture native du panneau", fermer); }
    else printf("[HIST] panneau sans element CLOSE : fermeture par le bouton du HUD\n");

    // Texte qui defile et sa barre.
    uintptr_t texte = TrouverParNom(el, "DESCRIPTION_TEXT");
    uintptr_t layout = TrouverParNom(el, "DESCRIPTION_LAYOUT");
    uintptr_t zone = TrouverParNom(el, "DESCRIPTION_SCROLL_PANE");
    uintptr_t barre = TrouverParNom(el, "DESCRIPTION_SCROLL_BAR");
    g_texteDesc = PrendrePoignee(texte);
    g_layoutDesc = PrendrePoignee(layout);
    g_zoneDesc = PrendrePoignee(zone);
    if (barre && ClasseEst(barre, "FrostpunkUIScrollBar") && g_zoneDesc) {
        LierBarre(barre);
    } else {
        printf("[HIST] barre de defilement absente ou inattendue (0x%llX) : cachee\n", (unsigned long long)barre);
        Montrer(barre, false);
    }
    DecrireElement("texte", texte);
    DecrireElement("zone", zone);
    // Legende de l'onglet "Legends" : dans le parent de la zone a defilement,
    // a l'emplacement du texte ; cachee hors de l'onglet (AfficherOnglet).
    uintptr_t parentZone = 0;
    float posZone[2] = {};
    if (zone && SafeRead((void*)(zone + EL_PARENT), &parentZone, 8) && parentZone > 0x10000) {
        SafeRead((void*)(zone + EL_POSITION), posZone, 8);
        // 9e test : icones sur la bordure de la liste, haut du cadre -> +40 / +30.
        ConstruireLegendeListe(parentZone, posZone[0] + LEGP_X, posZone[1] + LEGP_Y, g_piecesLegendePanneau);
        MontrerLegendePanneau(false);
    }
    printf("[HIST] panneau d'historique construit\n");
    return true;
}

void BasculerPanneau()
{
    uintptr_t p = ObjetDePoignee(g_panneau);
    if (!p) {
        if (!ConstruirePanneau()) return;
        p = ObjetDePoignee(g_panneau);
        MontrerAnime(p, true);
        ModalHistorique(true);
        AfficherOnglet(0);
#ifndef AP_RELEASE
        g_lignes = 0;
        printf("[HIST] panneau apres ouverture :\n");
        DecrirePanneau(p, 0);
#endif
        return;
    }
    bool visible = !EstVisible(p);
    MontrerAnime(p, visible);
    ModalHistorique(visible);
    if (!visible) MontrerLegendePanneau(false);
    if (visible) AfficherOnglet(g_onglet);
    printf("[HIST] panneau %s\n", visible ? "ouvert" : "ferme");
}

void CbBouton(uintptr_t, uintptr_t, uintptr_t, uintptr_t) { BasculerPanneau(); }
void CbRecus(uintptr_t, uintptr_t, uintptr_t, uintptr_t) { AfficherOnglet(0); }
void CbEnvoyes(uintptr_t, uintptr_t, uintptr_t, uintptr_t) { AfficherOnglet(1); }
void CbLegendes(uintptr_t, uintptr_t, uintptr_t, uintptr_t) { AfficherOnglet(2); }
void CbFermer(uintptr_t, uintptr_t, uintptr_t, uintptr_t)
{
    MontrerAnime(ObjetDePoignee(g_panneau), false);
    ModalHistorique(false);
    MontrerLegendePanneau(false);
    printf("[HIST] panneau ferme\n");
}

// --- Bouton du HUD --------------------------------------------------------------------
// Taille de police des textes d'une recette (UITextRecipe +0x210, "Font
// size", releve de la reflexion dans menu_connexion.md ; type deduit de la
// valeur : flottant ou entier plausible entre 4 et 200).
constexpr int REC_TEXTE_POLICE = 0x210;
struct PoliceSauvee { uintptr_t noeud; uint32_t brut; };
struct PolicesTextes { PoliceSauvee t[16]; int n = 0; };

void ReduirePolices(uintptr_t noeud, PolicesTextes& p, int prof)
{
    if (noeud < 0x10000 || prof > 12 || p.n >= 16) return;
    char cls[96] = "";
    GetClassNameFP(noeud, cls, sizeof(cls));
    if (strstr(cls, "UIText")) {
        uint32_t brut = 0;
        if (SafeRead((void*)(noeud + REC_TEXTE_POLICE), &brut, 4)) {
            float f;
            memcpy(&f, &brut, 4);
            uint32_t nouveau = brut;
            if (f > 4.f && f < 200.f) { float g = f / BOUTON_ECHELLE; memcpy(&nouveau, &g, 4); }
            else if ((int32_t)brut > 4 && (int32_t)brut < 200) nouveau = (uint32_t)((int32_t)brut / BOUTON_ECHELLE);
            printf("[HIST] police d'un texte du bouton (%s) : brut 0x%08X (flottant %.1f / entier %d)%s\n", cls, brut,
                f, (int32_t)brut, nouveau != brut ? " -> reduite" : " -> inchangee");
            if (nouveau != brut) {
                p.t[p.n++] = { noeud, brut };
                SafeWrite((void*)(noeud + REC_TEXTE_POLICE), &nouveau, 4);
            }
        }
    }
    uintptr_t tab = 0;
    int32_t c = 0;
    if (!SafeRead((void*)(noeud + REC_ENFANTS), &tab, 8) || !SafeRead((void*)(noeud + REC_COMPTE), &c, 4) ||
        tab < 0x10000 || c <= 0 || c > 4096) return;
    for (int i = 0; i < c; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8)) ReduirePolices(e, p, prof + 1);
    }
}

void RemettrePolices(const PolicesTextes& p)
{
    for (int i = 0; i < p.n; ++i) SafeWrite((void*)(p.t[i].noeud + REC_TEXTE_POLICE), &p.t[i].brut, 4);
}

// Copie d'INGAME_MENU_BUTTON dans le meme parent (TOP_LEFT_BUTTONS), decalee
// de (BOUTON_DX, BOUTON_DY), icone AP, textes "Archipelago history".
void ConstruireBouton(uintptr_t racine)
{
    uintptr_t menu = TrouverParNom(racine, "INGAME_MENU_BUTTON");
    uintptr_t parent = 0;
    if (!menu || !SafeRead((void*)(menu + EL_PARENT), &parent, 8) || parent < 0x10000) {
        printf("[HIST] INGAME_MENU_BUTTON introuvable : pas de bouton d'historique\n");
        return;
    }
    // 6e test : bouton deux fois plus grand (demande de l'auteur). Echelle de
    // la recette (UIElementRecipe +0xC0, 2 flottants) doublee le temps de
    // l'instanciation (donnees partagees : valeur remise).
    uintptr_t rec = 0;
    float echelle[2] = {}, double_[2] = {};
    bool echelleLue = SafeRead((void*)(menu + EL_RECETTE), &rec, 8) && rec > 0x10000 &&
                      SafeRead((void*)(rec + REC_ECHELLE), echelle, 8);
    if (echelleLue) {
        double_[0] = echelle[0] * BOUTON_ECHELLE;
        double_[1] = echelle[1] * BOUTON_ECHELLE;
        SafeWrite((void*)(rec + REC_ECHELLE), double_, 8);
    }
    // 7e test : le libelle qui se deploie au survol grossissait avec
    // l'echelle -> taille de police des textes de la recette divisee par
    // l'echelle le temps de l'instanciation (remise ensuite).
    PolicesTextes polices;
    if (echelleLue) ReduirePolices(rec, polices, 0);
    uintptr_t b = Cloner(menu);
    RemettrePolices(polices);
    if (echelleLue) SafeWrite((void*)(rec + REC_ECHELLE), echelle, 8);
    printf("[HIST] echelle de la recette du bouton : (%.2f, %.2f)%s\n", echelle[0], echelle[1],
        echelleLue ? "" : " (illisible)");
    if (!b) { printf("[HIST] copie du bouton du menu KO\n"); return; }
    // 4e test (2026-11-21) : sans animateur, la copie est INVISIBLE (ses
    // images ne sont rendues visibles que par l'etat joue). On garde donc
    // l'animateur (qui impose la meme translation que le bouton du menu) et
    // on decale un CONTENEUR vide qui porte la copie. 6e test : la copie de
    // DESCRIPTION_VIDEO dessinait un rectangle blanc de 704 x 208 qui captait
    // les clics -> copie de "LEFT" du HUD (UIElement 64 x 64 sans enfant),
    // taille nulle.
    uintptr_t conteneur = 0;
    if (ModeleTutoriel()) {
        uintptr_t noeud = ChercherNoeud(g_tuto.racine, "DESCRIPTION_VIDEO");
        conteneur = noeud ? InstancierNoeud(noeud, g_tuto.res) : 0;
    }
    if (conteneur) {
        uintptr_t zero = 0;
        SafeWrite((void*)(conteneur + EL_RECETTE), &zero, 8);
        if (!AjouterEnfant(parent, conteneur)) return;
        Montrer(conteneur, true);
        Dimensionner(conteneur, 0.f, 0.f);
        Placer(conteneur, BOUTON_DX, BOUTON_DY);
        if (!AjouterEnfant(conteneur, b)) return;
        DecrireElement("conteneur du bouton", conteneur);
    } else {
        printf("[HIST] pas de conteneur pour le bouton : pose directe (decalage impose par l'animation)\n");
        if (!AjouterEnfant(parent, b)) return;
    }
    PoserTousLesTextes(b, L"Archipelago history");
    uintptr_t icone = TrouverParNom(b, "icon");
    if (!icone || !IconeAPPoser(icone, BOUTON_ICONE)) printf("[HIST] icone du bouton non posee\n");
    // 6e test : icone trop transparente (etat de repos du bouton du menu) ->
    // icone opaque, animateur de l'icone seule retire pour que ca tienne.
    if (icone) {
        static const float opaque[4] = { 1.f, 1.f, 1.f, 1.f };
        uintptr_t zero = 0;
        SafeWrite((void*)(icone + EL_RECETTE), &zero, 8);
        PoserCouleurEl(icone, opaque);
        Montrer(icone, true);
    }
    BrancherClic(b, &CbBouton);
    Montrer(b, true);
    DecrireElement("bouton d'historique", b);
}

// --- Legende des icones ---------------------------------------------------------------
// Copie d'un noeud du modele, animateur d'etat retire (comme les icones des
// lois, lois_ap.cpp : sinon les etats du parent la reteignent), ajoutee a
// 'parent', montree, placee.
uintptr_t PiecePosee(uintptr_t noeud, uintptr_t parent, float x, float y)
{
    uintptr_t el = InstancierNoeud(noeud, g_tuto.res);
    if (!el) return 0;
    uintptr_t zero = 0;
    SafeWrite((void*)(el + EL_RECETTE), &zero, 8);
    if (!AjouterEnfant(parent, el)) return 0;
    Montrer(el, true);
    Placer(el, x, y);
    return el;
}

// Texte de la legende : copie d'un texte du modele, alignee a GAUCHE et sans
// majuscules forcees. 1er essai (2026-11-21) : TEXT de SUMMARY_ROW aligne a
// droite et en majuscules, DESCRIPTION_TITLE centre loin a droite. Les deux
// reglages vivent dans la recette (UITextRecipe +0x214 alignement horizontal,
// 0 = gauche ; +0x24C majuscules) et sont copies a l'instanciation : on les
// change le temps de l'appel (donnees partagees : valeurs remises).
constexpr int REC_TEXTE_ALIGN_H = 0x214, REC_TEXTE_MAJ = 0x24C;

uintptr_t TextePose(uintptr_t noeud, uintptr_t parent, float x, float y, float largeur, bool majuscules)
{
    if (noeud < 0x10000) return 0;
    int32_t align = 0, gauche = 0;
    uint8_t maj = 0, voulu = majuscules ? 1 : 0;
    SafeRead((void*)(noeud + REC_TEXTE_ALIGN_H), &align, 4);
    SafeRead((void*)(noeud + REC_TEXTE_MAJ), &maj, 1);
    SafeWrite((void*)(noeud + REC_TEXTE_ALIGN_H), &gauche, 4);
    SafeWrite((void*)(noeud + REC_TEXTE_MAJ), &voulu, 1);
    uintptr_t t = PiecePosee(noeud, parent, x, y);
    SafeWrite((void*)(noeud + REC_TEXTE_ALIGN_H), &align, 4);
    SafeWrite((void*)(noeud + REC_TEXTE_MAJ), &maj, 1);
    float taille[2] = {};
    if (t) SafeRead((void*)(t + EL_TAILLE), taille, 8);
    if (t) Dimensionner(t, largeur, taille[1]);
    return t;
}

// Image modele des icones : "icon" du bouton du menu en jeu (UIPicture 32 x 32,
// visible dans le HUD ; sa copie a montre l'icone AP au 1er essai). Au 1er
// essai, "small_deco_arrow" des tutoriels restait invisible.
uintptr_t ImageModele()
{
    uintptr_t racine = ObjetDePoignee(g_racineHud);
    uintptr_t menu = racine ? TrouverParNom(racine, "INGAME_MENU_BUTTON") : 0;
    return menu ? TrouverParNom(menu, "icon") : 0;
}

// Pieces de la legende de la carte : montrees seulement quand WORLD_MAP_HUD
// l'est (1er essai : visible en ville, la visibilite du parent ne s'applique
// pas a nos copies).
std::vector<uintptr_t> g_piecesCarte;   // poignees
int g_carteVisible = -1;

void ConstruireLegende(uintptr_t parent, const PlaceLegende& place, std::vector<uintptr_t>* pieces = nullptr)
{
    if (parent < 0x10000 || !ModeleTutoriel()) return;
    DecrireElement(place.ecran, parent);
    uintptr_t modeleImage = ImageModele();
    float x = place.x, y = place.y;
    float h = LEG_ENTETE + NB_LEGENDE * LEG_PAS + 10.f;
    std::vector<uintptr_t> poses;
    uintptr_t fond = g_tuto.centre ? PiecePosee(g_tuto.centre, parent, x, y) : 0;
    Dimensionner(fond, LEG_LARGEUR, h);
    poses.push_back(fond);
    uintptr_t titre = TextePose(g_tuto.texteLigne, parent, x + 14.f, y + 8.f, LEG_LARGEUR - 28.f, true);
    PoserTexte(titre, L"Archipelago icons");
    poses.push_back(titre);
    int icones = 0;
    for (int i = 0; i < NB_LEGENDE; ++i) {
        float ly = y + LEG_ENTETE + i * LEG_PAS;
        uintptr_t img = modeleImage ? IconeAPSurCopieDe(modeleImage, parent, LEGENDE[i].icone) : 0;
        if (img) {
            ++icones;
            Dimensionner(img, LEG_ICONE, LEG_ICONE);
            Placer(img, x + 14.f, ly);
        }
        poses.push_back(img);
        uintptr_t t = TextePose(g_tuto.texteCorps, parent, x + 22.f + LEG_ICONE, ly + 2.f,
            LEG_LARGEUR - 36.f - LEG_ICONE, false);
        PoserTexte(t, LEGENDE[i].sens);
        poses.push_back(t);
    }
    if (pieces)
        for (uintptr_t p : poses) if (p) pieces->push_back(PrendrePoignee(p));
    DecrireElement("legende : titre", titre);
    printf("[HIST] legende \"%s\" en (%.0f, %.0f) : fond 0x%llX, titre 0x%llX, %d/%d icone(s), image modele 0x%llX\n",
        place.ecran, x, y, (unsigned long long)fond, (unsigned long long)titre, icones, NB_LEGENDE,
        (unsigned long long)modeleImage);
}

// Legende complete en liste (onglet "Legends" du panneau d'historique) :
// lignes icone + sens, sans fond ni titre (le panneau a les siens), puis les
// paragraphes d'explication (11e test). Positions notees dans
// g_posLegendePanneau (cache = hors de l'ecran, MontrerLegendePanneau).
void ConstruireLegendeListe(uintptr_t parent, float x, float y, std::vector<uintptr_t>& pieces)
{
    if (parent < 0x10000 || !ModeleTutoriel()) return;
    uintptr_t modeleImage = ImageModele();
    int icones = 0;
    auto noter = [&](uintptr_t el, float px, float py) {
        if (!el) return;
        pieces.push_back(PrendrePoignee(el));
        g_posLegendePanneau.push_back(px);
        g_posLegendePanneau.push_back(py);
    };
    float pas = LEG_PAS + 8.f;
    for (int i = 0; i < NB_LEGENDE; ++i) {
        float ly = y + i * pas;
        uintptr_t img = modeleImage ? IconeAPSurCopieDe(modeleImage, parent, LEGENDE[i].icone) : 0;
        if (img) {
            ++icones;
            Dimensionner(img, LEG_ICONE + 6.f, LEG_ICONE + 6.f);
            Placer(img, x, ly);
            noter(img, x, ly);
        }
        float tx = x + LEG_ICONE + 18.f, ty = ly + LEGT_TEXTE_DY + 16.f;
        uintptr_t t = TextePose(g_tuto.texteCorps, parent, tx, ty, 420.f, false);
        PoserTexte(t, LEGENDE[i].sens);
        noter(t, tx, ty);
    }
    // Paragraphes : une seule zone de texte, sous la derniere ligne.
    float py = y + NB_LEGENDE * pas + 16.f;
    uintptr_t p = TextePose(g_tuto.texteCorps, parent, x, py, LEGP_LARGEUR_TEXTE, false);
    PoserTexte(p, LEGENDE_PARAGRAPHES);
    noter(p, x, py);
    printf("[HIST] legende de l'onglet Legends en (%.0f, %.0f) : %d/%d icone(s), paragraphes %s\n", x, y, icones,
        NB_LEGENDE, p ? "poses" : "KO");
}

// Pompe : pieces de la legende de la carte suivant la visibilite de
// WORLD_MAP_HUD (transitions journalisees).
void SuivreCarte()
{
    if (g_piecesCarte.empty()) return;
    uintptr_t racine = ObjetDePoignee(g_racineHud);
    uintptr_t carte = racine ? TrouverParNom(racine, "WORLD_MAP_HUD") : 0;
    int visible = EstVisible(carte) ? 1 : 0;
    if (visible == g_carteVisible) return;
    g_carteVisible = visible;
    for (uintptr_t h : g_piecesCarte) Montrer(ObjetDePoignee(h), visible != 0);
    printf("[HIST] WORLD_MAP_HUD %s : legende de la carte %s\n", visible ? "visible" : "cache",
        visible ? "montree" : "cachee");
}

// --- Legende de l'arbre des technos -------------------------------------------------
void DecrireArbreElements(uintptr_t el, int prof, int profMax);   // releve, plus bas
typedef void (*PoserCouleur_t)(uintptr_t, const float*);
static const uintptr_t OFF_POSER_COULEUR = Adr(0x10297F0);   // FUN_1410297f0

// Premier element dont le nom CONTIENT 'sous' (profondeur d'abord).
uintptr_t TrouverNomContenant(uintptr_t el, const char* sous, int prof)
{
    if (el < 0x10000 || prof > 16) return 0;
    uintptr_t c = 0;
    SafeRead((void*)(el + EL_ENFANT), &c, 8);
    for (int garde = 0; c > 0x10000 && garde < 512; ++garde) {
        char n[64];
        LireNom(c, EL_NOM, n, sizeof(n));
        if (n[0] && strstr(n, sous)) return c;
        uintptr_t r = TrouverNomContenant(c, sous, prof + 1);
        if (r) return r;
        if (!SafeRead((void*)(c + EL_FRERE), &c, 8)) break;
    }
    return 0;
}

// Anneau de "Tech Received" : copie (depuis sa recette) d'une image d'un vrai
// noeud de l'arbre dont le nom evoque l'anneau, teinte bleue des noeuds. Les
// noeuds n'existent pas encore a l'init du panneau (releve du 2026-11-21) :
// pose par la pompe des que le panneau en a construit (IconesTechnoPanneau).
uintptr_t g_parentLegendeTech = 0;     // poignee : parent des pieces de la legende
float     g_posAnneau[2] = {};
bool      g_anneauFait = true;

void PoserCouleurEl(uintptr_t el, const float* c)
{
    if (el < 0x10000) return;
    __try { ((PoserCouleur_t)(Base() + OFF_POSER_COULEUR))(el, c); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

bool NomEvoqueAnneau(const char* n)
{
    static const char* const mots[] = { "ring", "Ring", "RING", "circle", "Circle", "CIRCLE", "frame", "FRAME",
                                        "stroke", "border", "outline" };
    for (const char* m : mots) if (strstr(n, m)) return true;
    return false;
}

uintptr_t ImageAnneau(uintptr_t el, int prof = 0)
{
    if (el < 0x10000 || prof > 8) return 0;
    uintptr_t c = 0;
    SafeRead((void*)(el + EL_ENFANT), &c, 8);
    for (int garde = 0; c > 0x10000 && garde < 256; ++garde) {
        char n[64];
        LireNom(c, EL_NOM, n, sizeof(n));
        if (n[0] && NomEvoqueAnneau(n) && ClasseEst(c, "UIPicture")) return c;
        uintptr_t r = ImageAnneau(c, prof + 1);
        if (r) return r;
        if (!SafeRead((void*)(c + EL_FRERE), &c, 8)) break;
    }
    return 0;
}

void LegendeTechnosAnneau()
{
    if (g_anneauFait) return;
    uintptr_t parent = ObjetDePoignee(g_parentLegendeTech);
    uintptr_t panneau = IconesTechnoPanneau(), liste = 0, hNoeud = 0;
    int32_t n = 0;
    if (!parent) { g_anneauFait = true; return; }
    if (!panneau || !SafeRead((void*)(panneau + 0x1A8), &liste, 8) || !SafeRead((void*)(panneau + 0x1B0), &n, 4) ||
        liste < 0x10000 || n <= 0 || n > 1024) return;
    // 4e test (2026-11-21) : le choix par le nom tombait sur "highlight_circle"
    // (point plein). Noeud = GOAL_ENTRY > visibility_control > ...,
    // "researched_stroke" (70 x 70, ancre haut-gauche) = anneau des technos
    // recherchees. 6e test : le 1er noeud n'en avait pas, le repli par le nom
    // a pris "selection_circle" (encore un point) -> TOUS les noeuds sont
    // parcourus, nom exact seulement ; sinon on attend le noeud suivant.
    uintptr_t image = 0;
    for (int i = 0; i < n && !image; ++i) {
        if (!SafeRead((void*)(liste + (uintptr_t)i * 0x80 + 0x10), &hNoeud, 8)) continue;
        uintptr_t noeud = ObjetDePoignee(hNoeud);
        if (noeud > 0x10000) image = TrouverParNom(noeud, "researched_stroke");
    }
    if (!image) return;
    g_anneauFait = true;
    uintptr_t copie = Cloner(image);
    if (!copie) { printf("[HIST] copie de researched_stroke KO : pas d'anneau\n"); return; }
    uintptr_t zero = 0;
    SafeWrite((void*)(copie + EL_RECETTE), &zero, 8);
    if (!AjouterEnfant(parent, copie)) return;
    Montrer(copie, true);
    Dimensionner(copie, LEG_ICONE + 6.f, LEG_ICONE + 6.f);
    Placer(copie, g_posAnneau[0], g_posAnneau[1]);
    PoserCouleurEl(copie, LEGT_TEINTE);
    DecrireElement("technos : modele de l'anneau", image);
    DecrireElement("technos : anneau", copie);
}

void ConstruireLegendeTechnos(uintptr_t racine)
{
    if (racine < 0x10000 || !ModeleTutoriel()) return;
#ifndef AP_RELEASE
    static bool releve = false;   // arbre du panneau, une fois par lancement (reglage de la position)
    if (!releve) {
        releve = true;
        g_lignes = 0;
        printf("[HIST] arbre du panneau des technos :\n");
        DecrireArbreElements(racine, 0, 6);
    }
#endif
    uintptr_t fermer = TrouverNomContenant(racine, "CLOSE");
    uintptr_t parent = 0;
    if (!fermer || !SafeRead((void*)(fermer + EL_PARENT), &parent, 8) || parent < 0x10000) {
        printf("[HIST] technos : aucun bouton CLOSE, pas de legende\n");
        return;
    }
    float pos[2] = {}, taille[2] = {}, decal[2] = {};
    SafeRead((void*)(fermer + EL_POSITION), pos, 8);
    SafeRead((void*)(fermer + EL_TAILLE), taille, 8);
    if (ClasseEst(parent, "UILayout")) {           // un layout replacerait nos pieces
        SafeRead((void*)(parent + EL_POSITION), decal, 8);
        uintptr_t gp = 0;
        SafeRead((void*)(parent + EL_PARENT), &gp, 8);
        if (gp > 0x10000) parent = gp;
    }
    DecrireElement("technos : bouton de fermeture", fermer);
    DecrireElement("technos : parent de la legende", parent);
    uintptr_t modeleImage = ImageModele();
    float x0 = decal[0] + pos[0] - LEGT_LARGEUR, yc = decal[1] + pos[1] + taille[1] / 2.f;
    float yi = yc - LEG_ICONE / 2.f, yt = yc + LEGT_TEXTE_DY;

    uintptr_t icone = modeleImage ? IconeAPSurCopieDe(modeleImage, parent, "icon_done") : 0;
    if (icone) { Dimensionner(icone, LEG_ICONE, LEG_ICONE); Placer(icone, x0, yi); }
    uintptr_t t1 = TextePose(g_tuto.texteCorps, parent, x0 + LEG_ICONE + 6.f, yt, LEGT_TEXTE, false);
    PoserTexte(t1, L"Check Sended");

    float x1 = x0 + LEGT_ENTREE;
    uintptr_t t2 = TextePose(g_tuto.texteCorps, parent, x1 + LEG_ICONE + 6.f, yt, LEGT_TEXTE, false);
    PoserTexte(t2, L"Tech Received");
    // Anneau : pose par la pompe quand les noeuds existent.
    LacherPoignee(g_parentLegendeTech);
    g_parentLegendeTech = PrendrePoignee(parent);
    // 7e test : anneau ~17 trop bas (et collant au texte) -> remonte, decale.
    g_posAnneau[0] = x1 - 7.f;
    g_posAnneau[1] = yi - 20.f;
    g_anneauFait = false;

    DecrireElement("technos : texte 1", t1);
    DecrireElement("technos : texte 2", t2);
    printf("[HIST] legende des technos en (%.0f, %.0f) : icone 0x%llX (anneau a venir)\n", x0, yc,
        (unsigned long long)icone);
}

// --- Legende du livre des lois -------------------------------------------------------
// Demande de l'auteur (8e test) : comme l'arbre des technos, mais la 2e entree
// est l'icone "signee" du jeu (plume "circle_inside" des noeuds, teinte d'etat
// des lois signees, lois_ap.cpp) avec "Signed" (texte en jeu en anglais).
// Construite a la construction du livre (circle_inside y est capture), une
// fois par panneau, dans TREE_VIEW, a gauche de CUSTOM_CLOSE_BUTTON.
uintptr_t g_livreLegende = 0;

void ConstruireLegendeLivre(uintptr_t livre)
{
    if (livre < 0x10000 || livre == g_livreLegende || !ModeleTutoriel()) return;
    uintptr_t hRacine = 0, hArbre = 0;
    SafeRead((void*)(livre + PAN_RACINE), &hRacine, 8);
    SafeRead((void*)(livre + LIVRE_ARBRE), &hArbre, 8);
    uintptr_t racine = ObjetDePoignee(hRacine), arbre = ObjetDePoignee(hArbre);
    char nomArbre[64] = "";
    if (arbre) LireNom(arbre, EL_NOM, nomArbre, sizeof(nomArbre));
    if (!racine || !StrEq(nomArbre, "TREE_VIEW")) {
        printf("[HIST] livre 0x%llX : racine 0x%llX, vue \"%s\" : pas de legende\n", (unsigned long long)livre,
            (unsigned long long)racine, nomArbre);
        return;
    }
    uintptr_t fermer = TrouverParNom(racine, "CUSTOM_CLOSE_BUTTON");
    if (!fermer) fermer = TrouverNomContenant(racine, "CLOSE");
    if (!fermer) { printf("[HIST] livre : aucun bouton de fermeture, pas de legende\n"); return; }
    uintptr_t rec = 0, res = 0;
    if (!LoisModeleSigne(&rec, &res)) { printf("[HIST] livre : plume circle_inside pas encore connue\n"); return; }
    g_livreLegende = livre;

    float pos[2] = {}, taille[2] = {};
    SafeRead((void*)(fermer + EL_POSITION), pos, 8);
    SafeRead((void*)(fermer + EL_TAILLE), taille, 8);
    DecrireElement("livre : bouton de fermeture", fermer);
    DecrireElement("livre : parent de la legende", arbre);
    float x0 = pos[0] - LEGL_LARGEUR, yc = pos[1] + taille[1] / 2.f + LEGL_DY;
    float yi = yc - LEG_ICONE / 2.f, yt = yc + LEGT_TEXTE_DY;

    uintptr_t modeleImage = ImageModele();
    uintptr_t icone = modeleImage ? IconeAPSurCopieDe(modeleImage, arbre, "icon_done") : 0;
    if (icone) { Dimensionner(icone, LEG_ICONE, LEG_ICONE); Placer(icone, x0, yi); }
    uintptr_t t1 = TextePose(g_tuto.texteCorps, arbre, x0 + LEG_ICONE + 6.f, yt, LEGT_TEXTE, false);
    PoserTexte(t1, L"Check Sended");

    float x1 = x0 + LEGT_ENTREE;
    uintptr_t plume = InstancierBrut(rec, res);
    if (plume) {
        uintptr_t zero = 0;
        SafeWrite((void*)(plume + EL_RECETTE), &zero, 8);   // sans animateur (comme lois_ap.cpp)
        if (AjouterEnfant(arbre, plume)) {
            // 9e test : plume minuscule et ~20 trop bas -> 40 x 40, remontee.
            Montrer(plume, true);
            Dimensionner(plume, LEGL_PLUME, LEGL_PLUME);
            Placer(plume, x1 - (LEGL_PLUME - LEG_ICONE) / 2.f, yi - (LEGL_PLUME - LEG_ICONE) / 2.f - 20.f);
            PoserCouleurEl(plume, LEGT_TEINTE);
        }
    }
    uintptr_t t2 = TextePose(g_tuto.texteCorps, arbre, x1 + LEG_ICONE + 6.f, yt, LEGL_TEXTE2, false);
    PoserTexte(t2, L"Signed and effective");
    DecrireElement("livre : plume", plume);
    DecrireElement("livre : texte 2", t2);
    printf("[HIST] legende du livre en (%.0f, %.0f) : icone 0x%llX, plume 0x%llX\n", x0, yc,
        (unsigned long long)icone, (unsigned long long)plume);
}

void OublierPiecesCarte()
{
    for (uintptr_t& h : g_piecesCarte) LacherPoignee(h);
    g_piecesCarte.clear();
    g_carteVisible = -1;
}

// --- Releve : arbre des elements vivants (diagnostic) ----------------------------------
void DecrireArbreElements(uintptr_t el, int prof, int profMax)
{
    if (el < 0x10000 || prof > profMax || g_lignes >= 1500) return;
    char nom[64] = "", cls[96] = "?";
    float pos[2] = {}, taille[2] = {};
    LireNom(el, EL_NOM, nom, sizeof(nom));
    GetClassNameFP(el, cls, sizeof(cls));
    SafeRead((void*)(el + EL_POSITION), pos, 8);
    SafeRead((void*)(el + EL_TAILLE), taille, 8);
    printf("[HIST] %*s%s %s pos (%.0f, %.0f) taille %.0f x %.0f %s\n", prof * 2, "", nom[0] ? nom : "-", cls,
        pos[0], pos[1], taille[0], taille[1], EstVisible(el) ? "visible" : "cache");
    ++g_lignes;
    uintptr_t enfant = 0;
    SafeRead((void*)(el + EL_ENFANT), &enfant, 8);
    for (int garde = 0; enfant > 0x10000 && garde < 256; ++garde) {
        DecrireArbreElements(enfant, prof + 1, profMax);
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
}

void DecrireArbreRecette(uintptr_t noeud, int prof, int profMax)
{
    if (noeud < 0x10000 || prof > profMax || g_lignes >= 1500) return;
    char nom[96] = "", cls[96] = "?";
    float pos[2] = {}, ancre[2] = {};
    int32_t mode = -1, c = 0;
    uintptr_t tab = 0;
    LireNom(noeud, REC_NOM, nom, sizeof(nom));
    GetClassNameFP(noeud, cls, sizeof(cls));
    SafeRead((void*)(noeud + REC_POSITION), pos, 8);
    SafeRead((void*)(noeud + REC_ANCRE), ancre, 8);
    SafeRead((void*)(noeud + REC_MODE), &mode, 4);
    SafeRead((void*)(noeud + REC_ENFANTS), &tab, 8);
    SafeRead((void*)(noeud + REC_COMPTE), &c, 4);
    printf("[HIST] %*s%s %s pos (%.0f, %.0f) ancre (%.2f, %.2f) mode %d\n", prof * 2, "", nom[0] ? nom : "-", cls,
        pos[0], pos[1], ancre[0], ancre[1], mode);
    ++g_lignes;
    if (tab < 0x10000 || c <= 0 || c > 4096) return;
    for (int i = 0; i < c; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8)) DecrireArbreRecette(e, prof + 1, profMax);
    }
}

void ReleverRecette(const char* chemin)
{
    uintptr_t racine = 0;
    uintptr_t res = RessourceRecette(chemin, &racine);
    printf("[HIST] === recette \"%s\" : ressource 0x%llX, racine 0x%llX\n", chemin,
        (unsigned long long)res, (unsigned long long)racine);
    g_lignes = 0;
    DecrireArbreRecette(racine, 0, 12);
    printf("[HIST] recette \"%s\" : %d noeud(s) decrits\n", chemin, g_lignes);
}

void Relever()
{
    uintptr_t racine = ObjetDePoignee(g_racineHud);
    printf("[HIST] ===== releve de l'interface en jeu (racine FRAME 0x%llX) =====\n",
        (unsigned long long)racine);
    if (racine > 0x10000) {
        g_lignes = 0;
        __try { DecrireArbreElements(racine, 0, 8); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            printf("[HIST] EXCEPTION 0x%08lX pendant le releve du HUD\n", GetExceptionCode());
        }
        printf("[HIST] HUD : %d element(s) decrits\n", g_lignes);
    }
    static const char* const RECETTES[] = {
        "UI/Panels/General/MessagePanel",
        "UI/Panels/General/QuestPanel",
        "UI/Panels/City/TutorialSummaryPanel",
        "UI/Panels/Debug/DebugMessagesListPanel",
        "UI/Panels/City/SociotechPanel",
        "UI/Panels/City/TechnologyPanel",
        "UI/Panels/WorldMap/SelectionPanel",
        "UI/Panels/WorldMap/CompassPanel",
    };
    for (const char* r : RECETTES) ReleverRecette(r);
    printf("[HIST] ===== fin du releve =====\n");
}

// --- Hooks -----------------------------------------------------------------------------
uintptr_t hkCreerEcranPrincipal(uintptr_t p1, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oCreerEcranPrincipal(p1, p2, p3, p4);
    // Panneau perdu avec l'ancien HUD : notre raison de pause est retiree si
    // le minuteur du jeu est toujours le meme, sinon simplement oubliee.
    if (g_minuteurPause) {
        uintptr_t courant = 0;
        SafeRead((void*)(Base() + OFF_MINUTEUR_JEU), &courant, 8);
        if (courant == g_minuteurPause) PauseHistorique(false);
        else { printf("[HIST] minuteur change : raison de pause oubliee\n"); g_minuteurPause = 0; }
    }
    // Idem pour le verrou de la camera (12e test) ; le crochet de la molette
    // est retire.
    if (g_cameraVerrouillee) {
        uintptr_t courant = 0;
        SafeRead((void*)(Base() + OFF_CAMERA_JEU), &courant, 8);
        if (courant == g_cameraVerrouillee) VerrouCamera(false);
        else { printf("[HIST] camera changee : verrou oublie\n"); g_cameraVerrouillee = 0; }
    }
    CrochetMoletteActif(false);
    LacherPoignee(g_racineHud);
    OublierPanneau();
    if (r > 0x10000) g_racineHud = PrendrePoignee(r);
    g_instantCreation = GetTickCount64();
    g_hudEquipe = false;
    g_releveFait = !RELEVE_INTERFACE;
#ifdef AP_RELEASE
    g_releveFait = true;
#endif
    printf("[HIST] ecran en jeu cree : racine FRAME 0x%llX (poignee 0x%llX)\n",
        (unsigned long long)r, (unsigned long long)g_racineHud);
    return r;
}

void hkInitLivre(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    oInitLivre(panneau, p2, p3, p4);
    if (!LEGENDE_LIVRE) return;
    uintptr_t h = 0;
    SafeRead((void*)(panneau + LIVRE_ARBRE), &h, 8);
    ConstruireLegende(ObjetDePoignee(h), LEG_LIVRE);
}

void hkInitTechnos(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    oInitTechnos(panneau, p2, p3, p4);
    uintptr_t h = 0;
    SafeRead((void*)(panneau + PAN_RACINE), &h, 8);
    ConstruireLegendeTechnos(ObjetDePoignee(h));
}

} // namespace

void HistoriquePompe()
{
    if (!g_racineHud || GetTickCount64() - g_instantCreation < 1000) return;
    if (!g_hudEquipe) {
        g_hudEquipe = true;   // une seule fois par ecran cree
        uintptr_t racine = ObjetDePoignee(g_racineHud);
        if (racine > 0x10000) {
            ConstruireBouton(racine);
            OublierPiecesCarte();
            if (LEGENDE_CARTE)
                ConstruireLegende(TrouverParNom(racine, "WORLD_MAP_HUD"), LEG_CARTE, &g_piecesCarte);
        }
    }
    SuivreCarte();
    LegendeTechnosAnneau();
    if (!g_releveFait && GetTickCount64() - g_instantCreation >= 5000) {
        g_releveFait = true;
        Relever();
    }
}

void HistoriqueNoeudTechnoConstruit()
{
    LegendeTechnosAnneau();
}

void HistoriqueLivreConstruit(uintptr_t livre)
{
    ConstruireLegendeLivre(livre);
}

void InstallHistoriqueAP(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_CREER_ECRAN_PRINCIPAL, (void*)&hkCreerEcranPrincipal, (void**)&oCreerEcranPrincipal,
          "creation de l'ecran en jeu" },
        { OFF_INIT_LIVRE, (void*)&hkInitLivre, (void**)&oInitLivre, "init du livre des lois" },
        { OFF_INIT_TECHNOS, (void*)&hkInitTechnos, (void**)&oInitTechnos, "init de l'arbre des technos" },
    };
    for (auto& x : h) {
        if (!x.off) { printf("[HIST] adresse inconnue (%s) : pas de hook\n", x.nom); continue; }
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[HIST] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}
