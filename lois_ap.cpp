// ---- lois_ap.cpp ----
// Lois Archipelago (2026-10-30). Decompiles :
//  - FUN_1416fd350(sys, loi) = signer : frustration, SociotechData (map
//    sys +0xD8, compte +0xE0, elements de 0x18 {loi, date d'activation,
//    etat}) -> etat 2, delai (sys +0xF0 / +0xF8), FUN_1416fdbc0, espoir,
//    alignements, commentaires, evenement sys +0x30, livre (FUN_1416ffe70).
//  - FUN_1416fdbc0(sys, loi) = effets : modificateurs globaux / d'entite /
//    de lieu inscrits UNE fois dans le systeme de modificateurs (qui les
//    sauvegarde lui-meme, sans les recalculer depuis les lois au
//    chargement), puis effets +0xE0. Aussi appelee par le reglage de depart
//    (FUN_1416ffbe0) : on ne retient que les appels faits DANS une signature.
// Arbres : composantes des prerequis (SociotechRequirement, y compris dans
// les composites) et des groupes d'exclusion (+0x140, par categorie) ;
// Order + Faith = "Purpose", Left + Right = "Politics" (checks par rang).
// Rang d'une loi signee : 1 + somme des poids des lois signees avant elle
// dans son arbre (date d'activation) ; Ceremonial Funerals (Adaptation) pese
// 2 (decision de l'auteur). Une loi de rang r et de poids p envoie les checks
// r..r+p-1 ; son effet arrive quand l'arbre a recu au moins r items. Lois de
// depart du scenario (date d'activation = DAT_14293fde8) : ni check, ni
// retenue.
// Persistance : mods\ap_lois_<GameId>.txt, cumulative comme celle des
// technos (une sauvegarde plus ancienne garde checks et items) :
//   A\t<items recus>\t<arbre>
//   C\t<rang>\t<arbre>
//   E <guid a> <guid b> <date d'activation> <date d'application>   (hex)
// Au chargement, l'effet d'une loi signee est PRESENT dans la sauvegarde du
// jeu si un E porte la meme date d'activation et une date d'application
// <= date de la sauvegarde ; sinon il est reapplique (item recu) ou reste en
// attente. Limite : deux branches de sauvegardes divergentes peuvent tromper
// la comparaison des dates.
#include "lois_ap.h"
#include "lois.h"
#include "mod_common.h"
#include "archipelago.h"
#include "IconSubManager.h"   // ARCHIPELAGO_ICONS_TECH
#include "MinHook.h"
#include <cstring>
#include <cstdio>

namespace {

constexpr uintptr_t OFF_SIGNER         = 0x16FD350;
constexpr uintptr_t OFF_EFFETS         = 0x16FDBC0;
constexpr uintptr_t OFF_SOCIETY_CONFIG = 0x4070460;
constexpr uintptr_t OFF_SYSTEME_LOIS   = 0x408FBD0;
constexpr uintptr_t OFF_HORLOGE        = 0x2C32A40;   // *(...) +0xB0 = date courante
constexpr int       HORLOGE_DATE       = 0xB0;
constexpr uintptr_t OFF_DATE_DEPART    = 0x293FDE8;   // date des lois du reglage de depart

constexpr int CFG_LOIS        = 0xB0;
constexpr int LOI_NOM         = 0x18;
constexpr int LOI_GUID        = 0x20;
constexpr int LOI_ACTIVEE     = 0x30;
constexpr int LOI_PREREQUIS   = 0x38;
constexpr int LOI_AFFICHAGE   = 0x50;   // "Show requirements"
constexpr int LOI_EXCLUSION   = 0x140;
constexpr int LOI_CATEGORIE   = 0x148;
// Icone d'une loi (reflexion FUN_14145bdd0, FrostpunkProgressTreeEntry) :
// +0x80 "Icon tile" = case (x, y) seule, dans l'atlas commun a toutes les
// lois de FrostpunkSocietyConfig (reflexion FUN_141439ef0) : +0x328
// "Sociotech icons texture", +0x330 "Sociotech icons tiles count".
constexpr int LOI_ICONE          = 0x80;
constexpr int CFG_ICONES_TEXTURE = 0x328;
constexpr int CFG_ICONES_CASES   = 0x330;
constexpr int REQ_LIEN        = 0x30;
constexpr int COMPOSITE_LISTE = 0x38;
constexpr int SYS_ETATS       = 0xD8;
constexpr int ELEM_ETAT       = 0x18;
constexpr int ETAT_ACTIVE     = 2;

constexpr int MAX_LOIS   = 160;
constexpr int MAX_ARBRES = 64;
constexpr int MAX_RANG   = 63;   // checks d'un arbre : bits 1..63

struct Guid { uint64_t a, b; };

struct LoiAP {
    uintptr_t adr;
    Guid      guid, categorie;
    int       arbre;
    int       poids;
    char      nom[96];
    char      exclusion[64];
    bool      requiertLoi;     // a au moins une loi en prerequis (pas une racine)
    int       requises[4];     // lois requises directement (prerequis), 4 au plus
    int       nRequises;
    int       profondeur;      // etape dans l'arbre : 0 racine, 1 + max des requises
    // Sauvegarde locale : derniere application de l'effet (E) ...
    bool      enregistree;
    uint64_t  dateActivation, dateApplication;
    // ... et signature dont l'effet a ete retenu (P).
    bool      attente;
    uint64_t  dateAttente;
    // Effet present dans la partie en cours.
    bool      presente;
    // Signee mais effet retenu : ses prerequis hors lois (batiments...)
    // repondent "non" (lu par le hook de l'evaluateur, chemin chaud).
    volatile bool retenue;
    // Case d'icone d'origine (avant la premiere substitution).
    bool      caseSauvee;
    int32_t   caseOrigine[2];
};

struct ArbreAP {
    char     nom[96];
    int      recus;     // items progressifs recus
    uint64_t checks;    // bit r : check de rang r envoye
};

LoiAP   g_lois[MAX_LOIS];
int     g_nLois = 0;
ArbreAP g_arbres[MAX_ARBRES];
int     g_nArbres = 0;
bool    g_bati = false;

SRWLOCK  g_verrou = SRWLOCK_INIT;
uint64_t g_partie[2] = {};
bool     g_partieChargee = false;          // sauvegarde locale de g_partie relue
volatile bool g_rechargement = false;      // LoadGame vient de se terminer
uint64_t g_dateSauvegarde = 0;             // date de jeu a la sortie de LoadGame
volatile bool g_aTraiter = false;          // effets en attente a examiner
volatile bool g_testDemande = false;       // F5

thread_local uintptr_t t_signature = 0;    // loi en cours de signature
thread_local bool      t_differe = false;  // ses effets ont ete retenus

typedef uintptr_t(*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
Fonction4_t oSigner = nullptr, oEffets = nullptr, oEvaluer = nullptr;
constexpr uintptr_t OFF_EVALUER_PREREQUIS = 0x16A71F0;

uintptr_t Base() { return (uintptr_t)GetModuleHandleW(NULL); }

bool GuidEgal(const Guid& x, const Guid& y) { return x.a == y.a && x.b == y.b; }

void LireTexte8(uintptr_t adr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adr, &p, 8) || p <= 0x10000) return;
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        out[i] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }
    out[i] = 0;
}

bool LireTableau(uintptr_t adr, uintptr_t& tab, int& n)
{
    tab = 0; n = 0;
    uint32_t c = 0;
    if (!SafeRead((void*)adr, &tab, 8) || !SafeRead((void*)(adr + 8), &c, 4)) return false;
    if (c == 0) return true;
    if (tab <= 0x10000 || c > 1024) { tab = 0; return false; }
    n = (int)c;
    return true;
}

uint64_t DateCourante()
{
    uintptr_t horloge = 0;
    uint64_t d = 0;
    if (SafeRead((void*)(Base() + OFF_HORLOGE), &horloge, 8) && horloge > 0x10000)
        SafeRead((void*)(horloge + HORLOGE_DATE), &d, 8);
    return d;
}

int IndexParAdresse(uintptr_t adr)
{
    for (int i = 0; i < g_nLois; ++i) if (g_lois[i].adr == adr) return i;
    return -1;
}

int IndexParGuid(const Guid& g)
{
    for (int i = 0; i < g_nLois; ++i) if (GuidEgal(g_lois[i].guid, g)) return i;
    return -1;
}

int ArbreParNom(const char* nom)
{
    for (int a = 0; a < g_nArbres; ++a) if (_stricmp(g_arbres[a].nom, nom) == 0) return a;
    return -1;
}

void NomDeLoi(int i, char* out, int taille)
{
    if (!LoisNomAffiche(g_lois[i].adr, out, taille)) strcpy_s(out, taille, g_lois[i].nom);
}

// --- Prerequis des lois ----------------------------------------------------------
// Le deblocage des BATIMENTS d'une loi ne passe pas par ses effets : le
// batiment porte un prerequis "loi signee" (FrostpunkSociotechRequirement),
// evalue par FUN_1416a71f0 (frostpunkrequirementsystem.cpp, cas 7 ->
// FUN_1416ff990 "active ?"). Test du 2026-10-31 : effet retenu, cimetiere
// debloque quand meme a la signature. Ce meme evaluateur sert aux prerequis
// des lois (le livre) : on releve donc tous les objets de prerequis des lois
// (+0x38 et affichage +0x50, composites compris) ; un prerequis de loi suit
// la signature, tout autre (batiments, ...) suit la reception de l'item.
constexpr int MAX_REQ_LOIS = 2048;
uintptr_t g_reqLois[MAX_REQ_LOIS];      // trie apres BatirArbres
int       g_nReqLois = 0;
uintptr_t g_vtReqLoi = 0;               // vtable de FrostpunkSociotechRequirement

bool EstPrerequisDeLoi(uintptr_t req)
{
    int bas = 0, haut = g_nReqLois - 1;
    while (bas <= haut) {
        int m = (bas + haut) / 2;
        if (g_reqLois[m] == req) return true;
        if (g_reqLois[m] < req) bas = m + 1; else haut = m - 1;
    }
    return false;
}

// --- Arbres --------------------------------------------------------------------
int g_parent[MAX_LOIS];

int Racine(int i) { while (g_parent[i] != i) i = g_parent[i] = g_parent[g_parent[i]]; return i; }
void Unir(int i, int j) { i = Racine(i); j = Racine(j); if (i != j) g_parent[j < i ? i : j] = (j < i ? j : i); }

// Parcourt une liste de prerequis de la loi i : releve chaque objet, et si
// 'unir', relie la loi aux lois requises.
void UnirParExigences(uintptr_t adrTableau, int i, int profondeur, bool unir)
{
    uintptr_t tab = 0; int n = 0;
    if (profondeur > 4 || !LireTableau(adrTableau, tab, n)) return;
    for (int k = 0; k < n; ++k) {
        uintptr_t req = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)k * 8), &req, 8) || req <= 0x10000) continue;
        if (g_nReqLois < MAX_REQ_LOIS) g_reqLois[g_nReqLois++] = req;
        char cls[96] = "";
        if (!GetClassNameFP(req, cls, sizeof(cls))) continue;
        if (strcmp(cls, "FrostpunkSociotechRequirement") == 0) {
            if (!g_vtReqLoi) SafeRead((void*)req, &g_vtReqLoi, 8);
            Guid g{};
            if (unir && SafeRead((void*)(req + REQ_LIEN), &g, sizeof(g))) {
                int j = IndexParGuid(g);
                if (j >= 0) {
                    Unir(i, j);
                    LoiAP& l = g_lois[i];
                    l.requiertLoi = true;
                    bool deja = j == i;
                    for (int k2 = 0; k2 < l.nRequises; ++k2) deja |= l.requises[k2] == j;
                    if (!deja && l.nRequises < 4) l.requises[l.nRequises++] = j;
                }
            }
        } else if (strcmp(cls, "FrostpunkCompositeRequirement") == 0) {
            UnirParExigences(req + COMPOSITE_LISTE, i, profondeur + 1, unir);
        }
    }
}

// Construit la liste des lois signables et leurs arbres (donnees fixes du
// jeu : une seule fois). Thread du jeu.
bool BatirArbres()
{
    if (g_bati) return true;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(Base() + OFF_SOCIETY_CONFIG + CFG_LOIS, tab, n) || n == 0) return false;

    g_nLois = 0;
    for (int k = 0; k < n && g_nLois < MAX_LOIS; ++k) {
        uintptr_t e = 0;
        uint8_t activee = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)k * 8), &e, 8) || e <= 0x10000) continue;
        if (!SafeRead((void*)(e + LOI_ACTIVEE), &activee, 1) || !activee) continue;   // desactivees : exclues
        LoiAP& l = g_lois[g_nLois];
        memset(&l, 0, sizeof(l));
        l.adr = e;
        if (!SafeRead((void*)(e + LOI_GUID), &l.guid, sizeof(Guid)) ||
            !SafeRead((void*)(e + LOI_CATEGORIE), &l.categorie, sizeof(Guid))) continue;
        LireTexte8(e + LOI_NOM, l.nom, sizeof(l.nom));
        LireTexte8(e + LOI_EXCLUSION, l.exclusion, sizeof(l.exclusion));
        if (!strcmp(l.exclusion, "-")) l.exclusion[0] = 0;
        l.poids = (strncmp(l.nom, "[Adaptation", 11) == 0 && strstr(l.nom, "Burial Ceremony")) ? 2 : 1;
        l.arbre = -1;
        g_nLois++;
    }
    if (g_nLois == 0) return false;

    for (int i = 0; i < g_nLois; ++i) g_parent[i] = i;
    g_nReqLois = 0;
    for (int i = 0; i < g_nLois; ++i) {
        UnirParExigences(g_lois[i].adr + LOI_PREREQUIS, i, 0, true);
        UnirParExigences(g_lois[i].adr + LOI_AFFICHAGE, i, 0, false);
    }
    // Etape de chaque loi (rang affiche des lois non signees, RangAffiche) :
    // 0 pour une racine, 1 + la plus profonde des requises. Passes bornees
    // (un cycle ne bloque pas).
    for (int passe = 0; passe < 32; ++passe) {
        bool change = false;
        for (int i = 0; i < g_nLois; ++i) {
            int p = 0;
            for (int k = 0; k < g_lois[i].nRequises; ++k) {
                int q = g_lois[g_lois[i].requises[k]].profondeur + 1;
                if (q > p) p = q;
            }
            if (p > 31) p = 31;
            if (p != g_lois[i].profondeur) { g_lois[i].profondeur = p; change = true; }
        }
        if (!change) break;
    }
    for (int a = 1; a < g_nReqLois; ++a)          // tri par insertion (une seule fois)
        for (int b = a; b > 0 && g_reqLois[b - 1] > g_reqLois[b]; --b) {
            uintptr_t t = g_reqLois[b]; g_reqLois[b] = g_reqLois[b - 1]; g_reqLois[b - 1] = t;
        }
    printf("[LOIAP] %d prerequis de lois releves (vtable SociotechRequirement +%llX)\n",
        g_nReqLois, (unsigned long long)(g_vtReqLoi ? g_vtReqLoi - Base() : 0));
    for (int i = 0; i < g_nLois; ++i)
        for (int j = i + 1; j < g_nLois; ++j)
            if (g_lois[i].exclusion[0] && GuidEgal(g_lois[i].categorie, g_lois[j].categorie) &&
                strcmp(g_lois[i].exclusion, g_lois[j].exclusion) == 0)
                Unir(i, j);
    // Branches entieres exclusives : un seul arbre, checks par rang.
    int purpose = -1, politics = -1;
    for (int i = 0; i < g_nLois; ++i) {
        const char* nm = g_lois[i].nom;
        if (strstr(nm, "[Order") || strstr(nm, "[Faith")) { if (purpose < 0) purpose = i; else Unir(purpose, i); }
        if (strstr(nm, "[Left") || strstr(nm, "[Right")) { if (politics < 0) politics = i; else Unir(politics, i); }
    }

    // Nom d'un arbre : sa premiere racine (loi sans loi en prerequis) hors
    // lois Endless (toujours cachees) ; a defaut, sa premiere loi.
    int arbreDe[MAX_LOIS];
    for (int i = 0; i < g_nLois; ++i) arbreDe[i] = -1;
    g_nArbres = 0;
    for (int i = 0; i < g_nLois; ++i) {
        int r = Racine(i);
        if (arbreDe[r] < 0) {
            if (g_nArbres >= MAX_ARBRES) continue;
            ArbreAP& a = g_arbres[g_nArbres];
            memset(&a, 0, sizeof(a));
            if (purpose >= 0 && Racine(purpose) == r) strcpy_s(a.nom, "Purpose");
            else if (politics >= 0 && Racine(politics) == r) strcpy_s(a.nom, "Politics");
            else {
                int nommee = i;
                for (int j = i; j < g_nLois; ++j)
                    if (Racine(j) == r && !g_lois[j].requiertLoi && !strstr(g_lois[j].nom, "Endless")) { nommee = j; break; }
                strcpy_s(a.nom, g_lois[nommee].nom);
            }
            arbreDe[r] = g_nArbres++;
        }
        g_lois[i].arbre = arbreDe[r];
    }
    g_bati = true;

    printf("[LOIAP] %d lois signables, %d arbres :\n", g_nLois, g_nArbres);
    for (int a = 0; a < g_nArbres; ++a) {
        int nb = 0, poids = 0;
        for (int i = 0; i < g_nLois; ++i) if (g_lois[i].arbre == a) { ++nb; poids += g_lois[i].poids; }
        printf("[LOIAP]   arbre \"%s\" : %d loi(s), poids total %d\n", g_arbres[a].nom, nb, poids);
        for (int i = 0; i < g_nLois; ++i)
            if (g_lois[i].arbre == a) {
                int32_t c[2] = { -1, -1 };
                SafeRead((void*)(g_lois[i].adr + LOI_ICONE), c, sizeof(c));
                printf("[LOIAP]       case (%d,%d)  %s%s%s%s\n", c[0], c[1], g_lois[i].nom,
                    g_lois[i].exclusion[0] ? "  (exclusion " : "",
                    g_lois[i].exclusion[0] ? g_lois[i].exclusion : "",
                    g_lois[i].exclusion[0] ? (g_lois[i].poids > 1 ? ", poids 2)" : ")") :
                                            (g_lois[i].poids > 1 ? "  (poids 2)" : ""));
            }
    }
    return true;
}

// --- Etat des lois dans la partie ----------------------------------------------
struct Etats {
    uintptr_t sys;
    uint64_t  dateDepart;
    int       etat[MAX_LOIS];
    uint64_t  date[MAX_LOIS];
    bool Signee(int i) const { return etat[i] == ETAT_ACTIVE && date[i] != dateDepart; }
};

bool LireEtats(Etats& e)
{
    static uintptr_t sysValide = 0;
    e.sys = 0;
    for (int i = 0; i < g_nLois; ++i) { e.etat[i] = -1; e.date[i] = 0; }
    if (!SafeRead((void*)(Base() + OFF_SYSTEME_LOIS), &e.sys, 8) || e.sys <= 0x10000) return false;
    if (e.sys != sysValide) {
        char cls[64] = "";
        if (!GetClassNameFP(e.sys, cls, sizeof(cls)) || strcmp(cls, "FrostpunkSociotechSystem") != 0) return false;
        sysValide = e.sys;
    }
    SafeRead((void*)(Base() + OFF_DATE_DEPART), &e.dateDepart, 8);
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(e.sys + SYS_ETATS, tab, n)) return false;
    for (int k = 0; k < n; ++k) {
        uintptr_t elem[3] = {};
        if (!SafeRead((void*)(tab + (uintptr_t)k * ELEM_ETAT), elem, sizeof(elem))) continue;
        int i = IndexParAdresse(elem[0]);
        if (i < 0) continue;
        e.date[i] = elem[1];
        e.etat[i] = (int)(uint32_t)elem[2];
    }
    return true;
}

// Poids d'une loi dans la partie courante. Ceremonial Funerals (Adaptation)
// pese 2 pour equilibrer Organ Transplants + Organic Fertiliser ; la ou
// Organic Fertiliser n'existe pas (On the Edge : affichage "PAS scenario
// Reconquest"), elle ne pese que 1 (decision de l'auteur, 2026-11-03).
// Scenario courant : FrostpunkScenarioSystem (id 0x0C) +0x68 -> +0x00 nom.
int Poids(int i)
{
    if (g_lois[i].poids <= 1) return g_lois[i].poids;
    uintptr_t sys = SystemeParId(0x0C), scn = 0;
    char nom[32] = "";
    if (sys && SafeRead((void*)(sys + 0x68), &scn, 8) && scn > 0x10000) LireTexte8(scn, nom, sizeof(nom));
    return strcmp(nom, "Reconquest") == 0 ? 1 : g_lois[i].poids;
}

int Rang(int i, const Etats& e)
{
    int r = 1;
    for (int j = 0; j < g_nLois; ++j) {
        if (j == i || g_lois[j].arbre != g_lois[i].arbre || !e.Signee(j)) continue;
        if (e.date[j] < e.date[i] || (e.date[j] == e.date[i] && j < i)) r += Poids(j);
    }
    return r;
}

// --- Icones du livre des lois -------------------------------------------------------
// Comme pour les technos : une loi affiche le type du check qu'elle enverra,
// "icon_done" une fois signee. Checks par rang : une loi non signee enverra
// le prochain check de son arbre (1 + poids des lois deja signees), donc
// toutes les lois non signees d'un arbre montrent la meme icone (pour une loi
// de poids 2, celle de son premier check).

// TEMPORAIRE : type du check (arbre, rang) en attendant le serveur
// (equivalent de ApChargerTechnosDeTest). Stable d'une session a l'autre.
const char* IconeDuCheckDeTest(const char* arbre, int rang)
{
    static const char* const types[] = {
        "icon_vanilla", "icon_filler", "icon_important", "icon_progressive", "icon_trap" };
    uint32_t h = 2166136261u;
    for (const char* p = arbre; *p; ++p) h = (h ^ (uint8_t)*p) * 16777619u;
    h = (h ^ (uint32_t)rang) * 16777619u;
    return types[h % 5];
}

enum { ATLAS_ILLISIBLE, ATLAS_TECHNOS, ATLAS_BULLES, ATLAS_AUTRE };
int g_atlas = -1;   // -1 : pas encore lu

// Atlas des icones de lois. Le livre (FUN_141bbc320, image "TECH_IMAGE" de
// chaque noeud) pose la texture de la config +0x328 et la case de la loi
// +0x80 (FUN_141bb2d30, nombre de cases = config +0x330). Releve du
// 2026-10-31 : "UI/Tiles/Icons.dds" 16 x 16 = l'atlas des bulles (#2350),
// ou nos icones sont deja (bulleCol / bulleRow) ; toutes les lois y sont en
// (2,13) (fleche jaune), sauf Strike Busting (3,-8).
int AtlasDesLois()
{
    if (g_atlas >= 0) return g_atlas;
    char tex[160];
    LireTexte8(Base() + OFF_SOCIETY_CONFIG + CFG_ICONES_TEXTURE, tex, sizeof(tex));
    if (!tex[0]) return ATLAS_ILLISIBLE;
    int32_t nb[2] = {};
    SafeRead((void*)(Base() + OFF_SOCIETY_CONFIG + CFG_ICONES_CASES), nb, sizeof(nb));
    char min[160];
    int k = 0;
    for (; tex[k] && k < (int)sizeof(min) - 1; ++k)
        min[k] = (tex[k] >= 'A' && tex[k] <= 'Z') ? (char)(tex[k] - 'A' + 'a') : tex[k];
    min[k] = 0;
    g_atlas = strstr(min, "tech_tree_atlas") ? ATLAS_TECHNOS
            : strstr(min, "tiles/icons.dds") || strstr(min, "tiles\\icons.dds") ? ATLAS_BULLES
            : ATLAS_AUTRE;
    printf("[LOIAP] icones des lois : texture \"%s\", %d x %d cases -> %s\n", tex, nb[0], nb[1],
        g_atlas == ATLAS_TECHNOS ? "atlas de l'arbre des technos (#1179), cases techCol/techRow" :
        g_atlas == ATLAS_BULLES  ? "atlas des bulles (#2350), cases bulleCol/bulleRow" :
                                   "atlas inconnu : aucune case AP, rien ne sera pose");
    return g_atlas;
}

bool CaseDeLIcone(const TechIcons* ic, int atlas, int32_t c[2])
{
    if (!ic) return false;
    if (atlas == ATLAS_TECHNOS) { c[0] = ic->techCol;  c[1] = ic->techRow;  return true; }
    if (atlas == ATLAS_BULLES)  { c[0] = ic->bulleCol; c[1] = ic->bulleRow; return true; }
    return false;
}

int RangAffiche(int i, const Etats& e);   // plus bas (apres Accessible)

// Icone AP de la loi i : "icon_done" si active (lois de depart comprises),
// sinon celle du check de son etape (RangAffiche, 2026-11-04).
const TechIcons* IconeDeLoi(int i, const Etats& e)
{
    const char* nomIcone = nullptr;
    if (e.etat[i] == ETAT_ACTIVE) nomIcone = "icon_done";
    else {
        int r = RangAffiche(i, e);
        // Scout de la location (reseau) ; sans scout, l'ancienne valeur de test.
        char cle[128];
        sprintf_s(cle, "%s|%d", g_arbres[g_lois[i].arbre].nom, r);
        nomIcone = ApIconeDuCheck("law", cle);
        if (!nomIcone) nomIcone = IconeDuCheckDeTest(g_arbres[g_lois[i].arbre].nom, r);
    }
    for (const TechIcons& t : ARCHIPELAGO_ICONS_TECH) if (!strcmp(t.nom, nomIcone)) return &t;
    return nullptr;
}

// --- Lois devenues inaccessibles ------------------------------------------------
// Demande de l'auteur (2026-11-01) : une loi qu'on ne pourra plus jamais signer
// (branche exclue) perd son icone AP. Inaccessible = non active et (une autre
// loi de son groupe d'exclusion, dans la meme categorie, est active) ou (ses
// prerequis ne peuvent plus etre satisfaits : une loi requise inaccessible ;
// composites ET / OU evalues, NON et autres types tenus pour satisfaisables).
// Ex. : Cemetery signee -> Corpse Disposal exclue -> Organ Transplants (qui la
// requiert) puis Organic Fertiliser inaccessibles.
struct Accessibilite {
    const Etats* e;
    int8_t etat[MAX_LOIS];   // 0 inconnu, 1 en cours, 2 accessible, 3 inaccessible
};

bool Accessible(Accessibilite& a, int i);

// Une liste de prerequis (tableau de pointeurs) : vraie si tous (ET) ou l'un
// d'eux (OU) peuvent encore etre satisfaits.
bool ListeSatisfaisable(Accessibilite& a, uintptr_t adrTableau, bool ou, int profondeur)
{
    uintptr_t tab = 0; int n = 0;
    if (profondeur > 4 || !LireTableau(adrTableau, tab, n) || n == 0) return true;
    bool unVrai = false;
    for (int k = 0; k < n; ++k) {
        uintptr_t req = 0;
        bool v = true;
        if (SafeRead((void*)(tab + (uintptr_t)k * 8), &req, 8) && req > 0x10000) {
            char cls[96] = "";
            GetClassNameFP(req, cls, sizeof(cls));
            if (strcmp(cls, "FrostpunkSociotechRequirement") == 0) {
                Guid g{};
                int j = SafeRead((void*)(req + REQ_LIEN), &g, sizeof(g)) ? IndexParGuid(g) : -1;
                if (j >= 0) v = Accessible(a, j);
            } else if (strcmp(cls, "FrostpunkCompositeRequirement") == 0) {
                uint32_t op = 0;
                SafeRead((void*)(req + 0x30), &op, 4);            // 0 ET, 1 OU, 2 NON
                if (op == 0 || op == 1) v = ListeSatisfaisable(a, req + COMPOSITE_LISTE, op == 1, profondeur + 1);
            }
        }
        if (ou && v) return true;
        if (!ou && !v) return false;
        unVrai |= v;
    }
    return ou ? unVrai : true;
}

bool Accessible(Accessibilite& a, int i)
{
    if (a.etat[i] == 2 || a.etat[i] == 1) return true;   // en cours : cycle, tenu pour accessible
    if (a.etat[i] == 3) return false;
    if (a.e->etat[i] == ETAT_ACTIVE) { a.etat[i] = 2; return true; }
    a.etat[i] = 1;
    bool ok = true;
    if (g_lois[i].exclusion[0])
        for (int j = 0; j < g_nLois && ok; ++j)
            if (j != i && a.e->etat[j] == ETAT_ACTIVE && GuidEgal(g_lois[j].categorie, g_lois[i].categorie) &&
                !strcmp(g_lois[j].exclusion, g_lois[i].exclusion))
                ok = false;
    if (ok) ok = ListeSatisfaisable(a, g_lois[i].adr + LOI_PREREQUIS, false, 0);
    a.etat[i] = ok ? 2 : 3;
    return ok;
}

// Unite de check d'une loi (une etape = un check) : une paire exclusive
// compte une fois (cle = son groupe d'exclusion), des lois exclusives par
// leurs parents aussi (cle = groupe du parent + etape) ; sinon la loi seule.
void CleUnite(int i, char* out, size_t taille)
{
    const LoiAP& l = g_lois[i];
    if (l.exclusion[0]) { _snprintf_s(out, taille, _TRUNCATE, "X%d:%s", l.arbre, l.exclusion); return; }
    if (l.nRequises == 1 && g_lois[l.requises[0]].exclusion[0]) {
        _snprintf_s(out, taille, _TRUNCATE, "P%d:%s:%d", l.arbre, g_lois[l.requises[0]].exclusion, l.profondeur);
        return;
    }
    _snprintf_s(out, taille, _TRUNCATE, "L%d", i);
}

// Rang(s) FIXE(s) du check de la loi i (decision de l'auteur, 2026-11-04 :
// deux lois non exclusives n'ont jamais le meme check, ex. Duelling Law et
// Public House ; seules les lois exclusives partagent un check). Table de
// l'apworld (outils\lois_table.tsv -> AP_LOIS_RANGS). 0 : loi absente.
int RangsFixes(int i, int rangs[4])
{
    return ApRangsDeLoi(g_arbres[g_lois[i].arbre].nom, g_lois[i].nom, rangs, 4);
}

// Rang du check affiche par la loi i : son rang fixe (RangsFixes, le premier
// pour une loi de poids 2). Repli, loi absente de la table : loi signee ->
// son rang reel (Rang) ; sinon 1 + poids des lois signees de l'arbre + poids
// des etapes plus hautes (profondeur inferieure) encore a signer et
// accessibles, une unite par paire / par lois exclusives par leurs parents.
int RangAffiche(int i, const Etats& e)
{
    int fixes[4];
    if (RangsFixes(i, fixes) > 0) return fixes[0];
    if (e.Signee(i)) return Rang(i, e);
    Accessibilite acc;
    acc.e = &e;
    memset(acc.etat, 0, sizeof(acc.etat));
    int r = 1;
    char vues[64][80];
    int nVues = 0;
    for (int j = 0; j < g_nLois; ++j) {
        if (j == i || g_lois[j].arbre != g_lois[i].arbre) continue;
        if (e.Signee(j)) { r += Poids(j); continue; }
        if (g_lois[j].profondeur >= g_lois[i].profondeur || !Accessible(acc, j)) continue;
        char cle[80];
        CleUnite(j, cle, sizeof(cle));
        bool deja = false;
        for (int k = 0; k < nVues && !deja; ++k) deja = strcmp(vues[k], cle) == 0;
        if (deja) continue;
        if (nVues < 64) strcpy_s(vues[nVues++], cle);
        r += Poids(j);
    }
    return r;
}

// Pose l'icone de chaque loi dans sa case +0x80. Thread du jeu, apres une
// (re)lecture de la partie et apres chaque signature. Test du 2026-10-31 :
// sans effet visible (le noeud du livre n'a pas d'enfant "TECH_IMAGE") ;
// garde pour l'infobulle / le detail, au cas ou ils liraient +0x80.
void RafraichirIcones()
{
    Etats e;
    if (!LireEtats(e)) return;
    int atlas = AtlasDesLois();
    int changees = 0, sansCase = 0;
    for (int i = 0; i < g_nLois; ++i) {
        LoiAP& l = g_lois[i];
        const TechIcons* ic = IconeDeLoi(i, e);
        int32_t c[2];
        if (!CaseDeLIcone(ic, atlas, c)) { ++sansCase; continue; }
        int32_t avant[2] = {};
        if (!SafeRead((void*)(l.adr + LOI_ICONE), avant, sizeof(avant))) continue;
        if (!l.caseSauvee) { l.caseOrigine[0] = avant[0]; l.caseOrigine[1] = avant[1]; l.caseSauvee = true; }
        if (avant[0] == c[0] && avant[1] == c[1]) continue;
        if (SafeWrite((void*)(l.adr + LOI_ICONE), c, sizeof(c))) ++changees;
    }
    static long nJournal = 0;
    if ((changees || sansCase) && nJournal < 20) {
        ++nJournal;
        printf("[LOIAP] icones : %d case(s) changee(s)%s\n", changees,
            sansCase ? ", atlas des lois sans case AP : rien pose" : "");
    }
}

// --- Sauvegarde locale ------------------------------------------------------------
bool Chemin(char* out, size_t taille)
{
    if (!g_partie[0] && !g_partie[1]) return false;
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_lois_%016llX%016llX.txt", base,
        (unsigned long long)g_partie[1], (unsigned long long)g_partie[0]);
    return true;
}

void Sauver()
{
    if (!g_partieChargee) return;   // jamais avant la relecture : on ecraserait le fichier
    char chemin[MAX_PATH], temp[MAX_PATH + 8];
    if (!Chemin(chemin, sizeof(chemin))) return;
    _snprintf_s(temp, sizeof(temp), _TRUNCATE, "%s.tmp", chemin);
    AcquireSRWLockExclusive(&g_verrou);
    FILE* f = nullptr;
    if (fopen_s(&f, temp, "w") != 0 || !f) {
        ReleaseSRWLockExclusive(&g_verrou);
        printf("[LOIAP] ecriture de la sauvegarde locale impossible\n");
        return;
    }
    for (int a = 0; a < g_nArbres; ++a) {
        if (g_arbres[a].recus) fprintf(f, "A\t%d\t%s\n", g_arbres[a].recus, g_arbres[a].nom);
        for (int r = 1; r <= MAX_RANG; ++r)
            if (g_arbres[a].checks >> r & 1) fprintf(f, "C\t%d\t%s\n", r, g_arbres[a].nom);
    }
    for (int i = 0; i < g_nLois; ++i) {
        if (g_lois[i].enregistree)
            fprintf(f, "E %016llX %016llX %016llX %016llX\n",
                (unsigned long long)g_lois[i].guid.a, (unsigned long long)g_lois[i].guid.b,
                (unsigned long long)g_lois[i].dateActivation, (unsigned long long)g_lois[i].dateApplication);
        if (g_lois[i].attente)
            fprintf(f, "P %016llX %016llX %016llX\n",
                (unsigned long long)g_lois[i].guid.a, (unsigned long long)g_lois[i].guid.b,
                (unsigned long long)g_lois[i].dateAttente);
    }
    fclose(f);
    MoveFileExA(temp, chemin, MOVEFILE_REPLACE_EXISTING);
    ReleaseSRWLockExclusive(&g_verrou);
}

// Oublie la partie precedente et relit celle de 'gid'. Renvoie le nombre de
// lignes lues, -1 sans fichier.
int Charger(const uint64_t gid[2])
{
    AcquireSRWLockExclusive(&g_verrou);
    g_partie[0] = gid[0];
    g_partie[1] = gid[1];
    for (int a = 0; a < g_nArbres; ++a) { g_arbres[a].recus = 0; g_arbres[a].checks = 0; }
    for (int i = 0; i < g_nLois; ++i) {
        g_lois[i].enregistree = false;
        g_lois[i].dateActivation = g_lois[i].dateApplication = 0;
        g_lois[i].attente = false;
        g_lois[i].dateAttente = 0;
        g_lois[i].presente = false;
        g_lois[i].retenue = false;
    }
    int n = -1;
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (Chemin(chemin, sizeof(chemin)) && fopen_s(&f, chemin, "r") == 0 && f) {
        n = 0;
        char ligne[256];
        while (fgets(ligne, sizeof(ligne), f)) {
            size_t l = strlen(ligne);
            while (l && (ligne[l - 1] == '\n' || ligne[l - 1] == '\r')) ligne[--l] = 0;
            int v = 0;
            char* nom = nullptr;
            if ((ligne[0] == 'A' || ligne[0] == 'C') && ligne[1] == '\t' &&
                (nom = strchr(ligne + 2, '\t')) != nullptr && sscanf_s(ligne + 2, "%d", &v) == 1) {
                int a = ArbreParNom(nom + 1);
                if (a < 0) { printf("[LOIAP] sauvegarde locale : arbre inconnu \"%s\"\n", nom + 1); continue; }
                if (ligne[0] == 'A') g_arbres[a].recus = v;
                else if (v >= 1 && v <= MAX_RANG) g_arbres[a].checks |= 1ull << v;
                ++n;
            } else if (ligne[0] == 'E') {
                unsigned long long ga = 0, gb = 0, da = 0, dp = 0;
                if (sscanf_s(ligne + 2, "%llx %llx %llx %llx", &ga, &gb, &da, &dp) != 4) continue;
                int i = IndexParGuid(Guid{ ga, gb });
                if (i < 0) continue;
                g_lois[i].enregistree = true;
                g_lois[i].dateActivation = da;
                g_lois[i].dateApplication = dp;
                ++n;
            } else if (ligne[0] == 'P') {
                unsigned long long ga = 0, gb = 0, da = 0;
                if (sscanf_s(ligne + 2, "%llx %llx %llx", &ga, &gb, &da) != 3) continue;
                int i = IndexParGuid(Guid{ ga, gb });
                if (i < 0) continue;
                g_lois[i].attente = true;
                g_lois[i].dateAttente = da;
                ++n;
            }
        }
        fclose(f);
    }
    g_partieChargee = true;
    ReleaseSRWLockExclusive(&g_verrou);
    return n;
}

void NoterApplication(int i, uint64_t dateActivation)
{
    g_lois[i].enregistree = true;
    g_lois[i].dateActivation = dateActivation;
    g_lois[i].dateApplication = DateCourante();
    g_lois[i].attente = false;
    g_lois[i].presente = true;
    g_lois[i].retenue = false;
}

void NoterAttente(int i, uint64_t dateActivation)
{
    g_lois[i].attente = true;
    g_lois[i].dateAttente = dateActivation;
    g_lois[i].presente = false;
    g_lois[i].retenue = true;
}

// Checks pas encore envoyes de la loi signee i : ses rangs FIXES (table de
// l'apworld, RangsFixes) ; repli, loi absente de la table : rangs
// r..r+poids-1 d'apres l'ordre des signatures (Rang). Renvoie le nombre de
// checks envoyes. (L'effet, lui, suit toujours l'ordre des signatures :
// k-ieme item recu = effet de la k-ieme loi signee, cf. hkEffets.)
int EnvoyerChecks(int i, const Etats& e, bool differe)
{
    ArbreAP& a = g_arbres[g_lois[i].arbre];
    char nom[96];
    NomDeLoi(i, nom, sizeof(nom));
    int rangs[4], nr = RangsFixes(i, rangs);
    if (nr == 0) {
        int r = Rang(i, e);
        printf("[LOIAP]   \"%s\" absente de la table des rangs : rang d'apres les signatures (%d)\n", nom, r);
        for (int k = r; k < r + Poids(i) && nr < 4; ++k) rangs[nr++] = k;
    }
    int n = 0;
    for (int x = 0; x < nr; ++x) {
        int k = rangs[x];
        if (k < 1 || k > MAX_RANG) continue;
        if (a.checks >> k & 1) { printf("[LOIAP]   check %s #%d deja envoye\n", a.nom, k); continue; }
        a.checks |= 1ull << k;
        SendLawCheck(a.nom, k, nom, differe);
        ++n;
    }
    return n;
}

uintptr_t AppelerEffets(uintptr_t sys, uintptr_t loi, bool& ok)
{
    ok = true;
    __try { return oEffets(sys, loi, 0, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; return 0; }
}

// Effets en attente dont l'item est arrive. Thread du jeu.
void AppliquerEnAttente()
{
    Etats e;
    if (!LireEtats(e)) { g_aTraiter = true; return; }
    bool modifie = false;
    for (int i = 0; i < g_nLois; ++i) {
        if (!e.Signee(i) || g_lois[i].presente) continue;
        int r = Rang(i, e);
        const ArbreAP& a = g_arbres[g_lois[i].arbre];
        if (a.recus < r) continue;
        bool ok = false;
        AppelerEffets(e.sys, g_lois[i].adr, ok);
        char nom[96];
        NomDeLoi(i, nom, sizeof(nom));
        if (!ok) { printf("[LOIAP] EXCEPTION pendant les effets de \"%s\"\n", nom); continue; }
        NoterApplication(i, e.date[i]);
        modifie = true;
        printf("[LOIAP] effet applique : \"%s\" (arbre \"%s\", rang %d, %d item(s) recu(s))\n",
            nom, a.nom, r, a.recus);
    }
    if (modifie) Sauver();
}

// Apres une (re)lecture : l'effet de chaque loi signee est-il deja dans la
// partie ? Pour la signature vue dans la sauvegarde (meme date d'activation) :
//  - E applique avant la date de la sauvegarde -> present ;
//  - P (effet retenu a la signature), ou E applique APRES la sauvegarde ->
//    en attente ;
//  - aucune trace -> loi signee hors du suivi du mod (partie commencee sans
//    lui, ou sauvegarde anterieure au mod) : le jeu a applique l'effet ->
//    present. Le test du 2026-10-31 l'a montre : sans cette regle, l'effet
//    de "Child Labour - Safe Jobs" (signee avant) a ete applique une 2e fois.
// Puis les checks des lois signees qui manquent sont envoyes (rattrapage).
// 'exclure' : loi en cours de signature (relecture faite depuis la signature
// elle-meme), laissee a hkEffets / ApresSignature.
void VerifierPresence(uint64_t dateReference, uintptr_t exclure)
{
    Etats e;
    if (!LireEtats(e)) return;
    int signees = 0, presentes = 0, horsSuivi = 0, checks = 0;
    for (int i = 0; i < g_nLois; ++i) {
        LoiAP& l = g_lois[i];
        l.presente = false;
        l.retenue = false;
        if (!e.Signee(i) || l.adr == exclure) continue;
        ++signees;
        bool memeE = l.enregistree && l.dateActivation == e.date[i];
        bool memeP = l.attente && l.dateAttente == e.date[i];
        if (memeE && l.dateApplication <= dateReference) l.presente = true;
        else if (memeE || memeP) l.presente = false;
        else {
            l.presente = true;
            l.enregistree = true;
            l.dateActivation = l.dateApplication = e.date[i];
            l.attente = false;
            ++horsSuivi;
            char nom[96];
            NomDeLoi(i, nom, sizeof(nom));
            printf("[LOIAP] \"%s\" signee hors du suivi du mod : effet tenu pour present\n", nom);
        }
        l.retenue = !l.presente;
        if (l.presente) ++presentes;
    }
    for (int i = 0; i < g_nLois; ++i)
        if (e.Signee(i) && g_lois[i].adr != exclure) checks += EnvoyerChecks(i, e, !g_lois[i].presente);
    printf("[LOIAP] %d loi(s) signee(s), %d avec leur effet present (dont %d hors suivi), "
           "%d check(s) rattrape(s) (date de reference %llX)\n",
        signees, presentes, horsSuivi, checks, (unsigned long long)dateReference);
    Sauver();
}

// Checks de la loi qui vient d'etre signee. Thread du jeu.
void ApresSignature(uintptr_t loi, bool differe)
{
    int i = IndexParAdresse(loi);
    if (i < 0 || !g_partieChargee) return;
    Etats e;
    if (!LireEtats(e) || !e.Signee(i)) return;
    char nom[96];
    NomDeLoi(i, nom, sizeof(nom));
    printf("[LOIAP] loi signee : \"%s\" (arbre \"%s\", rang %d, poids %d) -> effet %s\n",
        nom, g_arbres[g_lois[i].arbre].nom, Rang(i, e), Poids(i), differe ? "EN ATTENTE" : "applique");
    EnvoyerChecks(i, e, differe);
    Sauver();
    RafraichirIcones();
}

// Relit la sauvegarde locale si la partie a change (autre GameId, ou
// LoadGame termine). Thread du jeu. Appelee par la pompe ET par la
// signature : test du 2026-10-31, une partie neuve mise en pause des le
// debut, la pompe (qui ne tourne pas en pause) n'avait pas encore vu le
// nouveau GameId ; la loi signee en pause a ete decidee avec l'etat de la
// partie precedente, puis, a la reprise, prise pour une loi "signee hors
// suivi" (effet tenu pour present -> batiment debloque, effet jamais
// applique). 'exclure' : loi en cours de signature.
bool AssurerPartie(uintptr_t exclure)
{
    if (!BatirArbres()) return false;
    uint64_t gid[2] = {};
    uintptr_t scn = SystemeParId(IdSysteme::Scenario);
    if (!scn || !SafeRead((void*)(scn + 0x110), gid, sizeof(gid)) || (!gid[0] && !gid[1])) return false;
    bool autrePartie = !g_partieChargee || gid[0] != g_partie[0] || gid[1] != g_partie[1];
    if (autrePartie || g_rechargement) {
        bool apresChargement = g_rechargement;
        g_rechargement = false;
        int n = Charger(gid);
        printf("[LOIAP] partie %016llX%016llX : %s\n", (unsigned long long)gid[1], (unsigned long long)gid[0],
            n < 0 ? "aucune sauvegarde locale (nouvelle partie)" : "sauvegarde locale relue");
        VerifierPresence(apresChargement ? g_dateSauvegarde : DateCourante(), exclure);
        RafraichirIcones();
        g_aTraiter = true;
    }
    return true;
}

// --- Hooks ----------------------------------------------------------------------
uintptr_t hkEffets(uintptr_t sys, uintptr_t loi, uintptr_t p3, uintptr_t p4)
{
    if (t_signature && t_signature == loi && AssurerPartie(loi)) {
        int i = IndexParAdresse(loi);
        Etats e;
        if (i >= 0 && LireEtats(e) && e.Signee(i)) {
            int r = Rang(i, e);
            if (g_arbres[g_lois[i].arbre].recus < r) {
                t_differe = true;
                NoterAttente(i, e.date[i]);     // sauve par ApresSignature
                return 0;                       // effets retenus jusqu'a l'item
            }
            uintptr_t res = oEffets(sys, loi, p3, p4);
            NoterApplication(i, e.date[i]);
            return res;
        }
    }
    return oEffets(sys, loi, p3, p4);
}

// FUN_1416a71f0(prerequis, contexte, sortie) : evaluation d'un prerequis
// (tous types). Un prerequis "loi signee" qui n'appartient pas a une loi
// (batiment, ...) repond "non" tant que l'effet de la loi est retenu.
uintptr_t hkEvaluerPrerequis(uintptr_t req, uintptr_t ctx, uintptr_t sortie, uintptr_t p4)
{
    uintptr_t vt = 0;
    if (g_bati && g_vtReqLoi && req > 0x10000 && SafeRead((void*)req, &vt, 8) && vt == g_vtReqLoi) {
        Guid g{};
        if (SafeRead((void*)(req + REQ_LIEN), &g, sizeof(g))) {
            int i = IndexParGuid(g);
            if (i >= 0 && g_lois[i].retenue) {
                bool deLoi = EstPrerequisDeLoi(req);
                // Journal des 10 premieres evaluations. Test du 2026-10-31 :
                // le menu de construction (FUN_1412f07c0, systeme des
                // batiments, sur l'evenement de signature) et l'UI
                // (FUN_141b81f40) evaluent bien ce prerequis.
                static long nTraces = 0;
                if (nTraces < 10) {
                    ++nTraces;
                    printf("[LOIAP] evaluation #%ld : prerequis 0x%llX de \"%s\" (%s) -> %s\n", nTraces,
                        (unsigned long long)req, g_lois[i].nom, deLoi ? "prerequis de loi" : "hors lois",
                        deLoi ? "laisse au jeu" : "REFUSE");
                }
                if (!deLoi) return 0;
            }
        }
    }
    return oEvaluer(req, ctx, sortie, p4);
}

// --- Diagnostic : noeuds du livre des lois -----------------------------------------
// Test du 2026-10-31 (capture) : la case +0x80 posee par RafraichirIcones ne
// se voit pas. Loi non signee : rond vide ; loi signee : plume (case (4,4) de
// Icons.dds) dans un rond bleu. L'image "TECH_IMAGE" que FUN_141bbc320 remplit
// avec la case +0x80 n'est donc pas celle qu'on voit. On releve l'arbre
// d'elements d'un noeud signe et d'un noeud non signe a la construction du
// livre : nom, classe, visibilite, alpha, et texture / case de chaque image.
// Livre (FUN_141bbc320, this) : noeuds en +0x1B0 (compte +0x1B8, pas 0x80) :
// +0x00 GUID de la loi, +0x10 poignee de l'element du noeud (*poignee).
constexpr uintptr_t OFF_CONSTRUIRE_LIVRE = 0x1BBC320;
constexpr int LIVRE_NOEUDS = 0x1B0, NOEUD_PAS = 0x80, NOEUD_POIGNEE = 0x10;
constexpr int ELEM_NOM = 0x1D8, ELEM_ENFANT = 0x220, ELEM_FRERE = 0x240;
constexpr int ELEM_COULEUR = 0x120, ELEM_DRAPEAUX = 0x270;
Fonction4_t oConstruireLivre = nullptr;

// Textures portees par un element : qwords de +0x280 a +0x600 qui pointent
// vers un texte en ".dds" ou ".texture", suivis de la case et du nombre de
// cases (forme UITextureTile).
void DecrireTexturesElement(uintptr_t c, int profondeur)
{
    for (int off = 0x280; off < 0x600; off += 8) {
        uintptr_t p = 0;
        char t[96] = "";
        if (!SafeRead((void*)(c + off), &p, 8) || p <= 0x10000 || !ReadAscii(p, t, sizeof(t))) continue;
        if (!strstr(t, ".dds") && !strstr(t, ".texture") && !strstr(t, "Tiles")) continue;
        int32_t v[4] = {};
        SafeRead((void*)(c + off + 8), v, sizeof(v));
        printf("[LOIUI]     %*s  +0x%X texture \"%s\" case (%d,%d) sur (%d,%d)\n",
            profondeur * 2, "", off, t, v[0], v[1], v[2], v[3]);
    }
}

void NomElementLoi(uintptr_t c, char* out, int taille)
{
    uintptr_t pn = 0;
    out[0] = 0;
    if (SafeRead((void*)(c + ELEM_NOM), &pn, 8) && pn > 0x10000) ReadAscii(pn, out, taille);
}

void DecrireArbreElement(uintptr_t c, int profondeur, int* budget)
{
    for (; c > 0x10000 && *budget > 0; --*budget) {
        char nom[64] = "", cls[96] = "?";
        uintptr_t pn = 0, enfant = 0, frere = 0;
        if (SafeRead((void*)(c + ELEM_NOM), &pn, 8) && pn > 0x10000) ReadAscii(pn, nom, sizeof(nom));
        GetClassNameFP(c, cls, sizeof(cls));
        uint32_t drapeaux = 0;
        float coul[4] = {};
        SafeRead((void*)(c + ELEM_DRAPEAUX), &drapeaux, 4);
        SafeRead((void*)(c + ELEM_COULEUR), coul, sizeof(coul));
        float tr[2] = {}, t[2] = {};
        SafeRead((void*)(c + 0x80), tr, sizeof(tr));    // translation
        SafeRead((void*)(c + 0x210), t, sizeof(t));     // taille
        printf("[LOIUI]     %*s\"%s\" %s 0x%llX %s couleur (%.2f %.2f %.2f a=%.2f) tr (%.1f, %.1f) taille %.1f x %.1f\n",
            profondeur * 2, "", nom, cls, (unsigned long long)c, (drapeaux & 1) ? "visible" : "CACHE",
            coul[0], coul[1], coul[2], coul[3], tr[0], tr[1], t[0], t[1]);
        DecrireTexturesElement(c, profondeur);
        SafeRead((void*)(c + ELEM_ENFANT), &enfant, 8);
        if (enfant && profondeur < 10) DecrireArbreElement(enfant, profondeur + 1, budget);
        if (profondeur == 0) return;   // pas les freres du noeud (1er releve : budget epuise par eux)
        if (!SafeRead((void*)(c + ELEM_FRERE), &frere, 8)) return;
        c = frere;
    }
}

// --- Icone AP sur les noeuds du livre ------------------------------------------------
// Releve du 2026-10-31 : le noeud (TECH_ENTRY / LEFT_OR_ENTRY / RIGHT_OR_ENTRY,
// UIButton) n'a AUCUN enfant "TECH_IMAGE" : la case +0x80 n'est jamais
// affichee. La plume d'une loi signee = "circle_inside" (UIPicture, dans
// STANDARD_NODE / STATE_ELEM), visible si signee, CACHE sinon, couleur
// (0.13, 0.78, 0.84). On y pose notre tuile (texture des lois, config
// +0x328 = UI/Tiles/Icons.dds, cases bulleCol / bulleRow) par la fonction
// du jeu FUN_1410260c0(element, &nom, tuile, x, y, nx, ny) (recherche
// recursive de l'enfant, comme pour l'icone du cercle des messages), puis on
// la montre (FUN_14105ac90) et on la remet en blanc (FUN_1410297f0), pour
// TOUS les noeuds. Thread du jeu (construction du livre).
typedef void(*NomConstruire_t)(uintptr_t* nom, const char* texte);
typedef void(*NomDetruire_t)(uintptr_t* nom);
typedef uintptr_t(*PoserTuileEnfant_t)(uintptr_t, uintptr_t, uintptr_t, uint32_t, uint32_t, uint32_t, uint32_t, uintptr_t);
typedef void(*Montrer_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void(*PoserCouleur_t)(uintptr_t, const float*);
typedef void(*PoserTaille_t)(uintptr_t, float, float);   // FUN_14102b170 : +0x210 / +0x214
constexpr uintptr_t OFF_POSER_TAILLE = 0x102B170;
typedef void(*PoserPosition_t)(uintptr_t, const float*);   // FUN_14102a3e0 : translation (x, y, z, w), en file
constexpr uintptr_t OFF_POSER_POSITION = 0x102A3E0;
constexpr int ELEM_TAILLE = 0x210, ELEM_TRANSLATION = 0x80;
constexpr float ICONE_REDUCTION = 3.f;
// 4e test : -4 trop haut (0 etait un chouilla trop bas).
constexpr float ICONE_DECALAGE_Y = -2.f;   // unites d'interface ; negatif = vers le haut
constexpr uintptr_t OFF_NOM_CONSTRUIRE = 0xE4FA20, OFF_NOM_DETRUIRE = 0xE4EAB0;
constexpr uintptr_t OFF_POSER_TUILE_ENFANT = 0x10260C0, OFF_MONTRER = 0x105AC90, OFF_POSER_COULEUR = 0x10297F0;
const char* const IMAGE_NOEUD = "circle_inside";

// Geometrie d'origine d'une image de noeud, relevee sur la 1re rencontree
// (les noeuds d'une meme sorte sont identiques ; FUN_14102b170 peut aussi
// ecrire dans la recette partagee : on repart toujours de cette reference).
struct GeoImage { const char* nom; float taille[2]; float tr[2]; bool vue; };
GeoImage g_geoInside = { "circle_inside", {}, {}, false };
// Paires exclusives (LEFT_OR_ENTRY / RIGHT_OR_ENTRY) : pas de "circle_inside"
// (releve du 2026-10-31) ; "BlurNotAvaiable" essaye puis abandonne
// (invisible) : voir PoserIconePaire.
const char* const NOEUD_GAUCHE_PAIRE = "LEFT_OR_ENTRY";
uintptr_t g_modeleInside[2] = {};   // recette (+0x1E8), ressource (+0x1F0) de "circle_inside"

typedef uintptr_t(*Instancier_t)(uintptr_t recette, uintptr_t ressource, uint32_t drapeaux, uintptr_t ctx);
typedef void(*AjouterEnfant_t)(uintptr_t parent, uintptr_t enfant);
constexpr uintptr_t OFF_INSTANCIER = 0x110C780, OFF_CTX_INSTANCIER = 0x2BE0438, OFF_AJOUTER_ENFANT = 0x1023D70;
constexpr int ELEM_RECETTE = 0x1E8, ELEM_RESSOURCE = 0x1F0;
// Pose directe d'une tuile sur une image (corps de FUN_1410260c0, sans la
// recherche par nom) : FUN_141107140(image, texture, 0, 0) puis
// FUN_141107360(image, x, y, nx, ny, &DAT_142929200, &DAT_142a12d90, &octet nul).
typedef void(*PoserTexture_t)(uintptr_t, uintptr_t, uintptr_t, uint32_t);
typedef void(*PoserCase_t)(uintptr_t, uint32_t, uint32_t, uint32_t, uint32_t, uintptr_t, uintptr_t, uint8_t*);
constexpr uintptr_t OFF_POSER_TEXTURE = 0x1107140, OFF_POSER_CASE = 0x1107360;
constexpr uintptr_t OFF_CASE_ARG6 = 0x2929200, OFF_CASE_ARG7 = 0x2A12D90;

void PoserTuileSur(uintptr_t image, const TechIcons* ic)
{
    int32_t nb[2] = {};
    SafeRead((void*)(Base() + OFF_SOCIETY_CONFIG + CFG_ICONES_CASES), nb, sizeof(nb));
    ((PoserTexture_t)(Base() + OFF_POSER_TEXTURE))(image, Base() + OFF_SOCIETY_CONFIG + CFG_ICONES_TEXTURE, 0, 0);
    uint8_t zero[16] = {};
    ((PoserCase_t)(Base() + OFF_POSER_CASE))(image, (uint32_t)ic->bulleCol, (uint32_t)ic->bulleRow,
        (uint32_t)nb[0], (uint32_t)nb[1], Base() + OFF_CASE_ARG6, Base() + OFF_CASE_ARG7, zero);
}

// Recherche recursive d'un element par nom ; rend aussi son parent.
bool TrouverElement(uintptr_t el, const char* nom, uintptr_t& parent, uintptr_t& trouve, int profondeur = 0)
{
    uintptr_t c = 0;
    if (!SafeRead((void*)(el + ELEM_ENFANT), &c, 8)) return false;
    for (int g = 0; c > 0x10000 && g < 64; ++g) {
        char nm[64];
        NomElementLoi(c, nm, sizeof(nm));
        if (!strcmp(nm, nom)) { parent = el; trouve = c; return true; }
        if (profondeur < 10 && TrouverElement(c, nom, parent, trouve, profondeur + 1)) return true;
        if (!SafeRead((void*)(c + ELEM_FRERE), &c, 8)) return false;
    }
    return false;
}

// Notre copie de "circle_inside" deja ajoutee a ce parent (enfant direct du
// meme nom, autre que l'original), sinon 0 : pas de doublon si le meme noeud
// est reconstruit.
uintptr_t CopieExistante(uintptr_t parent, uintptr_t original)
{
    uintptr_t c = 0;
    if (!SafeRead((void*)(parent + ELEM_ENFANT), &c, 8)) return 0;
    for (int g = 0; c > 0x10000 && g < 64; ++g) {
        char nm[64];
        NomElementLoi(c, nm, sizeof(nm));
        if (c != original && !strcmp(nm, IMAGE_NOEUD)) return c;
        if (!SafeRead((void*)(c + ELEM_FRERE), &c, 8)) return 0;
    }
    return 0;
}

// Copie de "circle_inside" (recette +0x1E8 / ressource +0x1F0 relevees sur
// un noeud simple, instanciee par FUN_14110c780, ajoutee par FUN_141023d70),
// tuile AP posee directement, montree, blanche, au tiers de 87 (nos cases
// remplissent leurs 128 px), centree sur (cx, cy). La position posee est celle
// du coin haut-gauche.
// 4e test (2026-10-31) : posee sur l'original, l'icone d'une loi SIGNEE etait
// verdatre : l'animation d'etat du noeud reteint "circle_inside" en
// (0.13, 0.78, 0.84) apres notre blanc, et la teinte multiplie l'icone. La
// copie, ajoutee apres l'instanciation du noeud, n'est pas liee a ses
// animations (la copie des paires gardait ses couleurs).
// 5e test (2026-10-31) : la copie est encore teintee, et un hook du poseur
// de couleur FUN_1410297f0 n'a vu AUCUN appel sur nos copies. La teinte vient
// de l'animateur d'etat (element +0x1E8, ce qu'on appelait la « recette ») :
// FUN_1410240d0 joue l'etat du noeud (signe, survol...) sur l'element par
// animateur->vtable[0x138], puis RECURSIVEMENT sur ses enfants ; notre copie
// porte l'animateur de "circle_inside" et rejoue donc sa teinte. On met
// l'animateur de la copie a 0 (FUN_1410240d0 teste ce pointeur) : plus aucun
// etat ne la touche. L'animateur copie est perdu (fuite minime).
uintptr_t NouvelleCopie(uintptr_t parent)
{
    uintptr_t copie = ((Instancier_t)(Base() + OFF_INSTANCIER))(g_modeleInside[0], g_modeleInside[1], 1,
        Base() + OFF_CTX_INSTANCIER);
    if (!copie) return 0;
    uintptr_t zero = 0;
    SafeWrite((void*)(copie + ELEM_RECETTE), &zero, 8);
    ((AjouterEnfant_t)(Base() + OFF_AJOUTER_ENFANT))(parent, copie);
    return copie;
}

uintptr_t IconeSurCopie(uintptr_t parent, uintptr_t original, const TechIcons* ic, float cx, float cy,
    bool nouvelle = false)
{
    uintptr_t copie = nouvelle ? 0 : CopieExistante(parent, original);
    if (!copie) copie = NouvelleCopie(parent);
    if (!copie) return 0;
    PoserTuileSur(copie, ic);
    static const float blanc[4] = { 1.f, 1.f, 1.f, 1.f };
    ((Montrer_t)(Base() + OFF_MONTRER))(copie, 1, 0, 1);
    ((PoserCouleur_t)(Base() + OFF_POSER_COULEUR))(copie, blanc);
    float l = g_geoInside.taille[0] / ICONE_REDUCTION, h = g_geoInside.taille[1] / ICONE_REDUCTION;
    ((PoserTaille_t)(Base() + OFF_POSER_TAILLE))(copie, l, h);
    float pos[4] = { cx - l / 2.f, cy - h / 2.f, 0.f, 1.f };
    ((PoserPosition_t)(Base() + OFF_POSER_POSITION))(copie, pos);
    return copie;
}

// Noeud simple : l'original "circle_inside" (plume du jeu, visible si la loi
// est signee) est reduit a une taille nulle, et notre copie prend sa place
// dans le meme parent (STATE_ELEM). Renvoie la copie, 0 si echec.
uintptr_t PoserIconeNoeud(uintptr_t elem, const TechIcons* ic)
{
    uintptr_t parent = 0, original = 0;
    if (!TrouverElement(elem, IMAGE_NOEUD, parent, original)) return 0;
    __try {
        if (!g_modeleInside[0]) {
            SafeRead((void*)(original + ELEM_RECETTE), &g_modeleInside[0], 8);
            SafeRead((void*)(original + ELEM_RESSOURCE), &g_modeleInside[1], 8);
        }
        if (!g_geoInside.vue) {
            float t[2] = {}, tr[2] = {};
            SafeRead((void*)(original + ELEM_TAILLE), t, sizeof(t));
            SafeRead((void*)(original + ELEM_TRANSLATION), tr, sizeof(tr));
            if (t[0] > 0.f) {
                g_geoInside.vue = true;
                g_geoInside.taille[0] = t[0]; g_geoInside.taille[1] = t[1];
                g_geoInside.tr[0] = tr[0];    g_geoInside.tr[1] = tr[1];
                printf("[LOIUI] \"%s\" : taille d'origine %.1f x %.1f, translation (%.1f, %.1f)\n",
                    IMAGE_NOEUD, t[0], t[1], tr[0], tr[1]);
            }
        }
        if (!g_geoInside.vue || !g_modeleInside[0]) return 0;
        ((PoserTaille_t)(Base() + OFF_POSER_TAILLE))(original, 0.f, 0.f);
        // "circle_inside" (87 x 87 en (-13, -28)) : le centre du rond est
        // (-13 + 43.5) sur les DEUX axes (2e test), + ICONE_DECALAGE_Y.
        float cx = g_geoInside.tr[0] + g_geoInside.taille[0] / 2.f;
        return IconeSurCopie(parent, original, ic, cx, cx + ICONE_DECALAGE_Y);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[LOIUI] EXCEPTION 0x%08lX a la pose d'une icone de noeud\n", GetExceptionCode());
        return 0;
    }
}

// --- Paires : notre propre image -----------------------------------------------------
// Au demi-rond GAUCHE, une copie de "circle_inside" centree sur le rond de la
// paire : chaine Clip -> STATE_ELEM -> defoult -> circle_bg.
// 4e test (2026-10-31) : centre calcule (129.5, 57.2) avec la demi-taille du
// rond, icone affichee ~(48, 47) trop a droite et trop bas, soit une
// demi-taille de rond (~95 / 2) : la translation de "circle_bg" est deja son
// centre. On ne cumule donc plus que les translations.
bool CentrePaire(uintptr_t racine, float c[2])
{
    static const char* const chemin[] = { "Clip", "STATE_ELEM", "defoult", "circle_bg" };
    static int nJournal = 0;
    bool journal = nJournal++ < 1;
    uintptr_t el = racine;
    c[0] = c[1] = 0.f;
    for (const char* n : chemin) {
        uintptr_t enfant = 0;
        if (!SafeRead((void*)(el + ELEM_ENFANT), &enfant, 8)) return false;
        el = 0;
        for (int g = 0; enfant > 0x10000 && g < 64; ++g) {
            char nm[64];
            NomElementLoi(enfant, nm, sizeof(nm));
            if (!strcmp(nm, n)) { el = enfant; break; }
            if (!SafeRead((void*)(enfant + ELEM_FRERE), &enfant, 8)) break;
        }
        if (!el) return false;
        float tr[2] = {}, t[2] = {};
        SafeRead((void*)(el + ELEM_TRANSLATION), tr, sizeof(tr));
        SafeRead((void*)(el + ELEM_TAILLE), t, sizeof(t));
        if (journal) printf("[LOIUI] paire : \"%s\" translation (%.1f, %.1f) taille %.1f x %.1f\n", n, tr[0], tr[1], t[0], t[1]);
        c[0] += tr[0]; c[1] += tr[1];
    }
    return true;
}

// 5e test (2026-10-31) : les translations cumulees ne donnent pas le centre
// (ancres / echelle des elements de la paire inconnues). Centre mesure sur
// deux captures de l'auteur (point rouge = centre du rond) : icone posee en
// (129.5, 53.2) -> (+59, +53) px du centre, en (86, 11.7) -> (-37, -51) px ;
// interpolation lineaire -> centre en (103, 32) dans le repere du demi-rond
// gauche (meme geometrie pour toutes les paires : chaine journalisee).
// 6e test : moyenne des 3 mesures (moindres carres) -> (101, 35).
constexpr float PAIRE_CENTRE[2] = { 101.f, 35.f };   // repere du demi-rond (abandonne)
// 9e test : en (-18, -7), icone ~(-16, +2) px du centre -> (-5, -9).
// 11e test : en (-5, -9), icone ~(-10, -3) px du centre (capture a ~1.55 px
// par unite : l'icone de 29 y fait ~45 px) -> (2, -7).
// 12e test : ~4 px trop haut -> y = -5 ; l'auteur : ~2 px trop a droite
// (~1.3 unite a 1.55 px / unite) -> x = 0.7.
// 13e test : l'auteur demande 2 px plus bas (~1.3 unite) -> y = -3.7.
// 14e test (capture plein ecran 1920, ~1.65 px / unite) : 3 px plus bas
// demandes (~1.8 unite) -> y = -1.9.
constexpr float PAIRE_DECALAGE[2] = { 0.7f, -1.9f };  // repere de TREE_PANEL, depuis la translation de la racine
// 6e test : enfant du demi-rond GAUCHE, l'icone passait SOUS le demi-rond
// droit (dessine apres). Elle est donc ajoutee en DERNIER au parent commun
// des noeuds (racine +0x230, cf. FUN_141023e70 : +0x230 parent, +0x228
// dernier enfant, +0x238 / +0x240 freres), a la translation de la racine
// + PAIRE_CENTRE (on suppose la racine sans echelle : translation journalisee
// avec les 0x40 octets qui suivent). Le parent survit peut-etre a la
// reconstruction du livre : nos anciennes copies y sont detruites
// (vtable +0xE0) si elles y sont encore rattachees.
constexpr int ELEM_PARENT = 0x230;
typedef void(*DetruireElem_t)(uintptr_t element);
struct CopiePaire { uintptr_t copie, parent; };
CopiePaire g_copiesPaires[64];
int g_nCopiesPaires = 0;

void DetruireCopiesPaires()
{
    for (int k = 0; k < g_nCopiesPaires; ++k) {
        uintptr_t p = 0, vt = 0, fn = 0;
        CopiePaire& c = g_copiesPaires[k];
        if (!SafeRead((void*)(c.copie + ELEM_PARENT), &p, 8) || p != c.parent) continue;   // partie avec le livre
        if (!SafeRead((void*)c.copie, &vt, 8) || !SafeRead((void*)(vt + 0xE0), &fn, 8) || !fn) continue;
        __try { ((DetruireElem_t)fn)(c.copie); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            printf("[LOIUI] EXCEPTION 0x%08lX a la destruction d'une ancienne copie de paire\n", GetExceptionCode());
        }
    }
    g_nCopiesPaires = 0;
}

uintptr_t PoserIconePaire(uintptr_t racine, const TechIcons* ic)
{
    if (!g_modeleInside[0] || !g_geoInside.vue) return 0;
    float centre[2];
    if (!CentrePaire(racine, centre)) return 0;   // journal de la chaine seulement
    uintptr_t parent = 0;
    if (!SafeRead((void*)(racine + ELEM_PARENT), &parent, 8) || parent <= 0x10000) return 0;
    float tr[16] = {}, taille[2] = {}, tPanneau[2] = {};
    SafeRead((void*)(racine + ELEM_TRANSLATION), tr, sizeof(tr));
    SafeRead((void*)(racine + ELEM_TAILLE), taille, sizeof(taille));
    SafeRead((void*)(parent + ELEM_TAILLE), tPanneau, sizeof(tPanneau));
    // 7e test : en tr + (101, 35) l'icone n'est plus du tout centree.
    // 8e test : en tr - 50 + (101, 35) = tr + (51, -15), capture : icone a
    // (+84, -10) px du centre, soit (+69, -8) unites (~1.22 px / unite :
    // l'icone de 29 fait ~35 px) -> centre du rond = tr + (-18, -7) dans
    // TREE_PANEL. La racine du demi-rond gauche est donc a peu pres au centre
    // de la paire.
    centre[0] = tr[0] + PAIRE_DECALAGE[0];
    centre[1] = tr[1] + PAIRE_DECALAGE[1];
    uintptr_t image = 0;
    __try {
        image = IconeSurCopie(parent, 0, ic, centre[0], centre[1], true);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[LOIUI] EXCEPTION 0x%08lX a la pose de l'icone d'une paire\n", GetExceptionCode());
        return 0;
    }
    if (image && g_nCopiesPaires < 64) g_copiesPaires[g_nCopiesPaires++] = { image, parent };
    static int nJournal = 0;
    if (nJournal++ < 3) {
        char nm[64], cl[96] = "?";
        NomElementLoi(parent, nm, sizeof(nm));
        GetClassNameFP(parent, cl, sizeof(cl));
        printf("[LOIUI] paire : copie 0x%llX dans \"%s\" (%s) 0x%llX, centree sur (%.1f, %.1f)\n",
            (unsigned long long)image, nm, cl, (unsigned long long)parent, centre[0], centre[1]);
        printf("[LOIUI]   taille de la racine %.1f x %.1f, du panneau %.1f x %.1f\n",
            taille[0], taille[1], tPanneau[0], tPanneau[1]);
        printf("[LOIUI]   racine +0x80..+0xBC :");
        for (int k = 0; k < 16; ++k) printf(" %.2f", tr[k]);
        printf("\n");
    }
    return image;
}

// Icone d'une paire exclusive : "icon_done" si l'une des deux lois est
// active, sinon celle du prochain check de l'arbre (la paire = un check).
const TechIcons* IconeDePaire(int i, const Etats& e)
{
    if (g_lois[i].exclusion[0])
        for (int j = 0; j < g_nLois; ++j)
            if (j != i && e.etat[j] == ETAT_ACTIVE && g_lois[j].arbre == g_lois[i].arbre &&
                !strcmp(g_lois[j].exclusion, g_lois[i].exclusion))
                return IconeDeLoi(j, e);
    return IconeDeLoi(i, e);
}

// Nom du noeud = nom du check (demande de l'auteur, 2026-11-01, comme
// l'arbre des technos) : le nom de l'arbre, + " [Signed]" pour une loi
// signee. Le rang du check n'est PAS affiche (decision de l'auteur,
// 2026-11-01 : il ne pourra pas etre tenu a jour quand le check sera chez un
// autre joueur). Meme nom pour toutes les lois d'un arbre et pour les deux
// demi-ronds d'une paire. Pose en texte brut (FUN_14111bef0) sur l'enfant
// "TECH_NAME" du noeud, apres la construction du livre.
typedef void(*PoserTexteBrut_t)(uintptr_t texte, const wchar_t* chaine, uintptr_t cfg, uintptr_t p4);
constexpr uintptr_t OFF_POSER_TEXTE_BRUT = 0x111BEF0, OFF_FORMAT_TEXTE_BRUT = 0x2BFD628;

// Reseau (bug du 2026-11-04) : le nom du check = l'item place sur la location
// (scout, ApScoutLoi) de SON rang fixe (RangAffiche, 2026-11-04 : signee ou
// non, une loi garde son check). Sans scout (jamais connecte, location
// absente) : le nom de l'arbre, comme avant.
void PoserTexteNomCheck(uintptr_t texte, int i, const Etats& e)
{
    bool signee = e.etat[i] == ETAT_ACTIVE;
    const char* arbre = g_arbres[g_lois[i].arbre].nom;
    int rang = RangAffiche(i, e);
    char check[200];
    ApScoutInfo s;
    if (ApScoutLoi(arbre, rang, &s)) ApNomDuScout(s, check, sizeof(check));
    else strcpy_s(check, arbre);
    char nom[220];
    sprintf_s(nom, "%s%s", check, signee ? " [Signed]" : "");
    wchar_t w[220];
    int j = 0;
    for (; nom[j] && j < 219; ++j) w[j] = (wchar_t)(unsigned char)nom[j];
    w[j] = 0;
    ((PoserTexteBrut_t)(Base() + OFF_POSER_TEXTE_BRUT))(texte, w, Base() + OFF_FORMAT_TEXTE_BRUT, 0);
}

void PoserNomCheckLoi(uintptr_t elem, int i, const Etats& e)
{
    uintptr_t parent = 0, texte = 0;
    if (!TrouverElement(elem, "TECH_NAME", parent, texte) || texte <= 0x10000) return;
    PoserTexteNomCheck(texte, i, e);
}

// Position (x, y) de 'el' dans le repere du parent de 'racine' : somme des
// translations (+0x80) de 'el' jusqu'a 'racine' comprise (echelles ignorees).
bool PositionDansPaire(uintptr_t racine, uintptr_t el, float out[2])
{
    out[0] = out[1] = 0.f;
    for (int garde = 0; el > 0x10000 && garde < 16; ++garde) {
        float tr[2] = {};
        if (!SafeRead((void*)(el + ELEM_TRANSLATION), tr, sizeof(tr))) return false;
        out[0] += tr[0];
        out[1] += tr[1];
        if (el == racine) return true;
        if (!SafeRead((void*)(el + ELEM_PARENT), &el, 8)) return false;
    }
    return false;
}

// Paire exclusive (demande de l'auteur, 2026-11-04) : le seul texte, celui
// du demi-rond gauche, est recentre sous le rond = a mi-chemin de la place du
// texte gauche et de celle du texte droit (meme mise en page en miroir : les
// ancres inconnues s'annulent). Decalage recalcule a chaque construction du
// livre : 0 si le texte est deja centre.
void CentrerNomPaire(uintptr_t gauche, uintptr_t droite)
{
    uintptr_t pG = 0, tG = 0, pD = 0, tD = 0;
    if (!TrouverElement(gauche, "TECH_NAME", pG, tG) || tG <= 0x10000 ||
        !TrouverElement(droite, "TECH_NAME", pD, tD) || tD <= 0x10000) return;
    float xG[2], xD[2], tr[4] = {};
    if (!PositionDansPaire(gauche, tG, xG) || !PositionDansPaire(droite, tD, xD) ||
        !SafeRead((void*)(tG + ELEM_TRANSLATION), tr, sizeof(tr))) return;
    float dx = (xD[0] - xG[0]) / 2.f;
    static int nJournal = 0;
    if (nJournal++ < 4)
        printf("[LOIUI] paire : texte gauche en (%.1f, %.1f), droit en (%.1f, %.1f) -> decalage %.1f\n",
            xG[0], xG[1], xD[0], xD[1], dx);
    if (dx > -0.5f && dx < 0.5f) return;
    float pos[4] = { tr[0] + dx, tr[1], tr[2], tr[3] };
    ((PoserPosition_t)(Base() + OFF_POSER_POSITION))(tG, pos);
}

// Demi-rond droit d'une paire : TECH_NAME sans texte (le nom du check est sur
// le demi-rond gauche). Test du 2026-11-04 : une chaine VIDE fait revenir le
// texte d'origine (nom de la loi, ex. "Child Shelters"), comme le champ du
// menu qui affichait "New save" : on pose un espace.
void EffacerNomLoi(uintptr_t elem)
{
    uintptr_t parent = 0, texte = 0;
    if (!TrouverElement(elem, "TECH_NAME", parent, texte) || texte <= 0x10000) return;
    ((PoserTexteBrut_t)(Base() + OFF_POSER_TEXTE_BRUT))(texte, L" ", Base() + OFF_FORMAT_TEXTE_BRUT, 0);
}

// Panneau d'info d'une loi selectionnee (demande de l'auteur, 2026-11-01) :
// FUN_141bc1440(livre) (appele par la selection FUN_141bc02f0, decompile le
// 2026-11-01) pose le nom de la loi selectionnee (GUID en livre +0x1C8,
// texte `loi +0x68`) sur TECH_NAME_TEXT (poignee livre +0x130) par
// FUN_14111bef0. Apres l'original, meme nom que sur le noeud : le check, si
// la loi est encore accessible.
constexpr uintptr_t OFF_INFO_LOI = 0x1BC1440;
constexpr int LIVRE_SELECTION = 0x1C8, LIVRE_NOM_INFO = 0x130;
Fonction4_t oInfoLoi = nullptr;

uintptr_t hkInfoLoi(uintptr_t livre, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oInfoLoi(livre, p2, p3, p4);
    __try {
        if (!g_bati) return r;
        Guid g{};
        if (!SafeRead((void*)(livre + LIVRE_SELECTION), &g, sizeof(g))) return r;
        int i = IndexParGuid(g);
        uintptr_t p = 0, texte = 0;
        if (i < 0 || !SafeRead((void*)(livre + LIVRE_NOM_INFO), &p, 8) || p <= 0x10000 ||
            !SafeRead((void*)p, &texte, 8) || texte <= 0x10000) return r;
        Etats e;
        if (!LireEtats(e)) return r;
        Accessibilite acc;
        acc.e = &e;
        memset(acc.etat, 0, sizeof(acc.etat));
        if (!Accessible(acc, i)) return r;
        PoserTexteNomCheck(texte, i, e);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static int n = 0;
        if (n++ < 5) printf("[LOIUI] EXCEPTION 0x%08lX au nom du panneau d'info\n", GetExceptionCode());
    }
    return r;
}

void DiagNoeudsLivre(uintptr_t livre)
{
    static int nAppels = 0;
    ++nAppels;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(livre + LIVRE_NOEUDS, tab, n)) { printf("[LOIUI] livre 0x%llX : noeuds illisibles\n", (unsigned long long)livre); return; }
    DetruireCopiesPaires();   // celles d'une construction precedente, si leur parent a survecu
    Etats e;
    bool etats = g_bati && LireEtats(e);
    Accessibilite acc;
    acc.e = &e;
    memset(acc.etat, 0, sizeof(acc.etat));
    bool journal = nAppels <= 4;
    if (journal) printf("[LOIUI] construction du livre #%d (0x%llX) : %d noeud(s)\n", nAppels, (unsigned long long)livre, n);
    bool signeVu = false, nonSigneVu = false;
    uintptr_t paires[64];
    const TechIcons* iconesPaires[64];
    int nPaires = 0;
    // Demi-ronds portant un nom (0 gauche) ou vides (1 droite), pour recentrer
    // le texte de chaque paire (CentrerNomPaire).
    uintptr_t demi[2][64];
    int loiDemi[2][64], nDemi[2] = {};
    for (int k = 0; k < n; ++k) {
        uintptr_t noeud = tab + (uintptr_t)k * NOEUD_PAS;
        Guid g{};
        uintptr_t poignee = 0, elem = 0;
        SafeRead((void*)noeud, &g, sizeof(g));
        if (SafeRead((void*)(noeud + NOEUD_POIGNEE), &poignee, 8) && poignee > 0x10000) SafeRead((void*)poignee, &elem, 8);
        int i = g_bati ? IndexParGuid(g) : -1;
        int etat = (i >= 0 && etats) ? e.etat[i] : -1;
        char nom[96] = "?";
        if (i >= 0) NomDeLoi(i, nom, sizeof(nom));
        if (elem <= 0x10000) {
            if (journal) printf("[LOIUI]   noeud %d : \"%s\" etat %d : AUCUN element d'interface (poignee 0x%llX)\n",
                k, nom, etat, (unsigned long long)poignee);
            continue;
        }
        char racine[32] = "";
        NomElementLoi(elem, racine, sizeof(racine));
        bool gauchePaire = !strcmp(racine, NOEUD_GAUCHE_PAIRE);
        const TechIcons* ic = (i >= 0 && etats) ? (gauchePaire ? IconeDePaire(i, e) : IconeDeLoi(i, e)) : nullptr;
        // Loi qu'on ne pourra plus signer (branche exclue) : pas d'icone AP.
        if (ic && !Accessible(acc, i)) {
            ic = nullptr;
            if (journal) printf("[LOIUI]   noeud %d : \"%s\" inaccessible (branche exclue) : pas d'icone\n", k, nom);
        }
        // Nom du noeud : celui de la loi, vanilla (changement de plan de
        // l'auteur, 2026-11-04 : le nom du check n'est plus que dans le
        // panneau d'info, hkInfoLoi). Plus de PoserNomCheckLoi ni
        // d'EffacerNomLoi ici ; demi[] reste vide, donc pas de recentrage
        // des paires (CentrerNomPaire), chaque demi-rond garde son nom.
        // 10e test (2026-10-31) : une fois la loi d'une paire signee, la plume
        // du jeu s'affiche exactement au centre du rond (sous notre icone).
        // Releve, une fois par cote, de l'arbre (avec translations et
        // tailles) du demi-rond de la loi SIGNEE, pour trouver cette image.
        {
            static bool vuGauche = false, vuDroite = false;
            bool droitePaire = !strcmp(racine, "RIGHT_OR_ENTRY");
            bool& vu = gauchePaire ? vuGauche : vuDroite;
            if ((gauchePaire || droitePaire) && etat == ETAT_ACTIVE && !vu) {
                vu = true;
                printf("[LOIUI]   arbre du demi-rond SIGNE \"%s\" (%s) :\n", nom, racine);
                int budget = 120;
                DecrireArbreElement(elem, 0, &budget);
            }
        }
        // Diagnostic (2026-11-04, test grandeur nature) : dans A New Home, le
        // demi-rond droit de Child Labour ("Child Shelters") n'a ni nom ni
        // clic, alors que les autres paires marchent. Releve, une fois par
        // loi, de l'arbre d'elements de ses deux demi-ronds et d'un demi-rond
        // droit sain ("Food Additives") pour comparaison, avec l'etat de la
        // loi et son accessibilite.
        {
            static bool vus[3] = {};
            static const char* const suivis[3] = { "Child Shelters", "Child Labour - Safe Jobs", "Food Additives" };
            for (int s = 0; s < 3; ++s) {
                if (vus[s] || strcmp(nom, suivis[s]) != 0) continue;
                vus[s] = true;
                printf("[LOIDIAG] demi-rond \"%s\" (%s) : loi %d, etat %d, accessible %d, livre #%d\n",
                    nom, racine, i, etat, (i >= 0 && etats) ? (int)Accessible(acc, i) : -1, nAppels);
                int budget = 60;
                DecrireArbreElement(elem, 0, &budget);
            }
        }
        float trRacine[2] = {};
        SafeRead((void*)(elem + 0x80), trRacine, sizeof(trRacine));
        // Paires : apres les noeuds simples (il faut le modele "circle_inside").
        // 10e test : quand une loi de la paire est signee, le livre l'affiche
        // en noeud SIMPLE (TECH_ENTRY, deja icon_done au centre) et l'autre
        // loi reste en demi-rond : notre icone de paire faisait doublon.
        // Plus d'icone de paire dans ce cas.
        if (gauchePaire) {
            bool partenaireSigne = i >= 0 && etats && IconeDePaire(i, e) != IconeDeLoi(i, e);
            if (ic && !partenaireSigne && nPaires < 64) { paires[nPaires] = elem; iconesPaires[nPaires++] = ic; }
            if (journal) printf("[LOIUI]   noeud %d : \"%s\" (%s) etat %d -> %s (paire%s), racine en (%.1f, %.1f)\n",
                k, nom, racine, etat, ic ? ic->nom : "(aucune icone)",
                partenaireSigne ? " dont l'autre loi est signee : pas d'icone" : ", posee apres",
                trRacine[0], trRacine[1]);
            continue;
        }
        uintptr_t image = ic ? PoserIconeNoeud(elem, ic) : 0;
        const char* cible = IMAGE_NOEUD;
        if (!journal) continue;
        printf("[LOIUI]   noeud %d : \"%s\" (%s) etat %d -> %s sur \"%s\" : element 0x%llX, racine en (%.1f, %.1f)\n",
            k, nom, racine, etat, ic ? ic->nom : "(aucune icone)", cible, (unsigned long long)image, trRacine[0], trRacine[1]);
        bool signe = etat == ETAT_ACTIVE;
        if (nAppels == 1 && ((signe && !signeVu) || (!signe && !nonSigneVu))) {
            (signe ? signeVu : nonSigneVu) = true;
            printf("[LOIUI]   arbre du noeud %s \"%s\" :\n", signe ? "SIGNE" : "NON SIGNE", nom);
            int budget = 80;
            DecrireArbreElement(elem, 0, &budget);
        }
    }
    // Texte de chaque paire recentre : demi-rond droit = loi du meme arbre et
    // du meme groupe d'exclusion que celle du gauche.
    for (int a = 0; a < nDemi[0]; ++a) {
        const LoiAP& lg = g_lois[loiDemi[0][a]];
        for (int b = 0; b < nDemi[1]; ++b) {
            const LoiAP& ld = g_lois[loiDemi[1][b]];
            if (loiDemi[1][b] == loiDemi[0][a] || ld.arbre != lg.arbre || !lg.exclusion[0] ||
                strcmp(ld.exclusion, lg.exclusion) != 0) continue;
            __try { CentrerNomPaire(demi[0][a], demi[1][b]); }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                printf("[LOIUI] EXCEPTION 0x%08lX au recentrage du texte d'une paire\n", GetExceptionCode());
            }
            break;
        }
    }
    int posees = 0;
    for (int p = 0; p < nPaires; ++p)
        if (PoserIconePaire(paires[p], iconesPaires[p])) ++posees;
    if (journal && nPaires)
        printf("[LOIUI]   paires : %d icone(s) posee(s) sur %d (modele circle_inside %s)\n",
            posees, nPaires, g_modeleInside[0] ? "connu" : "INCONNU");
}

uintptr_t hkConstruireLivre(uintptr_t livre, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t r = oConstruireLivre(livre, p2, p3, p4);
    __try { DiagNoeudsLivre(livre); }
    __except (EXCEPTION_EXECUTE_HANDLER) { printf("[LOIUI] EXCEPTION 0x%08lX pendant le releve\n", GetExceptionCode()); }
    return r;
}

uintptr_t hkSigner(uintptr_t sys, uintptr_t loi, uintptr_t p3, uintptr_t p4)
{
    uintptr_t precedente = t_signature;
    t_signature = loi;
    t_differe = false;
    uintptr_t r = oSigner(sys, loi, p3, p4);
    bool differe = t_differe;
    t_signature = precedente;
    ApresSignature(loi, differe);
    return r;
}

} // namespace

void InstallHooksLoisAP(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; Fonction4_t* orig; const char* nom; } h[] = {
        { OFF_SIGNER, (void*)&hkSigner, &oSigner, "signature d'une loi" },
        { OFF_EFFETS, (void*)&hkEffets, &oEffets, "effets d'une loi" },
        { OFF_EVALUER_PREREQUIS, (void*)&hkEvaluerPrerequis, &oEvaluer, "evaluation d'un prerequis" },
        { OFF_CONSTRUIRE_LIVRE, (void*)&hkConstruireLivre, &oConstruireLivre, "construction du livre (icones AP)" },
        { OFF_INFO_LOI, (void*)&hkInfoLoi, &oInfoLoi, "panneau d'info d'une loi (nom du check)" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, (void**)x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[LOIAP] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}

const char* IconeCheckDeTest(const char* cle, int rang)
{
    return IconeDuCheckDeTest(cle, rang);
}

uintptr_t IconeAPSurCopieDe(uintptr_t original, uintptr_t parent, const char* icone)
{
    const TechIcons* ic = nullptr;
    for (const TechIcons& t : ARCHIPELAGO_ICONS_TECH) if (icone && !strcmp(t.nom, icone)) ic = &t;
    uintptr_t modele[2] = {};
    if (!ic || !SafeRead((void*)(original + ELEM_RECETTE), &modele[0], 8) || !modele[0] ||
        !SafeRead((void*)(original + ELEM_RESSOURCE), &modele[1], 8)) return 0;
    uintptr_t copie = ((Instancier_t)(Base() + OFF_INSTANCIER))(modele[0], modele[1], 1, Base() + OFF_CTX_INSTANCIER);
    if (!copie) return 0;
    uintptr_t zero = 0;
    SafeWrite((void*)(copie + ELEM_RECETTE), &zero, 8);
    ((AjouterEnfant_t)(Base() + OFF_AJOUTER_ENFANT))(parent, copie);
    PoserTuileSur(copie, ic);
    static const float blanc[4] = { 1.f, 1.f, 1.f, 1.f };
    ((Montrer_t)(Base() + OFF_MONTRER))(copie, 1, 0, 1);
    ((PoserCouleur_t)(Base() + OFF_POSER_COULEUR))(copie, blanc);
    return copie;
}

bool IconeAPPoser(uintptr_t image, const char* icone)
{
    for (const TechIcons& t : ARCHIPELAGO_ICONS_TECH)
        if (icone && !strcmp(t.nom, icone)) { PoserTuileSur(image, &t); return true; }
    return false;
}

void ElementTailleNulle(uintptr_t el)
{
    ((PoserTaille_t)(Base() + OFF_POSER_TAILLE))(el, 0.f, 0.f);
}

void LoisAPApresChargement()
{
    g_dateSauvegarde = DateCourante();
    g_rechargement = true;
}

bool LoisAPRecevoir(const char* arbre, int* rang)
{
    if (rang) *rang = 0;
    if (!g_bati || !g_partieChargee || !arbre) return false;
    int a = ArbreParNom(arbre);
    if (a < 0) return false;
    AcquireSRWLockExclusive(&g_verrou);
    int n = ++g_arbres[a].recus;
    ReleaseSRWLockExclusive(&g_verrou);
    if (rang) *rang = n;
    printf("[LOIAP] item recu : arbre \"%s\", %d item(s)\n", g_arbres[a].nom, n);
    Sauver();
    g_aTraiter = true;
    return true;
}

int LoisAPItemsRecus(const char* arbre, const uint64_t gid[2])
{
    if (!g_bati || !g_partieChargee || !arbre) return -1;
    if (gid[0] != g_partie[0] || gid[1] != g_partie[1]) return -1;
    int a = ArbreParNom(arbre);
    if (a < 0) return -1;
    AcquireSRWLockShared(&g_verrou);
    int n = g_arbres[a].recus;
    ReleaseSRWLockShared(&g_verrou);
    return n;
}

bool LoisAPArbreDeLoi(uintptr_t loi, char* arbre, int taille, int* poids)
{
    if (arbre && taille > 0) arbre[0] = 0;
    if (poids) *poids = 0;
    if (!BatirArbres()) return false;
    int i = IndexParAdresse(loi);
    if (i < 0 || g_lois[i].arbre < 0) return false;
    if (arbre) strcpy_s(arbre, taille, g_arbres[g_lois[i].arbre].nom);
    if (poids) *poids = g_lois[i].poids;
    return true;
}

void LoisAPDemanderTest()
{
    g_testDemande = true;
    printf("[LOIAP] test demande : il s'execute sur le thread du jeu (jeu hors pause)\n");
}

void LoisAPPompe()
{
    if (!PartiePrete() || !AssurerPartie(0)) return;

    if (g_testDemande) {
        g_testDemande = false;
        // Loi en attente signee la premiere ; a defaut, derniere loi signee.
        Etats e;
        int enAttente = -1, derniere = -1;
        if (LireEtats(e)) {
            for (int i = 0; i < g_nLois; ++i) {
                if (!e.Signee(i)) continue;
                if (!g_lois[i].presente && (enAttente < 0 || e.date[i] < e.date[enAttente])) enAttente = i;
                if (derniere < 0 || e.date[i] > e.date[derniere]) derniere = i;
            }
        }
        int choix = enAttente >= 0 ? enAttente : derniere;
        if (choix < 0) printf("[LOIAP] test : aucune loi signee, rien a recevoir\n");
        else {
            // Recu par son id Archipelago (known_ap_ids.h), comme du serveur.
            const char* arbre = g_arbres[g_lois[choix].arbre].nom;
            int64_t id = ApItemIdCourant("law", arbre);
            if (!id) printf("[LOIAP] test : arbre %s sans item AP dans ce scenario\n", arbre);
            else ReceiveItem(id, "Test");
        }
    }

    if (g_aTraiter) {
        g_aTraiter = false;
        AppliquerEnAttente();
    }
}
