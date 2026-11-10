// menu_ap.cpp
// ---------------------------------------------------------------------------
// Ecran de connexion Archipelago dans le menu principal (2026-10-31).
//
// Decompiles du 2026-10-31 (adresses Ghidra) :
//  - FUN_141b28950 = init de FrostpunkUIMainMenuMainPanel : racine du panneau
//    = poignee en +0xA8, boutons CONTINUE_BUTTON ... EXIT_BUTTON branches par
//    FUN_141a324b0 -> FUN_14105a1c0(bouton, objet, fonction, 0, p5, 1)
//    (ecouteurs de clic bouton +0x258, rappel fonction(objet, ...)).
//  - Tout element d'interface garde sa recette en +0x1E8 et la ressource de
//    la recette en +0x1F0 (FUN_14102a5f0). FUN_14110c780(recette, ressource,
//    drapeaux, &DAT_142be0438) instancie une COPIE complete du sous-arbre :
//    c'est ainsi que le panneau Endless fabrique ses curseurs (FUN_141b3a420).
//  - Recette (UIElementRecipe) : +0x30 enfants, +0x38 compte, +0x48 nom,
//    +0x114 mode d'instanciation (0 toujours, 1 jamais, 2 avec drapeau 1).
//    FUN_141035c80(pool, chemin, 1) = ressource d'une recette (chargee au
//    besoin), FUN_141119140(ressource) = sa recette racine.
//  - UITextInput (uitextinput.cpp, 0x6E0) : +0x3E8 texte affiche, +0x678
//    tampon d'edition, +0x680 longueur, +0x684 maximum, +0x6C8 en edition ;
//    FUN_14111bef0(texte, wchar*, cfg) pose le texte affiche,
//    FUN_14112a920 commence l'edition (recopie +0x3E8 dans le tampon),
//    FUN_141030e90(ecran, input) donne le focus, FUN_1410261e0 = ecran.
//  - Element : +0x1D8 nom, +0x220 premier enfant, +0x230 parent, +0x240
//    frere suivant ; FUN_1410266a0(parent, enfant, avant) insere avant.
//
// Le menu n'a aucun champ de saisie : on clone celui d'un panneau en jeu
// (sauvegarde SAVE_NAME_INPUT, nom de la ville ENTITY_NAME_INPUT) depuis sa
// recette, et EXIT_BUTTON pour les boutons. Tout s'execute dans le hook de
// l'init du panneau et dans les rappels de clic : thread du jeu.
// ---------------------------------------------------------------------------
#include "menu_ap.h"
#include "mod_common.h"
#include "archipelago.h"
#include "MinHook.h"
#include <cstring>

namespace {

typedef uintptr_t (*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef uintptr_t (*Instancier_t)(uintptr_t recette, uintptr_t ressource, uint32_t drapeaux, uintptr_t ctx);
typedef uintptr_t (*ObtenirRecette_t)(uintptr_t pool, const char* chemin, uintptr_t charger);
typedef uintptr_t (*RacineRecette_t)(uintptr_t ressource);
typedef void      (*InsererAvant_t)(uintptr_t parent, uintptr_t enfant, uintptr_t avant, uintptr_t p4);
typedef void      (*BrancherClic_t)(uintptr_t bouton, uintptr_t objet, uintptr_t fonction, int p4,
                                    uintptr_t p5, int p6);
// Regle n. 5 : 4 arguments declares, meme quand le jeu en passe moins.
typedef void      (*PoserTexte_t)(uintptr_t texte, const wchar_t* chaine, uintptr_t cfg, uintptr_t p4);
typedef void      (*DebutEdition_t)(uintptr_t input, uintptr_t p2, uintptr_t p3, uintptr_t p4);
typedef void      (*PlacerCurseur_t)(uintptr_t input, int position);
typedef uintptr_t (*Ecran_t)(uintptr_t element);
typedef void      (*Focaliser_t)(uintptr_t ecran, uintptr_t input);
typedef uintptr_t (*Poignee_t)(uintptr_t objet);
typedef void      (*PoigneeRef_t)(uintptr_t poignee);
typedef uintptr_t (*ChercherEnfant_t)(uintptr_t racine, uintptr_t* nom);
typedef void      (*NomConstruire_t)(uintptr_t* nom, const char* texte);
typedef void      (*NomDetruire_t)(uintptr_t* nom);
typedef void      (*Relayout_t)(uintptr_t layout);

static const uintptr_t OFF_INIT_PANNEAU_PRINCIPAL = Adr(0x1B28950);
static const uintptr_t OFF_INSTANCIER      = Adr(0x110C780);
static const uintptr_t OFF_OBTENIR_RECETTE = Adr(0x1035C80);
static const uintptr_t OFF_RACINE_RECETTE  = Adr(0x1119140);
static const uintptr_t OFF_INSERER_AVANT   = Adr(0x10266A0);
static const uintptr_t OFF_BRANCHER_CLIC   = Adr(0x105A1C0);
static const uintptr_t OFF_POSER_TEXTE     = Adr(0x111BEF0);
static const uintptr_t OFF_DEBUT_EDITION   = Adr(0x112A920);
static const uintptr_t OFF_PLACER_CURSEUR  = Adr(0x112A360);
static const uintptr_t OFF_ECRAN           = Adr(0x10261E0);
static const uintptr_t OFF_FOCALISER       = Adr(0x1030E90);
static const uintptr_t OFF_POIGNEE         = Adr(0xE1A120);
static const uintptr_t OFF_POIGNEE_PLUS    = Adr(0xE1A110);
static const uintptr_t OFF_POIGNEE_MOINS   = Adr(0xE1A260);
static const uintptr_t OFF_CHERCHER_ENFANT = Adr(0x1025FF0);
static const uintptr_t OFF_NOM_CONSTRUIRE  = Adr(0xE4FA20);
static const uintptr_t OFF_NOM_DETRUIRE    = Adr(0xE4EAB0);
static const uintptr_t OFF_RELAYOUT        = Adr(0x11FD800);
static const uintptr_t OFF_CLIC_ECRAN      = Adr(0x102F720);   // FUN_14102f720 : clic souris d'un ecran
static const uintptr_t OFF_INSERER_CAR     = Adr(0x112AC10);   // FUN_14112ac10 : inserer un caractere
static const uintptr_t OFF_TOUCHE          = Adr(0x1129C90);   // FUN_141129c90 : touche (effacement, fleches)
static const uintptr_t OFF_POOL_TEMPLATES  = Adr(0x2B2E710);
static const uintptr_t OFF_CTX_INSTANCIER  = Adr(0x2BE0438);
static const uintptr_t OFF_CFG_TEXTE       = Adr(0x2BFD628);

constexpr int PAN_RACINE   = 0xA8;     // poignee de la racine du panneau
constexpr int EL_NOM       = 0x1D8;
constexpr int EL_RECETTE   = 0x1E8;
constexpr int EL_RESSOURCE = 0x1F0;
constexpr int EL_ENFANT    = 0x220;
constexpr int EL_PARENT    = 0x230;
constexpr int EL_FRERE     = 0x240;
constexpr int EL_TAILLE    = 0x210;    // largeur, hauteur (flottants)
constexpr int REC_ENFANTS  = 0x30;
constexpr int REC_COMPTE   = 0x38;
constexpr int REC_NOM      = 0x48;
constexpr int REC_MODE     = 0x114;
constexpr int IN_TEXTE     = 0x3E8;
constexpr int IN_TAMPON    = 0x678;
constexpr int IN_LONGUEUR  = 0x680;
constexpr int IN_MAX       = 0x684;
constexpr int IN_EDITION   = 0x6C8;
constexpr int IN_CURSEUR   = 0x688;    // position du curseur
constexpr int IN_LARGEUR_LIMITE = 0x6CC;  // "Limit to width" (drapeau, puis flottant)
constexpr int MAX_SAISIE   = 128;      // longueur voulue (le champ clone en permet 15)
constexpr int POOL_RACINE  = 0x28;
constexpr int REP_SOUSREPS = 0x10;
constexpr int REP_STUBS    = 0x28;
constexpr int STUB_NOM     = 0x08;
constexpr int STUB_TEMPLATE= 0x38;
constexpr int MAX_VALEUR   = 256;

Fonction4_t     oInitPanneau = nullptr;
PoserTexte_t    oPoserTexte = nullptr;
DebutEdition_t  oDebutEdition = nullptr;

// --- Les trois champs ----------------------------------------------------------
enum { CHAMP_SERVEUR = 0, CHAMP_SLOT = 1, CHAMP_MDP = 2, NB_CHAMPS = 3 };

struct Champ {
    const char*    cle;          // cle du fichier mods\ap_connexion.txt
    const wchar_t* libelle;      // texte EN JEU (anglais)
    uintptr_t      poignee;      // poignee du UITextInput
    uintptr_t      objet;        // adresse du UITextInput (comparaison rapide)
    wchar_t        valeur[MAX_VALEUR];
};

Champ g_champs[NB_CHAMPS] = {
    { "server",   L"Server",    0, 0, L"archipelago.gg:38281" },
    { "slot",     L"Slot name", 0, 0, L"Player 1" },
    { "password", L"Password",  0, 0, L"" },
};

uintptr_t g_panneauFait = 0;            // panneau deja equipe
uintptr_t g_colonne = 0, g_menuLayout = 0;   // diagnostic d'affichage
ULONGLONG g_instantPose = 0;
void ReleverPlusTard();
bool      g_valeursLues = false;
wchar_t   g_etoiles[MAX_VALEUR] = {};   // affichage du mot de passe

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

bool ClasseEst(uintptr_t obj, const char* nom)
{
    char cls[96] = "";
    return obj > 0x10000 && GetClassNameFP(obj, cls, sizeof(cls)) && StrEq(cls, nom);
}

// Texte d'un element : UIText ou UITextBox, pas un champ de saisie.
bool EstTexte(uintptr_t obj)
{
    return ClasseEst(obj, "UIText") || ClasseEst(obj, "UITextBox");
}

uintptr_t PrendrePoignee(uintptr_t obj)
{
    uintptr_t h = 0;
    __try {
        h = ((Poignee_t)(Base() + OFF_POIGNEE))(obj);
        if (h) ((PoigneeRef_t)(Base() + OFF_POIGNEE_PLUS))(h);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a la prise d'une poignee\n", GetExceptionCode());
        h = 0;
    }
    return h;
}

void LacherPoignee(uintptr_t& h)
{
    if (!h) return;
    __try { ((PoigneeRef_t)(Base() + OFF_POIGNEE_MOINS))(h); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    h = 0;
}

// Enfant nomme (recherche du jeu, FUN_141025ff0).
uintptr_t ChercherEnfant(uintptr_t racine, const char* nom)
{
    uintptr_t ns = 0, r = 0;
    __try {
        ((NomConstruire_t)(Base() + OFF_NOM_CONSTRUIRE))(&ns, nom);
        r = ((ChercherEnfant_t)(Base() + OFF_CHERCHER_ENFANT))(racine, &ns);
        ((NomDetruire_t)(Base() + OFF_NOM_DETRUIRE))(&ns);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX en cherchant \"%s\"\n", GetExceptionCode(), nom);
        r = 0;
    }
    return r;
}

// Premier descendant (profondeur d'abord) accepte par 'accepte', 'el' compris.
uintptr_t PremierDescendant(uintptr_t el, bool (*accepte)(uintptr_t), int prof)
{
    if (el < 0x10000 || prof > 12) return 0;
    if (accepte(el)) return el;
    uintptr_t enfant = 0;
    SafeRead((void*)(el + EL_ENFANT), &enfant, 8);
    for (int garde = 0; enfant > 0x10000 && garde < 256; ++garde) {
        uintptr_t r = PremierDescendant(enfant, accepte, prof + 1);
        if (r) return r;
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
    return 0;
}

bool EstChampDeSaisie(uintptr_t obj) { return ClasseEst(obj, "UITextInput"); }

void DecrireElement(const char* qui, uintptr_t el)
{
    char nom[64] = "", cls[96] = "?";
    uintptr_t rec = 0, res = 0, parent = 0;
    LireNom(el, EL_NOM, nom, sizeof(nom));
    GetClassNameFP(el, cls, sizeof(cls));
    SafeRead((void*)(el + EL_RECETTE), &rec, 8);
    SafeRead((void*)(el + EL_RESSOURCE), &res, 8);
    SafeRead((void*)(el + EL_PARENT), &parent, 8);
    printf("[MENU] %s : \"%s\" %s 0x%llX (recette 0x%llX, ressource 0x%llX, parent 0x%llX)\n", qui, nom, cls,
        (unsigned long long)el, (unsigned long long)rec, (unsigned long long)res, (unsigned long long)parent);
}

// --- Instanciation depuis une recette ----------------------------------------------
// Un noeud en mode 1 ("jamais") n'est instancie qu'en levant le mode le temps
// de l'appel (les recettes sont des donnees partagees : on remet la valeur).
uintptr_t InstancierNoeud(uintptr_t noeud, uintptr_t ressource)
{
    int32_t mode = 0;
    SafeRead((void*)(noeud + REC_MODE), &mode, 4);
    if (mode == 1) { int32_t zero = 0; SafeWrite((void*)(noeud + REC_MODE), &zero, 4); }
    uintptr_t el = 0;
    __try {
        el = ((Instancier_t)(Base() + OFF_INSTANCIER))(noeud, ressource, 1, Base() + OFF_CTX_INSTANCIER);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a l'instanciation de la recette 0x%llX\n", GetExceptionCode(),
            (unsigned long long)noeud);
        el = 0;
    }
    if (mode == 1) SafeWrite((void*)(noeud + REC_MODE), &mode, 4);
    return el;
}

// Copie d'un element existant, depuis sa propre recette (comme FUN_141b3a420).
uintptr_t Cloner(uintptr_t el)
{
    uintptr_t rec = 0, res = 0;
    if (!SafeRead((void*)(el + EL_RECETTE), &rec, 8) || rec < 0x10000 ||
        !SafeRead((void*)(el + EL_RESSOURCE), &res, 8)) return 0;
    return InstancierNoeud(rec, res);
}

// Noeud de recette nomme 'nom', recherche recursive SANS s'arreter aux noeuds
// en mode 1 / 2 (au contraire de FUN_14110dd50).
uintptr_t ChercherNoeud(uintptr_t noeud, const char* nom, int prof)
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

// Ressource de la recette 'chemin' (chargee au besoin, thread du jeu).
uintptr_t RessourceRecette(const char* chemin, uintptr_t* racine)
{
    uintptr_t res = 0;
    *racine = 0;
    char tampon[260];
    strcpy_s(tampon, chemin);
    JournalImmediat("[MENU] obtention de la recette \"%s\" ...\n", tampon);
    __try {
        res = ((ObtenirRecette_t)(Base() + OFF_OBTENIR_RECETTE))(Base() + OFF_POOL_TEMPLATES, tampon, 1);
        if (res) *racine = ((RacineRecette_t)(Base() + OFF_RACINE_RECETTE))(res);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[MENU] EXCEPTION 0x%08lX a l'obtention de la recette \"%s\"\n", GetExceptionCode(), tampon);
        res = 0;
    }
    return res;
}

// --- Diagnostic : recettes d'interface du pool --------------------------------------
// Meme arbre que les templates d'entites (groupes.cpp) : on liste les feuilles
// sous "UI/" et on retient celles dont le nom evoque un champ de nom.
struct ListeRecettes { int n; int nCand; char cand[8][260]; };

bool NomEvoqueChamp(const char* nom)
{
    static const char* mots[] = { "Save", "Naming", "Name", "Rename", "Search" };
    for (const char* m : mots) if (StrContains(nom, m)) return true;
    return false;
}

void ListerRep(uintptr_t rep, char* chemin, int len, int prof, bool dansUI, ListeRecettes& l)
{
    if (prof > 12) return;
    uintptr_t tab = 0;
    int32_t n = 0;
    if (dansUI && SafeRead((void*)(rep + REP_STUBS), &tab, 8) && SafeRead((void*)(rep + REP_STUBS + 8), &n, 4) &&
        tab > 0x10000 && n > 0 && n < 100000) {
        for (int i = 0; i < n; ++i) {
            uintptr_t stub = 0, tpl = 0;
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &stub, 8) || stub < 0x10000) continue;
            char nom[128];
            LireNom(stub, STUB_NOM, nom, sizeof(nom));
            SafeRead((void*)(stub + STUB_TEMPLATE), &tpl, 8);
            ++l.n;
            if (l.n <= 600) printf("[MENU]   recette %s%s%s\n", chemin, nom, tpl ? " (chargee)" : "");
            if (NomEvoqueChamp(nom) && l.nCand < 8)
                _snprintf_s(l.cand[l.nCand++], 260, _TRUNCATE, "%s%s", chemin, nom);
        }
    }
    if (!SafeRead((void*)(rep + REP_SOUSREPS), &tab, 8) || !SafeRead((void*)(rep + REP_SOUSREPS + 8), &n, 4) ||
        tab < 0x10000 || n <= 0 || n > 100000) return;
    for (int i = 0; i < n; ++i) {
        uintptr_t sous = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &sous, 8) || sous < 0x10000) continue;
        char nom[128];
        LireNom(sous, 0, nom, sizeof(nom));
        if (!dansUI && prof == 0 && _stricmp(nom, "UI") != 0) continue;
        int k = _snprintf_s(chemin + len, 260 - len, _TRUNCATE, "%s/", nom);
        if (k <= 0) { chemin[len] = 0; continue; }
        ListerRep(sous, chemin, len + k, prof + 1, true, l);
        chemin[len] = 0;
    }
}

// --- Source du champ de saisie ------------------------------------------------------
// Noeuds connus (decompiles des panneaux) : sauvegarde, nom de la ville,
// recherche. La recette qui les porte n'est pas connue du code : on essaie
// l'ecran en jeu, puis les recettes du pool dont le nom evoque un champ.
const char* NOEUDS_SAISIE[] = { "SAVE_NAME_INPUT", "ENTITY_NAME_INPUT", "NAME_INPUT" };

uintptr_t ChercherDansRecette(const char* chemin, uintptr_t* ressource)
{
    uintptr_t racine = 0;
    uintptr_t res = RessourceRecette(chemin, &racine);
    char cls[96] = "?";
    if (res) GetClassNameFP(res, cls, sizeof(cls));
    printf("[MENU] recette \"%s\" : ressource 0x%llX (%s), racine 0x%llX\n", chemin,
        (unsigned long long)res, cls, (unsigned long long)racine);
    if (!racine) return 0;
    for (const char* n : NOEUDS_SAISIE) {
        uintptr_t noeud = ChercherNoeud(racine, n, 0);
        if (!noeud) continue;
        int32_t mode = -1;
        SafeRead((void*)(noeud + REC_MODE), &mode, 4);
        printf("[MENU] noeud \"%s\" trouve dans \"%s\" : 0x%llX (mode %d)\n", n, chemin,
            (unsigned long long)noeud, mode);
        *ressource = res;
        return noeud;
    }
    printf("[MENU] aucun noeud de saisie dans \"%s\"\n", chemin);
    return 0;
}

// Noeud de recette d'un UITextInput, memorise pour les menus suivants.
uintptr_t SourceSaisie(uintptr_t* ressource)
{
    static uintptr_t noeud = 0, res = 0;
    static bool essaye = false;
    if (noeud || essaye) { *ressource = res; return noeud; }
    essaye = true;

    noeud = ChercherDansRecette("UI/MainScreen", &res);
    if (!noeud) {
        uintptr_t racine = 0;
        ListeRecettes l = {};
        char chemin[260] = "";
        if (SafeRead((void*)(Base() + OFF_POOL_TEMPLATES + POOL_RACINE), &racine, 8) && racine > 0x10000) {
            printf("[MENU] recettes d'interface du pool :\n");
            ListerRep(racine, chemin, 0, 0, false, l);
            printf("[MENU] %d recette(s) sous UI/, %d candidate(s)\n", l.n, l.nCand);
        }
        for (int i = 0; i < l.nCand && !noeud; ++i) noeud = ChercherDansRecette(l.cand[i], &res);
    }
    if (!noeud) printf("[MENU] AUCUNE source de champ de saisie : renvoyer ce journal\n");
    *ressource = res;
    return noeud;
}

// --- Fichier mods\ap_connexion.txt ------------------------------------------------
// Une ligne par champ, "cle=valeur" en UTF-8. Le mot de passe y est en clair
// (fichier local, comme la configuration d'un client Archipelago).
bool CheminFichier(char* out, size_t taille)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_connexion.txt", base);
    return true;
}

void ChargerValeurs()
{
    if (g_valeursLues) return;
    g_valeursLues = true;
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichier(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) {
        printf("[MENU] pas de mods\\ap_connexion.txt : valeurs par defaut\n");
        return;
    }
    char ligne[1024];
    while (fgets(ligne, sizeof(ligne), f)) {
        size_t l = strlen(ligne);
        while (l && (ligne[l - 1] == '\n' || ligne[l - 1] == '\r')) ligne[--l] = 0;
        char* egal = strchr(ligne, '=');
        if (!egal) continue;
        *egal = 0;
        for (Champ& c : g_champs) {
            if (!StrEq(ligne, c.cle)) continue;
            int n = MultiByteToWideChar(CP_UTF8, 0, egal + 1, -1, c.valeur, MAX_VALEUR);
            if (n <= 0) c.valeur[0] = 0;
            c.valeur[MAX_VALEUR - 1] = 0;
        }
    }
    fclose(f);
    printf("[MENU] valeurs relues dans mods\\ap_connexion.txt\n");
}

void SauverValeurs()
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichier(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "w") != 0 || !f) {
        printf("[MENU] ecriture de mods\\ap_connexion.txt impossible\n");
        return;
    }
    for (const Champ& c : g_champs) {
        char utf8[MAX_VALEUR * 4];
        if (WideCharToMultiByte(CP_UTF8, 0, c.valeur, -1, utf8, sizeof(utf8), nullptr, nullptr) <= 0) utf8[0] = 0;
        fprintf(f, "%s=%s\n", c.cle, utf8);
    }
    fclose(f);
    printf("[MENU] valeurs enregistrees dans mods\\ap_connexion.txt\n");
}

// --- Lecture d'un champ -------------------------------------------------------------
bool ChampVivant(const Champ& c, uintptr_t* input)
{
    uintptr_t o = ObjetDePoignee(c.poignee);
    if (!o || o != c.objet || !EstChampDeSaisie(o)) return false;
    *input = o;
    return true;
}

// Texte courant : tampon d'edition pendant l'edition, texte affiche sinon. Le
// mot de passe affiche des etoiles : sa valeur est tenue par hkPoserTexte.
void LireChamp(int i)
{
    Champ& c = g_champs[i];
    uintptr_t input = 0;
    if (i == CHAMP_MDP || !ChampVivant(c, &input)) return;
    uint8_t edition = 0;
    SafeRead((void*)(input + IN_EDITION), &edition, 1);
    uintptr_t chars = 0;
    int32_t longueur = MAX_VALEUR - 1;
    if (edition) {
        SafeRead((void*)(input + IN_TAMPON), &chars, 8);
        SafeRead((void*)(input + IN_LONGUEUR), &longueur, 4);
        if (longueur < 0 || longueur >= MAX_VALEUR) longueur = MAX_VALEUR - 1;
    }
    else SafeRead((void*)(input + IN_TEXTE), &chars, 8);
    if (chars < 0x10000) { c.valeur[0] = 0; return; }
    int j = 0;
    for (; j < longueur; ++j) {
        wchar_t ch = 0;
        if (!SafeRead((void*)(chars + (uintptr_t)j * 2), &ch, 2) || !ch) break;
        c.valeur[j] = ch;
    }
    c.valeur[j] = 0;
    // Espaces de tete et de fin retires (un champ vide affiche un espace).
    while (j > 0 && c.valeur[j - 1] == L' ') c.valeur[--j] = 0;
    int debut = 0;
    while (c.valeur[debut] == L' ') ++debut;
    if (debut) memmove(c.valeur, c.valeur + debut, (size_t)(j - debut + 1) * sizeof(wchar_t));
}

// Copie pour le journal : ASCII seulement (la console du jeu).
void VersAscii(const wchar_t* w, char* out, int maxlen)
{
    int i = 0;
    for (; w[i] && i < maxlen - 1; ++i) out[i] = (w[i] >= 0x20 && w[i] < 0x7F) ? (char)w[i] : '?';
    out[i] = 0;
}

// --- Rappels de clic (thread du jeu) --------------------------------------------------
void Focaliser(int i)
{
    uintptr_t input = 0;
    if (!ChampVivant(g_champs[i], &input)) {
        printf("[MENU] champ \"%s\" introuvable (detruit ?)\n", g_champs[i].cle);
        return;
    }
    __try {
        uintptr_t ecran = ((Ecran_t)(Base() + OFF_ECRAN))(input);
        if (ecran) ((Focaliser_t)(Base() + OFF_FOCALISER))(ecran, input);
        uint8_t edition = 0;
        int32_t max = 0, curseur = 0;
        SafeRead((void*)(input + IN_EDITION), &edition, 1);
        SafeRead((void*)(input + IN_MAX), &max, 4);
        SafeRead((void*)(input + IN_CURSEUR), &curseur, 4);
        printf("[MENU] focus sur le champ \"%s\" (ecran 0x%llX) : en edition %d, curseur %d, %d au plus\n",
            g_champs[i].cle, (unsigned long long)ecran, edition, curseur, max);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX au focus du champ \"%s\"\n", GetExceptionCode(), g_champs[i].cle);
    }
}

// Clic souris de l'ecran (FUN_14102f720) : element survole en ecran +0x380
// (poignee), focus d'un champ si sa vtable le declare focalisable (+0x108,
// +0x228). Le champ de SaveGamePanel est focalise par son panneau, pas par
// le clic : si le clic tombe sur un de nos champs (ou un de ses enfants) et
// qu'il n'a pas le focus, on le lui donne. Les intitules ne sont plus des
// boutons (demande de l'auteur, 2026-10-31).
constexpr int ECRAN_SURVOLE   = 0x380;
constexpr int ECRAN_FOCUS     = 0x328;
constexpr int ECRAN_BOUTON    = 0x3B2;   // bouton de souris enfonce (garde de FUN_14102f720)

typedef uintptr_t (*ClicEcran_t)(uintptr_t ecran, uintptr_t p2, uintptr_t p3, uintptr_t p4);
ClicEcran_t oClicEcran = nullptr;

int ChampSousElement(uintptr_t el)
{
    for (int prof = 0; el > 0x10000 && prof < 6; ++prof) {
        for (int i = 0; i < NB_CHAMPS; ++i)
            if (g_champs[i].objet && el == g_champs[i].objet) return i;
        if (!SafeRead((void*)(el + EL_PARENT), &el, 8)) break;
    }
    return -1;
}

uintptr_t hkClicEcran(uintptr_t ecran, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uint8_t enfonce = 0;
    SafeRead((void*)(ecran + ECRAN_BOUTON), &enfonce, 1);
    uintptr_t r = oClicEcran(ecran, p2, p3, p4);
    if (!enfonce) return r;
    ReleverPlusTard();
    uintptr_t survole = 0, focus = 0;
    SafeRead((void*)(ecran + ECRAN_SURVOLE), &survole, 8);
    SafeRead((void*)(ecran + ECRAN_FOCUS), &focus, 8);
    int i = ChampSousElement(ObjetDePoignee(survole));
    if (i >= 0 && ObjetDePoignee(focus) != g_champs[i].objet) Focaliser(i);
    return r;
}

void CbConnect(uintptr_t, uintptr_t, uintptr_t, uintptr_t)
{
    for (int i = 0; i < NB_CHAMPS; ++i) LireChamp(i);
    SauverValeurs();
    char utf8[NB_CHAMPS][MAX_VALEUR * 4];
    for (int i = 0; i < NB_CHAMPS; ++i)
        if (WideCharToMultiByte(CP_UTF8, 0, g_champs[i].valeur, -1, utf8[i], sizeof(utf8[i]), nullptr, nullptr) <= 0)
            utf8[i][0] = 0;
    RequestConnection(utf8[CHAMP_SERVEUR], utf8[CHAMP_SLOT], utf8[CHAMP_MDP]);
}

// --- Masque du mot de passe ---------------------------------------------------------
// 9e test : les etoiles du mot de passe etaient decalees. Le moteur place le
// curseur et le texte en mesurant le TAMPON d'edition (FUN_14112a360 :
// largeur du prefixe de +0x678, alignement +0x321 copie de la recette) ; or
// le tampon tenait le vrai mot de passe et l'affichage des etoiles. Desormais
// le tampon ne contient QUE des etoiles, comme l'affichage : la vraie valeur
// est tenue a cote (g_champs[CHAMP_MDP].valeur), mise a jour en miroir des
// operations du moteur : insertion d'un caractere (FUN_14112ac10, ou l'on
// insere '*') et touches d'effacement (FUN_141129c90 : retour arriere,
// suppression, et la selection, +0x690 debut / +0x694 longueur, effacee par
// FUN_1411293b0 si +0x6C9).
//
// Majuscules : la recette du champ de sauvegarde a "Upper case" (+0x24C,
// UITextRecipe), relu par FUN_14112ac10 et FUN_14112a920 qui convertissent
// le TAMPON en majuscules : le slot et le mot de passe auraient ete envoyes
// en majuscules. On coupe "Upper case" le temps de ces appels sur nos champs
// (recette partagee avec le panneau de sauvegarde du jeu : valeur remise).
constexpr int IN_SEL_DEBUT  = 0x690;
constexpr int IN_SEL_LONG   = 0x694;
constexpr int IN_SEL_ACTIVE = 0x6C9;
constexpr int REC_MAJUSCULES = 0x24C;

typedef void (*InsererCar_t)(uintptr_t input, uintptr_t car, uintptr_t p3, uintptr_t p4);
typedef void (*Touche_t)(uintptr_t input, uintptr_t vk, uintptr_t p3, uintptr_t p4);
InsererCar_t oInsererCar = nullptr;
Touche_t     oTouche = nullptr;

// Index du champ AP dont 'self' est le UITextInput vivant, ou -1.
int ChampDeSelf(uintptr_t self)
{
    if (!self) return -1;
    for (int i = 0; i < NB_CHAMPS; ++i)
        if (self == g_champs[i].objet && ObjetDePoignee(g_champs[i].poignee) == self) return i;
    return -1;
}

bool EstChampMdp(uintptr_t self) { return ChampDeSelf(self) == CHAMP_MDP; }

// Coupe "Upper case" dans la recette de 'self' ; renvoie l'ancienne valeur.
uint8_t SansMajuscules(uintptr_t self, uintptr_t* recette)
{
    uint8_t ancien = 0, zero = 0;
    *recette = 0;
    if (!SafeRead((void*)(self + EL_RECETTE), recette, 8) || *recette < 0x10000) { *recette = 0; return 0; }
    SafeRead((void*)(*recette + REC_MAJUSCULES), &ancien, 1);
    if (ancien) SafeWrite((void*)(*recette + REC_MAJUSCULES), &zero, 1);
    return ancien;
}

void RemettreMajuscules(uintptr_t recette, uint8_t ancien)
{
    if (recette && ancien) SafeWrite((void*)(recette + REC_MAJUSCULES), &ancien, 1);
}

struct EtatSaisie { int32_t longueur, curseur, selDebut, selLong; uint8_t selActive; };

EtatSaisie LireEtat(uintptr_t self)
{
    EtatSaisie e = {};
    SafeRead((void*)(self + IN_LONGUEUR), &e.longueur, 4);
    SafeRead((void*)(self + IN_CURSEUR), &e.curseur, 4);
    SafeRead((void*)(self + IN_SEL_DEBUT), &e.selDebut, 4);
    SafeRead((void*)(self + IN_SEL_LONG), &e.selLong, 4);
    SafeRead((void*)(self + IN_SEL_ACTIVE), &e.selActive, 1);
    return e;
}

// Vraie valeur du mot de passe : effacer [debut, debut + n) / inserer 'ch' en 'pos'.
void MdpEffacer(int debut, int n)
{
    wchar_t* v = g_champs[CHAMP_MDP].valeur;
    int l = (int)wcslen(v);
    if (debut < 0 || n <= 0 || debut >= l) return;
    if (debut + n > l) n = l - debut;
    memmove(v + debut, v + debut + n, (size_t)(l - debut - n + 1) * sizeof(wchar_t));
}

void MdpInserer(int pos, wchar_t ch)
{
    wchar_t* v = g_champs[CHAMP_MDP].valeur;
    int l = (int)wcslen(v);
    if (l >= MAX_VALEUR - 1) return;
    if (pos < 0) pos = 0;
    if (pos > l) pos = l;
    memmove(v + pos + 1, v + pos, (size_t)(l - pos + 1) * sizeof(wchar_t));
    v[pos] = ch;
}

// Selection effacee par le moteur avant une insertion / un effacement :
// meme calcul que FUN_1411293b0. Renvoie le nombre de caracteres effaces.
int MdpEffacerSelection(const EtatSaisie& e)
{
    if (!e.selActive || e.selLong <= 0) return 0;
    int debut = e.selDebut < e.longueur ? e.selDebut : e.longueur;
    int fin = debut + e.selLong;
    if (fin > e.longueur) fin = e.longueur;
    if (fin <= debut) return 0;
    MdpEffacer(debut, fin - debut);
    return fin - debut;
}

// 10e test : la colonne place chaque champ selon la largeur de son texte au
// moment de la mise en page (une seule fois, a l'ouverture du menu) ; le texte
// saisi ensuite part de cette position et deborde a droite. On remet la
// colonne en page (FUN_1411fd800, tache du thread de rendu) apres chaque
// modification d'un de nos champs.
void RecalculerColonne()
{
    if (!g_colonne) return;
    __try { ((Relayout_t)(Base() + OFF_RELAYOUT))(g_colonne); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a la remise en page de la colonne\n", GetExceptionCode());
    }
}

void hkInsererCar(uintptr_t self, uintptr_t car, uintptr_t p3, uintptr_t p4)
{
    int i = ChampDeSelf(self);
    if (i < 0) { oInsererCar(self, car, p3, p4); return; }
    uintptr_t recette = 0;
    uint8_t maj = SansMajuscules(self, &recette);
    if (i != CHAMP_MDP) {
        oInsererCar(self, car, p3, p4);
        RemettreMajuscules(recette, maj);
        RecalculerColonne();
        return;
    }
    EtatSaisie avant = LireEtat(self);
    oInsererCar(self, (car & ~(uintptr_t)0xFFFF) | L'*', p3, p4);
    RemettreMajuscules(recette, maj);
    EtatSaisie apres = LireEtat(self);
    int efface = MdpEffacerSelection(avant);
    if (apres.longueur == avant.longueur - efface + 1) MdpInserer(apres.curseur - 1, (wchar_t)(car & 0xFFFF));
    RecalculerColonne();
}

void hkTouche(uintptr_t self, uintptr_t vk, uintptr_t p3, uintptr_t p4)
{
    int i = ChampDeSelf(self);
    if (i < 0) { oTouche(self, vk, p3, p4); return; }
    EtatSaisie avant = LireEtat(self);
    oTouche(self, vk, p3, p4);
    EtatSaisie apres = LireEtat(self);
    int retire = avant.longueur - apres.longueur;
    if (retire <= 0) return;
    RecalculerColonne();
    if (i != CHAMP_MDP) return;
    if (avant.selActive && avant.selLong > 0) { MdpEffacerSelection(avant); return; }
    if ((int)vk == VK_BACK) MdpEffacer(avant.curseur - retire, retire);
    else MdpEffacer(avant.curseur, retire);   // VK_DELETE
}

// Pose d'un texte : sur le champ du mot de passe, un texte qui n'est pas le
// tampon d'edition (valeur posee par le mod) devient la vraie valeur et
// s'affiche en etoiles ; le tampon (deja en etoiles) passe tel quel.
void hkPoserTexte(uintptr_t self, const wchar_t* chaine, uintptr_t cfg, uintptr_t p4)
{
    if (!EstChampMdp(self) || chaine == g_etoiles) { oPoserTexte(self, chaine, cfg, p4); return; }
    uintptr_t tampon = 0;
    SafeRead((void*)(self + IN_TAMPON), &tampon, 8);
    if (tampon && (uintptr_t)chaine == tampon) { oPoserTexte(self, chaine, cfg, p4); return; }
    Champ& c = g_champs[CHAMP_MDP];
    int n = 0;
    for (; chaine && n < MAX_VALEUR - 1; ++n) {
        wchar_t ch = 0;
        if (!SafeRead(chaine + n, &ch, 2) || !ch) break;
        c.valeur[n] = ch;
    }
    c.valeur[n] = 0;
    // Le " " d'un champ vide n'est pas une valeur.
    if (n == 1 && c.valeur[0] == L' ') { c.valeur[0] = 0; n = 0; }
    for (int k = 0; k < n; ++k) g_etoiles[k] = L'*';
    g_etoiles[n] = 0;
    // Texte vide : le champ garderait son texte par defaut ("New save").
    if (n == 0) { g_etoiles[0] = L' '; g_etoiles[1] = 0; }
    oPoserTexte(self, g_etoiles, cfg, p4);
}

// Debut d'edition : le moteur recopie le texte affiche (etoiles pour le mot
// de passe) dans le tampon, en majuscules si la recette le demande.
void hkDebutEdition(uintptr_t self, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    int i = ChampDeSelf(self);
    if (i < 0) { oDebutEdition(self, p2, p3, p4); return; }
    uintptr_t recette = 0;
    uint8_t maj = SansMajuscules(self, &recette);
    oDebutEdition(self, p2, p3, p4);
    RemettreMajuscules(recette, maj);
    if (i != CHAMP_MDP || g_champs[CHAMP_MDP].valeur[0]) return;
    // Mot de passe vide : le " " affiche ne doit pas rester dans le tampon.
    uintptr_t tampon = 0;
    wchar_t zero = 0;
    int32_t nul = 0;
    if (!SafeRead((void*)(self + IN_TAMPON), &tampon, 8) || tampon < 0x10000) return;
    SafeWrite((void*)tampon, &zero, 2);
    SafeWrite((void*)(self + IN_LONGUEUR), &nul, 4);
    __try { ((PlacerCurseur_t)(Base() + OFF_PLACER_CURSEUR))(self, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX au debut d'edition du mot de passe\n", GetExceptionCode());
    }
}

// --- Construction ------------------------------------------------------------------
void PoserTexte(uintptr_t texte, const wchar_t* chaine)
{
    __try { hkPoserTexte(texte, chaine, Base() + OFF_CFG_TEXTE, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a la pose d'un texte\n", GetExceptionCode());
    }
}

bool Inserer(uintptr_t parent, uintptr_t el, uintptr_t avant)
{
    __try { ((InsererAvant_t)(Base() + OFF_INSERER_AVANT))(parent, el, avant, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a l'insertion de 0x%llX\n", GetExceptionCode(), (unsigned long long)el);
        return false;
    }
    return true;
}

bool BrancherClic(uintptr_t bouton, uintptr_t panneau, void (*fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))
{
    __try { ((BrancherClic_t)(Base() + OFF_BRANCHER_CLIC))(bouton, panneau, (uintptr_t)fn, 0, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX au branchement d'un clic\n", GetExceptionCode());
        return false;
    }
    return true;
}

// Bouton clone d'EXIT_BUTTON, libelle 'texte', rappel 'fn', insere avant 'avant'.
uintptr_t AjouterBouton(uintptr_t modele, uintptr_t parent, uintptr_t avant, uintptr_t panneau,
                        const wchar_t* texte, void (*fn)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))
{
    uintptr_t b = Cloner(modele);
    if (!b) { printf("[MENU] clonage du bouton KO\n"); return 0; }
    uintptr_t t = PremierDescendant(b, &EstTexte, 0);
    if (t) PoserTexte(t, texte);
    else printf("[MENU] bouton clone sans texte (0x%llX)\n", (unsigned long long)b);
    BrancherClic(b, panneau, fn);
    Inserer(parent, b, avant);
    return b;
}

// Intitule non cliquable : copie du texte d'EXIT_BUTTON seul (sa propre
// recette), sans le bouton qui l'entoure.
uintptr_t AjouterLibelle(uintptr_t texteModele, uintptr_t parent, uintptr_t avant, const wchar_t* texte)
{
    uintptr_t t = Cloner(texteModele);
    if (!t) { printf("[MENU] clonage d'un intitule KO\n"); return 0; }
    PoserTexte(t, texte);
    Inserer(parent, t, avant);
    return t;
}

// Le champ clone de SaveGamePanel (tests du 2026-10-31) : 264 x 40, pas de
// limite de largeur, mais 15 caracteres au plus (FUN_14112ac10 refuse tout
// caractere au-dela) ; la mise a la taille du bouton (135 x 32) le
// retrecissait : on garde sa taille. Tampon de MAX_SAISIE caracteres
// (allocateur du moteur, regle n. 1), limite de largeur coupee par securite.
void PreparerChamp(uintptr_t input, const char* cle)
{
    float taille[2] = {};
    int32_t max = 0, longueur = 0;
    uint8_t limite = 0;
    SafeRead((void*)(input + EL_TAILLE), taille, sizeof(taille));
    SafeRead((void*)(input + IN_MAX), &max, 4);
    SafeRead((void*)(input + IN_LONGUEUR), &longueur, 4);
    SafeRead((void*)(input + IN_LARGEUR_LIMITE), &limite, 1);
    printf("[MENU] champ \"%s\" d'origine : taille %.1f x %.1f, %d caracteres au plus, limite de largeur %d\n",
        cle, taille[0], taille[1], max, limite);

    uintptr_t ancien = 0;
    SafeRead((void*)(input + IN_TAMPON), &ancien, 8);
    uintptr_t tampon = MoteurAlloc ? MoteurAlloc((size_t)(MAX_SAISIE + 1) * 2) : 0;
    if (tampon) {
        memset((void*)tampon, 0, (size_t)(MAX_SAISIE + 1) * 2);
        if (longueur < 0 || longueur > max || longueur > MAX_SAISIE) longueur = 0;
        if (ancien > 0x10000 && longueur > 0) SafeRead((void*)ancien, (void*)tampon, (size_t)longueur * 2);
        int32_t nouveauMax = MAX_SAISIE;
        SafeWrite((void*)(input + IN_TAMPON), &tampon, 8);
        SafeWrite((void*)(input + IN_LONGUEUR), &longueur, 4);
        SafeWrite((void*)(input + IN_MAX), &nouveauMax, 4);
    }
    else printf("[MENU] champ \"%s\" : allocation du tampon KO, %d caracteres au plus\n", cle, max);

    uint8_t zero = 0;
    SafeWrite((void*)(input + IN_LARGEUR_LIMITE), &zero, 1);
}

// Colonne Archipelago : copie VIDE de MENU_LAYOUT, decalee a cote de lui.
// Reflexion de UIElementRecipe (FUN_14110fc60) : +0x80 Position (Vector),
// +0x90 Anchor, +0xA0 Dock (eUIDockStyle), +0x123 Ignore layout, +0x134
// Anchor by. La recette est une donnee partagee : on y met le temps de
// l'instanciation 0 enfant et une position decalee, puis on remet tout.
constexpr int REC_POSITION   = 0x80;
constexpr int REC_ANCRE      = 0x90;
constexpr int REC_DOCK       = 0xA0;
constexpr int REC_SANS_LAYOUT= 0x123;
constexpr int REC_ANCRE_PAR  = 0x134;

// Diagnostic du 3e test (colonne creee mais invisible) : chaine des parents
// avec translation locale (matrice de l'element +0x50, translation +0x80),
// taille +0x210, drapeaux +0x270 (bit 0 = visible, FUN_14105ac90), couleur
// +0x120 et, dans la recette, "Clip children" +0x128 et "Hidden" +0x122.
constexpr int EL_TRANSLATION = 0x80;
constexpr int EL_DRAPEAUX    = 0x270;
constexpr int EL_COULEUR     = 0x120;
constexpr int REC_CLIP       = 0x128;
constexpr int REC_CACHE      = 0x122;

typedef void (*Montrer_t)(uintptr_t element, char visible, char anime, char forcer);
static const uintptr_t OFF_MONTRER = Adr(0x105AC90);   // FUN_14105ac90 : montrer / cacher

void DecrireChaine(const char* qui, uintptr_t el)
{
    printf("[MENU] chaine de %s :\n", qui);
    for (int prof = 0; el > 0x10000 && prof < 10; ++prof) {
        char nom[64] = "", cls[96] = "?";
        float t[3] = {}, taille[2] = {}, coul[4] = {};
        uint32_t drapeaux = 0;
        uintptr_t rec = 0, parent = 0;
        uint8_t clip = 0, cache = 0;
        LireNom(el, EL_NOM, nom, sizeof(nom));
        GetClassNameFP(el, cls, sizeof(cls));
        SafeRead((void*)(el + EL_TRANSLATION), t, sizeof(t));
        SafeRead((void*)(el + EL_TAILLE), taille, sizeof(taille));
        SafeRead((void*)(el + EL_COULEUR), coul, sizeof(coul));
        SafeRead((void*)(el + EL_DRAPEAUX), &drapeaux, 4);
        if (SafeRead((void*)(el + EL_RECETTE), &rec, 8) && rec > 0x10000) {
            SafeRead((void*)(rec + REC_CLIP), &clip, 1);
            SafeRead((void*)(rec + REC_CACHE), &cache, 1);
        }
        printf("[MENU]   %*s\"%s\" %s 0x%llX : translation (%.1f, %.1f, %.1f), taille %.1f x %.1f, "
               "drapeaux 0x%X, couleur (%.2f, %.2f, %.2f, %.2f), clip %d, cache %d\n",
            prof * 2, "", nom, cls, (unsigned long long)el, t[0], t[1], t[2], taille[0], taille[1],
            drapeaux, coul[0], coul[1], coul[2], coul[3], clip, cache);
        if (!SafeRead((void*)(el + EL_PARENT), &parent, 8)) break;
        el = parent;
    }
}

void Montrer(uintptr_t el)
{
    __try { ((Montrer_t)(Base() + OFF_MONTRER))(el, 1, 0, 1); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a l'affichage de 0x%llX\n", GetExceptionCode(), (unsigned long long)el);
    }
}

constexpr int LAY_TYPE           = 0x200;
constexpr int LAY_ORIENTATION    = 0x204;
constexpr int LAY_ALIGNEMENT     = 0x208;
constexpr int LAY_ALIGN_CELLULE  = 0x20C;
constexpr int LAY_TAILLE_CONTENU = 0x211;
constexpr int LAY_PADDING        = 0x214;
constexpr int LAY_MARGE          = 0x224;
constexpr float MARGE_VERTICALE  = 12.0f;

uintptr_t CreerColonne(uintptr_t modele, float decalageX)
{
    uintptr_t rec = 0, res = 0, parent = 0;
    if (!SafeRead((void*)(modele + EL_RECETTE), &rec, 8) || rec < 0x10000 ||
        !SafeRead((void*)(modele + EL_RESSOURCE), &res, 8) ||
        !SafeRead((void*)(modele + EL_PARENT), &parent, 8) || parent < 0x10000) return 0;

    float pos[4] = {}, ancre[4] = {};
    int32_t dock = -1, ancrePar = -1, compte = 0;
    uint8_t sansLayout = 0;
    SafeRead((void*)(rec + REC_POSITION), pos, sizeof(pos));
    SafeRead((void*)(rec + REC_ANCRE), ancre, sizeof(ancre));
    SafeRead((void*)(rec + REC_DOCK), &dock, 4);
    SafeRead((void*)(rec + REC_SANS_LAYOUT), &sansLayout, 1);
    SafeRead((void*)(rec + REC_ANCRE_PAR), &ancrePar, 4);
    SafeRead((void*)(rec + REC_COMPTE), &compte, 4);
    char clsParent[96] = "?";
    GetClassNameFP(parent, clsParent, sizeof(clsParent));
    printf("[MENU] recette de MENU_LAYOUT : position (%.1f, %.1f, %.1f), ancre (%.2f, %.2f), dock %d, "
           "ancre par %d, ignore layout %d, %d enfant(s) ; parent %s\n",
        pos[0], pos[1], pos[2], ancre[0], ancre[1], dock, ancrePar, sansLayout, compte, clsParent);

    // 5e test : intitules et champs se chevauchent un peu dans notre colonne.
    // Reflexion de UILayoutRecipe (FUN_141111850) : +0x200 Layout Type, +0x204
    // Orientation, +0x208 Alignment, +0x20C Cell alignment, +0x211 Use content
    // size, +0x214..+0x220 Padding (gauche, droite, haut, bas), +0x224 / +0x228
    // Cell margin horizontale / VERTICALE. On ajoute MARGE_VERTICALE a la
    // marge verticale, le temps de l'instanciation comme le reste.
    uint32_t type = 0, orientation = 0, alignement = 0, alignCellule = 0;
    uint8_t tailleContenu = 0;
    float marge[2] = {}, padding[4] = {};
    SafeRead((void*)(rec + LAY_TYPE), &type, 4);
    SafeRead((void*)(rec + LAY_ORIENTATION), &orientation, 4);
    SafeRead((void*)(rec + LAY_ALIGNEMENT), &alignement, 4);
    SafeRead((void*)(rec + LAY_ALIGN_CELLULE), &alignCellule, 4);
    SafeRead((void*)(rec + LAY_TAILLE_CONTENU), &tailleContenu, 1);
    SafeRead((void*)(rec + LAY_PADDING), padding, sizeof(padding));
    SafeRead((void*)(rec + LAY_MARGE), marge, sizeof(marge));
    printf("[MENU] layout de MENU_LAYOUT : type %u, orientation %u, alignement %u / %u, taille du contenu %d, "
           "padding (%.1f, %.1f, %.1f, %.1f), marge des cellules (%.1f, %.1f)\n", type, orientation, alignement,
        alignCellule, tailleContenu, padding[0], padding[1], padding[2], padding[3], marge[0], marge[1]);

    float decale[4] = { pos[0] + decalageX, pos[1], pos[2], pos[3] };
    float margeV = marge[1] + MARGE_VERTICALE;
    int32_t zero = 0;
    SafeWrite((void*)(rec + REC_COMPTE), &zero, 4);
    SafeWrite((void*)(rec + REC_POSITION), decale, sizeof(decale));
    SafeWrite((void*)(rec + LAY_MARGE + 4), &margeV, 4);
    uintptr_t colonne = InstancierNoeud(rec, res);
    SafeWrite((void*)(rec + LAY_MARGE + 4), &marge[1], 4);
    SafeWrite((void*)(rec + REC_POSITION), pos, sizeof(pos));
    SafeWrite((void*)(rec + REC_COMPTE), &compte, 4);
    if (!colonne) { printf("[MENU] copie de MENU_LAYOUT KO\n"); return 0; }

    // 3e test (2026-10-31) : le parent de MENU_LAYOUT est "SFX_TXT_MASK"
    // (UITemplateView), le MASQUE en tache d'encre : tout enfant n'est visible
    // que dans la zone noire (la colonne decalee n'apparaissait qu'en filigrane
    // au bord de la tache). Ce masque est en (0, 0) dans "INK_PANEL" : la
    // colonne va dans INK_PANEL, memes coordonnees, ajoutee en dernier (dessinee
    // par-dessus le masque).
    uintptr_t hote = parent;
    if (StrContains(clsParent, "TemplateView")) {
        uintptr_t grand = 0;
        if (SafeRead((void*)(parent + EL_PARENT), &grand, 8) && grand > 0x10000) hote = grand;
    }
    Inserer(hote, colonne, 0);
    char nomHote[64] = "";
    LireNom(hote, EL_NOM, nomHote, sizeof(nomHote));
    printf("[MENU] colonne Archipelago 0x%llX, decalee de %.1f en x, dans \"%s\"\n", (unsigned long long)colonne,
        decalageX, nomHote);
    return colonne;
}

// Instanciation d'un champ. Reflexion de UITextRecipe (FUN_1411c4960) :
// +0x214 Horizontal alignment (0 Left, 1 Right, 2 Center), +0x218 Vertical
// alignment, +0x24C Upper case, +0x25B / +0x25C Adjust width / height. La
// recette est partagee avec le panneau de sauvegarde du jeu : toute
// modification ne dure que le temps de l'instanciation. (L'essai "Center" du
// 8e test est retire, cf. plus bas.)
constexpr int REC_TEXTE_ALIGN_H = 0x214;

uintptr_t InstancierChampCentre(uintptr_t noeud, uintptr_t ressource)
{
    uint32_t align = 0;
    SafeRead((void*)(noeud + REC_TEXTE_ALIGN_H), &align, 4);
    static bool journalise = false;
    if (!journalise) {
        journalise = true;
        printf("[MENU] alignement horizontal du champ de sauvegarde : %u (0 gauche, 1 droite, 2 centre)\n", align);
    }
    // "Upper case" coupe aussi pendant l'instanciation (affichage tel que
    // saisi, si l'element en garde une copie ; cf. REC_MAJUSCULES).
    uint8_t maj = 0, zero = 0;
    SafeRead((void*)(noeud + 0x24C), &maj, 1);
    // 10e test : "Center" decale le texte (la colonne place le champ selon la
    // largeur de son texte a la mise en page, puis le texte est centre dans
    // une boite de 264). On garde l'alignement d'origine (gauche) et la
    // colonne est remise en page a chaque modification (RecalculerColonne).
    if (maj) SafeWrite((void*)(noeud + 0x24C), &zero, 1);
    uintptr_t copie = InstancierNoeud(noeud, ressource);
    if (maj) SafeWrite((void*)(noeud + 0x24C), &maj, 1);
    return copie;
}

// La barre blanche au debut de chaque champ (5e test) = SAVE_NAME_INPUT_CURSOR,
// le curseur de saisie, enfant du champ dans la recette de SaveGamePanel :
// copie avec lui, visible et jamais rattache au champ (le panneau de
// sauvegarde le lie par FUN_14112a1d0). On le detruit (vtable +0xE0, comme
// FUN_14112a1d0 pour un ancien curseur) : au focus, FUN_14112a920 cree son
// propre curseur, cache en fin d'edition par FUN_141129940.
typedef void (*Detruire_t)(uintptr_t element);

void RetirerCurseurCopie(uintptr_t copie, const char* cle)
{
    uintptr_t curseur = ChercherEnfant(copie, "SAVE_NAME_INPUT_CURSOR");
    if (!curseur) { printf("[MENU] champ \"%s\" : pas de curseur copie\n", cle); return; }
    uintptr_t vt = 0, fn = 0;
    if (!SafeRead((void*)curseur, &vt, 8) || !SafeRead((void*)(vt + 0xE0), &fn, 8) || !fn) return;
    __try { ((Detruire_t)fn)(curseur); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX a la destruction du curseur copie\n", GetExceptionCode());
        return;
    }
    printf("[MENU] champ \"%s\" : curseur copie 0x%llX detruit\n", cle, (unsigned long long)curseur);
}

// Trait pointille entre les groupes (demande de l'auteur, 2026-10-31), comme
// entre les boutons du menu : modele = premier enfant de MENU_LAYOUT qui n'est
// pas un bouton. Tous les enfants sont journalises (nom, classe) pour le
// verifier.
uintptr_t TrouverSeparateur(uintptr_t colonne)
{
    uintptr_t enfant = 0, modele = 0;
    SafeRead((void*)(colonne + EL_ENFANT), &enfant, 8);
    printf("[MENU] enfants de MENU_LAYOUT :\n");
    for (int garde = 0; enfant > 0x10000 && garde < 64; ++garde) {
        char nom[64] = "", cls[96] = "?";
        LireNom(enfant, EL_NOM, nom, sizeof(nom));
        GetClassNameFP(enfant, cls, sizeof(cls));
        printf("[MENU]   \"%s\" %s 0x%llX\n", nom, cls, (unsigned long long)enfant);
        if (!modele && !StrEq(cls, "UIButton")) modele = enfant;
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
    if (modele) DecrireElement("separateur modele", modele);
    else printf("[MENU] aucun separateur dans MENU_LAYOUT\n");
    return modele;
}

void AjouterSeparateur(uintptr_t modele, uintptr_t parent, uintptr_t avant)
{
    if (!modele) return;
    uintptr_t s = Cloner(modele);
    if (!s) { printf("[MENU] clonage d'un separateur KO\n"); return; }
    Inserer(parent, s, avant);
}

// --- Libelle d'etat de la connexion (bug du 2026-11-04) ----------------------------
// Au menu, aucune pompe ne tourne et les messages attendent la partie : l'etat
// de la connexion (ApEtatConnexion, archipelago.cpp) s'affiche dans un intitule
// sous Connect. Aucun de nos hooks ne tourne a chaque image au menu (le clic
// de l'ecran FUN_14102f720 ne voit que la souris) : minuterie Windows SANS
// fenetre, armee depuis Construire (thread de l'interface) ; son rappel est
// execute par la boucle de messages de CE thread (DispatchMessage), donc sur
// le thread de l'interface. Le rappel verifie le thread (journal [MENU]).
uintptr_t g_poigneeEtat = 0;     // poignee de l'intitule d'etat
long      g_etatVu = 0;          // generation de l'etat affiche
DWORD     g_threadInterface = 0; // thread de Construire
UINT_PTR  g_minuterieEtat = 0;

void VerrouSuivreConnexion();   // plus bas (boutons du menu sans connexion)

VOID CALLBACK MinuterieEtat(HWND, UINT, UINT_PTR, DWORD)
{
    static bool premierPassage = true;
    if (premierPassage) {
        premierPassage = false;
        printf("[MENU] minuterie d'etat : thread %lu (interface %lu)%s\n", GetCurrentThreadId(), g_threadInterface,
            GetCurrentThreadId() == g_threadInterface ? "" : " -> DIFFERENT, libelle non mis a jour");
    }
    if (GetCurrentThreadId() != g_threadInterface || !g_poigneeEtat) return;
    char etat[96];
    long gen = ApEtatConnexion(etat, sizeof(etat));
    if (gen == g_etatVu) return;
    uintptr_t t = ObjetDePoignee(g_poigneeEtat);
    if (!t || !EstTexte(t)) return;   // panneau detruit (partie lancee)
    // Panneau vivant (son intitule l'est) : boutons selon la connexion.
    VerrouSuivreConnexion();
    // Latin-1 -> UTF-16 : chaque octet est son point de code.
    wchar_t w[96];
    int i = 0;
    for (; etat[i] && i < 95; ++i) w[i] = (wchar_t)(unsigned char)etat[i];
    w[i] = 0;
    g_etatVu = gen;
    PoserTexte(t, w);
    RecalculerColonne();
}

void Construire(uintptr_t panneau)
{
    uintptr_t poigneeRacine = 0;
    SafeRead((void*)(panneau + PAN_RACINE), &poigneeRacine, 8);
    uintptr_t racine = ObjetDePoignee(poigneeRacine);
    if (!racine) { printf("[MENU] panneau principal sans racine\n"); return; }

    uintptr_t sortie = ChercherEnfant(racine, "EXIT_BUTTON");
    if (!sortie) { printf("[MENU] EXIT_BUTTON introuvable\n"); return; }
    DecrireElement("bouton modele", sortie);
    uintptr_t parent = 0;
    SafeRead((void*)(sortie + EL_PARENT), &parent, 8);
    if (parent < 0x10000) { printf("[MENU] EXIT_BUTTON sans parent\n"); return; }
    DecrireElement("colonne des boutons", parent);

    // Notre bloc va dans une colonne a cote de MENU_LAYOUT (test du
    // 2026-10-31 : en tete de MENU_LAYOUT, il repoussait les options du jeu
    // trop bas). Repli : en tete de MENU_LAYOUT.
    float tailleColonne[2] = {};
    SafeRead((void*)(parent + EL_TAILLE), tailleColonne, sizeof(tailleColonne));
    float decalage = (tailleColonne[0] > 264.0f ? tailleColonne[0] : 264.0f) + 60.0f;
    // Steam : colonne a GAUCHE de MENU_LAYOUT (demande de l'auteur,
    // 2026-11-10), meme ecart ; Epic : a droite, comme avant.
    if (VersionDuJeu() == VersionJeu::Steam) decalage = -decalage;
    printf("[MENU] taille de MENU_LAYOUT : %.1f x %.1f\n", tailleColonne[0], tailleColonne[1]);
    uintptr_t cible = CreerColonne(parent, decalage);
    uintptr_t avant = 0;
    if (!cible) {
        cible = parent;
        SafeRead((void*)(parent + EL_ENFANT), &avant, 8);
        if (avant < 0x10000) avant = sortie;
    }

    uintptr_t separateur = TrouverSeparateur(parent);
    uintptr_t texteModele = PremierDescendant(sortie, &EstTexte, 0);
    if (texteModele) DecrireElement("texte modele", texteModele);
    else printf("[MENU] EXIT_BUTTON sans texte : pas d'intitules\n");

    ChargerValeurs();
    uintptr_t ressource = 0;
    uintptr_t noeud = SourceSaisie(&ressource);

    for (int i = 0; i < NB_CHAMPS; ++i) {
        Champ& c = g_champs[i];
        LacherPoignee(c.poignee);
        c.objet = 0;
        if (texteModele) AjouterLibelle(texteModele, cible, avant, c.libelle);
        if (!noeud) continue;
        uintptr_t copie = InstancierChampCentre(noeud, ressource);
        uintptr_t input = PremierDescendant(copie, &EstChampDeSaisie, 0);
        if (!input) {
            printf("[MENU] champ \"%s\" : copie 0x%llX sans UITextInput\n", c.cle, (unsigned long long)copie);
            continue;
        }
        if (i == 0) DecrireElement("champ clone", copie);
        RetirerCurseurCopie(copie, c.cle);
        PreparerChamp(input, c.cle);
        c.objet = input;
        c.poignee = PrendrePoignee(input);
        Inserer(cible, copie, avant);
        // Valeur vide : un espace, sinon le champ affiche son texte par
        // defaut "New save" (7e test). LireChamp retire les espaces.
        PoserTexte(input, c.valeur[0] ? c.valeur : L" ");
        float taille[2] = {};
        SafeRead((void*)(input + EL_TAILLE), taille, sizeof(taille));
        printf("[MENU] champ \"%s\" : UITextInput 0x%llX, taille %.1f x %.1f, %d caracteres au plus\n", c.cle,
            (unsigned long long)input, taille[0], taille[1], MAX_SAISIE);
        AjouterSeparateur(separateur, cible, avant);   // apres chaque champ, avant Connect compris
    }
    AjouterBouton(sortie, cible, avant, panneau, L"Connect", &CbConnect);

    // Intitule d'etat sous Connect, rafraichi par la minuterie (texte pose au
    // premier passage : g_etatVu remis a zero).
    LacherPoignee(g_poigneeEtat);
    g_etatVu = 0;
    if (texteModele) {
        uintptr_t e = AjouterLibelle(texteModele, cible, avant, L"Not connected");
        if (e) g_poigneeEtat = PrendrePoignee(e);
    }
    g_threadInterface = GetCurrentThreadId();
    if (!g_minuterieEtat) {
        g_minuterieEtat = SetTimer(nullptr, 0, 250, &MinuterieEtat);
        printf("[MENU] minuterie d'etat %s (thread %lu)\n", g_minuterieEtat ? "armee" : "KO", g_threadInterface);
    }

    char cls[96] = "";
    GetClassNameFP(cible, cls, sizeof(cls));
    if (StrContains(cls, "Layout")) {
        __try { ((Relayout_t)(Base() + OFF_RELAYOUT))(cible); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            printf("[MENU] EXCEPTION 0x%08lX au recalcul de la colonne\n", GetExceptionCode());
        }
    }
    float tailleCible[2] = {};
    SafeRead((void*)(cible + EL_TAILLE), tailleCible, sizeof(tailleCible));
    if (cible != parent) Montrer(cible);
    printf("[MENU] ecran de connexion Archipelago pose (champs %s, colonne %s, %.1f x %.1f)\n",
        noeud ? "OK" : "ABSENTS", cible == parent ? "MENU_LAYOUT" : "propre", tailleCible[0], tailleCible[1]);
    DecrireChaine("MENU_LAYOUT", parent);
    if (cible != parent) DecrireChaine("la colonne Archipelago", cible);
    g_colonne = cible != parent ? cible : 0;
    g_menuLayout = parent;
    g_instantPose = GetTickCount64();
}

// Second releve 3 s apres la pose (le panneau anime son apparition) : appele
// par le clic de l'ecran, donc sur le thread du jeu, une seule fois.
void ReleverPlusTard()
{
    if (!g_instantPose || GetTickCount64() - g_instantPose < 3000) return;
    g_instantPose = 0;
    printf("[MENU] releve 3 s apres la pose :\n");
    DecrireChaine("MENU_LAYOUT", g_menuLayout);
    if (!g_colonne) return;
    DecrireChaine("la colonne Archipelago", g_colonne);
    // Placement de nos elements par le layout (chevauchement du 5e test).
    uintptr_t enfant = 0;
    SafeRead((void*)(g_colonne + EL_ENFANT), &enfant, 8);
    for (int garde = 0; enfant > 0x10000 && garde < 32; ++garde) {
        char nom[64] = "", cls[96] = "?";
        float t[3] = {}, taille[2] = {};
        LireNom(enfant, EL_NOM, nom, sizeof(nom));
        GetClassNameFP(enfant, cls, sizeof(cls));
        SafeRead((void*)(enfant + EL_TRANSLATION), t, sizeof(t));
        SafeRead((void*)(enfant + EL_TAILLE), taille, sizeof(taille));
        printf("[MENU]   enfant \"%s\" %s : translation (%.1f, %.1f), taille %.1f x %.1f\n", nom, cls, t[0], t[1],
            taille[0], taille[1]);
        if (!SafeRead((void*)(enfant + EL_FRERE), &enfant, 8)) break;
    }
}

// --- Boutons du menu sans sauvegarde (2026-11-04) -------------------------------
// FUN_141b28f10 = rafraichissement du panneau principal (vtable +0x08 du
// panneau ; decompile le 2026-11-04). Profil FUN_14125aed0 : nombre de
// sauvegardes en profil +0x1B8 (liste filtree par sauvegardes_ap.cpp). Sans
// sauvegarde : CONTINUE_BUTTON (+0xD0) cache, NEWGAME_BUTTON (+0xD8, lance
// A New Home) montre, SCENARIO_BUTTON (+0xE8) desactive sauf scenario
// "a debloquer" passe. Demande de l'auteur : le choix du scenario toujours
// possible, le bouton qui lance A New Home grise et non cliquable.
// FUN_14105a3a0(element, actif, 1) = cliquable ; FUN_1410240d0(element,
// &nom, 1, 0, 0, 0, 1, 0, 0) = etat d'animation "ENABLED" / "DISABLED".
static const uintptr_t OFF_RAFRAICHIR_PANNEAU = Adr(0x1B28F10);
static const uintptr_t OFF_ACTIVER_ELEMENT    = Adr(0x105A3A0);
static const uintptr_t OFF_ETAT_ANIMATION     = Adr(0x10240D0);
constexpr int PAN_BOUTON_CONTINUER       = 0xD0;
constexpr int PAN_BOUTON_NOUVELLE_PARTIE = 0xD8;
constexpr int PAN_BOUTON_CHARGER         = 0xE0;   // LOAD (actif si sauvegardes, FUN_141b28f10)
constexpr int PAN_BOUTON_SCENARIOS       = 0xE8;
// Steam seulement (init du panneau FUN_1419c5b80 de l'exe Steam, 2026-11-07) :
// CUSTOM_SCENARIO_BUTTON range en +0xF0 (ENDLESS_BUTTON passe en +0xF8).
// Scenarios personnalises = jamais Archipelago -> toujours inactif.
constexpr int PAN_BOUTON_PERSO_STEAM     = 0xF0;

typedef void (*Rafraichir_t)(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4);
typedef void (*ActiverElement_t)(uintptr_t element, uintptr_t actif, uintptr_t p3, uintptr_t p4);
typedef void (*EtatAnimation_t)(uintptr_t element, uintptr_t* nom, uintptr_t p3, uintptr_t p4, uintptr_t p5,
                                uintptr_t p6, uintptr_t p7, uintptr_t p8, uintptr_t p9);
Rafraichir_t oRafraichirPanneau = nullptr;
uintptr_t    g_panneauRafraichi = 0;   // dernier panneau rafraichi (verrou de connexion)

// Bouton du panneau (poignee rangee en panneau + off), actif ou non.
void ForcerBouton(uintptr_t panneau, int off, bool actif, const char* qui)
{
    uintptr_t poignee = 0;
    SafeRead((void*)(panneau + off), &poignee, 8);
    uintptr_t bouton = ObjetDePoignee(poignee);
    if (!bouton) return;
    uintptr_t nom = 0;
    __try {
        ((ActiverElement_t)(Base() + OFF_ACTIVER_ELEMENT))(bouton, actif ? 1 : 0, 1, 0);
        ((NomConstruire_t)(Base() + OFF_NOM_CONSTRUIRE))(&nom, actif ? "ENABLED" : "DISABLED");
        ((EtatAnimation_t)(Base() + OFF_ETAT_ANIMATION))(bouton, &nom, 1, 0, 0, 0, 1, 0, 0);
        ((NomDetruire_t)(Base() + OFF_NOM_DETRUIRE))(&nom);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX sur le bouton %s\n", GetExceptionCode(), qui);
        return;
    }
    // Journal a chaque changement d'etat d'un bouton (decalages 0xD0..0xF0).
    static int dernier[5] = { -1, -1, -1, -1, -1 };
    int k = (off - PAN_BOUTON_CONTINUER) / 8;
    if (k >= 0 && k < 5 && dernier[k] != (actif ? 1 : 0)) {
        dernier[k] = actif ? 1 : 0;
        printf("[MENU] bouton %s force %s\n", qui, actif ? "actif" : "inactif");
    }
}

// Verrou des boutons selon la connexion (demande de l'auteur, 2026-11-04) :
// sans connexion au serveur, ni choix du scenario, ni Continue, ni Load game
// (New game, qui lance A New Home, est toujours inactif). Connecte : Scenario
// actif ; Continue et Load gardent l'etat pose par le jeu (sauvegardes de la
// session, sauvegardes_ap.cpp) -- d'ou l'appel apres l'original seulement.
bool g_verrouApplique = false, g_verrouConnecte = false;

void AppliquerVerrouConnexion(uintptr_t panneau)
{
    bool connecte = ApEstConnecte();
    ForcerBouton(panneau, PAN_BOUTON_NOUVELLE_PARTIE, false, "NEWGAME_BUTTON");
    ForcerBouton(panneau, PAN_BOUTON_SCENARIOS, connecte, "SCENARIO_BUTTON");
    if (VersionDuJeu() == VersionJeu::Steam)
        ForcerBouton(panneau, PAN_BOUTON_PERSO_STEAM, false, "CUSTOM_SCENARIO_BUTTON");
    if (!connecte) {
        ForcerBouton(panneau, PAN_BOUTON_CONTINUER, false, "CONTINUE_BUTTON");
        ForcerBouton(panneau, PAN_BOUTON_CHARGER, false, "LOAD_BUTTON");
    }
    g_verrouApplique = true;
    g_verrouConnecte = connecte;
}

void hkRafraichirPanneau(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    oRafraichirPanneau(panneau, p2, p3, p4);
    if (!panneau) return;
    g_panneauRafraichi = panneau;
    AppliquerVerrouConnexion(panneau);
}

// Minuterie d'etat : la connexion a change -> rafraichissement complet du
// panneau (le jeu repose Continue / Load, puis notre verrou).
void VerrouSuivreConnexion()
{
    if (!g_panneauRafraichi || !g_verrouApplique || ApEstConnecte() == g_verrouConnecte) return;
    printf("[MENU] connexion %s : boutons du menu mis a jour\n", ApEstConnecte() ? "etablie" : "perdue");
    __try { hkRafraichirPanneau(g_panneauRafraichi, 0, 0, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MENU] EXCEPTION 0x%08lX au rafraichissement du panneau\n", GetExceptionCode());
        g_panneauRafraichi = 0;
    }
}

uintptr_t hkInitPanneau(uintptr_t panneau, uintptr_t a, uintptr_t b, uintptr_t c)
{
    uintptr_t r = oInitPanneau(panneau, a, b, c);
    if (panneau && panneau != g_panneauFait) {
        g_panneauFait = panneau;
        printf("[MENU] init du panneau principal 0x%llX\n", (unsigned long long)panneau);
        Construire(panneau);
    }
    return r;
}

} // namespace

void InstallMenuAP(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_INIT_PANNEAU_PRINCIPAL, (void*)&hkInitPanneau, (void**)&oInitPanneau, "init du panneau principal" },
        { OFF_POSER_TEXTE, (void*)&hkPoserTexte, (void**)&oPoserTexte, "pose d'un texte" },
        { OFF_DEBUT_EDITION, (void*)&hkDebutEdition, (void**)&oDebutEdition, "debut d'edition" },
        { OFF_CLIC_ECRAN, (void*)&hkClicEcran, (void**)&oClicEcran, "clic de l'ecran" },
        { OFF_INSERER_CAR, (void*)&hkInsererCar, (void**)&oInsererCar, "insertion d'un caractere" },
        { OFF_TOUCHE, (void*)&hkTouche, (void**)&oTouche, "touche d'un champ" },
        { OFF_RAFRAICHIR_PANNEAU, (void*)&hkRafraichirPanneau, (void**)&oRafraichirPanneau,
          "rafraichissement du panneau principal" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[MENU] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}
