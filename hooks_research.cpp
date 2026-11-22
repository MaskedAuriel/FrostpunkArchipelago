// hooks_research.cpp
#include "hooks_research.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "tech_brain.h"
#include "hooks_loc.h"
#include "known_techs.h"   // TechByHash (journal [TECHPRE])
#include "MinHook.h"


// --- Pointeurs vers les originaux (internes a ce fichier) -------------------
typedef char (*CanResearch_t)(uintptr_t self, uintptr_t tech, uintptr_t ctx);
// ResearchStart (FUN_141329610) : 3e argument (r8) = liste d'erreurs de
// l'appelant, transmise telle quelle a CanResearch (desassemble le
// 2026-11-11). 4 arguments declares (regle n. 5).
typedef uintptr_t (*ResearchStart_t)(uintptr_t self, uintptr_t tech, uintptr_t erreurs, uintptr_t p4);
typedef void (*ResearchComplete_t)(uintptr_t self);
typedef char (*Cancel_t)(uintptr_t self);

static CanResearch_t      oCanResearch      = nullptr;
static ResearchStart_t    oResearchStart    = nullptr;
static ResearchComplete_t oResearchComplete = nullptr;
static Cancel_t           oCancel           = nullptr;

// --- AddResearchedTechnology (FUN_141325e40, decompile le 2026-11-04) -------
// (systeme, techno, p3) : si l'etat n'est pas deja 2, etat = 2, effets de la
// techno (FUN_141326650 : modificateurs...) puis ecouteurs de l'evenement
// "techno recherchee" (systeme +0x2108, compteur +0x2120). ResearchComplete
// (FUN_141327120) remet l'etat a 0 puis l'appelle.
// Bug du 2026-11-04 (ScoutsLimit recue, pas recherchee : pas de 2e equipe) :
// le mod ecrivait l'etat 2 a la reception, ce qui debloque les batiments
// (lus sur l'etat) mais n'applique NI les effets NI les ecouteurs. Desormais
// la reception passe par cette fonction (TechAppliquerEffets), et une
// recherche terminee d'une techno deja recue ne la rappelle pas (effets
// appliques une seule fois).
typedef uintptr_t (*AjouterRecherchee_t)(uintptr_t self, uintptr_t tech, uintptr_t p3, uintptr_t p4);
static AjouterRecherchee_t oAjouterRecherchee = nullptr;
static const uintptr_t OFF_AJOUTER_RECHERCHEE = Adr(0x1325E40);
static thread_local uintptr_t t_completionTech = 0;   // techno de la ResearchComplete en cours

static uintptr_t hkAjouterRecherchee(uintptr_t self, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    if (tech && tech == t_completionTech) {
        uint64_t h = HashOfTechPtr(tech);
        TechRuntime* r = h ? RT_Get(h) : nullptr;
        if (r && r->received && TechEstUnCheck(h)) {
            // Effets deja appliques a la reception : pas une 2e fois. L'etat
            // final (2) est pose par OnResearchComplete (SyncGameState).
            printf("[AP] recherche d'une techno deja recue : effets non reappliques\n");
            return 1;                  // "AddResearchedTechnology should succeed"
        }
        // Recherche d'une techno AP pas encore recue. Bug du 2026-11-21 (On the
        // Edge, auteur : effets de "R VaultUpgrade1" sur les handcarts actifs
        // sans l'item) : l'original etait appele avec le 3e argument a 0 (pas
        // de bleu, 2026-11-07), mais il applique quand meme les effets
        // (FUN_141326650). Desormais rien n'est fait : etat laisse a 0 (remis
        // par ResearchComplete), ni effets ni ecouteurs "techno recherchee" ;
        // tout vient a la reception (OnItemReceived -> TechAppliquerEffets,
        // 3e argument 1 : bleu a la reception). L'evenement "recherche
        // terminee" (+0x2160) est emis par ResearchComplete apres ce retour.
        if (r && TechEstUnCheck(h)) {
            printf("[AP] recherche d'une techno pas encore recue : effets retenus jusqu'a l'item\n");
            return 1;                  // "AddResearchedTechnology should succeed"
        }
    }
    return oAjouterRecherchee(self, tech, p3, p4);
}

// --- Modificateurs globaux refuses a la reception (2026-11-22) ---------------
// Bug (On the Edge, auteur) : bonus d'efficacite de l'army warehouse absent,
// technos Vault recues et appliquees (journal "effets appliques"). Les effets
// d'une techno (FUN_141326650) ajoutent ses modificateurs globaux par
// FUN_14179d480(systeme des modificateurs, modificateur), qui evalue d'abord
// les CONDITIONS du modificateur (FUN_14179d9e0 : "Evaluating modifier
// conditions...") et renvoie 0 sans rien garder si elles sont fausses. En
// vanilla une techno Vault n'est recherchee qu'apres la decouverte de
// l'entrepot (condition d'affichage "unlock [Reconquest] SteamCores") ; les
// items AP l'appliquent des la premiere seconde de la partie. Hypothese : le
// modificateur est refuse a ce moment et n'est jamais retente. Le hook note
// chaque modificateur global refuse pendant l'application d'une techno AP ;
// TechModifsEnAttentePompe (thread du jeu) le retente toutes les 3 s.
// Un modificateur deja present n'est pas cumule au-dela de son maximum
// d'empilement (FUN_14179d9e0), mais on ne retente QUE les refuses.
// Steam : 0x1638480 (octets identiques a 96 %, 2026-11-22).
typedef uintptr_t (*AjouterModifGlobal_t)(uintptr_t systeme, uintptr_t modif, uintptr_t p3, uintptr_t p4);
static AjouterModifGlobal_t oAjouterModifGlobal = nullptr;
static const uintptr_t OFF_AJOUTER_MODIF_GLOBAL = Adr(0x179D480);
static thread_local uint64_t t_techEffets = 0;     // techno AP en cours d'application

struct ModifEnAttente { uintptr_t systeme, modif; uint64_t hash; };
static const int MAX_MODIFS_ATTENTE = 64;
static ModifEnAttente g_modifsAttente[MAX_MODIFS_ATTENTE];
static int g_nModifsAttente = 0;                   // thread du jeu seulement
static volatile long g_viderModifs = 0;            // demande du thread du mod (nouvelle partie)
static void ViderSiNouvellePartie();               // vide aussi les files des modificateurs d'entite

static const char* NomTechno(uint64_t h)
{
    const TechInfo* t = TechByHash(h);
    return t ? t->name : "?";
}

static uintptr_t hkAjouterModifGlobal(uintptr_t systeme, uintptr_t modif, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oAjouterModifGlobal(systeme, modif, p3, p4);
    if (t_techEffets) {
        bool accepte = (r & 0xFF) != 0;
        char classe[96] = "?";
        GetClassNameFP(modif, classe, sizeof(classe));
        printf("[TECH] %s : modificateur global 0x%llX (%s) %s\n", NomTechno(t_techEffets),
               (unsigned long long)modif, classe, accepte ? "applique" : "REFUSE (conditions ?) : sera retente");
        // File de l'ancienne partie videe avant le premier ajout de la nouvelle
        // (la pompe passe apres la livraison des items).
        ViderSiNouvellePartie();
        if (!accepte && g_nModifsAttente < MAX_MODIFS_ATTENTE) {
            bool deja = false;
            for (int k = 0; k < g_nModifsAttente; ++k)
                if (g_modifsAttente[k].modif == modif) { deja = true; break; }
            if (!deja) g_modifsAttente[g_nModifsAttente++] = { systeme, modif, t_techEffets };
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// 2026-11-22, suite : aucune des 43 technos recues d'On the Edge n'a ajoute de
// modificateur GLOBAL (journal). Leurs effets passent par les modificateurs
// d'ENTITE (tech +0xD8, entrees de 0x20 = {gabarit, modificateur}) :
// FUN_14179d140(systeme, entree, modif) inscrit le modificateur pour le
// gabarit (entites futures) puis l'applique a chaque entite existante de ce
// gabarit par FUN_14179d240(systeme, entite, modif), qui evalue lui aussi les
// CONDITIONS du modificateur (FUN_14179d9e0) et renvoie 0 sans rien garder si
// elles sont fausses. Une entite deja presente (l'entrepot d'On the Edge, sur
// la carte des le depart, fouille plus tard : "E: Vault searched") n'est
// jamais reessayee. Les hooks notent les modificateurs d'entite des technos
// AP, journalisent chaque application (aussi celles que le jeu fait plus tard
// pour une entite nouvelle) et retentent les refus toutes les 3 s, tant que
// l'entite est encore dans la liste du systeme (+0x2060, nombre +0x2068).
// Steam : 0x1638140 (octets identiques a 100 %), 0x1638240 (90 %).
typedef uintptr_t (*InscrireModifEntite_t)(uintptr_t systeme, uintptr_t entree, uintptr_t modif, uintptr_t p4);
typedef uintptr_t (*AppliquerModifEntite_t)(uintptr_t systeme, uintptr_t entite, uintptr_t modif, uintptr_t p4);
static InscrireModifEntite_t  oInscrireModifEntite  = nullptr;
static AppliquerModifEntite_t oAppliquerModifEntite = nullptr;
static const uintptr_t OFF_INSCRIRE_MODIF_ENTITE  = Adr(0x179D140);
static const uintptr_t OFF_APPLIQUER_MODIF_ENTITE = Adr(0x179D240);

struct ModifTechno { uintptr_t modif; uint64_t hash; };
static const int MAX_MODIFS_TECHNOS = 256;
static ModifTechno g_modifsTechnos[MAX_MODIFS_TECHNOS];     // modificateurs d'entite des technos AP
static int g_nModifsTechnos = 0;                            // thread du jeu seulement

struct ModifEntiteEnAttente { uintptr_t systeme, entite, modif; uint64_t hash; };
static const int MAX_MODIFS_ENTITE_ATTENTE = 128;
static ModifEntiteEnAttente g_modifsEntiteAttente[MAX_MODIFS_ENTITE_ATTENTE];
static int g_nModifsEntiteAttente = 0;                      // thread du jeu seulement
static thread_local int t_entitesVues = 0;                  // entites traitees par l'inscription en cours

// ---------------------------------------------------------------------------
// 2026-11-22, 3e canal : journal de 15:38 -> seule R VaultInsulation a un
// modificateur "Entities of type" (applique a l'entrepot, gabarit AAAF...).
// Les autres technos Vault n'ont rien journalise ; or une techno n'a que 3
// listes d'effets (FrostpunkTechnologyEntry : +0xC0 Global modifiers, +0xD8
// Entities of type modifiers, +0xF0 Buildings of category modifiers). Reste
// donc +0xF0 : FUN_14179d030(systeme, categorie, modif) range le modificateur
// dans la table des categories (systeme +0x23C8) puis l'applique
// (FUN_14179d240) a chaque batiment du systeme des batiments (global
// DAT_142c3c408, liste +0x2060, nombre +0x2068) de cette categorie
// (FUN_1412ef5f0(systemeBatiments, batiment, categorie) : liste des
// categories du gabarit du batiment). Aucun journal pour ces technos => aucun
// batiment de la categorie a la reception (l'entrepot n'etait pas encore un
// batiment de cette categorie / pas encore fouille). Rattrapage : toutes les
// 3 s, chaque batiment de la categorie qui n'a pas encore le modificateur le
// recoit (FUN_14179d240). Paires {batiment, modif} acceptees notees (aussi
// celles que le jeu applique lui-meme : hook de FUN_14179d240) : jamais deux
// fois. Steam : FUN_14179d030 0x1638030 (97 %), FUN_1412ef5f0 0x1189EB0
// (86 %), DAT_142c3c408 0x2B992B8 (meme decalage de l'instruction).
typedef uintptr_t (*InscrireModifCategorie_t)(uintptr_t systeme, uintptr_t categorie, uintptr_t modif, uintptr_t p4);
typedef uintptr_t (*BatimentDeCategorie_t)(uintptr_t systemeBat, uintptr_t batiment, uintptr_t categorie, uintptr_t p4);
static InscrireModifCategorie_t oInscrireModifCategorie = nullptr;
static const uintptr_t OFF_INSCRIRE_MODIF_CATEGORIE = Adr(0x179D030);
static const uintptr_t OFF_BATIMENT_DE_CATEGORIE    = Adr(0x12EF5F0);
static const uintptr_t OFF_SYSTEME_BATIMENTS        = Adr(0x2C3C408);
static uintptr_t g_baseRecherche = 0;

struct ModifCategorie { uintptr_t systeme, categorie, modif; uint64_t hash; };
static const int MAX_MODIFS_CATEGORIE = 128;
static ModifCategorie g_modifsCategorie[MAX_MODIFS_CATEGORIE];
static int g_nModifsCategorie = 0;                          // thread du jeu seulement

struct PaireModif { uintptr_t entite, modif; };
static const int MAX_PAIRES = 2048;
static PaireModif g_pairesAppliquees[MAX_PAIRES];           // acceptees (par nous ou par le jeu)
static int g_nPairesAppliquees = 0;
static PaireModif g_pairesRefusNote[MAX_PAIRES];            // refus deja journalises par le rattrapage
static int g_nPairesRefusNote = 0;

static bool PaireDans(const PaireModif* t, int n, uintptr_t entite, uintptr_t modif)
{
    for (int k = 0; k < n; ++k)
        if (t[k].entite == entite && t[k].modif == modif) return true;
    return false;
}

static void NoterPaire(PaireModif* t, int& n, uintptr_t entite, uintptr_t modif)
{
    if (n < MAX_PAIRES && !PaireDans(t, n, entite, modif)) t[n++] = { entite, modif };
}

static void ViderSiNouvellePartie()
{
    if (!InterlockedExchange(&g_viderModifs, 0)) return;
    if (g_nModifsAttente || g_nModifsEntiteAttente || g_nModifsCategorie)
        printf("[TECH] nouvelle partie : %d modificateur(s) global(aux), %d d'entite en attente et %d de categorie oublie(s)\n",
               g_nModifsAttente, g_nModifsEntiteAttente, g_nModifsCategorie);
    g_nModifsAttente = 0;
    g_nModifsEntiteAttente = 0;
    g_nModifsTechnos = 0;
    g_nModifsCategorie = 0;
    g_nPairesAppliquees = 0;
    g_nPairesRefusNote = 0;
}

static uint64_t TechnoDuModif(uintptr_t modif)
{
    for (int k = 0; k < g_nModifsTechnos; ++k)
        if (g_modifsTechnos[k].modif == modif) return g_modifsTechnos[k].hash;
    return 0;
}

static int TypeDuModif(uintptr_t modif)
{
    int type = -1;
    __try { type = ((int (*)(uintptr_t))(*(uintptr_t**)modif)[0xE0 / 8])(modif); }
    __except (EXCEPTION_EXECUTE_HANDLER) { type = -1; }
    return type;
}

static uintptr_t hkInscrireModifEntite(uintptr_t systeme, uintptr_t entree, uintptr_t modif, uintptr_t p4)
{
    if (!t_techEffets) return oInscrireModifEntite(systeme, entree, modif, p4);
    ViderSiNouvellePartie();
    if (!TechnoDuModif(modif) && g_nModifsTechnos < MAX_MODIFS_TECHNOS)
        g_modifsTechnos[g_nModifsTechnos++] = { modif, t_techEffets };
    int nEntites = 0;
    SafeRead((const void*)(systeme + 0x2068), &nEntites, sizeof(nEntites));
    t_entitesVues = 0;
    uintptr_t r = oInscrireModifEntite(systeme, entree, modif, p4);
    char classe[96] = "?";
    GetClassNameFP(modif, classe, sizeof(classe));
    uint64_t gab[2] = { 0, 0 };
    SafeRead((const void*)entree, gab, sizeof(gab));
    printf("[TECH] %s : modificateur d'entite 0x%llX (%s, type %d) inscrit pour le gabarit %016llX%016llX : %d entite(s) existante(s) de ce gabarit sur %d\n",
           NomTechno(t_techEffets), (unsigned long long)modif, classe, TypeDuModif(modif),
           (unsigned long long)gab[0], (unsigned long long)gab[1], t_entitesVues, nEntites);
    return r;
}

static uintptr_t hkAppliquerModifEntite(uintptr_t systeme, uintptr_t entite, uintptr_t modif, uintptr_t p4)
{
    uintptr_t r = oAppliquerModifEntite(systeme, entite, modif, p4);
    uint64_t hash = t_techEffets ? t_techEffets : TechnoDuModif(modif);
    if (!hash) return r;
    if (t_techEffets) ++t_entitesVues;
    bool accepte = (r & 0xFF) != 0;
    if (accepte) NoterPaire(g_pairesAppliquees, g_nPairesAppliquees, entite, modif);
    char classe[96] = "?";
    GetClassNameFP(entite, classe, sizeof(classe));
    printf("[TECH] %s : modificateur d'entite 0x%llX sur l'entite 0x%llX (%s) %s%s\n",
           NomTechno(hash), (unsigned long long)modif, (unsigned long long)entite, classe,
           accepte ? "applique" : "REFUSE (conditions ?) : sera retente",
           t_techEffets ? "" : " [entite nouvelle]");
    if (!accepte && g_nModifsEntiteAttente < MAX_MODIFS_ENTITE_ATTENTE) {
        bool deja = false;
        for (int k = 0; k < g_nModifsEntiteAttente; ++k)
            if (g_modifsEntiteAttente[k].modif == modif && g_modifsEntiteAttente[k].entite == entite) { deja = true; break; }
        if (!deja) g_modifsEntiteAttente[g_nModifsEntiteAttente++] = { systeme, entite, modif, hash };
    }
    return r;
}

// L'entite est-elle encore dans la liste du systeme des modificateurs ?
static bool EntiteToujoursLa(uintptr_t systeme, uintptr_t entite)
{
    uintptr_t liste = 0;
    int n = 0;
    if (!SafeRead((const void*)(systeme + 0x2060), &liste, sizeof(liste)) || !SafeRead((const void*)(systeme + 0x2068), &n, sizeof(n))) return false;
    if (!liste || n <= 0 || n > 1000000) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (!SafeRead((const void*)(liste + (uintptr_t)i * 8), &e, sizeof(e))) return false;
        if (e == entite) return true;
    }
    return false;
}

static uintptr_t hkInscrireModifCategorie(uintptr_t systeme, uintptr_t categorie, uintptr_t modif, uintptr_t p4)
{
    if (!t_techEffets) return oInscrireModifCategorie(systeme, categorie, modif, p4);
    ViderSiNouvellePartie();
    if (!TechnoDuModif(modif) && g_nModifsTechnos < MAX_MODIFS_TECHNOS)
        g_modifsTechnos[g_nModifsTechnos++] = { modif, t_techEffets };
    bool deja = false;
    for (int k = 0; k < g_nModifsCategorie; ++k)
        if (g_modifsCategorie[k].modif == modif && g_modifsCategorie[k].categorie == categorie) { deja = true; break; }
    if (!deja && g_nModifsCategorie < MAX_MODIFS_CATEGORIE)
        g_modifsCategorie[g_nModifsCategorie++] = { systeme, categorie, modif, t_techEffets };
    t_entitesVues = 0;
    uintptr_t r = oInscrireModifCategorie(systeme, categorie, modif, p4);
    char classe[96] = "?", classeCat[96] = "?";
    GetClassNameFP(modif, classe, sizeof(classe));
    if (categorie) GetClassNameFP(categorie, classeCat, sizeof(classeCat));
    printf("[TECH] %s : modificateur de categorie 0x%llX (%s, type %d) inscrit pour la categorie 0x%llX (%s) : %d batiment(s) de cette categorie a la reception\n",
           NomTechno(t_techEffets), (unsigned long long)modif, classe, TypeDuModif(modif),
           (unsigned long long)categorie, classeCat, t_entitesVues);
    return r;
}

// Rattrapage des modificateurs de categorie (thread du jeu).
static void RattraperModifsCategorie()
{
    if (!g_nModifsCategorie || !oAppliquerModifEntite || !g_baseRecherche) return;
    uintptr_t sysBat = 0, liste = 0;
    int n = 0;
    if (!SafeRead((const void*)(g_baseRecherche + OFF_SYSTEME_BATIMENTS), &sysBat, sizeof(sysBat)) || !sysBat) return;
    // Garde-fou (adresse Steam deduite) : le global doit etre le systeme des batiments.
    static uintptr_t sysVerifie = 0;
    if (sysBat != sysVerifie) {
        char cls[96] = "?";
        GetClassNameFP(sysBat, cls, sizeof(cls));
        if (!strstr(cls, "BuildingSystem")) {
            static bool dit = false;
            if (!dit) { dit = true; printf("[TECH] rattrapage des modificateurs de categorie impossible : global des batiments = %s\n", cls); }
            return;
        }
        sysVerifie = sysBat;
        printf("[TECH] systeme des batiments : 0x%llX (%s)\n", (unsigned long long)sysBat, cls);
    }
    if (!SafeRead((const void*)(sysBat + 0x2060), &liste, sizeof(liste)) || !SafeRead((const void*)(sysBat + 0x2068), &n, sizeof(n))) return;
    if (!liste || n <= 0 || n > 100000) return;
    BatimentDeCategorie_t deCategorie = (BatimentDeCategorie_t)(g_baseRecherche + OFF_BATIMENT_DE_CATEGORIE);
    for (int i = 0; i < n; ++i) {
        uintptr_t bat = 0;
        if (!SafeRead((const void*)(liste + (uintptr_t)i * 8), &bat, sizeof(bat)) || !bat) continue;
        for (int k = 0; k < g_nModifsCategorie; ++k) {
            const ModifCategorie& m = g_modifsCategorie[k];
            if (PaireDans(g_pairesAppliquees, g_nPairesAppliquees, bat, m.modif)) continue;
            uintptr_t dedans = 0, r = 0;
            bool ok = true;
            __try {
                dedans = deCategorie(sysBat, bat, m.categorie, 0);
                if (dedans & 0xFF) r = oAppliquerModifEntite(m.systeme, bat, m.modif, 0);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
            if (!ok) {
                printf("[TECH] %s : EXCEPTION au rattrapage du modificateur de categorie 0x%llX, abandonne\n",
                       NomTechno(m.hash), (unsigned long long)m.modif);
                g_modifsCategorie[k] = g_modifsCategorie[--g_nModifsCategorie];
                return;
            }
            if (!(dedans & 0xFF)) continue;
            char classe[96] = "?";
            GetClassNameFP(bat, classe, sizeof(classe));
            if (r & 0xFF) {
                NoterPaire(g_pairesAppliquees, g_nPairesAppliquees, bat, m.modif);
                printf("[TECH] %s : modificateur de categorie 0x%llX applique au batiment 0x%llX (%s) (rattrapage)\n",
                       NomTechno(m.hash), (unsigned long long)m.modif, (unsigned long long)bat, classe);
            } else if (!PaireDans(g_pairesRefusNote, g_nPairesRefusNote, bat, m.modif)) {
                NoterPaire(g_pairesRefusNote, g_nPairesRefusNote, bat, m.modif);
                printf("[TECH] %s : modificateur de categorie 0x%llX REFUSE par le batiment 0x%llX (%s) (conditions ?) : retente toutes les 3 s\n",
                       NomTechno(m.hash), (unsigned long long)m.modif, (unsigned long long)bat, classe);
            }
        }
    }
}

void TechModifsOublier()
{
    InterlockedExchange(&g_viderModifs, 1);
}

void TechModifsEnAttentePompe()
{
    ViderSiNouvellePartie();
    static DWORD prochain = 0;
    if ((!g_nModifsAttente && !g_nModifsEntiteAttente && !g_nModifsCategorie) || GetTickCount() < prochain) return;
    prochain = GetTickCount() + 3000;
    RattraperModifsCategorie();
    for (int k = 0; oAjouterModifGlobal && k < g_nModifsAttente; ) {
        ModifEnAttente m = g_modifsAttente[k];
        uintptr_t r = 0;
        bool ok = true;
        __try { r = oAjouterModifGlobal(m.systeme, m.modif, 0, 0); }
        __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
        if (ok && (r & 0xFF) == 0) { ++k; continue; }   // toujours refuse : plus tard
        printf("[TECH] %s : modificateur global 0x%llX %s\n", NomTechno(m.hash),
               (unsigned long long)m.modif, ok ? "applique au nouvel essai (conditions remplies)" : "EXCEPTION, abandonne");
        g_modifsAttente[k] = g_modifsAttente[--g_nModifsAttente];
    }
    for (int k = 0; oAppliquerModifEntite && k < g_nModifsEntiteAttente; ) {
        ModifEntiteEnAttente m = g_modifsEntiteAttente[k];
        const char* issue = nullptr;
        if (!EntiteToujoursLa(m.systeme, m.entite)) issue = "abandonne : entite disparue";
        else {
            uintptr_t r = 0;
            bool ok = true;
            // Appel de l'original : le hook ne doit pas remettre le refus en file.
            __try { r = oAppliquerModifEntite(m.systeme, m.entite, m.modif, 0); }
            __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
            if (ok && (r & 0xFF) == 0) { ++k; continue; }   // toujours refuse : plus tard
            if (ok) NoterPaire(g_pairesAppliquees, g_nPairesAppliquees, m.entite, m.modif);
            issue = ok ? "applique au nouvel essai (conditions remplies)" : "EXCEPTION, abandonne";
        }
        printf("[TECH] %s : modificateur d'entite 0x%llX sur l'entite 0x%llX %s\n", NomTechno(m.hash),
               (unsigned long long)m.modif, (unsigned long long)m.entite, issue);
        g_modifsEntiteAttente[k] = g_modifsEntiteAttente[--g_nModifsEntiteAttente];
    }
}

// ---------------------------------------------------------------------------
// 2026-11-22, 4e piste (journal de 15:50) : aucune des 6 technos Vault restantes
// n'a de modificateur, ni global, ni d'entite, ni de categorie. Leur effet est
// un EVENEMENT SCRIPTE du scenario, declenche par
// FrostpunkTechnologyResearchEventTrigger (proprietes "Technology" +
// "Event" eTechnologyResearchEvent : Started / Finished / Canceled). Ghidra :
// l'evenement "recherche" du systeme des technos est self +0x2160
// (Event<techno, eTechnologyResearchEvent> : ecouteurs +0x18, nombre +0x20,
// pas 0x10 = {objet, fonction(objet, techno, evenement)}, compteur
// d'invocation +0x30 ; invocation FUN_14128c240). ResearchComplete l'invoque
// avec 1 (Finished) APRES AddResearchedTechnology ; CancelResearch avec 2.
// AddResearchedTechnology (= notre reception) ne l'invoque PAS : les
// evenements scriptes des technos ne partaient donc jamais a la reception, et
// partaient a la recherche meme sans l'item.
// Regle de l'auteur (effets a la reception, la recherche n'envoie que le
// check) : pour une techno AP, les ecouteurs "evenements scriptes" (classe de
// l'objet contenant "Event", hors interface) sont rendus muets pendant
// ResearchComplete (fonction remplacee par EcouteurMuetFn, puis retablie) et
// invoques avec Finished a la reception. Les autres ecouteurs (interface,
// file de recherche...) restent a la recherche. Liste des ecouteurs
// journalisee une fois par partie.
static const int MAX_ECOUTEURS_MUETS = 16;
struct EcouteurMuet { uintptr_t objet, fonction; };
static const uintptr_t OFF_EVT_RECHERCHE = 0x2160;
static const int EVT_FINISHED = 1;

static void EcouteurMuetFn(uintptr_t, uintptr_t, uintptr_t) {}

// Journal de 15:59 : les objets des 7 ecouteurs sont des sous-objets (pas de
// nom de classe RTTI) et leurs fonctions des "thunks" d'ajustement
// (48 8B C1 ... 48 83/81 C1 <ajust> ... E9 <saut>). L'ecouteur des evenements
// scriptes est celui dont le thunk saute vers FUN_1417c0590 (systeme des
// evenements : table {techno, evenement} -> evenements, +0x2870, puis
// FUN_1417b9b70 = invocation d'un evenement, journal [EVT]) ; Steam 0x165B590
// (thunk 0x1643DE0, ajustement -0x20D8).
static const uintptr_t OFF_ECOUTEUR_EVENEMENTS = Adr(0x17C0590);

static uintptr_t CibleDuThunk(uintptr_t fonction)
{
    uint8_t c[40] = {};
    if (!fonction || !SafeRead((const void*)fonction, c, sizeof(c))) return fonction;
    if (!(c[0] == 0x48 && c[1] == 0x8B && c[2] == 0xC1)) return fonction;   // pas un thunk
    for (int i = 3; i + 5 <= (int)sizeof(c); ++i) {
        if (c[i] != 0xE9) continue;
        int32_t rel = 0;
        memcpy(&rel, c + i + 1, 4);
        return fonction + i + 5 + (intptr_t)rel;
    }
    return fonction;
}

static bool EcouteurEstEffet(uintptr_t fonction)
{
    if (!fonction || !g_baseRecherche) return false;
    uintptr_t cible = g_baseRecherche + OFF_ECOUTEUR_EVENEMENTS;
    return fonction == cible || CibleDuThunk(fonction) == cible;
}

static int RendreMuetsEcouteursEffets(uintptr_t self, EcouteurMuet* muets)
{
    if (!self) return 0;
    uintptr_t evt = self + OFF_EVT_RECHERCHE, tab = 0;
    int n = 0, nMuets = 0;
    if (!SafeRead((const void*)(evt + 0x18), &tab, 8) || !SafeRead((const void*)(evt + 0x20), &n, 4)) return 0;
    for (int i = 0; tab && i < n && i < 64 && nMuets < MAX_ECOUTEURS_MUETS; ++i) {
        uintptr_t e[2] = { 0, 0 };
        if (!SafeRead((const void*)(tab + (uintptr_t)i * 0x10), e, sizeof(e))) break;
        if (!EcouteurEstEffet(e[1])) continue;
        uintptr_t muet = (uintptr_t)&EcouteurMuetFn;
        if (!SafeWrite((void*)(tab + (uintptr_t)i * 0x10 + 8), &muet, 8)) continue;
        muets[nMuets++] = { e[0], e[1] };
    }
    return nMuets;
}

// Retablit les fonctions d'origine (recherche par objet : le tableau a pu bouger).
static void RetablirEcouteurs(uintptr_t self, const EcouteurMuet* muets, int nMuets)
{
    uintptr_t evt = self + OFF_EVT_RECHERCHE, tab = 0;
    int n = 0;
    if (!SafeRead((const void*)(evt + 0x18), &tab, 8) || !SafeRead((const void*)(evt + 0x20), &n, 4)) return;
    for (int i = 0; tab && i < n && i < 64; ++i) {
        uintptr_t e[2] = { 0, 0 };
        if (!SafeRead((const void*)(tab + (uintptr_t)i * 0x10), e, sizeof(e))) break;
        if (e[1] != (uintptr_t)&EcouteurMuetFn) continue;
        for (int k = 0; k < nMuets; ++k)
            if (muets[k].objet == e[0]) { SafeWrite((void*)(tab + (uintptr_t)i * 0x10 + 8), &muets[k].fonction, 8); break; }
    }
}

// Reception : invoque les ecouteurs "evenements scriptes" avec Finished, comme
// FUN_14128c240 (compteur d'invocation +0x30).
static int DeclencherEvenementsTerminee(uintptr_t self, uintptr_t tech)
{
    uintptr_t evt = self + OFF_EVT_RECHERCHE, tab = 0;
    int n = 0;
    short compteur = 0;
    if (!SafeRead((const void*)(evt + 0x18), &tab, 8) || !SafeRead((const void*)(evt + 0x20), &n, 4)
        || !SafeRead((const void*)(evt + 0x30), &compteur, 2) || !tab || n <= 0 || n > 64) return 0;
    if (compteur != 0) { printf("[TECH] evenement \"recherche\" deja en cours d'invocation : evenements scriptes non declenches\n"); return -1; }
    int nAppels = 0;
    short c = 1;
    SafeWrite((void*)(evt + 0x30), &c, 2);
    for (int i = 0; i < n; ++i) {
        uintptr_t e[2] = { 0, 0 };
        if (!SafeRead((const void*)(tab + (uintptr_t)i * 0x10), e, sizeof(e))) break;
        if (!EcouteurEstEffet(e[1]) || !e[1] || e[1] == (uintptr_t)&EcouteurMuetFn) continue;
        __try { ((void (*)(uintptr_t, uintptr_t, uintptr_t))e[1])(e[0], tech, EVT_FINISHED); ++nAppels; }
        __except (EXCEPTION_EXECUTE_HANDLER) { printf("[TECH] EXCEPTION dans l'ecouteur %d de \"recherche terminee\"\n", i); }
        SafeRead((const void*)(evt + 0x18), &tab, 8);     // relu : le tableau a pu bouger
        SafeRead((const void*)(evt + 0x20), &n, 4);
        if (!tab || n > 64) break;
    }
    c = 0;
    SafeWrite((void*)(evt + 0x30), &c, 2);
    return nAppels;
}

bool TechAppliquerEffets(uint64_t hash)
{
    uintptr_t self = g_selfTech, tech = FindTechPtrByHash(hash);
    if (!self || !tech || !oAjouterRecherchee) return false;
    if (TechStateByPtr(self, tech) == 2) return true;     // deja faite (sauvegarde)
    bool ok = true;
    // 3e argument : transmis aux ecouteurs "techno recherchee"
    // (ResearchComplete passe 1). 2026-11-06 : essai a 0 -> plus de bleu du
    // tout. Voulu par l'auteur (2026-11-07) : bleu a la RECEPTION, pas a la
    // recherche -> 1 ici, 0 pour la recherche (hkAjouterRecherchee).
    t_techEffets = hash;                                  // hkAjouterModifGlobal
    __try { oAjouterRecherchee(self, tech, 1, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    t_techEffets = 0;
    if (!ok) printf("[AP] EXCEPTION en appliquant les effets de la techno %016llX\n", (unsigned long long)hash);
    bool faite = ok && TechStateByPtr(self, tech) == 2;
    // Evenements scriptes "recherche terminee" (retenus a la recherche).
    if (faite && TechEstUnCheck(hash)) {
        int n = DeclencherEvenementsTerminee(self, tech);
        if (n > 0) printf("[TECH] %s : evenements scriptes de \"recherche terminee\" declenches a la reception (%d ecouteur(s))\n",
                          NomTechno(hash), n);
    }
    return faite;
}

// ---------------------------------------------------------------------------
// Prerequis lus sur la RECHERCHE, pas sur la reception (demande de l'auteur,
// 2026-11-10). L'etat du jeu d'une techno AP vaut 2 seulement si elle est
// recue (SyncGameState) ; or le jeu lit cet etat pour les prerequis de ses
// enfants (parent, palier TierUnlockN) : le check suivant d'un arbre n'etait
// accessible qu'apres l'item. Pendant l'evaluation (CanResearch, et
// ResearchStart qui peut la refaire), les technos AP recherchees mais pas
// recues (etat 0) passent a l'etat 2, puis reviennent a 0. Jamais la techno
// evaluee elle-meme (le jeu plante en evaluant une techno a l'etat 2).
// Liste des pointeurs recalculee au plus toutes les 500 ms (CanResearch est
// appelee pour chaque noeud de l'arbre) ; l'etat est reverifie avant ecriture.
// ---------------------------------------------------------------------------
static const int MAX_ELEVEES = 256;

static int EleverRecherchees(uintptr_t self, uintptr_t cible, uintptr_t* elevees)
{
    static uintptr_t cache[MAX_ELEVEES];
    static int nCache = 0;
    static DWORD prochain = 0;
    static long versionVue = -1;
    // Recalcul toutes les 500 ms, et aussitot que l'etat runtime change
    // (2026-11-11 : apres la relecture de la sauvegarde locale, le cache
    // calcule avec un runtime vide privait l'arbre de toute elevation).
    if (GetTickCount() >= prochain || versionVue != g_versionTechnos) {
        prochain = GetTickCount() + 500;
        versionVue = g_versionTechnos;
        nCache = 0;
        for (int i = 0; i < RT_Count() && nCache < MAX_ELEVEES; ++i) {
            TechRuntime* r = RT_At(i);
            if (!r || !r->researched || r->received || !TechEstUnCheck(r->hash)) continue;
            uintptr_t t = FindTechPtrByHash(r->hash);
            if (t) cache[nCache++] = t;
        }
    }
    int n = 0;
    for (int i = 0; i < nCache; ++i) {
        uintptr_t t = cache[i];
        if (t == cible || TechStateByPtr(self, t) != 0) continue;
        if (WriteTechStateByPtr(self, t, 2)) elevees[n++] = t;
    }
    return n;
}

// Bug du 2026-11-10 (The Arks : Steam WallDrill et Improved Heaters grises,
// debloques au rechargement) : un item peut etre RECU pendant l'appel du jeu
// (le jeu modifie une ressource -> hook du setter -> pompe des items ->
// OnItemReceived -> etat 2). Le rabaissement remettait alors a 0 une techno
// recue (journal : WallDrill recue l. 5815, etat 0 l. 5879). Une techno
// recue entre-temps garde son etat 2.
static void RabaisserRecherchees(uintptr_t self, const uintptr_t* elevees, int n)
{
    for (int i = 0; i < n; ++i) {
        uint64_t h = HashOfTechPtr(elevees[i]);
        TechRuntime* r = h ? RT_Get(h) : nullptr;
        if (r && r->received) {
            printf("[AP] %s recue pendant l'evaluation : etat 2 garde\n", TechByHash(h) ? TechByHash(h)->name : "?");
            continue;
        }
        WriteTechStateByPtr(self, elevees[i], 0);
    }
}

// DIAG (2026-11-10, Steam Sawmill non recherchable, Sawmill recherchee mais
// pas recue) : une ligne par techno AP quand la reponse du jeu change :
// reponse, parents eleves, parent (table) recherche / recu / etat du jeu.
static void JournalCanResearch(uintptr_t self, uint64_t hash, char vanilla, int nElevees)
{
    static uint64_t vus[128];
    static char reponses[128];
    static int nVus = 0;
    int k = 0;
    for (; k < nVus; ++k) if (vus[k] == hash) break;
    if (k < nVus && reponses[k] == vanilla) return;
    if (k == nVus) { if (nVus >= 128) return; vus[nVus++] = hash; }
    reponses[k] = vanilla;
    const TechInfo* t = TechByHash(hash);
    const TechInfo* p = (t && t->parent) ? TechByHash(t->parent) : nullptr;
    TechRuntime* rp = p ? RT_Get(p->hash) : nullptr;
    uintptr_t pp = p ? FindTechPtrByHash(p->hash) : 0;
    printf("[TECHPRE] %s : jeu %d, %d techno(s) recherchee(s) elevee(s) ; parent %s (recherche %d, recu %d, etat %d), palier %d\n",
        t ? t->name : "?", (int)vanilla, nElevees, p ? p->name : "-", rp && rp->researched ? 1 : 0,
        rp && rp->received ? 1 : 0, pp ? TechStateByPtr(self, pp) : -1, t ? t->tier : -1);
}

// ---------------------------------------------------------------------------
// CanResearch : decide si le clic sur une techno est accepte.
//
// Contrainte du jeu : il PLANTE si on evalue une techno a l'etat 2 (le chemin
// "TechnologyAlreadyResearched" n'est pas prevu pour etre re-parcouru). On
// abaisse donc l'etat le temps de l'appel original, puis on le restaure.
//
// Cas particulier : une techno a l'etat 2 ET en cours de recherche correspond
// a un clic d'ANNULATION. Cette combinaison fait planter le jeu quoi qu'on
// fasse -> on refuse l'annulation (decision assumee, cas rare).
// ---------------------------------------------------------------------------
static char hkCanResearch(uintptr_t self, uintptr_t tech, uintptr_t ctx)
{
    int32_t st = TechStateByPtr(self, tech);

    // Techno sans check dans ce scenario (techno de depart de Winterhome ou
    // On the Edge, decision de l'auteur du 2026-11-03) : le jeu decide seul ;
    // a l'etat 2 elle est deja recherchee, donc refusee.
    uint64_t h0 = HashOfTechPtr(tech);
    if (h0 && !TechEstUnCheck(h0))
        return st == 2 ? 0 : oCanResearch(self, tech, ctx);

    // Annulation d'une recherche portant sur une techno recue : interdite.
    if (st == 2 && IsInProgress(self, tech))
        return 0;                      // le clic ne fait rien

    // Demarrage sur une techno a l'etat 2 : abaisser le temps de l'evaluation.
    bool restore2 = false;
    if (st == 2) { WriteTechStateByPtr(self, tech, 0); restore2 = true; }

    // Prerequis : parents recherches comptes comme faits (2026-11-10).
    uintptr_t elevees[MAX_ELEVEES];
    int nElevees = EleverRecherchees(self, tech, elevees);
    char vanilla = oCanResearch(self, tech, ctx);
    RabaisserRecherchees(self, elevees, nElevees);

    if (restore2) WriteTechStateByPtr(self, tech, 2);

    uint64_t hash = HashOfTechPtr(tech);
    if (!hash) return vanilla;
    JournalCanResearch(self, hash, vanilla, nElevees);

    if (!RT_CanResearch(hash)) return 0;      // deja recherchee -> refus ferme
    // Plus de "if (restore2 && vanilla == 0) return 1" (crash du 2026-11-11,
    // Steam +0x1511CF5) : l'etat etant abaisse a 0 pendant l'evaluation, le
    // jeu ne refuse plus pour "TechnologyAlreadyResearched" ; ses autres
    // refus (decompile FUN_141326a60 : verrouillee, recherche en cours, pas
    // d'atelier libre, prerequis, RESSOURCES) sont legitimes. Forcer 1 sans
    // ressources lancait ResearchStart, dont le CanResearch interne echouait
    // et ecrivait l'erreur dans la liste r8 que l'ancien hook a 2 arguments
    // ne transmettait pas (valeur quelconque) : AV dans FUN_141676d90.
    return vanilla;                           // le jeu decide
}

// ---------------------------------------------------------------------------
// ResearchStart : le demarrage natif plante si la techno est a l'etat 2.
// On l'abaisse a 0 avant, on la restaure a 2 apres.
// ---------------------------------------------------------------------------
static uintptr_t hkResearchStart(uintptr_t self, uintptr_t tech, uintptr_t erreurs, uintptr_t p4)
{
    if (self) g_selfTech = self;

    bool was2 = (TechStateByPtr(self, tech) == 2);
    if (was2) WriteTechStateByPtr(self, tech, 0);

    // Prerequis : parents recherches comptes comme faits (2026-11-10).
    uintptr_t elevees[MAX_ELEVEES];
    int nElevees = EleverRecherchees(self, tech, elevees);
    uintptr_t r = oResearchStart(self, tech, erreurs, p4);   // liste d'erreurs transmise
    RabaisserRecherchees(self, elevees, nElevees);

    if (was2) WriteTechStateByPtr(self, tech, 2);
    return r;
}

// ---------------------------------------------------------------------------
// ResearchComplete : c'est NOTRE location check.
// Ordre critique :
//   1. lire le hash AVANT l'original (il remet self+0x2260 a 0)
//   2. laisser le jeu terminer sa completion
//   3. APRES seulement, appliquer notre logique -> dernier mot sur l'etat
// ---------------------------------------------------------------------------
static void hkResearchComplete(uintptr_t self)
{
    if (self) g_selfTech = self;
    uintptr_t tech = 0; uint64_t hash = 0;
    if (SafeRead((void*)(self + TECH_ACTIVE_OFF), &tech, 8))
        hash = HashOfTechPtr(tech);

    // Techno AP : les ecouteurs "evenements scriptes" de "recherche terminee"
    // sont rendus muets pendant la recherche, ils partent a la reception
    // (TechAppliquerEffets).
    EcouteurMuet muets[MAX_ECOUTEURS_MUETS];
    int nMuets = (hash && TechEstUnCheck(hash)) ? RendreMuetsEcouteursEffets(self, muets) : 0;
    t_completionTech = tech;            // lu par hkAjouterRecherchee
    oResearchComplete(self);
    t_completionTech = 0;
    if (nMuets) {
        RetablirEcouteurs(self, muets, nMuets);
        printf("[TECH] %s : %d ecouteur(s) d'evenements scriptes de \"recherche terminee\" retenu(s) jusqu'a la reception\n",
               NomTechno(hash), nMuets);
    }

    // Icone "icon_done" et [Researched] : posees sur le noeud de l'arbre deja
    // construit (sinon seulement a sa prochaine reconstruction).
    if (hash) {
        OnResearchComplete(hash);
        IconesTechnoRafraichirNoeud(tech);
    }
}

// ---------------------------------------------------------------------------
// CancelResearch : annulation normale (techno a l'etat 1). Le cas "etat 2 en
// cours" n'arrive plus, il est refuse en amont par hkCanResearch.
// ---------------------------------------------------------------------------
static char hkCancel(uintptr_t self)
{
    if (self) g_selfTech = self;

    uintptr_t tech = 0;
    if (self) SafeRead((void*)(self + TECH_ACTIVE_OFF), &tech, 8);
    uint64_t hash = HashOfTechPtr(tech);

    char ret = oCancel(self);

    // L'annulation native remet l'etat a 0 : on resynchronise selon received.
    if (hash) SyncGameState(RT_Get(hash));
    return ret;
}

// ---------------------------------------------------------------------------
// Arbre des technos : un noeud n'est SELECTIONNABLE que si FUN_141bc3cb0 le
// juge disponible, par FUN_141bc5100(panneau, techno, 1) (prerequis lus sur
// l'etat du jeu) ; CanResearch n'est appelee qu'une fois le noeud
// selectionne (bouton du panneau, FUN_141bc9b30). Test de l'auteur du
// 2026-11-10 (Steam Sawmill non selectionnable, Sawmill recherchee mais pas
// recue ; aucune ligne [TECHPRE]) -> meme elevation autour de
// FUN_141bc5100 et du rafraichissement d'un noeud FUN_141bca5a0(panneau,
// GUID de la techno) (liens du noeud : "NOT_AVAILABLE" si le parent n'est
// pas a l'etat 2). La techno du noeud elle-meme n'est jamais elevee.
// Decompiles du 2026-11-10 (Epic) ; Steam : 0x1A65E40, 0x1A6B2E0.
typedef uintptr_t (*Fonction4R_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static const uintptr_t OFF_NOEUD_DISPONIBLE = Adr(0x1BC5100), OFF_RAFRAICHIR_NOEUD = Adr(0x1BCA5A0);
static Fonction4R_t oNoeudDisponible = nullptr, oRafraichirNoeud = nullptr;

// DIAG (2026-11-10, "Generator Upgrade - less coal" grise alors que
// CanResearch l'accepte) : pour l'appel de profondeur 1 (FUN_141bc3cb0),
// une ligne par techno AP quand la reponse change : reponse, technos
// elevees, et chaque prerequis "techno" (tech +0x38, compte +0x40 ; type
// vtable +0xE0 == 4 ; lien GUID +0x30, hash = 8 premiers octets) avec son
// etat PENDANT l'appel (apres elevation).
// 2026-11-10 (The Arks, "Boiler Upgrade" grise, Boilers et TierUnlock2 recus) :
// une seule ligne au depart (indisponible, prerequis a 0), puis plus rien
// alors que l'arbre a ete reconstruit -> on journalise aussi quand la
// signature des prerequis (hash, pointeur, etat pendant l'appel) change, et
// le hash et le pointeur de chaque prerequis (doublon de techno ?).
static void JournalNoeudDisponible(uintptr_t self, uintptr_t tech, uintptr_t r, int nElevees)
{
    uint64_t hash = HashOfTechPtr(tech);
    if (!hash || !TechEstUnCheck(hash)) return;
    static uint64_t vus[256];
    static uint64_t signatures[256];
    static int nVus = 0;
    char rep = (char)(r & 0xFF);
    const TechInfo* t = TechByHash(hash);
    char reqs[768] = "";
    uint64_t sig = 1469598103934665603ULL ^ (uint64_t)(unsigned char)rep;
    uintptr_t tab = 0;
    int32_t nReq = 0;
    SafeRead((void*)(tech + 0x38), &tab, 8);
    SafeRead((void*)(tech + 0x40), &nReq, 4);
    for (int i = 0; tab > 0x10000 && i < nReq && i < 8; ++i) {
        uintptr_t req = 0, vt = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &req, 8) || req <= 0x10000 || !SafeRead((void*)req, &vt, 8)) continue;
        int type = -1;
        __try { type = ((int(*)(uintptr_t))(*(uintptr_t*)(vt + 0xE0)))(req); }
        __except (EXCEPTION_EXECUTE_HANDLER) { type = -2; }
        uint64_t h = 0;
        SafeRead((void*)(req + 0x30), &h, 8);
        const TechInfo* rt = (type == 4 && h) ? TechByHash(h) : nullptr;
        uintptr_t rp = (type == 4 && h) ? FindTechPtrByHash(h) : 0;
        int etat = rp ? TechStateByPtr(self, rp) : -1;
        sig = (sig ^ h) * 1099511628211ULL;
        sig = (sig ^ (uint64_t)(etat + 2)) * 1099511628211ULL;
        char un[160];
        _snprintf_s(un, sizeof(un), _TRUNCATE, "%s[type %d] %s %016llX (0x%llX) etat %d", i ? " ; " : "", type,
            rt ? rt->name : (type == 4 ? "?" : "-"), (unsigned long long)h, (unsigned long long)rp, etat);
        strcat_s(reqs, un);
    }
    int k = 0;
    for (; k < nVus; ++k) if (vus[k] == hash) break;
    if (k < nVus && signatures[k] == sig) return;
    if (k == nVus) { if (nVus >= 256) return; vus[nVus++] = hash; }
    signatures[k] = sig;
    printf("[TECHUI] %s : disponible %d, %d elevee(s) ; prerequis : %s\n", t ? t->name : "?", (int)rep, nElevees,
        reqs[0] ? reqs : "(aucun)");
}

static uintptr_t hkNoeudDisponible(uintptr_t panneau, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    uintptr_t self = g_selfTech;
    if (!self) return oNoeudDisponible(panneau, tech, p3, p4);
    uintptr_t elevees[MAX_ELEVEES];
    int n = EleverRecherchees(self, tech, elevees);
    uintptr_t r = oNoeudDisponible(panneau, tech, p3, p4);
    if ((int)p3 == 1) JournalNoeudDisponible(self, tech, r, n);
    RabaisserRecherchees(self, elevees, n);
    return r;
}

static uintptr_t hkRafraichirNoeud(uintptr_t panneau, uintptr_t guid, uintptr_t p3, uintptr_t p4)
{
    uintptr_t self = g_selfTech;
    uint64_t h = 0;
    if (!self || !guid || !SafeRead((void*)guid, &h, 8)) return oRafraichirNoeud(panneau, guid, p3, p4);
    uintptr_t cible = h ? FindTechPtrByHash(h) : 0;
    uintptr_t elevees[MAX_ELEVEES];
    int n = EleverRecherchees(self, cible, elevees);
    uintptr_t r = oRafraichirNoeud(panneau, guid, p3, p4);
    RabaisserRecherchees(self, elevees, n);
    return r;
}

// ---------------------------------------------------------------------------
void InstallResearchHooks(uintptr_t modBase)
{
    g_baseRecherche = modBase;                // rattrapage des modificateurs de categorie
    struct { uintptr_t off; void* detour; void** orig; const char* name; } H[] = {
        { GameOffsets::CanResearch,      (void*)&hkCanResearch,      (void**)&oCanResearch,      "CanResearch" },
        { GameOffsets::ResearchStart,    (void*)&hkResearchStart,    (void**)&oResearchStart,    "ResearchStart" },
        { GameOffsets::ResearchComplete, (void*)&hkResearchComplete, (void**)&oResearchComplete, "ResearchComplete" },
        { GameOffsets::CancelResearch,   (void*)&hkCancel,           (void**)&oCancel,           "CancelResearch" },
        { OFF_AJOUTER_RECHERCHEE,        (void*)&hkAjouterRecherchee, (void**)&oAjouterRecherchee, "AddResearchedTechnology" },
        { OFF_NOEUD_DISPONIBLE,          (void*)&hkNoeudDisponible,  (void**)&oNoeudDisponible,  "noeud disponible (arbre)" },
        { OFF_RAFRAICHIR_NOEUD,          (void*)&hkRafraichirNoeud,  (void**)&oRafraichirNoeud,  "rafraichissement d'un noeud (arbre)" },
        { OFF_AJOUTER_MODIF_GLOBAL,      (void*)&hkAjouterModifGlobal, (void**)&oAjouterModifGlobal, "ajout d'un modificateur global" },
        { OFF_INSCRIRE_MODIF_ENTITE,     (void*)&hkInscrireModifEntite, (void**)&oInscrireModifEntite, "inscription d'un modificateur d'entite" },
        { OFF_APPLIQUER_MODIF_ENTITE,    (void*)&hkAppliquerModifEntite, (void**)&oAppliquerModifEntite, "application d'un modificateur d'entite" },
        { OFF_INSCRIRE_MODIF_CATEGORIE,  (void*)&hkInscrireModifCategorie, (void**)&oInscrireModifCategorie, "inscription d'un modificateur de categorie" },
    };
    for (auto& h : H) {
        void* target = (void*)(modBase + h.off);
        if (MH_CreateHook(target, h.detour, h.orig) != MH_OK || MH_EnableHook(target) != MH_OK)
            printf("[!] hook %s KO\n", h.name);
    }
    InstallHooksIconesTechnos(modBase);
}
