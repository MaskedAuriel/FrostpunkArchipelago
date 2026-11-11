// groupes.cpp
// ---------------------------------------------------------------------------
// Creation de groupes de survivants sur la carte du monde.
//
// Le jeu n'a aucune classe qui cree un groupe : en vanilla ils sont poses a
// l'avance dans la scene. On passe donc par la creation generique d'entite
// du moteur, celle qu'emploie la creation d'une equipe d'eclaireurs
// (FUN_1418a2f20) :
//
//   entite = Creer(gestionnaire, template, parent, &transform, drapeaux)
//            +0x1016250, gestionnaire = *(base + 0x2B2DE98)
//
// Entite (entity.cpp) : +0x28 CompoundTemplate, +0x40 parent (SceneEntity),
// +0x48 nom, +0xD0 transformation locale 4x4 (position a +0x100), +0x110
// globale, +0x24C scene / calque (copie du parent par Creer).
//
// Valide le 2026-10-28 (Refugees) : copie conforme d'un groupe existant
// (nom, contenu, marche), 2 -> 3 -> 4 groupes dans le systeme.
// ---------------------------------------------------------------------------
#include "groupes.h"
#include "mod_common.h"
#include "MinHook.h"
#include "archipelago.h"
#include "evenements.h"
#include "resources.h"   // Res_Definition (cases de l'inventaire)
#include "scooting.h"    // ScootingExpeditionAffichee (infobulle de l'item AP)
#include "sites_contenu.h"  // SitesEntreeSurvivants (survivants par classe)
#include "quetes.h"         // QuetesDemanderFinQuete (refugies de 006)
#include "ap_ressources.h"  // ApPorteurDef (item AP de l'inventaire des refugies)
#include "scenario.h"       // ScenarioVictoryId (groupes de The Refugees)
#include <cmath>
#include <cstring>

namespace {

typedef uintptr_t(*CreerEntite_t)(uintptr_t gestionnaire, uintptr_t tpl,
    uintptr_t parent, const void* transform, uint32_t drapeaux);

static const uintptr_t OFF_CREER_ENTITE   = Adr(0x1016250);
static const uintptr_t OFF_GESTIONNAIRE   = Adr(0x2B2DE98);
constexpr int       ENT_TEMPLATE       = 0x28;
constexpr int       ENT_PARENT         = 0x40;
constexpr int       ENT_NOM            = 0x48;
constexpr int       ENT_TRANSFORM      = 0xD0;
constexpr int       ENT_POSITION       = 0x100;
constexpr int       ENT_SCENE          = 0x24C;
constexpr int       ID_SURVIVOR_GROUP  = 0x6D;

volatile long g_creationsDemandees = 0;

// --- Arbre des templates du moteur (EntityTemplateStub) ---------------------
// Decompiles du 2026-10-28 : le pool DAT_142b2e710 commence par une section
// critique ; +0x28 pointe la racine d'un arbre de repertoires. Chaque
// repertoire : sous-repertoires tries a +0x10 (compte +0x18, sousRep[0] = nom),
// feuilles triees a +0x28 (compte +0x30). Une feuille est un
// EntityTemplateStub : +0x08 nom, +0x38 template charge (0 sinon).
// FUN_141035dd0(pool, "a/b/nom", 1) cherche la feuille (sans charger) ;
// FUN_14111d100(stub, 0) renvoie le template en le chargeant au besoin, sur
// le thread appelant (chemin du chargeur de CompoundTemplate FUN_1411d0940).
typedef uintptr_t(*ChercherStub_t)(uintptr_t pool, const char* chemin, uintptr_t p3);
typedef uintptr_t(*ObtenirTemplate_t)(uintptr_t stub, uintptr_t p2);

static const uintptr_t OFF_POOL_TEMPLATES   = Adr(0x2B2E710);
static const uintptr_t OFF_CHERCHER_STUB    = Adr(0x1035DD0);
static const uintptr_t OFF_OBTENIR_TEMPLATE = Adr(0x111D100);
constexpr int       POOL_RACINE          = 0x28;
constexpr int       REP_SOUSREPS         = 0x10;
constexpr int       REP_STUBS            = 0x28;
constexpr int       STUB_NOM             = 0x08;
constexpr int       STUB_TEMPLATE        = 0x38;
constexpr int       MAX_CHEMIN           = 260;
constexpr int       MAX_CANDIDATS        = 32;

// Feuille trouvee sous un repertoire "SurvivorGroups".
struct Candidat { uintptr_t stub; uintptr_t tpl; char chemin[MAX_CHEMIN]; };

struct Parcours {
    bool     bavard;
    int      nRep, nStub;
    Candidat cand[MAX_CANDIDATS];
    int      nCand;
};

// Nom (texte 8 bits) pointe par le premier qword de 'obj' + off, ou "?".
void LireNomPointe(uintptr_t obj, int off, char* out, int maxlen)
{
    uintptr_t p = 0;
    out[0] = '?'; out[1] = 0;
    if (SafeRead((void*)(obj + off), &p, 8) && p > 0x10000) ReadAscii(p, out, maxlen);
}

// Tableau dynamique {ptr, compte 32 bits} a 'obj + off' ; false si absurde.
bool LireTableau(uintptr_t obj, int off, uintptr_t* tab, int32_t* n)
{
    *tab = 0; *n = 0;
    return SafeRead((void*)(obj + off), tab, 8) && SafeRead((void*)(obj + off + 8), n, 4) &&
           *tab > 0x10000 && *n > 0 && *n < 100000;
}

// 'chemin' (longueur 'len') = chemin du repertoire, relatif a la racine et
// termine par '/' : c'est la forme qu'attend FUN_141035dd0.
void ParcourirRep(uintptr_t rep, char* chemin, int len, int prof, bool dansGroupes, Parcours& p)
{
    if (prof > 16 || p.nRep > 200000) return;
    ++p.nRep;

    uintptr_t tab = 0;
    int32_t n = 0;
    if (LireTableau(rep, REP_STUBS, &tab, &n)) {
        p.nStub += n;
        for (int i = 0; dansGroupes && i < n; ++i) {
            uintptr_t stub = 0, tpl = 0;
            if (!SafeRead((void*)(tab + i * 8), &stub, 8) || stub < 0x10000) continue;
            char nom[128];
            LireNomPointe(stub, STUB_NOM, nom, sizeof(nom));
            SafeRead((void*)(stub + STUB_TEMPLATE), &tpl, 8);
            if (p.bavard)
                printf("[ARBRE]   %s%s : stub 0x%llX, template %s 0x%llX\n", chemin, nom,
                    (unsigned long long)stub, tpl ? "charge" : "non charge", (unsigned long long)tpl);
            if (p.nCand < MAX_CANDIDATS) {
                Candidat& c = p.cand[p.nCand++];
                c.stub = stub;
                c.tpl = tpl;
                snprintf(c.chemin, sizeof(c.chemin), "%s%s", chemin, nom);
            }
        }
    }

    if (!LireTableau(rep, REP_SOUSREPS, &tab, &n)) return;
    for (int i = 0; i < n; ++i) {
        uintptr_t sous = 0;
        if (!SafeRead((void*)(tab + i * 8), &sous, 8) || sous < 0x10000) continue;
        char nom[128];
        LireNomPointe(sous, 0, nom, sizeof(nom));
        int l = snprintf(chemin + len, MAX_CHEMIN - len, "%s/", nom);
        if (l <= 0 || len + l >= MAX_CHEMIN) { chemin[len] = 0; continue; }
        bool groupes = dansGroupes || _stricmp(nom, "SurvivorGroups") == 0;
        if (p.bavard && (prof <= 1 || groupes)) printf("[ARBRE] rep %s\n", chemin);
        ParcourirRep(sous, chemin, len + l, prof + 1, groupes, p);
        chemin[len] = 0;
    }
}

// Parcours complet depuis la racine du pool (lecture seule, sans verrou :
// SafeRead suffit pour un diagnostic). false si la racine est absente.
bool ParcourirArbre(Parcours& p)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t racine = 0;
    if (!SafeRead((void*)(base + OFF_POOL_TEMPLATES + POOL_RACINE), &racine, 8) || racine < 0x10000) {
        printf("[ARBRE] racine du pool absente (pool +0x28 = 0x%llX)\n", (unsigned long long)racine);
        return false;
    }
    if (p.bavard) {
        char nom[128];
        LireNomPointe(racine, 0, nom, sizeof(nom));
        printf("[ARBRE] racine 0x%llX \"%s\"\n", (unsigned long long)racine, nom);
    }
    char chemin[MAX_CHEMIN] = "";
    ParcourirRep(racine, chemin, 0, 0, false, p);
    if (p.bavard)
        printf("[ARBRE] %d repertoire(s), %d stub(s), %d sous SurvivorGroups\n", p.nRep, p.nStub, p.nCand);
    return true;
}

// Candidat dont le template est celui du groupe existant, sinon le premier.
int ChoisirCandidat(const Parcours& p, uintptr_t tplVoulu)
{
    for (int i = 0; tplVoulu && i < p.nCand; ++i)
        if (p.cand[i].tpl == tplVoulu) return i;
    return p.nCand > 0 ? 0 : -1;
}

// Thread du jeu. Meme sequence que FUN_1411d0940 : verrou du pool, recherche
// du stub, obtention (chargement synchrone au besoin), deverrouillage.
uintptr_t ObtenirTemplateParChemin(const char* chemin, uintptr_t* stubTrouve)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t pool = base + OFF_POOL_TEMPLATES;
    ChercherStub_t chercher = (ChercherStub_t)(base + OFF_CHERCHER_STUB);
    ObtenirTemplate_t obtenir = (ObtenirTemplate_t)(base + OFF_OBTENIR_TEMPLATE);

    char tampon[MAX_CHEMIN];
    strcpy_s(tampon, chemin);
    uintptr_t stub = 0, tpl = 0;
    volatile bool verrou = false;
    JournalImmediat("[GRP] obtention du template \"%s\" ...\n", tampon);
    __try {
        EnterCriticalSection((LPCRITICAL_SECTION)pool);
        verrou = true;
        stub = chercher(pool, tampon, 1);
        if (stub) tpl = obtenir(stub, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[GRP] EXCEPTION 0x%08lX pendant l'obtention du template\n", GetExceptionCode());
    }
    if (verrou) LeaveCriticalSection((LPCRITICAL_SECTION)pool);
    *stubTrouve = stub;
    return tpl;
}

bool PremierGroupe(uintptr_t* entite, uintptr_t* composant);

// F9 : structure de l'arbre et templates de groupe disponibles, y compris
// au menu ou dans un scenario sans groupe.
void RapportArbre()
{
    Parcours p = {};
    p.bavard = true;
    if (!ParcourirArbre(p)) return;

    uintptr_t groupe = 0, comp = 0, tplGroupe = 0;
    if (PartiePrete() && PremierGroupe(&groupe, &comp)) {
        SafeRead((void*)(groupe + ENT_TEMPLATE), &tplGroupe, 8);
        int k = ChoisirCandidat(p, tplGroupe);
        if (k >= 0 && p.cand[k].tpl == tplGroupe)
            printf("[ARBRE] template du premier groupe (0x%llX) = stub \"%s\"\n",
                (unsigned long long)tplGroupe, p.cand[k].chemin);
        else
            printf("[ARBRE] template du premier groupe (0x%llX) absent des stubs SurvivorGroups\n",
                (unsigned long long)tplGroupe);
    }
}

int CompteGroupes()
{
    uintptr_t sys = SystemeParId(ID_SURVIVOR_GROUP);
    int n = 0;
    if (!sys || !SafeRead((void*)(sys + 0x2080), &n, 4)) return -1;
    return n;
}

// Premier groupe vivant de la liste du systeme : modele a copier.
bool PremierGroupe(uintptr_t* entite, uintptr_t* composant)
{
    uintptr_t comps[16];
    int n = ComposantsDuSysteme(ID_SURVIVOR_GROUP, "FrostpunkSurvivorGroupComponent", comps, 16);
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(comps[i] + 0x18), &e, 8) && e > 0x10000) {
            *entite = e;
            *composant = comps[i];
            return true;
        }
    }
    return false;
}

// Entite du premier composant d'un systeme (site, ville...), 0 sinon.
uintptr_t EntiteDuSysteme(int idSysteme, const char* classe)
{
    uintptr_t comps[1];
    if (ComposantsDuSysteme(idSysteme, classe, comps, 1) < 1) return 0;
    uintptr_t e = 0;
    SafeRead((void*)(comps[0] + 0x18), &e, 8);
    return e;
}

// Une ligne par entite : nom, scene, position, puis sa chaine de parents.
void DecrireEntite(const char* etiquette, uintptr_t e)
{
    if (!e) { printf("[GRP] %-6s : absente\n", etiquette); return; }
    uintptr_t pNom = 0;
    char nom[64] = "?";
    if (SafeRead((void*)(e + ENT_NOM), &pNom, 8) && pNom > 0x10000) ReadAscii(pNom, nom, sizeof(nom));
    int32_t scene = -1;
    float pos[3] = {};
    SafeRead((void*)(e + ENT_SCENE), &scene, 4);
    SafeRead((void*)(e + ENT_POSITION), pos, sizeof(pos));
    printf("[GRP] %-6s : 0x%llX \"%s\" scene %d, position %.1f / %.1f / %.1f\n", etiquette,
        (unsigned long long)e, nom, scene, pos[0], pos[1], pos[2]);

    uintptr_t p = e;
    for (int niveau = 1; niveau <= 5; ++niveau) {
        if (!SafeRead((void*)(p + ENT_PARENT), &p, 8) || !p) break;
        char cls[64] = "?";
        GetClassNameFP(p, cls, sizeof(cls));
        nom[0] = '?'; nom[1] = 0;
        if (SafeRead((void*)(p + ENT_NOM), &pNom, 8) && pNom > 0x10000) ReadAscii(pNom, nom, sizeof(nom));
        SafeRead((void*)(p + ENT_SCENE), &scene, 4);
        printf("         parent %d : 0x%llX (%s) \"%s\" scene %d\n", niveau,
            (unsigned long long)p, cls, nom, scene);
    }
}

uintptr_t AppelerCreer(uintptr_t gestionnaire, uintptr_t tpl, uintptr_t parent, const float* m)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    CreerEntite_t creer = (CreerEntite_t)(base + OFF_CREER_ENTITE);
    JournalImmediat("[GRP] appel de Creer (+1016250) ...\n");
    __try {
        return creer(gestionnaire, tpl, parent, m, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[GRP] EXCEPTION 0x%08lX dans Creer\n", GetExceptionCode());
        return 0;
    }
}

// --- Groupes Archipelago : template clone, textes, contenu, duree ------------
// Pour personnaliser UN groupe, on lui donne sa propre copie du template de
// composant (FrostpunkSurvivorGroupComponentTemplate, 0x140 octets). La copie
// est posee par le hook de FUN_1418b1cc0 (composant ajoute au systeme des
// groupes, appele pendant Creer), AVANT que le jeu n'y lise survivants, nom,
// icone et vitesse : tout le reste suit le chemin natif. La copie n'est pas
// sauvegardee (la sauvegarde reference le template d'origine par GUID).
//
// Decompiles du 2026-10-28 :
// - template : +0x40 vitesse (ChangeRate), +0x50/+0x58 nom (2 variantes),
//   +0x60/+0x68 description, +0x70/+0x88 icone (0x18 chacune), +0x110 mort ;
// - composant : +0x20 template, +0x34 Initialized (marche lancee), +0x48
//   contenu propre au groupe (SurvivorsEntry de 0x24 : +0 Count, +4 classe
//   sociale, +0x14 statistiques ; compte +0x50), +0x78 taux de mort ;
// - LinearMovementComponent (FUN_141884960(DAT_14409ac18, entite)) : +0x30
//   depart, +0x40 destination, +0x50 date de depart, +0x58 date d'arrivee ;
//   date courante *(DAT_142c32a40) + 0xB0, en secondes x 2^31 ; une heure
//   de jeu = *(DAT_142c38ff0) secondes.
typedef void(*ComposantAjoute_t)(uintptr_t sys, uintptr_t entite, uintptr_t comp, uintptr_t p4);
typedef uintptr_t(*MouvementDe_t)(uintptr_t sysMouvement, uintptr_t entite);

static const uintptr_t OFF_COMPOSANT_AJOUTE   = Adr(0x18B1CC0);
static const uintptr_t OFF_MOUVEMENT_DE       = Adr(0x1884960);
static const uintptr_t OFF_SYS_MOUVEMENT      = Adr(0x409AC18);
static const uintptr_t OFF_HORLOGE            = Adr(0x2C32A40);
static const uintptr_t OFF_SECONDES_PAR_HEURE = Adr(0x2C38FF0);
constexpr int       HORLOGE_DATE           = 0xB0;
constexpr double    TICKS_PAR_SECONDE      = 2147483648.0;
constexpr int       TAILLE_TPL_GROUPE      = 0x140;
constexpr int       TPL_VITESSE            = 0x40;    // ChangeRate : float, unite +0x44
constexpr int       TPL_NOM                = 0x50;
constexpr int       TPL_DESCRIPTION        = 0x60;
constexpr int       COMP_TEMPLATE          = 0x20;
constexpr int       COMP_INITIALISE        = 0x34;
constexpr int       COMP_CONTENU           = 0x48;
constexpr int       COMP_TAUX_MORT         = 0x78;
constexpr int       ENTREE_SURVIVANTS      = 0x24;
constexpr int       ENT_POSITION_MONDE     = 0x140;
constexpr int       MOUV_DEPART            = 0x30;
constexpr int       MOUV_DATE_DEPART       = 0x50;
constexpr int       MOUV_DATE_ARRIVEE      = 0x58;
constexpr int       MAX_GROUPES_AP         = 16;
constexpr int       TPL_DILEMME_ARRIVEE    = 0xF8;    // lien (GUID 16 octets) "City reached dilemma"
constexpr int       COMP_DILEMME_ARRIVEE   = 0x80;    // dilemme resolu (FUN_141a0f440)
constexpr int       COMP_ARRIVE            = 0x35;    // ReachedCity
constexpr int       TPL_ICONE              = 0x70;    // Icon : 2 variantes de UITextureTile (0x18)
constexpr int       TPL_IMAGE              = 0xA0;
constexpr int       TAILLE_TUILE_GRP       = 0x18;
constexpr int       TPL_SURVIVANTS         = 0x28;    // Dynarray<SurvivorsEntry> : ptr, +8 compte, +0xC capacite
static const uintptr_t OFF_GROUPE_ARRIVE      = Adr(0x18B2870);
constexpr int       CT_ELEMENTS            = 0x120;   // CompoundTemplate : elements, compte +0x128, pas 0xA0
constexpr int       CT_PAS_ELEMENT         = 0xA0;

struct GroupeSuivi {
    bool      actif;
    bool      arrive;            // arrive en ville : slot reutilisable, textes gardes
    uintptr_t entite, comp, clone;
    float     heures;            // <= 0 : duree native
    bool      dureeAppliquee;
    void    (*surArrivee)(int slot);
    char      nom[128];
    char      description[1024];
    char      dilemmeTitre[128];
    char      dilemmeTexte[1024];
    char      dilemmeChoix[128];
    char      icone[32];
    struct { char ressource[24]; int quantite; } cargaison[MAX_CARGAISON_AP];
    int       nCargaison;
    char      expediteur[64];         // joueur AP ("" : le nom du groupe), garde dans ap_groupes.txt
    bool      inventaireJournalise;   // log de la 1re injection dans l'inventaire
    bool      inventaireEchec;        // exception a l'ajout d'une case : plus d'essai
    uint64_t  guid[2];                // identifiant de l'entite (+0x18 / +0x20), sauvegarde par le jeu
    bool      sansMorts;
    bool      recolle;                // retrouve dans notre sauvegarde au chargement
};

// Lien du dilemme "[Beacon]S: Survivors group" (bulle "New Arrivals" de
// l'evenement "S: Survivors - universal"), releve le 2026-10-28.
const uint8_t GUID_DILEMME_SURVIVANTS[16] = {
    0xFF, 0xDC, 0x2F, 0x34, 0xB3, 0x3E, 0x65, 0x9F, 0x85, 0x4F, 0x08, 0x02, 0x53, 0x01, 0x0C, 0x5A };
GroupeSuivi g_groupes[MAX_GROUPES_AP];
unsigned    g_groupesGeneration = 0xFFFFFFFF;

typedef void(*GroupeArrive_t)(uintptr_t sys, uintptr_t entite, uintptr_t p3, uintptr_t p4);
ComposantAjoute_t g_origComposantAjoute = nullptr;
GroupeArrive_t    g_origGroupeArrive    = nullptr;
int       g_slotAPoser = -1;     // thread du jeu : groupe en cours de creation
uintptr_t g_compClone  = 0;      // composant qui a recu la copie
uintptr_t g_survivantsAPoser = 0;  // tableau de SurvivorsEntry pour la copie (0 : contenu du template)
int       g_nSurvivantsAPoser = 1; // nombre d'entrees de ce tableau
bool      g_chargementEnCours = false;  // entre l'entree et la sortie de FrostpunkGame::LoadGame
bool      g_chargementReinit  = false;  // slots remis a zero pour ce chargement
bool      g_lectureFaite      = false;  // la sauvegarde a ete lue pendant ce LoadGame
uintptr_t g_compsConstruits[64];        // SurvivorGroupComponent construits pendant LoadGame
int       g_nCompsConstruits  = 0;

} // namespace : CleMoteur est publique (groupes.h, utilisee par evenements.cpp)

// Cle 8 bits du moteur ("@AP/..."), en-tete de comptage clone de 'modele'
// (une cle valide du jeu), compteur fige : allouee par l'allocateur du
// moteur (regle n° 1) et jamais liberee.
uintptr_t CleMoteur(const char* texte, uintptr_t modele)
{
    uintptr_t self = 0;
    if (!MoteurAlloc || modele <= 0x10000 || !SafeRead((void*)(modele - 0x0C), &self, 8) ||
        self != modele - 0x14) return 0;
    size_t n = strlen(texte);
    uint8_t* b = (uint8_t*)MoteurAlloc(0x14 + n + 1);
    if (!b || !SafeRead((void*)(modele - 0x14), b, 0x14)) return 0;
    uintptr_t debut = (uintptr_t)b;
    memcpy(b + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(b + 0x10, &refs, 4);
    memcpy(b + 0x14, texte, n + 1);
    return (uintptr_t)(b + 0x14);
}

namespace {

// Champs texte du template (cles 8 bits "@...") que l'on remplace. Test F3 du
// 2026-10-28 : +0x50 et +0x60 sont NULS (SurvivorsFromExpeditionSite) ; les
// cles sont en +0x58 (nom) et +0x68 (description). +0xE0 / +0xE8 = noms
// d'agents (avec / sans origine) : le nom affiche "Survivors" vient
// peut-etre de la. On n'ecrit que dans un champ qui portait deja une cle
// valide (ne jamais transformer un champ nul en pointeur).
struct ChampTexte { int off; bool nom; const char* libelle; };
const ChampTexte CHAMPS_TEXTE[] = {
    { 0x50, true,  "nom +0x50" },          { 0x58, true,  "nom +0x58" },
    { 0x60, false, "description +0x60" },  { 0x68, false, "description +0x68" },
    { 0xE0, true,  "agents avec origine +0xE0" }, { 0xE8, true,  "agents sans origine +0xE8" },
};

// Copie du template de composant, textes remplaces par nos cles
// @AP/Groupe/<slot>/N et /D (traduites par hkTranslate via GroupeTexte).
uintptr_t ClonerTemplateGroupe(uintptr_t orig, int slot)
{
    if (!ClasseExacte(orig, "FrostpunkSurvivorGroupComponentTemplate")) return 0;
    uintptr_t clone = MoteurAlloc ? (uintptr_t)MoteurAlloc(TAILLE_TPL_GROUPE) : 0;
    if (!clone || !SafeRead((void*)orig, (void*)clone, TAILLE_TPL_GROUPE)) return 0;

    // Modele d'en-tete : la premiere cle valide du template.
    uintptr_t cleNom = 0, cleDesc = 0;
    char cleN[64], cleD[64];
    snprintf(cleN, sizeof(cleN), "@AP/Groupe/%d/N", slot);
    snprintf(cleD, sizeof(cleD), "@AP/Groupe/%d/D", slot);
    for (const ChampTexte& c : CHAMPS_TEXTE) {
        uintptr_t modele = 0;
        SafeRead((void*)(orig + c.off), &modele, 8);
        char texte[128];
        texte[0] = '-'; texte[1] = 0;
        if (modele > 0x10000) ReadAscii(modele, texte, sizeof(texte));
        printf("[GRP] template %s = 0x%llX \"%s\"\n", c.libelle, (unsigned long long)modele, texte);
        if (!cleNom && modele > 0x10000) {
            cleNom = CleMoteur(cleN, modele);
            cleDesc = CleMoteur(cleD, modele);
        }
    }
    if (!cleNom || !cleDesc) {
        printf("[GRP] cles de texte non creees (aucune cle valide dans le template) : textes d'origine conserves\n");
        return clone;
    }
    for (const ChampTexte& c : CHAMPS_TEXTE) {
        uintptr_t v = 0;
        SafeRead((void*)(orig + c.off), &v, 8);
        uintptr_t self = 0;
        if (v <= 0x10000 || !SafeRead((void*)(v - 0x0C), &self, 8) || self != v - 0x14) continue;
        SafeWrite((void*)(clone + c.off), c.nom ? &cleNom : &cleDesc, 8);
        printf("[GRP] copie : %s remplace\n", c.libelle);
    }
    return clone;
}

// Icone du groupe : le composant ajoute (FUN_1418b1cc0) copie
// tpl + 0x70 + variante x 0x18 (FUN_141a0f2a0) dans le composant de carte de
// l'entite (+0x40..+0x58). On met dans les deux variantes de la copie
// (+0x70, +0x88) la case de l'icone dans l'atlas des bulles (#2350, nos
// icones), la seule texture que l'on sache afficher pour l'instant. +0xA0
// (Image) est seulement journalisee.
void PoserIconeGroupe(uintptr_t clone, const char* icone)
{
    static const int VARIANTES[] = { TPL_ICONE, TPL_ICONE + TAILLE_TUILE_GRP };
    static const int RELEVES[] = { TPL_ICONE, TPL_ICONE + TAILLE_TUILE_GRP, TPL_IMAGE };
    for (int off : RELEVES) {
        uintptr_t tex = 0;
        int32_t v[4] = {};
        char nom[128] = "";
        SafeRead((void*)(clone + off), &tex, 8);
        SafeRead((void*)(clone + off + 8), v, sizeof(v));
        if (tex > 0x10000) ReadAscii(tex, nom, sizeof(nom));
        printf("[GRP] tuile d'origine +0x%X : texture 0x%llX \"%s\" case (%d,%d) sur (%d,%d)\n", off,
            (unsigned long long)tex, nom, v[0], v[1], v[2], v[3]);
    }
    uint8_t tuile[TAILLE_TUILE_GRP];
    if (!TuileAtlasBulles(icone, tuile)) {
        printf("[GRP] icone \"%s\" non posee (atlas des bulles indisponible)\n", icone);
        return;
    }
    for (int off : VARIANTES) SafeWrite((void*)(clone + off), tuile, TAILLE_TUILE_GRP);
    int32_t v[4];
    memcpy(v, tuile + 8, sizeof(v));
    printf("[GRP] icone \"%s\" posee sur les 2 variantes : case (%d,%d) sur (%d,%d)\n", icone, v[0], v[1], v[2], v[3]);
}

// --- Sauvegarde AP des groupes (2026-10-29) ----------------------------------
// La sauvegarde du jeu garde le groupe (survivants, dates du trajet, nom
// d'entite, identifiant 128 bits de l'entite en +0x18 / +0x20) mais reference
// le template d'origine : nom, description, icone, cargaison et bulle
// d'arrivee sont perdus. Releve [SAV] du 2026-10-29 : l'identifiant de
// l'entite est le meme avant la sauvegarde et apres le chargement.
// Fichier mods\ap_groupes.txt (le jeu ne connait pas nos donnees) : une
// ligne par groupe AP cree, champs separes par des tabulations :
//   G <id 32 hex> <sansMorts> <icone> <nom> <description> <titre de la
//   bulle> <texte> <choix> <cargaison "res=q;res=q">
// Lignes jamais effacees : charger une sauvegarde plus ancienne retrouve un
// groupe arrive depuis. Identifiants aleatoires : un seul fichier pour toutes
// les parties. La derniere ligne d'un identifiant l'emporte.
constexpr int ENT_ID = 0x18;   // 2 qwords

bool CheminFichierGroupes(char* out, size_t taille)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_groupes.txt", base);
    return true;
}

// Texte sans tabulation ni fin de ligne (separateurs du fichier).
void EcrireChamp(FILE* f, const char* t)
{
    fputc('\t', f);
    for (; t && *t; ++t) fputc((*t == '\t' || *t == '\r' || *t == '\n') ? ' ' : *t, f);
}

void SauverGroupe(const GroupeSuivi& s)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichierGroupes(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "a") != 0 || !f) {
        printf("[SAV] ecriture de mods\\ap_groupes.txt impossible\n");
        return;
    }
    char cargaison[512] = "";
    for (int i = 0; i < s.nCargaison; ++i) {
        char un[64];
        _snprintf_s(un, sizeof(un), _TRUNCATE, "%s%s=%d", i ? ";" : "", s.cargaison[i].ressource,
            s.cargaison[i].quantite);
        strcat_s(cargaison, un);
    }
    fprintf(f, "G %016llX%016llX\t%d", (unsigned long long)s.guid[0], (unsigned long long)s.guid[1], s.sansMorts ? 1 : 0);
    EcrireChamp(f, s.icone);
    EcrireChamp(f, s.nom);
    EcrireChamp(f, s.description);
    EcrireChamp(f, s.dilemmeTitre);
    EcrireChamp(f, s.dilemmeTexte);
    EcrireChamp(f, s.dilemmeChoix);
    EcrireChamp(f, cargaison);
    EcrireChamp(f, s.expediteur);   // champ 8 (absent des lignes anciennes)
    fputc('\n', f);
    fclose(f);
    printf("[SAV] groupe %016llX%016llX \"%s\" enregistre (%d ressource(s))\n", (unsigned long long)s.guid[0],
        (unsigned long long)s.guid[1], s.nom, s.nCargaison);
}

// Champ n (0 = le premier apres l'identifiant) d'une ligne, dans out.
void ChampLigne(const char* ligne, int n, char* out, size_t taille)
{
    const char* p = strchr(ligne, '\t');
    for (int i = 0; p && i < n; ++i) p = strchr(p + 1, '\t');
    out[0] = 0;
    if (!p) return;
    ++p;
    size_t k = 0;
    while (p[k] && p[k] != '\t' && p[k] != '\n' && p[k] != '\r' && k + 1 < taille) { out[k] = p[k]; ++k; }
    out[k] = 0;
}

// Cherche l'identifiant dans le fichier ; remplit les textes, l'icone et la
// cargaison du slot. false si absent.
bool LireGroupe(const uint64_t* guid, GroupeSuivi& s)
{
    char chemin[MAX_PATH], cle[40];
    FILE* f = nullptr;
    if (!CheminFichierGroupes(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) return false;
    _snprintf_s(cle, sizeof(cle), _TRUNCATE, "G %016llX%016llX\t", (unsigned long long)guid[0],
        (unsigned long long)guid[1]);
    static char ligne[8192], trouvee[8192];
    bool ok = false;
    while (fgets(ligne, sizeof(ligne), f))
        if (!strncmp(ligne, cle, strlen(cle))) { strcpy_s(trouvee, ligne); ok = true; }
    fclose(f);
    if (!ok) return false;

    char v[16], cargaison[512];
    ChampLigne(trouvee, 0, v, sizeof(v));
    s.sansMorts = v[0] == '1';
    ChampLigne(trouvee, 1, s.icone, sizeof(s.icone));
    ChampLigne(trouvee, 2, s.nom, sizeof(s.nom));
    ChampLigne(trouvee, 3, s.description, sizeof(s.description));
    ChampLigne(trouvee, 4, s.dilemmeTitre, sizeof(s.dilemmeTitre));
    ChampLigne(trouvee, 5, s.dilemmeTexte, sizeof(s.dilemmeTexte));
    ChampLigne(trouvee, 6, s.dilemmeChoix, sizeof(s.dilemmeChoix));
    ChampLigne(trouvee, 7, cargaison, sizeof(cargaison));
    ChampLigne(trouvee, 8, s.expediteur, sizeof(s.expediteur));
    s.nCargaison = 0;
    for (char* tok = cargaison; tok && *tok && s.nCargaison < MAX_CARGAISON_AP; ) {
        char* fin = strchr(tok, ';');
        if (fin) *fin = 0;
        char* egal = strchr(tok, '=');
        if (egal) {
            *egal = 0;
            strncpy_s(s.cargaison[s.nCargaison].ressource, tok, _TRUNCATE);
            s.cargaison[s.nCargaison].quantite = atoi(egal + 1);
            if (s.cargaison[s.nCargaison].quantite > 0) s.nCargaison++;
        }
        tok = fin ? fin + 1 : nullptr;
    }
    if (!s.icone[0]) strcpy_s(s.icone, "icon_vanilla_groupe");
    return true;
}

// Copie du template posee sur le composant (creation et recollage), AVANT que
// FUN_1418b1cc0 ne recopie nom et icone dans le composant de carte de
// l'entite (entite +0x2A0 + type x 8 : +0x30 nom, +0x40..+0x50 icone).
uintptr_t PoserCopieTemplate(uintptr_t comp, int slot, uintptr_t survivants, int nSurvivants)
{
    uintptr_t orig = 0;
    SafeRead((void*)(comp + COMP_TEMPLATE), &orig, 8);
    uintptr_t clone = ClonerTemplateGroupe(orig, slot);
    if (clone) {
        if (survivants && nSurvivants > 0) {
            int32_t compte[2] = { nSurvivants, nSurvivants };   // compte, capacite
            SafeWrite((void*)(clone + TPL_SURVIVANTS), &survivants, 8);
            SafeWrite((void*)(clone + TPL_SURVIVANTS + 8), compte, 8);
            printf("[GRP] copie : survivants remplaces par nos %d entree(s) 0x%llX\n", nSurvivants,
                (unsigned long long)survivants);
        }
        // Pas de dilemme a l'arrivee : lien "City reached dilemma" (GUID
        // de 16 octets, compare au GUID nul par FUN_140e1a930) efface dans
        // la copie, et dilemme deja resolu (FUN_141a0f440, comp +0x80)
        // efface dans le composant.
        uint8_t nul[16] = {};
        uintptr_t dilemme = 0;
        SafeWrite((void*)(clone + TPL_DILEMME_ARRIVEE), nul, sizeof(nul));
        SafeRead((void*)(comp + COMP_DILEMME_ARRIVEE), &dilemme, 8);
        SafeWrite((void*)(comp + COMP_DILEMME_ARRIVEE), nul, 8);
        printf("[GRP] dilemme d'arrivee supprime (composant : 0x%llX -> 0)\n", (unsigned long long)dilemme);

        PoserIconeGroupe(clone, g_groupes[slot].icone);
        SafeWrite((void*)(comp + COMP_TEMPLATE), &clone, 8);
        g_groupes[slot].clone = clone;
    }
    printf("[GRP] composant 0x%llX : template 0x%llX -> copie 0x%llX (slot %d)\n",
        (unsigned long long)comp, (unsigned long long)orig, (unsigned long long)clone, slot);
    return clone;
}

// Composant de carte de l'entite (celui de la carte et de la liste a droite) :
// FUN_1418b1cc0 y recopie le nom et l'icone du template quand le composant
// de groupe est ajoute. Test du 2026-10-29 : au chargement, ce chemin n'est
// PAS emprunte (aucun appel du hook pendant LoadGame) ; le composant de carte
// garde le nom et l'icone du template du jeu, deserialises. On refait la
// meme copie, avec les fonctions du jeu (comptage de references) :
//   carte = *(entite + 0x2A0 + FUN_1412ec200() x 8) ;
//   nom   = *FUN_141a0f1c0(comp) (+1 ref FUN_140e1ce20), ancien relache
//           (FUN_140e1d180), pose en carte +0x30 ;
//   icone = FUN_141a0f2a0(comp) : texture copiee par FUN_140e4f9a0 en carte
//           +0x40, case et nombre de cases en +0x48 / +0x50.
typedef uintptr_t* (*NomDuGroupe_t)(uintptr_t comp);
typedef uintptr_t  (*IconeDuGroupe_t)(uintptr_t comp);
typedef int        (*TypeComposantCarte_t)();
typedef void       (*ChaineRef_t)(uintptr_t* chaine);
typedef void       (*CopierNomTexture_t)(uintptr_t dest, uintptr_t src);
static const uintptr_t OFF_NOM_DU_GROUPE     = Adr(0x1A0F1C0);
static const uintptr_t OFF_ICONE_DU_GROUPE   = Adr(0x1A0F2A0);
static const uintptr_t OFF_TYPE_CARTE        = Adr(0x12EC200);
static const uintptr_t OFF_CHAINE_PLUS       = Adr(0x0E1CE20);
static const uintptr_t OFF_CHAINE_MOINS      = Adr(0x0E1D180);
static const uintptr_t OFF_COPIER_TEXTURE    = Adr(0x0E4F9A0);
constexpr int       ENT_COMPOSANTS        = 0x2A0;
constexpr int       CARTE_NOM             = 0x30;
constexpr int       CARTE_ICONE           = 0x40;

bool MettreAJourCarte(uintptr_t entite, uintptr_t comp)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t carte = 0;
    __try {
        int type = ((TypeComposantCarte_t)(base + OFF_TYPE_CARTE))();
        if (type < 0 || type > 512 || !SafeRead((void*)(entite + ENT_COMPOSANTS + (uintptr_t)type * 8), &carte, 8) ||
            carte <= 0x10000) {
            printf("[SAV]   composant de carte absent (type %d)\n", type);
            return false;
        }
        uintptr_t* champNom = ((NomDuGroupe_t)(base + OFF_NOM_DU_GROUPE))(comp);
        uintptr_t nom = champNom ? *champNom : 0;
        ((ChaineRef_t)(base + OFF_CHAINE_PLUS))(&nom);
        ((ChaineRef_t)(base + OFF_CHAINE_MOINS))((uintptr_t*)(carte + CARTE_NOM));
        *(uintptr_t*)(carte + CARTE_NOM) = nom;

        uintptr_t tuile = ((IconeDuGroupe_t)(base + OFF_ICONE_DU_GROUPE))(comp);
        ((CopierNomTexture_t)(base + OFF_COPIER_TEXTURE))(carte + CARTE_ICONE, tuile);
        *(uint64_t*)(carte + CARTE_ICONE + 8) = *(uint64_t*)(tuile + 8);
        *(uint64_t*)(carte + CARTE_ICONE + 0x10) = *(uint64_t*)(tuile + 0x10);

        char texte[96] = "?";
        if (nom > 0x10000) ReadAscii(nom, texte, sizeof(texte));
        int32_t v[4] = {};
        memcpy(v, (void*)(tuile + 8), sizeof(v));
        printf("[SAV]   composant de carte 0x%llX : nom \"%s\", icone case (%d,%d) sur (%d,%d)\n",
            (unsigned long long)carte, texte, v[0], v[1], v[2], v[3]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[SAV]   EXCEPTION 0x%08lX a la mise a jour du composant de carte 0x%llX\n",
            GetExceptionCode(), (unsigned long long)carte);
        return false;
    }
    return true;
}

// Un groupe de la sauvegarde est-il un des notres ? Si oui, slot + copie du
// template. Thread du jeu. Renvoie le slot, ou -1.
int RecollerGroupe(uintptr_t entite, uintptr_t comp)
{
    if (!g_chargementReinit) {
        // 1er groupe de ce chargement : les slots de l'ancienne partie sont perimes.
        memset(g_groupes, 0, sizeof(g_groupes));
        g_chargementReinit = true;
    }
    uint64_t guid[2] = {};
    char nomEnt[96] = "?";
    if (!SafeRead((void*)(entite + ENT_ID), guid, sizeof(guid))) return -1;
    LireNomPointe(entite, ENT_NOM, nomEnt, sizeof(nomEnt));
    int slot = -1;
    for (int i = 0; i < MAX_GROUPES_AP && slot < 0; ++i) if (!g_groupes[i].actif) slot = i;
    if (slot < 0) { printf("[SAV] plus de slot libre pour \"%s\"\n", nomEnt); return -1; }

    GroupeSuivi& s = g_groupes[slot];
    memset(&s, 0, sizeof(s));
    if (!LireGroupe(guid, s)) {
        printf("[SAV] groupe \"%s\" (%016llX%016llX) : pas dans notre sauvegarde (groupe du jeu)\n", nomEnt,
            (unsigned long long)guid[0], (unsigned long long)guid[1]);
        memset(&s, 0, sizeof(s));
        return -1;
    }
    s.guid[0] = guid[0];
    s.guid[1] = guid[1];
    s.entite = entite;
    s.comp = comp;
    s.dureeAppliquee = true;     // les dates du trajet sont dans la sauvegarde du jeu
    s.recolle = true;
    s.actif = true;
    PoserCopieTemplate(comp, slot, 0, 0);
    printf("[SAV] groupe \"%s\" recolle dans le slot %d : \"%s\", icone %s, %d ressource(s)\n", nomEnt, slot,
        s.nom, s.icone, s.nCargaison);
    return slot;
}

// Hook de FUN_1418b1cc0 (systeme des groupes : composant ajoute), appele a la
// creation d'un groupe ET au chargement d'une sauvegarde. Le jeu y copie les
// survivants du template (FUN_1418afb80 : SurvivorsEntry de tpl +0x28 vers
// comp +0x48), le nom et l'icone vers le composant de carte, puis lance la
// marche (FUN_1418b1620) si le systeme est initialise. On pose NOTRE copie
// du template avant l'appel original : a la creation (g_slotAPoser), et
// pendant LoadGame pour un groupe present dans notre sauvegarde.
void hkComposantAjoute(uintptr_t sys, uintptr_t entite, uintptr_t comp, uintptr_t p4)
{
    if (g_slotAPoser >= 0 && ClasseExacte(comp, "FrostpunkSurvivorGroupComponent")) {
        int slot = g_slotAPoser;
        g_slotAPoser = -1;
        PoserCopieTemplate(comp, slot, g_survivantsAPoser, g_nSurvivantsAPoser);
        g_compClone = comp;
    }
    else if (g_chargementEnCours && entite > 0x10000 && ClasseExacte(comp, "FrostpunkSurvivorGroupComponent")) {
        RecollerGroupe(entite, comp);
    }
    g_origComposantAjoute(sys, entite, comp, p4);
}

// Groupes de The Refugees (2026-11-10, demandes de l'auteur) : [OurPeople]
// (10 groupes, premiers refugies compris) et Lords (3 groupes). Leurs gens
// sont des items AP ; chaque groupe porte un check (location "refugies").
// Numero FIXE par gabarit de composant (comp +0x20) : son GUID (le blob de
// templates.dat commence par lui) est cherche dans les 0x200 premiers octets
// du gabarit en memoire. Ordre = celui de REFUGIES["REFUGEES"] (generer.py) :
// la carte (inventaire) et l'arrivee donnent le meme check, et un
// rechargement ne decale rien.
// Repli si aucun GUID n'est trouve : famille par la cle
// "@Expeditions/SurvivorPanelHeader/[RefugeesScenario]OurPeople" / "...Lords"
// du gabarit (les convois AP et les sites ne l'ont pas) ; Lords d'apres les
// ouvriers du gabarit (50 / 60 / 0), [OurPeople] = premier numero libre.
// 0 : pas un groupe de Refugees.
static const uint8_t CLASSE_OUVRIERS[4] = { 0x35, 0xF2, 0x53, 0xD1 };

// Documentation seulement (2026-11-11) : GUID et composition des gabarits de
// composant (templates.dat, releve [REFGRP]) ; le GUID n'est pas retrouve en
// memoire, la numerotation passe par NumeroAttribue (plus bas).
struct GabaritRefugees { uint64_t q1, q2; int numero; };
[[maybe_unused]] static const GabaritRefugees GABARITS_REFUGEES[] = {
    { 0x9C4F77273A737E6DULL, 0x3F9141341EE742FAULL,  1 },   // premiers refugies : 10 ouvriers, 20 enfants
    { 0x992252BEFFBEB0E0ULL, 0x2B9E07D723974DC3ULL,  2 },   // RefugeeScenarioSurvivors01 : 15 ouvriers
    { 0xA36612B67007C19CULL, 0xEF97BB8B3D3946B8ULL,  3 },   // RefugeeScenarioSurvivors02 : 10 ingenieurs
    { 0xA6320EFCCA44A59DULL, 0x3C11B61E7E3F4899ULL,  4 },   // 10 ingenieurs, 10 enfants
    { 0xAC26ACB5495AEC10ULL, 0xEE16686456234A5BULL,  5 },   // 15 ouvriers
    { 0xAEBD8A3F2621A437ULL, 0xBA5952C2F1774092ULL,  6 },   // 15 ouvriers
    { 0x9DC30EEC1180DA53ULL, 0x902EA8154B7145D7ULL,  7 },   // 20 ouvriers, 20 enfants
    { 0xB0E2B852CE5B3375ULL, 0xE996EE95EDB94FC4ULL,  8 },   // 10 ingenieurs, 10 ouvriers
    { 0xB6FE7714002708A3ULL, 0xC0C93C31D99C4387ULL,  9 },   // 15 ouvriers
    { 0xBCAB7CCE003B8114ULL, 0x2966C9B775ED40BBULL, 10 },   // 15 ouvriers
    { 0x8F59EBF7A9B8E764ULL, 0xE0EE7DAA0779437BULL, 11 },   // Lords01 : 50 ouvriers, 35 enfants, 15 ingenieurs
    { 0xAEC2CEB0C53DF017ULL, 0x0716757162784202ULL, 12 },   // Lords02 : 60 ouvriers, 35 enfants, 5 ingenieurs
    { 0xAD8D1B7D68664E72ULL, 0x1143D2A8BB3A481BULL, 13 },   // Lords03 : 30 ingenieurs, 70 enfants
};

// Attribution persistante des numeros (2026-11-11). Test de l'auteur : le
// GUID n'est jamais trouve dans le gabarit en memoire ("sans GUID connu"),
// et le repli "premier numero pas encore coche" donnait le MEME numero a deux
// groupes affiches avant leur arrivee (RefugeeScenarioSurvivors01 et 02 :
// tous deux 8). Les items ne sont lies a aucun groupe : il suffit que chaque
// groupe ait un numero distinct et stable. Cle = identifiant 128 bits de
// l'entite (entite +0x18 / +0x20, sauvegarde par le jeu) ; plus petit numero
// libre de la famille dans cette partie ; fichier par partie
// mods\ap_refugies_<GameId>.txt ("<id hex> <numero>"), relu au changement de
// partie : un rechargement retrouve le meme numero.
struct AttributionRefugees { uint64_t id[2]; int numero; };
static AttributionRefugees g_attrib[32];
static int g_nAttrib = 0;
static uint64_t g_attribPartie[2] = { ~0ULL, ~0ULL };

static bool CheminAttributions(char* out, size_t taille)
{
    if (!g_run.valid || (!g_run.gameId[0] && !g_run.gameId[1])) return false;
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_refugies_%016llX%016llX.txt", base,
        (unsigned long long)g_run.gameId[1], (unsigned long long)g_run.gameId[0]);
    return true;
}

static void AttributionsCharger()
{
    if (g_attribPartie[0] == g_run.gameId[0] && g_attribPartie[1] == g_run.gameId[1]) return;
    g_attribPartie[0] = g_run.gameId[0];
    g_attribPartie[1] = g_run.gameId[1];
    g_nAttrib = 0;
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminAttributions(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) return;
    unsigned long long a = 0, b = 0;
    int k = 0;
    while (g_nAttrib < 32 && fscanf_s(f, "%llx %llx %d", &a, &b, &k) == 3)
        g_attrib[g_nAttrib++] = { { a, b }, k };
    fclose(f);
    printf("[GRP] attributions des groupes de Refugees relues : %d\n", g_nAttrib);
}

static void AttributionsSauver()
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminAttributions(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "w") != 0 || !f) return;
    for (int i = 0; i < g_nAttrib; ++i)
        fprintf(f, "%016llX %016llX %d\n", (unsigned long long)g_attrib[i].id[0],
            (unsigned long long)g_attrib[i].id[1], g_attrib[i].numero);
    fclose(f);
}

// Numero de l'entite dans [premier, dernier] ; 'voulu' (> 0) s'il est libre
// (Lords reconnus par leur composition). 0 si la famille est pleine.
static int NumeroAttribue(uintptr_t entite, int premier, int dernier, int voulu)
{
    uint64_t id[2] = {};
    if (!entite || !SafeRead((void*)(entite + ENT_ID), id, 16) || (!id[0] && !id[1])) return voulu;
    AttributionsCharger();
    for (int i = 0; i < g_nAttrib; ++i)
        if (g_attrib[i].id[0] == id[0] && g_attrib[i].id[1] == id[1]) return g_attrib[i].numero;
    auto pris = [&](int k) { for (int i = 0; i < g_nAttrib; ++i) if (g_attrib[i].numero == k) return true; return false; };
    int k = (voulu >= premier && voulu <= dernier && !pris(voulu)) ? voulu : 0;
    // D'abord un numero dont le check n'est pas encore coche sur le serveur
    // (partie commencee avant cette table : 8, 9, 10 deja envoyes), sinon
    // n'importe quel numero libre (nouvelle partie d'un slot deja joue).
    for (int c = premier; c <= dernier && !k; ++c) if (!pris(c) && !ApRefugiesDejaCoche(c)) k = c;
    for (int c = premier; c <= dernier && !k; ++c) if (!pris(c)) k = c;
    if (!k || g_nAttrib >= 32) {
        printf("[GRP]   groupe de Refugees : plus aucun numero libre (%d..%d)\n", premier, dernier);
        return 0;
    }
    g_attrib[g_nAttrib++] = { { id[0], id[1] }, k };
    AttributionsSauver();
    printf("[GRP]   groupe de Refugees %016llX%016llX : numero %d attribue\n",
        (unsigned long long)id[1], (unsigned long long)id[0], k);
    return k;
}

static int NumeroGroupeRefugeesCalcule(uintptr_t comp)
{
    uintptr_t tpl = 0;
    if (!SafeRead((void*)(comp + COMP_TEMPLATE), &tpl, 8) || tpl <= 0x10000) return 0;

    // Famille par la cle du panneau du gabarit (le GUID, cherche jusqu'au
    // 2026-11-11, n'est pas dans le gabarit en memoire : retire).
    int famille = 0;                              // 1 [OurPeople], 2 Lords
    for (int off = 0x08; off < 0x200 && !famille; off += 8) {
        uintptr_t p = 0;
        char texte[160];
        if (!SafeRead((void*)(tpl + off), &p, 8) || p <= 0x10000 || !ReadAscii(p, texte, sizeof(texte))) continue;
        if (strstr(texte, "[RefugeesScenario]OurPeople")) famille = 1;
        else if (strstr(texte, "[RefugeesScenario]Lords")) famille = 2;
    }
    if (!famille) return 0;
    uintptr_t entite = 0;
    SafeRead((void*)(comp + 0x18), &entite, 8);
    int voulu = 0;
    if (famille == 2) {
        // Ouvriers du gabarit (le contenu du composant peut deja etre vide).
        uintptr_t tab = 0;
        int32_t n = 0, ouvriers = 0;
        if (SafeRead((void*)(tpl + TPL_SURVIVANTS), &tab, 8) && SafeRead((void*)(tpl + TPL_SURVIVANTS + 8), &n, 4) &&
            tab > 0x10000 && n > 0 && n <= 16) {
            for (int i = 0; i < n; ++i) {
                uint8_t cls[4] = {};
                int32_t nb = 0;
                SafeRead((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS), &nb, 4);
                SafeRead((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS + 4), cls, 4);
                if (memcmp(cls, CLASSE_OUVRIERS, 4) == 0) ouvriers += nb;
            }
        }
        voulu = ouvriers == 50 ? 11 : ouvriers == 60 ? 12 : ouvriers == 0 ? 13 : 0;
        printf("[GRP]   Lords : %d ouvrier(s) dans le gabarit -> groupe %d voulu\n", ouvriers, voulu);
    }
    return NumeroAttribue(entite, famille == 1 ? 1 : 11, famille == 1 ? 10 : 13, voulu);
}

int NumeroGroupeRefugees(uintptr_t comp)
{
    const char* scn = ScenarioVictoryId();
    if (!scn || _stricmp(scn, "REFUGEES") != 0 || !comp) return 0;
    uintptr_t entite = 0;
    SafeRead((void*)(comp + 0x18), &entite, 8);
    // Cache par composant (appele a chaque remplissage de la liste de droite).
    struct Connu { uintptr_t comp, entite; int numero; };
    static Connu connus[32];
    static int prochain = 0;
    for (const Connu& c : connus) if (c.comp == comp && c.entite == entite) return c.numero;
    int numero = NumeroGroupeRefugeesCalcule(comp);
    connus[prochain] = { comp, entite, numero };
    prochain = (prochain + 1) % 32;
    return numero;
}

// Hook de FUN_1418b2870(systeme, entite) = groupe arrive en ville (decompile
// du 2026-10-28) : cree les citoyens du contenu (comp +0x48, via
// FUN_1418b7c00) a l'emplacement du drapeau d'arrivee, pose ReachedCity
// (+0x35), vide le contenu et emet l'evenement du systeme (+0x2128). Avec un
// contenu vide il ne se passe rien de visible. On laisse le jeu faire, puis
// on livre ce que le groupe portait.
void hkGroupeArrive(uintptr_t sys, uintptr_t entite, uintptr_t p3, uintptr_t p4)
{
    int slot = -1;
    for (int i = 0; i < MAX_GROUPES_AP && slot < 0; ++i)
        if (g_groupes[i].actif && !g_groupes[i].arrive && g_groupes[i].entite == entite) slot = i;
    // Releve (2026-11-05, refugies de l'apres-005) : composition d'un groupe
    // DU JEU qui arrive, avant que l'original ne vide son contenu (comp +0x48 :
    // SurvivorsEntry de 0x24, +0 Count, +4 classe sociale ; compte +0x50).
    int vagueRefugies = 0;                   // 1..3 : groupe StoryModeRefugeesK
    if (slot < 0) {
        char ctx[256] = "";
        DecrireEntiteContexte(entite, ctx, sizeof(ctx));
        // Nom de l'entite (releve des groupes de Refugees, 2026-11-10).
        uintptr_t pNomEnt = 0;
        char nomEnt[96] = "?";
        if (SafeRead((void*)(entite + ENT_NOM), &pNomEnt, 8) && pNomEnt > 0x10000) ReadAscii(pNomEnt, nomEnt, sizeof(nomEnt));
        printf("[GRP] groupe du jeu arrive en ville : \"%s\" %s\n", nomEnt, ctx);
        const char* sm = strstr(ctx, "StoryModeRefugees");
        if (sm && sm[17] >= '1' && sm[17] <= '3') vagueRefugies = sm[17] - '0';
        uintptr_t comp = 0;
        static uintptr_t comps[64];
        int n = ComposantsDuSysteme(0x6D /* SurvivorGroup (CLAUDE.md, section 8) */,
                                    "FrostpunkSurvivorGroupComponent", comps, 64);
        for (int i = 0; i < n && !comp; ++i) {
            uintptr_t e = 0;                                   // composant +0x18 = entite proprietaire
            if (SafeRead((void*)(comps[i] + 0x18), &e, 8) && e == entite) comp = comps[i];
        }
        uintptr_t tab = 0;
        int32_t nb = 0;
        if (comp && SafeRead((void*)(comp + COMP_CONTENU), &tab, 8) && SafeRead((void*)(comp + COMP_CONTENU + 8), &nb, 4) &&
            tab > 0x10000 && nb > 0 && nb <= 16) {
            for (int i = 0; i < nb; ++i) {
                int32_t compte = 0;
                uint8_t cls[16] = {};
                SafeRead((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS), &compte, 4);
                SafeRead((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS + 4), cls, 16);
                printf("[GRP]   entree %d : %d survivant(s), classe sociale", i, compte);
                for (int b = 0; b < 16; ++b) printf(" %02X", cls[b]);
                printf("\n");
            }
        }
        else printf("[GRP]   contenu illisible (composant 0x%llX, %d entree(s))\n", (unsigned long long)comp, nb);
        // Refugies de l'apres-005 (2026-11-07, demande de l'auteur) : leurs gens
        // sont des items AP ("Refugees (Group k)") ; le groupe du jeu arrive
        // VIDE (compte du contenu a 0 : l'original ne cree aucun citoyen).
        if (vagueRefugies && comp) {
            int32_t zero = 0;
            SafeWrite((void*)(comp + COMP_CONTENU + 8), &zero, 4);
            printf("[GRP]   refugies du groupe %d retires (items AP)\n", vagueRefugies);
            EvenementsBullePorteCreee(true);
        }
        // The Refugees (2026-11-10) : [OurPeople] et Lords. Check envoye a
        // l'arrivee, groupe vide (ses gens sont des items AP).
        int numero = vagueRefugies ? 0 : NumeroGroupeRefugees(comp);
        if (numero) {
            int32_t zero = 0;
            SafeWrite((void*)(comp + COMP_CONTENU + 8), &zero, 4);
            // Groupe vide + dilemme en comp +0x80 = crash du 2026-11-10
            // (survivants secourus nuls, FUN_141697870) : dilemme retire,
            // l'effet prend alors le dilemme de son propre lien, sans
            // survivants. 2026-11-11 (auteur : les sites reveles par ces
            // bulles n'apparaissaient plus, ex. White Pass par "Survivors07 -
            // LostRefugees") : le dilemme du groupe est remis a la place de la
            // bulle generique a sa creation (EvenementsDilemmeGroupeVide).
            uintptr_t dil = 0, nul = 0;
            if (SafeRead((void*)(comp + COMP_DILEMME_ARRIVEE), &dil, 8) && dil) {
                SafeWrite((void*)(comp + COMP_DILEMME_ARRIVEE), &nul, 8);
                EvenementsDilemmeGroupeVide(entite, dil);
                printf("[GRP]   dilemme d'arrivee du groupe (0x%llX) retire de comp +0x80, cree sans survivants\n",
                    (unsigned long long)dil);
            }
            printf("[GRP]   groupe de Refugees %d : gens retires (items AP), check envoye\n", numero);
            SendRefugeesCheck(numero);
        }
    }
    if (slot >= 0) EvenementsArriveeConvoiAP(true);
    if (slot >= 0) {
        JournalImmediat("[GRP] groupe Archipelago slot %d : arrivee en ville ...\n", slot);
        // Bonne sante (2026-11-11, demande de l'auteur) : les statistiques
        // initiales de chaque entree du contenu (+0x14) deviennent celles de
        // "Survivors" avant que l'original ne cree les citoyens ; vaut aussi
        // pour les groupes partis avant ce correctif ou relus d'une sauvegarde.
        {
            uint8_t sain[16];
            uintptr_t tab = 0;
            int32_t n = 0;
            GroupeSuivi& g = g_groupes[slot];
            if (ClasseExacte(g.comp, "FrostpunkSurvivorGroupComponent") && StatsBonneSante(sain) &&
                SafeRead((void*)(g.comp + COMP_CONTENU), &tab, 8) && SafeRead((void*)(g.comp + COMP_CONTENU + 8), &n, 4) &&
                tab > 0x10000 && n > 0 && n <= 64) {
                for (int i = 0; i < n; ++i)
                    SafeWrite((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS + 0x14), sain, 16);
                printf("[GRP] slot %d : %d entree(s) de survivants en bonne sante\n", slot, n);
            }
        }
        // La bulle de l'arrivee est un evenement du jeu dont le contexte est
        // le groupe (releve detaille). Son effet CreateSurvivoursDilemmaEffect
        // ouvre le dilemme pose en comp +0x80 s'il y en a un : on y met une
        // copie de "[Beacon]S: Survivors group" avec nos cles.
        EvenementsSuivreEntite(entite);
        GroupeSuivi& s = g_groupes[slot];
        uintptr_t orig = DilemmeParGuid(GUID_DILEMME_SURVIVANTS);
        char cleT[48], cleX[48], cleC[48];
        snprintf(cleT, sizeof(cleT), "@AP/Groupe/%d/T", slot);
        snprintf(cleX, sizeof(cleX), "@AP/Groupe/%d/X", slot);
        snprintf(cleC, sizeof(cleC), "@AP/Groupe/%d/C", slot);
        uintptr_t dilemme = orig ? DilemmeCloner(orig, cleT, cleX, cleC) : 0;
        if (dilemme) {
            // Test du 2026-10-28 : comp +0x80 seul ne suffit pas (le contexte
            // de l'effet est sans doute un survivant secouru, pas le groupe).
            // On remplace donc aussi a la creation du dilemme.
            EvenementsRemplacerDilemme(entite, orig, dilemme);
            // Crash du 2026-11-10 (Refugees, Steam +0x15334EF = Epic
            // 0x14169854F) : avec un dilemme en comp +0x80,
            // CreateSurvivoursDilemmaEffect (FUN_141697870) lit les survivants
            // secourus du groupe (*(*FUN_1418b0e10(...))) sans test ; un convoi
            // sans survivants n'en a aucun -> pointeur nul. comp +0x80 n'est
            // donc pose que si le groupe porte des survivants ; sinon la bulle
            // passe par le lien du dilemme, remplace a la creation.
            int32_t entrees = 0, survivants = 0;
            uintptr_t tab = 0;
            if (ClasseExacte(s.comp, "FrostpunkSurvivorGroupComponent") &&
                SafeRead((void*)(s.comp + COMP_CONTENU), &tab, 8) &&
                SafeRead((void*)(s.comp + COMP_CONTENU + 8), &entrees, 4) && tab > 0x10000 &&
                entrees > 0 && entrees <= 64) {
                for (int i = 0; i < entrees; ++i) {
                    int32_t nb = 0;
                    SafeRead((void*)(tab + (uintptr_t)i * ENTREE_SURVIVANTS), &nb, 4);
                    if (nb > 0) survivants += nb;
                }
            }
            if (survivants > 0) SafeWrite((void*)(s.comp + COMP_DILEMME_ARRIVEE), &dilemme, 8);
            printf("[GRP] dilemme d'arrivee du slot %d : copie 0x%llX (remplacement a la creation%s, %d survivant(s))\n",
                slot, (unsigned long long)dilemme, survivants > 0 ? " + comp +0x80" : "", survivants);
        }
        else printf("[GRP] dilemme d'arrivee non remplace (original 0x%llX, copie 0x%llX)\n",
            (unsigned long long)orig, (unsigned long long)dilemme);
    }
    g_origGroupeArrive(sys, entite, p3, p4);
    if (slot >= 0) EvenementsArriveeConvoiAP(false);
    // Groupe de refugies vide : si le jeu n'a pas ouvert la bulle de la porte
    // (elle porte le check et fait avancer 006), filet : check envoye ici et,
    // au 3e groupe, 006 terminee si elle est encore active.
    if (vagueRefugies) {
        if (!EvenementsBullePorteCreee(true)) {
            printf("[GRP]   pas de bulle de la porte pour le groupe %d : check envoye directement\n", vagueRefugies);
            SendRefugeesCheck(vagueRefugies);
            if (vagueRefugies == 3) QuetesDemanderFinQuete("[Main Narrative Line] 006 REFUGEES");
        }
    }
    if (slot < 0) return;

    GroupeSuivi& s = g_groupes[slot];
    s.arrive = true;
    uint8_t atteint = 0;
    SafeRead((void*)(s.comp + 0x35), &atteint, 1);
    int32_t secourus = 0;
    SafeRead((void*)(s.comp + 0x90), &secourus, 4);
    printf("[GRP] slot %d \"%s\" arrive en ville : ReachedCity %d, %d citoyen(s) secouru(s)\n",
        slot, s.nom, atteint, secourus);
    // Livraison de la cargaison (celle que montre l'inventaire), comme un scout
    // qui decharge en ville : un seul message pour toutes les ressources
    // (2026-11-10, demande de l'auteur).
    if (s.nCargaison > 0) {
        const char* noms[MAX_CARGAISON_AP];
        int quantites[MAX_CARGAISON_AP];
        int n = 0;
        for (int i = 0; i < s.nCargaison && n < MAX_CARGAISON_AP; ++i, ++n) {
            noms[n] = s.cargaison[i].ressource;
            quantites[n] = s.cargaison[i].quantite;
        }
        ReceiveResources(noms, quantites, n, s.expediteur[0] ? s.expediteur : s.nom);
    }
    if (s.surArrivee) s.surArrivee(slot);
}

// Template de composant de groupe d'un CompoundTemplate : on cherche, dans
// chaque element (+0x120, pas 0xA0), un pointeur vers un objet de la classe.
// Test du 2026-10-28 : +0x120 ne donnait rien. FUN_1411d0940 recoit la
// RESSOURCE (= CompoundTemplate + 0x18) : les elements sont donc
// probablement en +0x138 du CompoundTemplate. On essaie les deux, puis, en
// repli, les templates captures a leur construction dont la cle de nom
// (+0x58, "@Generated/Templates/<chemin>/...") contient le chemin du donneur.
extern ListeCaptures g_capTpl;

uintptr_t TemplateGroupeDe(uintptr_t compound, const char* chemin)
{
    static const int BASES[] = { CT_ELEMENTS + 0x18, CT_ELEMENTS };
    for (int base : BASES) {
        uintptr_t elements = 0;
        int32_t n = 0;
        if (!SafeRead((void*)(compound + base), &elements, 8) ||
            !SafeRead((void*)(compound + base + 8), &n, 4) || elements <= 0x10000 || n <= 0 || n > 64) {
            printf("[GRP]   elements en +0x%X : 0x%llX, %d (rejete)\n", base, (unsigned long long)elements, n);
            continue;
        }
        for (int i = 0; i < n; ++i)
            for (int off = 0; off < CT_PAS_ELEMENT; off += 8) {
                uintptr_t p = 0;
                if (SafeRead((void*)(elements + i * CT_PAS_ELEMENT + off), &p, 8) && p > 0x10000 &&
                    ClasseExacte(p, "FrostpunkSurvivorGroupComponentTemplate")) {
                    printf("[GRP]   template trouve : elements en +0x%X, element %d, +0x%X\n", base, i, off);
                    return p;
                }
            }
        printf("[GRP]   elements en +0x%X : %d element(s), aucun template de groupe ; contenu :\n", base, n);
        for (int i = 0; i < n && i < 2; ++i)
            for (int off = 0; off < CT_PAS_ELEMENT; off += 8) {
                uintptr_t p = 0;
                if (!SafeRead((void*)(elements + i * CT_PAS_ELEMENT + off), &p, 8) || !p) continue;
                char texte[96];
                texte[0] = 0;
                if (p > 0x10000 && !GetClassNameFP(p, texte, sizeof(texte))) {
                    texte[0] = 0;
                    if (!ReadAscii(p, texte, sizeof(texte))) texte[0] = 0;
                }
                printf("[GRP]     [%d] +0x%02X = 0x%llX %s\n", i, off, (unsigned long long)p, texte);
            }
    }

    char motif[MAX_CHEMIN + 2];
    snprintf(motif, sizeof(motif), "/%s/", chemin);
    uintptr_t tpls[256];
    int nt = CaptureLire(g_capTpl, tpls, 256);
    for (int i = 0; i < nt; ++i) {
        uintptr_t cle = 0;
        char texte[MAX_CHEMIN * 2];
        if (!SafeRead((void*)(tpls[i] + 0x58), &cle, 8) || cle <= 0x10000) continue;
        ReadAscii(cle, texte, sizeof(texte));
        if (strstr(texte, motif)) {
            printf("[GRP]   template trouve par capture (%d sur %d) : %s\n", i, nt, texte);
            return tpls[i];
        }
    }
    printf("[GRP]   aucun template capture ne correspond (%d capture(s))\n", nt);
    return 0;
}

// Prepare g_survivantsAPoser : une SurvivorsEntry (classe sociale et
// statistiques prises a la 1re entree d'un template "donneur" charge par son
// chemin), Count = survivants. Le template de base (SurvivorsFromExpeditionSite)
// n'a aucune entree : en vanilla le site remplit le groupe lui-meme.
bool PreparerSurvivants(const GroupeAP& g)
{
    g_survivantsAPoser = 0;
    g_nSurvivantsAPoser = 1;

    // Survivants par classe : une entree par classe trouvee dans les sites ;
    // une classe introuvable rejoint la premiere entree (citoyens livres,
    // classe fausse) plutot que d'etre perdue.
    if (g.classes[0].classe) {
        uint8_t* tab = MoteurAlloc ? (uint8_t*)MoteurAlloc(MAX_CLASSES_AP * ENTREE_SURVIVANTS) : nullptr;
        if (!tab) return false;
        int n = 0, orphelins = 0;
        for (int i = 0; i < MAX_CLASSES_AP && g.classes[i].classe; ++i) {
            uint8_t* e = tab + n * ENTREE_SURVIVANTS;
            if (g.classes[i].nombre <= 0) continue;
            if (!SitesEntreeSurvivants(g.classes[i].classe, e, g.site)) {
                printf("[GRP] classe \"%s\" introuvable : %d survivant(s) reportes\n", g.classes[i].classe, g.classes[i].nombre);
                orphelins += g.classes[i].nombre;
                continue;
            }
            int32_t nb = g.classes[i].nombre;
            memcpy(e, &nb, 4);
            printf("[GRP] entree %d : %d %s\n", n, nb, g.classes[i].classe);
            ++n;
        }
        if (n == 0) {
            printf("[GRP] aucune classe trouvee : %d survivant(s) non livres\n", orphelins);
            return false;
        }
        if (orphelins) {
            int32_t nb = 0;
            memcpy(&nb, tab, 4);
            nb += orphelins;
            memcpy(tab, &nb, 4);
            printf("[GRP] %d survivant(s) reportes ajoutes a l'entree 0\n", orphelins);
        }
        g_survivantsAPoser = (uintptr_t)tab;
        g_nSurvivantsAPoser = n;
        return true;
    }

    if (g.survivants < 0) return true;
    static const char* const DONNEURS[] = {
        "GameEntities/WorldMap/SurvivorGroups/Builders/BuildersScenarioWorkers",
        "GameEntities/WorldMap/SurvivorGroups/Refugees/RefugeeScenarioSurvivors01",
        "GameEntities/WorldMap/SurvivorGroups/TechHaven/TechHavenManchesterSurvivor",
        "GameEntities/WorldMap/SurvivorGroups/Winterhome/WinterhomeEngineersSurvivors",
    };
    const char* const* liste = g.cheminSurvivants ? &g.cheminSurvivants : DONNEURS;
    int nListe = g.cheminSurvivants ? 1 : (int)(sizeof(DONNEURS) / sizeof(DONNEURS[0]));
    for (int i = 0; i < nListe; ++i) {
        // Deja trouve : un template de groupe charge n'est pas reconstruit,
        // il n'apparaitrait plus dans les captures.
        struct DonneurConnu { char chemin[MAX_CHEMIN]; uintptr_t tpl; };
        static DonneurConnu connus[8];
        static int nConnus = 0;
        uintptr_t tplConnu = 0;
        for (int c = 0; c < nConnus && !tplConnu; ++c)
            if (!strcmp(connus[c].chemin, liste[i]) &&
                ClasseExacte(connus[c].tpl, "FrostpunkSurvivorGroupComponentTemplate")) tplConnu = connus[c].tpl;

        // Sinon, le template de composant du donneur est construit pendant son
        // chargement : c'est la capture apparue entre avant et apres.
        uintptr_t avant[256];
        int nAvant = CaptureLire(g_capTpl, avant, 256);
        uintptr_t stub = 0;
        uintptr_t compound = ObtenirTemplateParChemin(liste[i], &stub);
        uintptr_t apres[256];
        int nApres = CaptureLire(g_capTpl, apres, 256);
        uintptr_t tpl = tplConnu;
        if (tpl) printf("[GRP]   template deja connu : 0x%llX\n", (unsigned long long)tpl);
        for (int a = 0; a < nApres && !tpl; ++a) {
            bool deja = false;
            for (int b = 0; b < nAvant && !deja; ++b) deja = apres[a] == avant[b];
            if (!deja) tpl = apres[a];
        }
        if (tpl && !tplConnu) {
            printf("[GRP]   template construit pendant le chargement : 0x%llX\n", (unsigned long long)tpl);
            int c = nConnus < 8 ? nConnus++ : 7;
            strncpy_s(connus[c].chemin, liste[i], _TRUNCATE);
            connus[c].tpl = tpl;
        }
        else if (!tpl && compound) tpl = TemplateGroupeDe(compound, liste[i]);
        uintptr_t tab = 0;
        int32_t n = 0;
        if (tpl) {
            SafeRead((void*)(tpl + TPL_SURVIVANTS), &tab, 8);
            SafeRead((void*)(tpl + TPL_SURVIVANTS + 8), &n, 4);
        }
        printf("[GRP] donneur \"%s\" : compound 0x%llX, template 0x%llX, %d entree(s)\n", liste[i],
            (unsigned long long)compound, (unsigned long long)tpl, n);
        if (!tab || n <= 0 || n > 64) continue;

        uint8_t* e = MoteurAlloc ? (uint8_t*)MoteurAlloc(ENTREE_SURVIVANTS) : nullptr;
        if (!e || !SafeRead((void*)tab, e, ENTREE_SURVIVANTS)) return false;
        int32_t nb = g.survivants;
        memcpy(e, &nb, 4);
        printf("[GRP] entree de survivants : %d, classe sociale", nb);
        for (int j = 4; j < 0x14; ++j) printf(" %02X", e[j]);
        printf(", statistiques");
        for (int j = 0x14; j < 0x24; ++j) printf(" %02X", e[j]);
        printf("\n");
        g_survivantsAPoser = (uintptr_t)e;
        return true;
    }
    printf("[GRP] aucun donneur de survivants : contenu du template conserve\n");
    return false;
}

void DecrireContenu(uintptr_t comp)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(comp + COMP_CONTENU), &tab, 8) || !SafeRead((void*)(comp + COMP_CONTENU + 8), &n, 4) ||
        !tab || n < 0 || n > 64) {
        printf("[GRP] contenu illisible (0x%llX, %d)\n", (unsigned long long)tab, n);
        return;
    }
    printf("[GRP] contenu : %d entree(s)\n", n);
    for (int i = 0; i < n; ++i) {
        uint8_t e[ENTREE_SURVIVANTS] = {};
        SafeRead((void*)(tab + i * ENTREE_SURVIVANTS), e, ENTREE_SURVIVANTS);
        int32_t nb;
        memcpy(&nb, e, 4);
        printf("   [%d] %d survivant(s), classe sociale", i, nb);
        for (int j = 4; j < 0x14; ++j) printf(" %02X", e[j]);
        printf(", statistiques");
        for (int j = 0x14; j < 0x24; ++j) printf(" %02X", e[j]);
        printf("\n");
    }
}

// survivants >= 0 : tout dans la premiere entree (sa classe sociale), les
// autres a 0. sansMorts : taux de mort du groupe a 0 (le tick de mortalite
// FUN_1418b2f70 ne fait rien si le taux est nul).
void AppliquerContenu(uintptr_t comp, int survivants, bool sansMorts)
{
    DecrireContenu(comp);
    uintptr_t tab = 0;
    int32_t n = 0;
    if (survivants >= 0 && SafeRead((void*)(comp + COMP_CONTENU), &tab, 8) &&
        SafeRead((void*)(comp + COMP_CONTENU + 8), &n, 4) && tab && n > 0 && n <= 64) {
        for (int i = 0; i < n; ++i) {
            int32_t nb = i == 0 ? survivants : 0;
            SafeWrite((void*)(tab + i * ENTREE_SURVIVANTS), &nb, 4);
        }
        printf("[GRP] contenu pose : %d survivant(s) dans l'entree 0\n", survivants);
    }
    if (sansMorts) {
        float zero = 0.0f;
        SafeWrite((void*)(comp + COMP_TAUX_MORT), &zero, 4);
        printf("[GRP] taux de mort mis a 0\n");
    }
}

// Fixe le temps restant d'un groupe en marche : le trajet repart maintenant,
// de la position courante, pour arriver dans 'heures' de jeu. Si 'clone' est
// fourni, sa vitesse est ajustee dans la meme proportion (au cas ou le jeu
// recalculerait le trajet depuis la vitesse).
bool ReglerDuree(uintptr_t entite, float heures, uintptr_t clone)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sysMouv = 0, horloge = 0;
    int32_t secParHeure = 0;
    int64_t maintenant = 0;
    SafeRead((void*)(base + OFF_SYS_MOUVEMENT), &sysMouv, 8);
    SafeRead((void*)(base + OFF_HORLOGE), &horloge, 8);
    SafeRead((void*)(base + OFF_SECONDES_PAR_HEURE), &secParHeure, 4);
    if (!sysMouv || !horloge || secParHeure <= 0 ||
        !SafeRead((void*)(horloge + HORLOGE_DATE), &maintenant, 8)) {
        printf("[GRP] duree : references absentes (mouvement 0x%llX, horloge 0x%llX, %d s/h)\n",
            (unsigned long long)sysMouv, (unsigned long long)horloge, secParHeure);
        return false;
    }

    MouvementDe_t mouvementDe = (MouvementDe_t)(base + OFF_MOUVEMENT_DE);
    uintptr_t mouv = 0;
    __try {
        mouv = mouvementDe(sysMouv, entite);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        mouv = 0;
    }
    int64_t depart = 0, arrivee = 0;
    float pos[4] = {};
    if (!mouv || !SafeRead((void*)(mouv + MOUV_DATE_DEPART), &depart, 8) ||
        !SafeRead((void*)(mouv + MOUV_DATE_ARRIVEE), &arrivee, 8) ||
        !SafeRead((void*)(entite + ENT_POSITION_MONDE), pos, sizeof(pos))) {
        printf("[GRP] duree : composant de mouvement illisible (0x%llX)\n", (unsigned long long)mouv);
        return false;
    }

    double ticksParHeure = secParHeure * TICKS_PAR_SECONDE;
    double restantAvant = (double)(arrivee - maintenant) / ticksParHeure;
    int64_t nouvelle = maintenant + (int64_t)(heures * ticksParHeure);
    SafeWrite((void*)(mouv + MOUV_DEPART), pos, sizeof(pos));
    SafeWrite((void*)(mouv + MOUV_DATE_DEPART), &maintenant, 8);
    SafeWrite((void*)(mouv + MOUV_DATE_ARRIVEE), &nouvelle, 8);

    float vitesse = 0;
    if (clone && restantAvant > 0.01 && heures > 0.01f && SafeRead((void*)(clone + TPL_VITESSE), &vitesse, 4)) {
        float nouvelleVitesse = (float)(vitesse * restantAvant / heures);
        SafeWrite((void*)(clone + TPL_VITESSE), &nouvelleVitesse, 4);
        printf("[GRP] vitesse de la copie : %.1f -> %.1f\n", vitesse, nouvelleVitesse);
    }
    JournalImmediat("[GRP] duree : %.2f h -> %.2f h restantes (mouvement 0x%llX, %d s par heure, "
                    "position %.1f / %.1f)\n", restantAvant, heures, (unsigned long long)mouv,
                    secParHeure, pos[0], pos[2]);
    return true;
}

// --- Releve d'un groupe (persistance, 2026-10-29) -----------------------------
// Avant d'ecrire notre propre sauvegarde des groupes AP, on releve ce qui
// survit a un chargement : template du composant (notre copie ou celui du
// jeu), nom d'entite (suffixe *NNNN : identifiant stable ?), premiers champs
// de l'entite (autre identifiant ?), contenu, ReachedCity, dates du
// mouvement. Meme releve a la creation (etiquette "cree") pour comparer.
// Thread du jeu (appel de FUN_141884960).
void ReleverUnGroupe(const char* quand, uintptr_t comp)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t e = 0, tpl = 0, cle = 0;
    char nomEnt[96] = "?", cleNom[128] = "?";
    SafeRead((void*)(comp + 0x18), &e, 8);
    SafeRead((void*)(comp + COMP_TEMPLATE), &tpl, 8);
    if (e > 0x10000) LireNomPointe(e, ENT_NOM, nomEnt, sizeof(nomEnt));
    if (tpl > 0x10000 && SafeRead((void*)(tpl + 0x58), &cle, 8) && cle > 0x10000) ReadAscii(cle, cleNom, sizeof(cleNom));
    bool notreCopie = false;
    for (const GroupeSuivi& s : g_groupes) if (s.actif && s.clone && s.clone == tpl) notreCopie = true;
    uint8_t guid[16] = {}, atteint = 0;
    if (tpl > 0x10000) SafeRead((void*)(tpl + 0xD0), guid, 16);
    SafeRead((void*)(comp + COMP_ARRIVE), &atteint, 1);
    uintptr_t contenu = 0;
    int32_t nContenu = 0, premierCount = 0;
    SafeRead((void*)(comp + COMP_CONTENU), &contenu, 8);
    SafeRead((void*)(comp + COMP_CONTENU + 8), &nContenu, 4);
    if (contenu > 0x10000 && nContenu > 0) SafeRead((void*)contenu, &premierCount, 4);

    printf("[SAV] groupe %s : composant 0x%llX, entite 0x%llX \"%s\"\n", quand, (unsigned long long)comp,
        (unsigned long long)e, nomEnt);
    printf("[SAV]   template 0x%llX%s, cle du nom \"%s\", GUID", (unsigned long long)tpl,
        notreCopie ? " (NOTRE COPIE)" : "", cleNom);
    for (int i = 0; i < 16; ++i) printf(" %02X", guid[i]);
    printf("\n[SAV]   contenu : %d entree(s), 1re Count %d ; ReachedCity %d\n", nContenu, premierCount, atteint);

    uintptr_t sysMouv = 0, horloge = 0, mouv = 0;
    int32_t secParHeure = 0;
    int64_t maintenant = 0, depart = 0, arrivee = 0;
    SafeRead((void*)(base + OFF_SYS_MOUVEMENT), &sysMouv, 8);
    SafeRead((void*)(base + OFF_HORLOGE), &horloge, 8);
    SafeRead((void*)(base + OFF_SECONDES_PAR_HEURE), &secParHeure, 4);
    if (horloge) SafeRead((void*)(horloge + HORLOGE_DATE), &maintenant, 8);
    if (sysMouv && e > 0x10000) {
        __try { mouv = ((MouvementDe_t)(base + OFF_MOUVEMENT_DE))(sysMouv, e); }
        __except (EXCEPTION_EXECUTE_HANDLER) { mouv = 0; }
    }
    if (mouv && SafeRead((void*)(mouv + MOUV_DATE_DEPART), &depart, 8) &&
        SafeRead((void*)(mouv + MOUV_DATE_ARRIVEE), &arrivee, 8) && secParHeure > 0) {
        double tph = secParHeure * TICKS_PAR_SECONDE;
        printf("[SAV]   mouvement 0x%llX : depart 0x%llX, arrivee 0x%llX, maintenant 0x%llX (%.2f h restantes)\n",
            (unsigned long long)mouv, (unsigned long long)depart, (unsigned long long)arrivee,
            (unsigned long long)maintenant, (double)(arrivee - maintenant) / tph);
    }
    else printf("[SAV]   mouvement illisible (0x%llX)\n", (unsigned long long)mouv);

    // Premiers champs de l'entite : un identifiant stable y est peut-etre.
    printf("[SAV]   entite :");
    for (int off = 0x08; off < 0x48 && e > 0x10000; off += 8) {
        uint64_t q = 0;
        SafeRead((void*)(e + off), &q, 8);
        printf(" +%02X=%llX", off, (unsigned long long)q);
    }
    printf("\n");
}

// Tous les groupes de la partie (apres un chargement).
void ReleverGroupes(const char* quand)
{
    uintptr_t comps[32];
    int n = ComposantsDuSysteme(ID_SURVIVOR_GROUP, "FrostpunkSurvivorGroupComponent", comps, 32);
    printf("[SAV] releve des groupes (%s) : %d groupe(s)\n", quand, n);
    for (int i = 0; i < n; ++i) ReleverUnGroupe(quand, comps[i]);
}

// Thread du jeu : applique les durees en attente (groupe pas encore en marche
// a sa creation) et oublie les groupes disparus ou d'une autre partie.
void SuivreGroupes()
{
    unsigned gen = PartieGeneration();
    if (gen != g_groupesGeneration) {
        // Changement de partie vu par PartieTick, BIEN APRES le chargement :
        // les slots recolles pendant LoadGame sont deja la. On ne garde que
        // les slots dont le composant est vivant, a la meme entite, de meme
        // identifiant (les adresses sont reutilisees d'un chargement a l'autre).
        int gardes = 0;
        for (GroupeSuivi& s : g_groupes) {
            if (!s.actif) continue;
            uintptr_t ent = 0;
            uint64_t guid[2] = {};
            bool valide = ClasseExacte(s.comp, "FrostpunkSurvivorGroupComponent") &&
                          SafeRead((void*)(s.comp + 0x18), &ent, 8) && ent == s.entite &&
                          SafeRead((void*)(ent + ENT_ID), guid, sizeof(guid)) &&
                          guid[0] == s.guid[0] && guid[1] == s.guid[1];
            if (valide) ++gardes;
            else memset(&s, 0, sizeof(s));
        }
        g_groupesGeneration = gen;
        printf("[SAV] nouvelle partie (generation %u) : %d groupe(s) AP suivi(s)\n", gen, gardes);
        return;
    }
    for (int i = 0; i < MAX_GROUPES_AP; ++i) {
        GroupeSuivi& s = g_groupes[i];
        if (!s.actif || s.arrive || s.dureeAppliquee || !s.comp) continue;
        uintptr_t ent = 0;
        if (!ClasseExacte(s.comp, "FrostpunkSurvivorGroupComponent") ||
            !SafeRead((void*)(s.comp + 0x18), &ent, 8) || ent != s.entite) {
            printf("[GRP] groupe du slot %d disparu\n", i);
            s.actif = false;
            continue;
        }
        uint8_t init = 0;
        SafeRead((void*)(s.comp + COMP_INITIALISE), &init, 1);
        if (!init) continue;
        ReglerDuree(s.entite, s.heures, s.clone);
        s.dureeAppliquee = true;    // une seule tentative : pas de boucle de logs
    }
}

// Test F3 : 5 survivants, et une cargaison (inventaire + livraison a l'arrivee).
void CreerGroupeDeTest()
{
    GroupeAP g = {};
    g.cheminTemplate = "GameEntities/WorldMap/SurvivorGroups/SurvivorsFromExpeditionSite";
    g.nom = "Archipelago Delivery";
    g.description = "Heading to the city: 5 survivors, 50 coal, 1 steam core (F3 test).";
    g.survivants = 5;
    g.heures = 1.0f;
    g.sansMorts = true;
    g.cargaison[0] = { "coal", 50 };
    g.cargaison[1] = { "steamcore", 1 };
    g.dilemmeTitre = "Archipelago Delivery";
    g.dilemmeTexte = "An Archipelago convoy has reached the city: 5 survivors, 50 coal and 1 steam core.";
    g.dilemmeChoix = "Welcome the convoy";
    int slot = GroupeCreer(g);
    printf("[GRP] groupe de test : slot %d\n", slot);
}

// --- Capture des templates de composant de groupe ---------------------------
// Pour creer un groupe dans n'importe quel scenario, il faut trouver son
// template sans qu'un groupe existe. Comme pour les gabarits de batiment, on
// capture chaque FrostpunkSurvivorGroupComponentTemplate a sa construction.
// Les fonctions qui chargent sa vtable sont retrouvees par RTTI au demarrage
// (pas d'adresse en dur) ; une destruction ne pollue pas la liste car
// CaptureLire revalide la classe.
typedef uintptr_t(*FonctionTpl_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
constexpr int MAX_FONCTIONS_TPL = 4;
FonctionTpl_t g_origTpl[MAX_FONCTIONS_TPL] = {};
uintptr_t     g_tplObjets[256];
ListeCaptures g_capTpl = { SRWLOCK_INIT, g_tplObjets, 256, 0,
                           "FrostpunkSurvivorGroupComponentTemplate", 0 };
volatile long g_traceTpl = 0;

uintptr_t CaptureTpl(int i, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    uintptr_t r = g_origTpl[i](a, b, c, d);
    if (ClasseExacte(a, "FrostpunkSurvivorGroupComponentTemplate")) {
        CaptureAjouter(g_capTpl, a);
        long n = InterlockedIncrement(&g_traceTpl);
        if (n <= 3) TracerPile("SurvivorGroupComponentTemplate", a, n);
    }
    return r;
}
uintptr_t hkTpl0(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return CaptureTpl(0, a, b, c, d); }
uintptr_t hkTpl1(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return CaptureTpl(1, a, b, c, d); }
uintptr_t hkTpl2(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return CaptureTpl(2, a, b, c, d); }
uintptr_t hkTpl3(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) { return CaptureTpl(3, a, b, c, d); }
void* const g_hkTpl[MAX_FONCTIONS_TPL] = { (void*)&hkTpl0, (void*)&hkTpl1, (void*)&hkTpl2, (void*)&hkTpl3 };

// Chaine du moteur (texte 8 bits) pointee a 'adr', ou "-".
void LireTexte(uintptr_t obj, int off, char* out, int maxlen)
{
    uintptr_t p = 0;
    out[0] = '-'; out[1] = 0;
    if (SafeRead((void*)(obj + off), &p, 8) && p > 0x10000) ReadAscii(p, out, maxlen);
}

void RapportTemplates()
{
    uintptr_t tpls[256];
    int n = CaptureLire(g_capTpl, tpls, 256);
    printf("[GRP] templates de groupe : %ld construction(s), %d vivant(s)\n", g_capTpl.appels, n);
    for (int i = 0; i < n; ++i) {
        char nom[96], chemin[96];
        LireTexte(tpls[i], 0x58, nom, sizeof(nom));
        LireTexte(tpls[i], 0x68, chemin, sizeof(chemin));
        float vitesse = 0;
        int32_t nSurv = 0, etatDepart = 0;
        SafeRead((void*)(tpls[i] + 0x40), &vitesse, 4);
        SafeRead((void*)(tpls[i] + 0x30), &nSurv, 4);
        SafeRead((void*)(tpls[i] + 0x10C), &etatDepart, 4);
        printf("   [%d] 0x%llX vitesse %.0f, survivants %d, etat de depart %d\n       %s\n       %s\n",
            i, (unsigned long long)tpls[i], vitesse, nSurv, etatDepart, nom, chemin);
        // Format des icones a identifier : 2 variantes de 0x18 (+0x70, +0x88)
        // puis l'image (+0xA0).
        if (i < 3) DecrireChamps(tpls[i] + 0x70, 0x48, "icones (+0x70, +0x88) et image (+0xA0)");
    }
}

// --- Instantanes d'un groupe : qu'est-ce qui change pendant la marche ? -----
// Chaque F9 relit les memes zones du premier groupe ; si l'objet n'a pas
// change d'adresse, on affiche les dwords modifies depuis l'appui precedent.
struct Zone { const char* nom; uintptr_t adr; int taille; uint8_t octets[0x100]; };
Zone g_zones[5];
int  g_nZones = 0;

void ComparerZone(Zone& z, uintptr_t adr)
{
    uint8_t neuf[0x100] = {};
    if (!adr || !SafeRead((void*)adr, neuf, z.taille)) {
        printf("[DIFF] %s : illisible\n", z.nom);
        z.adr = 0;
        return;
    }
    if (z.adr != adr) {
        printf("[DIFF] %s = 0x%llX : premier instantane\n", z.nom, (unsigned long long)adr);
    }
    else {
        int changes = 0;
        for (int off = 0; off + 4 <= z.taille; off += 4) {
            uint32_t a, b;
            memcpy(&a, z.octets + off, 4);
            memcpy(&b, neuf + off, 4);
            if (a == b) continue;
            float fa, fb;
            memcpy(&fa, &a, 4);
            memcpy(&fb, &b, 4);
            printf("[DIFF] %s +0x%03X : %08X -> %08X  (i32 %d -> %d, f %.4g -> %.4g)\n",
                z.nom, off, a, b, (int32_t)a, (int32_t)b, fa, fb);
            ++changes;
        }
        printf("[DIFF] %s : %d dword(s) modifie(s)\n", z.nom, changes);
    }
    z.adr = adr;
    memcpy(z.octets, neuf, z.taille);
}

// --- Regler le temps de trajet restant d'un groupe --------------------------
// L'ancien F8 avancait le qword +0x38 du composant, pris pour un temps de
// marche : c'est le minuteur des morts (FUN_1418b2f70). Le trajet vit dans le
// LinearMovementComponent (voir ReglerDuree). F8 ramene a TEMPS_RESTANT_VISE
// le dernier groupe Archipelago cree, sinon le premier groupe de la carte.
constexpr float TEMPS_RESTANT_VISE = 0.5f;   // heures de jeu

volatile long g_rapprochementsDemandes = 0;

// Un groupe deja arrive en ville (ReachedCity, comp +0x35) n'a plus de trajet :
// le regler donnait une duree absurde (test du 2026-10-28 : 171798662 h).
void RapprocherPremierGroupe()
{
    uintptr_t e = 0, comp = 0, clone = 0;
    for (int i = MAX_GROUPES_AP - 1; i >= 0 && !e; --i) {
        if (!g_groupes[i].actif || g_groupes[i].arrive ||
            !ClasseExacte(g_groupes[i].comp, "FrostpunkSurvivorGroupComponent")) continue;
        e = g_groupes[i].entite;
        comp = g_groupes[i].comp;
        clone = g_groupes[i].clone;
    }
    if (!e && !PremierGroupe(&e, &comp)) { printf("[GRP] aucun groupe a rapprocher\n"); return; }
    uint8_t atteint = 0;
    SafeRead((void*)(comp + COMP_ARRIVE), &atteint, 1);
    if (atteint) { printf("[GRP] groupe 0x%llX deja arrive en ville : rien a rapprocher\n", (unsigned long long)e); return; }
    printf("[GRP] rapprochement du groupe 0x%llX (%s)\n", (unsigned long long)e,
        clone ? "groupe Archipelago" : "premier groupe de la carte");
    ReglerDuree(e, TEMPS_RESTANT_VISE, clone);
}

// --- Carte du monde toujours accessible -------------------------------------
// FrostpunkWorldMapSystem, decompiles du 2026-10-28 (frostpunkworldmapsystem) :
//   +0x2281 WorldMapUnlocked (octet), +0x2284 WorldMapAvailabilityBuildingsCount
//   (batiments qui ouvrent la carte : la balise), +0x21D0 compteur
//   d'invocation de l'evenement LockedUnlocked.
// Bouton actif ssi Unlocked && compte >= 1 && *(DAT_14408dd50 + 0xA8) != 2.
// FUN_141332cd0(sys) = deverrouiller (pose +0x2281, bouton, evenement) ;
// FUN_141331940(sys, 2) = recalcule la disponibilite : bouton (DAT_144065ae0
// +0x21C) et evenement AvailabilityChanged (+0x2248).
// Le compte n'est releve a 1 que s'il est nul : une vraie balise le porte a 2,
// sa destruction le ramene a 1 (jamais sous 0, assert du jeu).
typedef void(*DeverrouillerCarte_t)(uintptr_t sys);
typedef void(*RafraichirCarte_t)(uintptr_t sys, uintptr_t etat);

static const uintptr_t OFF_DEVERROUILLER_CARTE = Adr(0x1332CD0);
static const uintptr_t OFF_RAFRAICHIR_CARTE    = Adr(0x1331940);
constexpr int       CARTE_COMPTEUR_EVT      = 0x21D0;
constexpr int       CARTE_DEVERROUILLEE     = 0x2281;
constexpr int       CARTE_NB_BATIMENTS      = 0x2284;
constexpr int       MAX_LOGS_CARTE          = 5;

unsigned g_carteGeneration = 0xFFFFFFFF;  // partie deja controlee
bool     g_carteValide     = false;
int      g_carteLogs       = 0;

void ForcerAccesCarte(uintptr_t sys, bool deverrouiller, bool releverCompte)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    DeverrouillerCarte_t dever = (DeverrouillerCarte_t)(base + OFF_DEVERROUILLER_CARTE);
    RafraichirCarte_t rafraichir = (RafraichirCarte_t)(base + OFF_RAFRAICHIR_CARTE);
    __try {
        if (deverrouiller) dever(sys);
        if (releverCompte) {
            int32_t un = 1;
            SafeWrite((void*)(sys + CARTE_NB_BATIMENTS), &un, 4);
        }
        rafraichir(sys, 2);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        JournalImmediat("[CARTE] EXCEPTION 0x%08lX pendant le forcage de l'acces\n", GetExceptionCode());
    }
}

// --- Inventaire d'un groupe AP dans la liste de droite ----------------------
// Decompiles du 2026-10-28 : la ligne de la liste de droite est un
// FrostpunkUIExpeditionControl, le MEME pour un scout et pour un groupe
// (controle +0x290 poignee de l'expedition, +0x2A8 poignee du composant de
// groupe ; zones "CARGO_LAYOUT" : tableau de poignees en +0x310, compte
// +0x318). FUN_141c2b6e0(controle) remplit l'inventaire : pour un scout la
// cargaison (+0x168 ressources {definition, quantite}, +0x180 technos, +0x198
// artefacts), pour un groupe ses seuls survivants (comp +0x48). Chaque case :
//   suivante = FUN_1416b7350(*DAT_14408ec18, &DAT_1440af6f0, case a reutiliser
//              (0 : creee depuis le layout), layout, &texte (FUN_141ce67f0
//              depuis definition +0x28), tuile (definition +0x98),
//              {1, quantite}, -1, &DAT_142a22e20)
// renvoie la case suivante (case +0x240) ; puis FUN_141a31660(suivante,
// &DAT_1440af6f0) supprime les cases restantes (s'il en reste) et
// FUN_141a316b0(layout) finalise. On ajoute nos ressources, avec la meme
// fonction que les scouts, juste avant la suppression des restes (nos cases
// du rafraichissement precedent sont alors reutilisees), ou, s'il n'y a pas
// de reste, juste avant la finalisation du layout.
static const uintptr_t OFF_UI_CARGAISON       = Adr(0x1C2B6E0);
static const uintptr_t OFF_UI_AJOUTER_CASE    = Adr(0x16B7350);
static const uintptr_t OFF_UI_SUPPRIMER_RESTE = Adr(0x1A31660);
static const uintptr_t OFF_UI_FINALISER       = Adr(0x1A316B0);
static const uintptr_t OFF_UI_PREMIERE_CASE   = Adr(0x1025CC0);
static const uintptr_t OFF_UI_SUPPORT         = Adr(0x408EC18);   // DAT_14408ec18 (valeur)
static const uintptr_t OFF_UI_NOM_CASE        = Adr(0x40AF6F0);   // DAT_1440af6f0 (adresse)
static const uintptr_t OFF_UI_PRESET_CASE     = Adr(0x2A22E20);   // DAT_142a22e20 (adresse)
static const uintptr_t OFF_TEXTE_DEPUIS_CLE   = Adr(0x1CE67F0);
static const uintptr_t OFF_TEXTE_LIBERER      = Adr(0x0E1D180);
constexpr int       CTRL_EXPEDITION        = 0x290;   // poignee de l'expedition (scout)
constexpr int       CTRL_GROUPE            = 0x2A8;
constexpr int       CTRL_LAYOUTS           = 0x310;
constexpr int       DEF_RES_NOM            = 0x28;
constexpr int       DEF_RES_ICONE          = 0x98;
constexpr int       MAX_LAYOUTS_UI         = 4;

typedef void(*UICargaison_t)(uintptr_t ctrl);
typedef uintptr_t(*UIAjouterCase_t)(uintptr_t support, uintptr_t nomCase, uintptr_t caseReutilisee,
                                    uintptr_t layout, uintptr_t* texte, uintptr_t tuile, uint64_t quantite,
                                    int64_t p8, uintptr_t preset);
typedef void(*UISupprimerReste_t)(uintptr_t premier, uintptr_t nomCase);
typedef void(*UIFinaliser_t)(uintptr_t element);
typedef uintptr_t(*UIPremiereCase_t)(uintptr_t layout);
typedef uintptr_t(*TexteDepuisCle_t)(uintptr_t* sortie, uintptr_t cle);
typedef void(*TexteLiberer_t)(uintptr_t* texte);

UICargaison_t      g_origUICargaison    = nullptr;
UIAjouterCase_t    g_origAjouterCase    = nullptr;
UISupprimerReste_t g_origSupprimerReste = nullptr;
UIFinaliser_t      g_origFinaliser      = nullptr;

// Rafraichissement en cours d'un controle de NOTRE groupe (thread du jeu).
// 'vague' (2026-11-10) : groupe de refugies de l'apres-005 du jeu (1..3) ;
// ses survivants sont masques et une seule case est posee : l'item AP (le
// porteur des scouts), dont le nom traduit est le check porte.
struct RafraichissementAP {
    bool      actif;
    int       slot;
    int       vague;
    int       nLayouts;
    uintptr_t layouts[MAX_LAYOUTS_UI];
    uintptr_t suivante[MAX_LAYOUTS_UI];   // dernier retour de AjouterCase pour ce layout
    bool      vu[MAX_LAYOUTS_UI];         // le jeu y a pose au moins une case
    bool      fait[MAX_LAYOUTS_UI];       // nos cases y sont
} g_rafr = {};

// Entree FrostpunkResourceEntry d'une ressource. Test du 2026-10-28 : l'objet
// du conteneur de l'economie (Res_Definition) a bien le GUID en +0x08, mais
// son +0x28 n'est PAS la cle du nom (faute dans FUN_141ce5a40 en lisant le
// 1er caractere). Comme FUN_141c2b6e0 pour la cargaison d'un transport, on
// resout l'entree par son GUID dans la collection des ressources
// (FUN_1413ebe70 = &DAT_14406ad18, methode +0x88(collection, &guid)).
// Verifie : cle du nom commencant par '@'. 0 sinon.
static const uintptr_t OFF_COLLECTION_RESSOURCES = Adr(0x406AD18);
typedef uintptr_t(*EntreeParGuid_t)(uintptr_t collection, const void* guid);

// Vraie entree : meme GUID en +0x08 et cle du nom (+0x28) commencant par '@'.
bool EstEntreeRessource(uintptr_t e, const uint8_t* guid)
{
    uint8_t g[16];
    uintptr_t cle = 0;
    char premier = 0;
    return e > 0x10000 && SafeRead((void*)(e + 8), g, 16) && !memcmp(g, guid, 16) &&
           SafeRead((void*)(e + DEF_RES_NOM), &cle, 8) && cle > 0x10000 &&
           SafeRead((void*)cle, &premier, 1) && premier == '@';
}

// Releve du 2026-10-28 : l'entree est a Res_Definition() + 8 (= resId) :
// +0x00 nom interne ("Coal"), +0x08 GUID, +0x18 / +0x1C quantite initiale /
// plafond, +0x28 "@Resources/Names/Coal", +0x98 icone. L'ancien essai lisait
// le GUID 8 octets trop tot (la collection ne trouvait donc rien).
// Test du 2026-10-29 : apres le reancrage de l'economie (Res_InitDepuisEconomie),
// Res_Definition renvoie l'entree elle-meme, alors qu'avant (contexte capture
// sur un appel du jeu) elle renvoyait l'entree - 8. On ne suppose plus rien :
// GUID de la table de resources.cpp, et on essaie def, def + 8, def - 8.
uintptr_t EntreeRessource(const char* nom)
{
    uintptr_t def = Res_Definition(nom);
    uint8_t guid[16];
    if (!def || !Res_Guid(nom, guid)) return 0;
    static const intptr_t DECALAGES[] = { 0, 8, -8 };
    for (intptr_t d : DECALAGES)
        if (EstEntreeRessource(def + d, guid)) return def + d;
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);

    // 1) La collection des ressources, comme le jeu.
    uintptr_t coll = base + OFF_COLLECTION_RESSOURCES, vt = 0, fn = 0, entree = 0;
    bool exc = false;
    if (SafeRead((void*)coll, &vt, 8) && vt && SafeRead((void*)(vt + 0x88), &fn, 8) && fn) {
        __try { entree = ((EntreeParGuid_t)fn)(coll, guid); }
        __except (EXCEPTION_EXECUTE_HANDLER) { exc = true; entree = 0; }
    }
    if (EstEntreeRessource(entree, guid)) return entree;

    // 2) L'objet de l'economie lui-meme, ou un des pointeurs qu'il porte
    //    (poignee EntryPtr ?).
    if (EstEntreeRessource(def, guid)) return def;
    for (int off = 0; off < 0x40; off += 8) {
        uintptr_t q = 0;
        if (SafeRead((void*)(def + off), &q, 8) && EstEntreeRessource(q, guid)) {
            printf("[GRP] inventaire : entree de \"%s\" = *(definition de l'economie + 0x%X)\n", nom, off);
            return q;
        }
    }

    // Echec : releve pour comprendre.
    char cls[96] = "?";
    GetClassNameFP(coll, cls, sizeof(cls));
    printf("[GRP] inventaire : \"%s\" introuvable ; collection 0x%llX (%s), vtable Ghidra 0x%llX, +0x88 Ghidra 0x%llX, "
        "retour 0x%llX%s\n", nom, (unsigned long long)coll, cls,
        (unsigned long long)(vt ? vt - base + 0x140000000ULL : 0), (unsigned long long)(fn ? fn - base + 0x140000000ULL : 0),
        (unsigned long long)entree, exc ? " (EXCEPTION)" : "");
    if (entree > 0x10000) {
        uint8_t g[16] = {};
        uintptr_t cle = 0;
        SafeRead((void*)(entree + 8), g, 16);
        SafeRead((void*)(entree + DEF_RES_NOM), &cle, 8);
        printf("[GRP]   retour : GUID %s, +0x28 = 0x%llX\n", memcmp(g, guid, 16) ? "DIFFERENT" : "identique",
            (unsigned long long)cle);
    }
    printf("[GRP]   definition de l'economie 0x%llX :\n", (unsigned long long)def);
    for (int off = 0; off < 0x70; off += 8) {
        uintptr_t q = 0;
        if (!SafeRead((void*)(def + off), &q, 8)) break;
        char c2[96] = "", t[64] = "";
        if (q > 0x10000) {
            if (!GetClassNameFP(q, c2, sizeof(c2))) c2[0] = 0;
            if (!c2[0] && ReadAscii(q, t, sizeof(t)) && (strlen(t) < 3 || (unsigned char)t[0] < 0x20)) t[0] = 0;
        }
        printf("[GRP]     +0x%02X = 0x%016llX %s%s\n", off, (unsigned long long)q, c2, t);
    }
    return 0;
}

int FiltreInventaire(EXCEPTION_POINTERS* ep, DWORD* code, uintptr_t* adresse)
{
    *code = ep->ExceptionRecord->ExceptionCode;
    *adresse = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}

// Groupe de refugies en cours d'affichage (1..3, 0 sinon) : lu par
// hkTranslate pour le nom de l'item AP pendant le remplissage de sa case.
int g_vagueAffichee = 0;

// Case de l'item AP d'un groupe de refugies (comme l'item porte par un
// scout) : definition = le porteur (FrostpunkResourceEntry clonee, nom
// "@Resources/Names/AP_Item" en +0x28, icone en +0x98), quantite 1.
uintptr_t PoserCaseRefugies(int k, uintptr_t suivante)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t support = 0, def = ApPorteurDef();
    if (!def || !SafeRead((void*)(base + OFF_UI_SUPPORT), &support, 8) || !support) return suivante;
    TexteDepuisCle_t texteDepuisCle = (TexteDepuisCle_t)(base + OFF_TEXTE_DEPUIS_CLE);
    TexteLiberer_t liberer = (TexteLiberer_t)(base + OFF_TEXTE_LIBERER);
    uintptr_t texte = 0;
    uint64_t quantite = ((uint64_t)1 << 32) | 1;
    DWORD code = 0;
    uintptr_t fautive = 0;
    g_vagueAffichee = g_rafr.vague;
    __try {
        texteDepuisCle(&texte, def + DEF_RES_NOM);
        suivante = g_origAjouterCase(support, base + OFF_UI_NOM_CASE, suivante, g_rafr.layouts[k],
                                     &texte, def + DEF_RES_ICONE, quantite, -1, base + OFF_UI_PRESET_CASE);
        if (texte) liberer(&texte);
    }
    __except (FiltreInventaire(GetExceptionInformation(), &code, &fautive)) {
        g_vagueAffichee = 0;
        JournalImmediat("[GRP] inventaire des refugies %d : EXCEPTION 0x%08lX, adresse Ghidra 0x%llX\n", g_rafr.vague,
            code, (unsigned long long)(fautive - base + 0x140000000ULL));
        return 0;
    }
    g_vagueAffichee = 0;
    static bool dit[16] = {};
    if (g_rafr.vague >= 1 && g_rafr.vague <= 13 && !dit[g_rafr.vague]) {
        dit[g_rafr.vague] = true;
        printf("[GRP] inventaire des refugies %d : case de l'item AP posee (layout %d)\n", g_rafr.vague, k);
    }
    return suivante;
}

// Pose nos cases a partir de 'suivante' (case a reutiliser, ou 0 : creee).
// Renvoie la case qui suit les notres.
uintptr_t PoserCasesCargaison(int k, uintptr_t suivante)
{
    if (g_rafr.vague) return PoserCaseRefugies(k, suivante);
    GroupeSuivi& s = g_groupes[g_rafr.slot];
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t support = 0;
    if (!SafeRead((void*)(base + OFF_UI_SUPPORT), &support, 8) || !support) return suivante;
    TexteDepuisCle_t texteDepuisCle = (TexteDepuisCle_t)(base + OFF_TEXTE_DEPUIS_CLE);
    TexteLiberer_t liberer = (TexteLiberer_t)(base + OFF_TEXTE_LIBERER);
    int poses = 0;
    for (int i = 0; i < s.nCargaison; ++i) {
        uintptr_t def = EntreeRessource(s.cargaison[i].ressource);
        if (!def) {
            if (!s.inventaireJournalise)
                printf("[GRP] inventaire : entree de \"%s\" introuvable (definition de l'economie 0x%llX)\n",
                    s.cargaison[i].ressource, (unsigned long long)Res_Definition(s.cargaison[i].ressource));
            continue;
        }
        uintptr_t texte = 0;
        uint64_t quantite = ((uint64_t)(uint32_t)s.cargaison[i].quantite << 32) | 1;
        int etape = 0;
        DWORD code = 0;
        uintptr_t fautive = 0;
        __try {
            texteDepuisCle(&texte, def + DEF_RES_NOM);
            etape = 1;
            suivante = g_origAjouterCase(support, base + OFF_UI_NOM_CASE, suivante, g_rafr.layouts[k],
                                         &texte, def + DEF_RES_ICONE, quantite, -1, base + OFF_UI_PRESET_CASE);
            etape = 2;
            if (texte) liberer(&texte);
            ++poses;
        }
        __except (FiltreInventaire(GetExceptionInformation(), &code, &fautive)) {
            // Une seule tentative par groupe : ne pas interrompre le jeu a chaque image.
            s.inventaireEchec = true;
            JournalImmediat("[GRP] inventaire : EXCEPTION 0x%08lX a la case \"%s\" (etape %d : %s), adresse Ghidra 0x%llX ; "
                "support 0x%llX, layout 0x%llX, case 0x%llX, definition 0x%llX, texte 0x%llX -- inventaire du slot %d desactive\n",
                code, s.cargaison[i].ressource, etape,
                etape == 0 ? "texte du nom" : etape == 1 ? "ajout de la case" : "liberation du texte",
                (unsigned long long)(fautive - base + 0x140000000ULL), (unsigned long long)support,
                (unsigned long long)g_rafr.layouts[k], (unsigned long long)suivante, (unsigned long long)def,
                (unsigned long long)texte, g_rafr.slot);
            return 0;
        }
        if (!s.inventaireJournalise) {
            uintptr_t cle = 0;
            char nomCle[96] = "?";
            if (SafeRead((void*)(def + DEF_RES_NOM), &cle, 8) && cle > 0x10000) ReadAscii(cle, nomCle, sizeof(nomCle));
            printf("[GRP] inventaire du slot %d : case \"%s\" x%d (definition 0x%llX, nom \"%s\")\n", g_rafr.slot,
                s.cargaison[i].ressource, s.cargaison[i].quantite, (unsigned long long)def, nomCle);
        }
    }
    if (!s.inventaireJournalise) {
        s.inventaireJournalise = true;
        printf("[GRP] inventaire du slot %d : %d case(s) de ressources sur le layout %d (%s)\n", g_rafr.slot,
            poses, k, g_rafr.vu[k] ? "apres les survivants" : "sans survivants");
    }
    return suivante;
}

int LayoutDe(uintptr_t element)
{
    for (int k = 0; k < g_rafr.nLayouts; ++k) if (g_rafr.layouts[k] == element) return k;
    return -1;
}

uintptr_t hkAjouterCase(uintptr_t support, uintptr_t nomCase, uintptr_t caseReutilisee, uintptr_t layout,
                        uintptr_t* texte, uintptr_t tuile, uint64_t quantite, int64_t p8, uintptr_t preset)
{
    uintptr_t r = g_origAjouterCase(support, nomCase, caseReutilisee, layout, texte, tuile, quantite, p8, preset);
    if (g_rafr.actif) {
        int k = LayoutDe(layout);
        if (k >= 0 && !g_rafr.fait[k]) { g_rafr.suivante[k] = r; g_rafr.vu[k] = true; }
    }
    return r;
}

void hkSupprimerReste(uintptr_t premier, uintptr_t nomCase)
{
    if (g_rafr.actif) {
        // Le jeu supprime les restes du layout qu'il vient de remplir : le
        // dernier ou il a pose une case.
        int k = -1;
        for (int j = 0; j < g_rafr.nLayouts; ++j) if (g_rafr.vu[j] && !g_rafr.fait[j]) k = j;
        if (k >= 0 && premier && g_rafr.suivante[k] == premier) {
            g_rafr.fait[k] = true;
            premier = PoserCasesCargaison(k, premier);
        }
    }
    g_origSupprimerReste(premier, nomCase);
}

void hkFinaliser(uintptr_t element)
{
    if (g_rafr.actif) {
        int k = LayoutDe(element);
        if (k >= 0 && !g_rafr.fait[k]) {
            g_rafr.fait[k] = true;
            uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
            uintptr_t suivante = g_rafr.suivante[k];
            if (!g_rafr.vu[k]) suivante = ((UIPremiereCase_t)(base + OFF_UI_PREMIERE_CASE))(element);
            suivante = PoserCasesCargaison(k, suivante);
            if (suivante) g_origSupprimerReste(suivante, base + OFF_UI_NOM_CASE);
        }
    }
    g_origFinaliser(element);
}

// Vague (1..3) d'un groupe de refugies de l'apres-005 PAS ENCORE ARRIVE
// (contexte "StoryModeRefugeesK", comme hkGroupeArrive), 0 sinon. Cache
// par composant (le contexte est une chaine construite).
int VagueRefugiesDuComposant(uintptr_t comp)
{
    if (!ClasseExacte(comp, "FrostpunkSurvivorGroupComponent")) return 0;
    uint8_t arrive = 0;
    SafeRead((void*)(comp + COMP_ARRIVE), &arrive, 1);
    if (arrive) return 0;
    uintptr_t entite = 0;
    if (!SafeRead((void*)(comp + 0x18), &entite, 8) || entite <= 0x10000) return 0;
    struct Connu { uintptr_t comp, entite; int vague; };
    static Connu connus[16];
    static int prochain = 0;
    for (const Connu& c : connus) if (c.comp == comp && c.entite == entite) return c.vague;
    char ctx[256] = "";
    DecrireEntiteContexte(entite, ctx, sizeof(ctx));
    const char* sm = strstr(ctx, "StoryModeRefugees");
    int vague = (sm && sm[17] >= '1' && sm[17] <= '3') ? sm[17] - '0' : 0;
    // The Refugees (2026-11-10) : [OurPeople] 1..10, Lords 11..13 (numero
    // du check, NumeroGroupeRefugees) ; inventaire = item AP comme ci-dessus.
    if (!vague) vague = NumeroGroupeRefugees(comp);
    connus[prochain] = { comp, entite, vague };
    prochain = (prochain + 1) % 16;
    return vague;
}

void hkUICargaison(uintptr_t ctrl)
{
    // Scout : l'infobulle de la case de l'item AP est traduite pendant ce
    // remplissage ; on dit a scooting quelle expedition est affichee.
    uintptr_t poigneeExp = 0, compExp = 0;
    if (SafeRead((void*)(ctrl + CTRL_EXPEDITION), &poigneeExp, 8) && poigneeExp > 0x10000)
        SafeRead((void*)poigneeExp, &compExp, 8);
    ScootingExpeditionAffichee(compExp);

    int slot = -1;
    uintptr_t poignee = 0, comp = 0;
    if (SafeRead((void*)(ctrl + CTRL_GROUPE), &poignee, 8) && poignee && SafeRead((void*)poignee, &comp, 8) && comp)
        for (int i = 0; i < MAX_GROUPES_AP && slot < 0; ++i)
            if (g_groupes[i].actif && !g_groupes[i].arrive && g_groupes[i].comp == comp && g_groupes[i].nCargaison > 0 &&
                !g_groupes[i].inventaireEchec)
                slot = i;
    // Groupe de refugies de l'apres-005, pas encore arrive (2026-11-10,
    // demande de l'auteur) : ses gens sont des items AP ; la liste montre a
    // la place le check qu'il porte (Refugees Arrival k), comme un scout.
    int vague = (slot < 0 && comp) ? VagueRefugiesDuComposant(comp) : 0;
    if ((slot < 0 && !vague) || g_rafr.actif) {
        g_origUICargaison(ctrl);
        ScootingExpeditionAffichee(0);
        return;
    }

    memset(&g_rafr, 0, sizeof(g_rafr));
    g_rafr.slot = slot;
    g_rafr.vague = vague;
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(ctrl + CTRL_LAYOUTS), &tab, 8);
    SafeRead((void*)(ctrl + CTRL_LAYOUTS + 8), &n, 4);
    for (int i = 0; i < n && tab && g_rafr.nLayouts < MAX_LAYOUTS_UI; ++i) {
        uintptr_t p = 0, e = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &p, 8) && p && SafeRead((void*)p, &e, 8) && e)
            g_rafr.layouts[g_rafr.nLayouts++] = e;
    }
    g_rafr.actif = g_rafr.nLayouts > 0;
    // Refugies : leurs gens sont des items AP, retires a l'arrivee
    // (hkGroupeArrive). Test du 2026-11-10 : masques le temps du remplissage
    // seulement, ils restaient visibles dans le panneau de selection de la
    // carte -> retires DES leur 1er affichage (compte du contenu a 0, comme a
    // l'arrivee : meme resultat, aucun ecran ne les montre plus).
    int32_t nContenu = 0;
    if (vague && SafeRead((void*)(comp + COMP_CONTENU + 8), &nContenu, 4) && nContenu > 0) {
        int32_t zero = 0;
        SafeWrite((void*)(comp + COMP_CONTENU + 8), &zero, 4);
        printf("[GRP] refugies du groupe %d : %d entree(s) de survivants retiree(s) des le trajet (items AP)\n",
            vague, nContenu);
    }
    g_origUICargaison(ctrl);
    g_rafr.actif = false;
    ScootingExpeditionAffichee(0);
}

// Groupe de refugies (1..3) survole ou selectionne sur la carte, 0 sinon :
// l'infobulle de la case est traduite AU SURVOL, apres le remplissage
// (comme pour les scouts, ExpeditionVisee : survol +0x2298, puis selection
// +0x2250 {entite, ...} au pas de 0x10, compte +0x2258).
static int VagueDeLEntite(uintptr_t ent)
{
    if (ent <= 0x10000) return 0;
    static uintptr_t comps[64];
    int n = ComposantsDuSysteme(0x6D, "FrostpunkSurvivorGroupComponent", comps, 64);
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(comps[i] + 0x18), &e, 8) && e == ent) return VagueRefugiesDuComposant(comps[i]);
    }
    return 0;
}

int VagueRefugiesVisee()
{
    uintptr_t sel = SystemeParId(IdSysteme::Selection);
    if (!sel) return 0;
    uintptr_t survol = 0;
    if (SafeRead((void*)(sel + 0x2298), &survol, 8)) {
        int v = VagueDeLEntite(survol);
        if (v) return v;
    }
    uintptr_t tab = 0;
    uint32_t n = 0;
    if (!SafeRead((void*)(sel + 0x2250), &tab, 8) || !SafeRead((void*)(sel + 0x2258), &n, 4) || tab <= 0x10000 || n > 64)
        return 0;
    for (uint32_t k = 0; k < n; ++k) {
        uintptr_t ent = 0;
        if (SafeRead((void*)(tab + (uintptr_t)k * 0x10), &ent, 8)) {
            int v = VagueDeLEntite(ent);
            if (v) return v;
        }
    }
    return 0;
}

} // namespace

int GroupesVagueRefugiesAffichee()
{
    return g_vagueAffichee;
}

int GroupesVagueRefugiesVisee()
{
    return VagueRefugiesVisee();
}

void CarteAssurerAcces()
{
    if (!PartiePrete()) return;
    uintptr_t sys = SystemeParId(IdSysteme::WorldMap);
    if (!sys) return;

    unsigned gen = PartieGeneration();
    if (gen != g_carteGeneration) {
        g_carteGeneration = gen;
        g_carteLogs = 0;
        g_carteValide = ClasseExacte(sys, "FrostpunkWorldMapSystem");
        if (!g_carteValide) {
            char cls[64] = "?";
            GetClassNameFP(sys, cls, sizeof(cls));
            printf("[CARTE] systeme 0x%llX de classe %s : acces non force\n", (unsigned long long)sys, cls);
        }
    }
    if (!g_carteValide) return;

    uint8_t deverrouillee = 0;
    int32_t nb = 0;
    int16_t evt = 0;
    if (!SafeRead((void*)(sys + CARTE_DEVERROUILLEE), &deverrouillee, 1) ||
        !SafeRead((void*)(sys + CARTE_NB_BATIMENTS), &nb, 4) ||
        !SafeRead((void*)(sys + CARTE_COMPTEUR_EVT), &evt, 2)) return;
    if (deverrouillee && nb >= 1) return;
    if (evt != 0) return;  // evenement LockedUnlocked en cours

    if (g_carteLogs < MAX_LOGS_CARTE) {
        ++g_carteLogs;
        JournalImmediat("[CARTE] acces force (generation %u) : deverrouillee %d, batiments %d -> %d\n",
            gen, deverrouillee, nb, nb < 1 ? 1 : nb);
    }
    ForcerAccesCarte(sys, !deverrouillee, nb < 1);
}

void GroupesDemanderRapprochement()
{
    InterlockedIncrement(&g_rapprochementsDemandes);
    printf("[GRP] rapprochement demande : execute sur le thread du jeu au prochain mouvement "
           "de ressource (jeu hors pause)\n");
}

void InstallCaptureTemplatesGroupes(uintptr_t modBase)
{
    void* ajoute = (void*)(modBase + OFF_COMPOSANT_AJOUTE);
    bool okAjoute = MH_CreateHook(ajoute, (void*)&hkComposantAjoute, (void**)&g_origComposantAjoute) == MH_OK &&
                    MH_EnableHook(ajoute) == MH_OK;
    printf("[CAPGRP] hook composant de groupe ajoute (+%llX) %s\n",
        (unsigned long long)OFF_COMPOSANT_AJOUTE, okAjoute ? "OK" : "KO");
    void* arrive = (void*)(modBase + OFF_GROUPE_ARRIVE);
    bool okArrive = MH_CreateHook(arrive, (void*)&hkGroupeArrive, (void**)&g_origGroupeArrive) == MH_OK &&
                    MH_EnableHook(arrive) == MH_OK;
    printf("[CAPGRP] hook groupe arrive en ville (+%llX) %s\n",
        (unsigned long long)OFF_GROUPE_ARRIVE, okArrive ? "OK" : "KO");

    // Inventaire des groupes AP dans la liste de droite (cargaison).
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } hooksUI[] = {
        { OFF_UI_CARGAISON,       (void*)&hkUICargaison,    (void**)&g_origUICargaison,    "inventaire de la liste" },
        { OFF_UI_AJOUTER_CASE,    (void*)&hkAjouterCase,    (void**)&g_origAjouterCase,    "ajout d'une case" },
        { OFF_UI_SUPPRIMER_RESTE, (void*)&hkSupprimerReste, (void**)&g_origSupprimerReste, "suppression des restes" },
        { OFF_UI_FINALISER,       (void*)&hkFinaliser,      (void**)&g_origFinaliser,      "finalisation d'un layout" },
    };
    for (auto& h : hooksUI) {
        void* p = (void*)(modBase + h.off);
        bool ok = MH_CreateHook(p, h.hk, h.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[CAPGRP] hook %s (+%llX) %s\n", h.nom, (unsigned long long)h.off, ok ? "OK" : "KO");
    }

    uintptr_t vt[4];
    int nvt = VtablesParNomDeClasse("FrostpunkSurvivorGroupComponentTemplate", vt, 4);
    int nf = 0;
    uintptr_t fonctions[MAX_FONCTIONS_TPL];
    for (int v = 0; v < nvt && nf < MAX_FONCTIONS_TPL; ++v)
        nf += FonctionsChargeantVtable(vt[v], fonctions + nf, MAX_FONCTIONS_TPL - nf);
    if (nf == 0) { printf("[CAPGRP] aucune fonction trouvee (vtables : %d)\n", nvt); return; }
    for (int i = 0; i < nf; ++i) {
        void* p = (void*)fonctions[i];
        bool ok = MH_CreateHook(p, g_hkTpl[i], (void**)&g_origTpl[i]) == MH_OK &&
                  MH_EnableHook(p) == MH_OK;
        printf("[CAPGRP] hook +%llX (Ghidra 0x%llX) %s\n",
            (unsigned long long)(fonctions[i] - modBase),
            (unsigned long long)(fonctions[i] - modBase + 0x140000000ULL), ok ? "OK" : "KO");
    }
}

void DiagGroupes()
{
    RapportTemplates();
    RapportArbre();
    if (!PartiePrete()) { printf("[GRP] partie pas prete\n"); return; }
    ReleverGroupes("F9");

    uintptr_t e = 0, comp = 0;
    if (!PremierGroupe(&e, &comp)) { printf("[GRP] aucun groupe sur la carte\n"); return; }

    uintptr_t tplCompose = 0, tplComp = 0, p48 = 0, p80 = 0;
    SafeRead((void*)(e + ENT_TEMPLATE), &tplCompose, 8);
    SafeRead((void*)(comp + 0x20), &tplComp, 8);
    SafeRead((void*)(comp + 0x48), &p48, 8);
    SafeRead((void*)(comp + 0x80), &p80, 8);
    printf("[GRP] groupe : entite 0x%llX, composant 0x%llX, template 0x%llX, CompoundTemplate 0x%llX\n",
        (unsigned long long)e, (unsigned long long)comp,
        (unsigned long long)tplComp, (unsigned long long)tplCompose);
    DecrireContenu(comp);

    // Ou le CompoundTemplate range-t-il ses templates de composant ?
    static bool compoundDecrit = false;
    if (!compoundDecrit) {
        DecrireChamps(tplCompose, 0x100, "CompoundTemplate du groupe");
        compoundDecrit = true;
    }

    if (g_nZones == 0) {
        g_zones[0] = { "composant", 0, 0xA0 };
        g_zones[1] = { "comp+0x48", 0, 0x100 };
        g_zones[2] = { "comp+0x80", 0, 0x100 };
        g_zones[3] = { "transform", 0, 0x80 };
        g_nZones = 4;
    }
    ComparerZone(g_zones[0], comp);
    ComparerZone(g_zones[1], p48);
    ComparerZone(g_zones[2], p80);
    ComparerZone(g_zones[3], e + ENT_TRANSFORM);
}

int GroupeCreer(const GroupeAP& g)
{
    int slot = -1;
    for (int i = 0; i < MAX_GROUPES_AP && slot < 0; ++i) if (!g_groupes[i].actif || g_groupes[i].arrive) slot = i;
    if (slot < 0) { printf("[GRP] plus de slot libre (%d groupes suivis)\n", MAX_GROUPES_AP); return -1; }

    uintptr_t stub = 0;
    uintptr_t tpl = ObtenirTemplateParChemin(g.cheminTemplate, &stub);
    if (!tpl || !ClasseExacte(tpl, "CompoundTemplate")) {
        printf("[GRP] template \"%s\" introuvable (stub 0x%llX, template 0x%llX)\n", g.cheminTemplate,
            (unsigned long long)stub, (unsigned long long)tpl);
        return -1;
    }

    // Pose sur le premier site, sous le meme parent (la scene de la carte) :
    // le point de depart n'importe pas, la duree est fixee ensuite.
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t gestionnaire = 0, parent = 0;
    uintptr_t source = EntiteDuSysteme(IdSysteme::ExpeditionSite, "FrostpunkExpeditionSiteComponent");
    SafeRead((void*)(base + OFF_GESTIONNAIRE), &gestionnaire, 8);
    if (source) SafeRead((void*)(source + ENT_PARENT), &parent, 8);
    alignas(16) float m[16] = {};
    if (!gestionnaire || !parent || !SafeRead((void*)(source + ENT_TRANSFORM), m, sizeof(m))) {
        printf("[GRP] references de pose absentes (site 0x%llX, parent 0x%llX, gestionnaire 0x%llX)\n",
            (unsigned long long)source, (unsigned long long)parent, (unsigned long long)gestionnaire);
        return -1;
    }

    // Les textes doivent exister avant Creer : le jeu peut les traduire tout de suite.
    GroupeSuivi& s = g_groupes[slot];
    memset(&s, 0, sizeof(s));
    strncpy_s(s.nom, g.nom ? g.nom : "", _TRUNCATE);
    strncpy_s(s.description, g.description ? g.description : "", _TRUNCATE);
    strncpy_s(s.dilemmeTitre, g.dilemmeTitre ? g.dilemmeTitre : s.nom, _TRUNCATE);
    strncpy_s(s.dilemmeTexte, g.dilemmeTexte ? g.dilemmeTexte : s.description, _TRUNCATE);
    strncpy_s(s.dilemmeChoix, g.dilemmeChoix ? g.dilemmeChoix : "Welcome them", _TRUNCATE);
    strncpy_s(s.icone, g.icone ? g.icone : "icon_vanilla_groupe", _TRUNCATE);
    strncpy_s(s.expediteur, g.expediteur ? g.expediteur : "", _TRUNCATE);
    s.heures = g.heures;
    s.dureeAppliquee = g.heures <= 0;
    s.surArrivee = g.surArrivee;
    for (int i = 0; i < MAX_CARGAISON_AP && g.cargaison[i].ressource; ++i) {
        if (g.cargaison[i].quantite <= 0) continue;
        strncpy_s(s.cargaison[s.nCargaison].ressource, g.cargaison[i].ressource, _TRUNCATE);
        s.cargaison[s.nCargaison].quantite = g.cargaison[i].quantite;
        s.nCargaison++;
    }
    s.actif = true;

    // Le donneur est charge AVANT Creer (chargement synchrone hors du hook).
    PreparerSurvivants(g);

    g_compClone = 0;
    g_slotAPoser = slot;
    uintptr_t e = AppelerCreer(gestionnaire, tpl, parent, m);
    g_slotAPoser = -1;
    g_survivantsAPoser = 0;
    g_nSurvivantsAPoser = 1;
    if (!e) { s.actif = false; return -1; }
    s.entite = e;
    DecrireEntite("cree", e);

    if (g_compClone) s.comp = g_compClone;
    else {
        printf("[GRP] le hook du composant ajoute n'a pas vu ce groupe : textes non personnalises\n");
        uintptr_t comps[64];
        int n = ComposantsDuSysteme(ID_SURVIVOR_GROUP, "FrostpunkSurvivorGroupComponent", comps, 64);
        for (int i = 0; i < n && !s.comp; ++i) {
            uintptr_t ent = 0;
            if (SafeRead((void*)(comps[i] + 0x18), &ent, 8) && ent == e) s.comp = comps[i];
        }
        if (!s.comp) { printf("[GRP] composant du groupe introuvable\n"); s.actif = false; return -1; }
    }

    // Par classe : comptes deja poses dans les entrees de la copie du template.
    AppliquerContenu(s.comp, g.classes[0].classe ? -1 : g.survivants, g.sansMorts);
    uintptr_t dilemme = 0;
    SafeRead((void*)(s.comp + COMP_DILEMME_ARRIVEE), &dilemme, 8);
    printf("[GRP] dilemme d'arrivee apres Creer : 0x%llX%s\n", (unsigned long long)dilemme,
        dilemme ? " (REPOSE par le jeu apres le hook)" : "");

    uint8_t init = 0;
    SafeRead((void*)(s.comp + COMP_INITIALISE), &init, 1);
    if (!s.dureeAppliquee) {
        if (init) {
            ReglerDuree(e, s.heures, s.clone);
            s.dureeAppliquee = true;
        }
        else printf("[GRP] groupe pas encore en marche : duree appliquee a son depart\n");
    }
    JournalImmediat("[GRP] groupe Archipelago slot %d : entite 0x%llX, composant 0x%llX, copie 0x%llX, \"%s\"\n",
        slot, (unsigned long long)e, (unsigned long long)s.comp, (unsigned long long)s.clone, s.nom);
    ReleverUnGroupe("cree", s.comp);
    // Notre sauvegarde : identifiant de l'entite (garde par la sauvegarde du jeu).
    s.sansMorts = g.sansMorts;
    if (SafeRead((void*)(e + ENT_ID), s.guid, sizeof(s.guid))) SauverGroupe(s);
    else printf("[SAV] identifiant de l'entite illisible : groupe non enregistre\n");
    return slot;
}

const char* GroupeTexte(int slot, char champ)
{
    if (slot < 0 || slot >= MAX_GROUPES_AP || !g_groupes[slot].actif) return nullptr;
    const GroupeSuivi& s = g_groupes[slot];
    switch (champ) {
    case 'N': return s.nom;
    case 'D': return s.description;
    case 'T': return s.dilemmeTitre;
    case 'X': return s.dilemmeTexte;
    case 'C': return s.dilemmeChoix;
    default:  return nullptr;
    }
}

void GroupesAvantChargement()
{
    g_chargementEnCours = true;
    g_chargementReinit = false;
    g_lectureFaite = false;
    g_nCompsConstruits = 0;
}

void GroupesComposantConstruit(uintptr_t comp)
{
    if (g_chargementEnCours && g_nCompsConstruits < 64) g_compsConstruits[g_nCompsConstruits++] = comp;
}

bool DejaRecolle(uintptr_t comp)
{
    for (const GroupeSuivi& s : g_groupes) if (s.actif && s.comp == comp) return true;
    return false;
}

// Recolle un groupe s'il est des notres et pas encore arrive. 'quand' : log.
void RecollerSiNotre(uintptr_t comp, const char* quand)
{
    uintptr_t e = 0, tpl = 0;
    uint8_t arrive = 0;
    if (!ClasseExacte(comp, "FrostpunkSurvivorGroupComponent") || !SafeRead((void*)(comp + 0x18), &e, 8) ||
        e <= 0x10000 || !SafeRead((void*)(comp + COMP_TEMPLATE), &tpl, 8) ||
        !ClasseExacte(tpl, "FrostpunkSurvivorGroupComponentTemplate")) {
        printf("[SAV] (%s) composant 0x%llX pas encore complet (entite 0x%llX, template 0x%llX)\n", quand,
            (unsigned long long)comp, (unsigned long long)e, (unsigned long long)tpl);
        return;
    }
    SafeRead((void*)(comp + COMP_ARRIVE), &arrive, 1);
    if (arrive || DejaRecolle(comp)) return;   // deja arrive en ville : plus rien a livrer
    int slot = RecollerGroupe(e, comp);
    if (slot >= 0) {
        printf("[SAV]   (%s)\n", quand);
        MettreAJourCarte(e, comp);
    }
}

// Fin de la lecture de la sauvegarde. Test du 2026-10-29 : recoller a la
// sortie de LoadGame est trop tard pour l'icone de la CARTE (le marqueur la
// lit a l'activation des groupes, entre la fin de la lecture et la sortie de
// LoadGame) ; la liste a droite, elle, suit. Les groupes ne sont pas encore
// inscrits dans leur systeme : on prend les composants construits pendant
// le chargement (hook du constructeur, mod_common).
void GroupesLectureFaite()
{
    g_lectureFaite = true;
    if (!g_chargementEnCours) return;
    printf("[SAV] fin de la lecture : %d composant(s) de groupe construit(s)\n", g_nCompsConstruits);
    if (!g_chargementReinit) { memset(g_groupes, 0, sizeof(g_groupes)); g_chargementReinit = true; }
    for (int i = 0; i < g_nCompsConstruits; ++i) RecollerSiNotre(g_compsConstruits[i], "fin de la lecture");
}

// Sortie de LoadGame : filet pour les groupes que la fin de la lecture n'a
// pas pu traiter (composant incomplet), puis finalisation. Seulement si la
// sauvegarde a vraiment ete lue (LoadGame est aussi appele une 2e fois, a vide).
void GroupesApresChargement()
{
    g_chargementEnCours = false;
    if (!g_lectureFaite) return;
    g_lectureFaite = false;
    if (!g_chargementReinit) { memset(g_groupes, 0, sizeof(g_groupes)); g_chargementReinit = true; }
    {
        uintptr_t comps[64];
        int n = ComposantsDuSysteme(ID_SURVIVOR_GROUP, "FrostpunkSurvivorGroupComponent", comps, 64);
        for (int i = 0; i < n; ++i) RecollerSiNotre(comps[i], "sortie de LoadGame");
    }
    g_chargementReinit = false;
    int recolles = 0;
    for (GroupeSuivi& s : g_groupes) {
        if (!s.actif || !s.recolle) continue;
        ++recolles;
        // Taux de mort du groupe : copie du template a l'init du composant.
        if (s.sansMorts) {
            float zero = 0;
            SafeWrite((void*)(s.comp + COMP_TAUX_MORT), &zero, 4);
        }
    }
    JournalImmediat("[SAV] chargement termine : %d groupe(s) AP recolle(s)\n", recolles);
    ReleverGroupes("sortie de LoadGame");
}

float GroupesHeuresAvantDerniereArrivee()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t sysMouv = 0, horloge = 0;
    int32_t secParHeure = 0;
    int64_t maintenant = 0;
    SafeRead((void*)(base + OFF_SYS_MOUVEMENT), &sysMouv, 8);
    SafeRead((void*)(base + OFF_HORLOGE), &horloge, 8);
    SafeRead((void*)(base + OFF_SECONDES_PAR_HEURE), &secParHeure, 4);
    if (!sysMouv || !horloge || secParHeure <= 0 || !SafeRead((void*)(horloge + HORLOGE_DATE), &maintenant, 8))
        return 0.0f;
    double tph = secParHeure * TICKS_PAR_SECONDE;
    float plusTard = 0.0f;
    for (const GroupeSuivi& s : g_groupes) {
        if (!s.actif || s.arrive || s.entite <= 0x10000) continue;
        // Duree pas encore posee : celle qui le sera.
        if (!s.dureeAppliquee) { if (s.heures > plusTard) plusTard = s.heures; continue; }
        uintptr_t mouv = 0;
        int64_t arrivee = 0;
        __try { mouv = ((MouvementDe_t)(base + OFF_MOUVEMENT_DE))(sysMouv, s.entite); }
        __except (EXCEPTION_EXECUTE_HANDLER) { mouv = 0; }
        if (!mouv || !SafeRead((void*)(mouv + MOUV_DATE_ARRIVEE), &arrivee, 8)) continue;
        float restant = (float)((double)(arrivee - maintenant) / tph);
        if (restant > plusTard) plusTard = restant;
    }
    return plusTard;
}

void GroupesDemanderCreation()
{
    InterlockedIncrement(&g_creationsDemandees);
    printf("[GRP] creation demandee : executee sur le thread du jeu au prochain mouvement "
           "de ressource (jeu hors pause)\n");
}

void GroupesPompe()
{
    // Partie pas prete (chargement en cours) : les demandes restent en
    // attente au lieu d'etre perdues.
    if (!PartiePrete()) return;
    SuivreGroupes();
    if (g_creationsDemandees <= 0 && g_rapprochementsDemandes <= 0) return;
    bool creer = InterlockedExchange(&g_creationsDemandees, 0) > 0;
    bool rapprocher = InterlockedExchange(&g_rapprochementsDemandes, 0) > 0;
    if (creer) CreerGroupeDeTest();
    if (rapprocher) RapprocherPremierGroupe();
}
