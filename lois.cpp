// ---- lois.cpp ----
// Lois du jeu ("sociotechs" dans le code). Decompiles du 2026-10-30 :
//   FrostpunkProgressTreeEntry (base commune, 0x90) : +0x18 nom (NameString),
//     +0x20 GUID, +0x30 Enabled, +0x38 Requirements (pointeurs, compte +0x40),
//     +0x50 Show requirements, +0x68 Display name, +0x70 Description,
//     +0x78 Short description, +0x80 Icon tile, +0x88 Grid position
//   FrostpunkSociotechEntry (0x240) : +0x90 Frustration cost, +0x94 Signable,
//     +0xC8 Base time cost, +0xD0 Activation dilemma, +0xE0 Effects,
//     +0x140 Exclusion group, +0x148 Category (lien GUID), +0x158 texte de
//     la popup, +0x1C8 Special node
//   FrostpunkSociotechCategoryEntry (0x90) : +0x00 nom, +0x08 GUID,
//     +0x18 Display name, +0x78 Requirements
//   FrostpunkSociotechRequirement (0x40) : +0x30 lien vers la loi requise
//   FrostpunkScenarioEntry : +0x00 nom, +0x08 GUID, +0x1B0 Sociotech setups
//     (liens de 0x10, compte +0x1B8)
//   FrostpunkScenarioSociotechSetupEntry (0x48) : +0x00 nom, +0x08 GUID,
//     +0x18 Starting sociotechs, +0x30 Locked sociotechs (liens, compte +8)
//   FrostpunkSociotechSystem : +0xD8 Map {loi, SociotechData} au pas de 0x18,
//     compte +0xE0 ; +0x10 de l'element = etat (2 = loi active d'office)
#include "lois.h"
#include "lois_ap.h"    // LoisAPArbreDeLoi (releve de la table des lois)
#include "mod_common.h"
#include "scenario.h"   // FindSceneSystem
#include <cstring>

namespace {

constexpr uintptr_t OFF_SOCIETY_CONFIG   = 0x4070460;
constexpr uintptr_t OFF_SCENARIOS_CONFIG = 0x2C34490;
constexpr uintptr_t OFF_SYSTEME_LOIS     = 0x408FBD0;

constexpr int CFG_CATEGORIES = 0x78;
constexpr int CFG_LOIS       = 0xB0;
constexpr int CFG_SCENARIOS  = 0x88;
constexpr int CFG_REGLAGES   = 0x210;

constexpr int LOI_NOM        = 0x18;
constexpr int LOI_GUID       = 0x20;
constexpr int LOI_ACTIVEE    = 0x30;
constexpr int LOI_PREREQUIS  = 0x38;
constexpr int LOI_NOM_AFFICHE= 0x68;
constexpr int LOI_GRILLE     = 0x88;
constexpr int LOI_SIGNABLE   = 0x94;
constexpr int LOI_EXCLUSION  = 0x140;
constexpr int LOI_CATEGORIE  = 0x148;
constexpr int LOI_SPECIALE   = 0x1C8;

constexpr int CAT_NOM        = 0x00;
constexpr int CAT_GUID       = 0x08;
constexpr int CAT_NOM_AFFICHE= 0x18;

constexpr int REQ_TEXTE_ERREUR = 0x28;
constexpr int REQ_LIEN       = 0x30;
constexpr int COMPOSITE_OP   = 0x30;
constexpr int COMPOSITE_LISTE= 0x38;

constexpr uintptr_t OFF_UNLOCKS_PROPMAN = 0x4082EC0;
constexpr int CFG_UNLOCKS    = 0x78;
constexpr int UNLOCK_NOM     = 0x00;
constexpr int UNLOCK_GUID    = 0x08;

constexpr int CAT_PREREQUIS  = 0x78;
constexpr int LOI_AFFICHAGE  = 0x50;   // "Show requirements" (FrostpunkProgressTreeEntry)

// FrostpunkUnlocksSystem (systeme de jeu, par l'annuaire) : +0x68 Set des
// unlocks actifs (tableau trie de pointeurs d'entree, compte +8).
constexpr int SYS_UNLOCKS_ACTIFS = 0x68;
// Reglage de lois du scenario courant : *( *(base + 0x4062870) + 0xE0 )
// (cf. FUN_1416ffbe0).
constexpr uintptr_t OFF_REGLAGE_COURANT = 0x4062870;
constexpr int REGLAGE_COURANT = 0xE0;

constexpr int SCN_NOM        = 0x00;
constexpr int SCN_REGLAGES   = 0x1B0;

constexpr int REG_NOM        = 0x00;
constexpr int REG_GUID       = 0x08;
constexpr int REG_DEPART     = 0x18;
constexpr int REG_VERROUILLE = 0x30;

constexpr int SYS_ETATS      = 0xD8;

constexpr int MAX_ELEMENTS   = 1024;

struct Guid { uint64_t a, b; };

const char* NomAfficheDe(const char* cle);   // plus bas (noms releves par hkTranslate)

bool LireGuid(uintptr_t adr, Guid& g)
{
    return SafeRead((void*)adr, &g, sizeof(g));
}

bool GuidEgal(const Guid& x, const Guid& y) { return x.a == y.a && x.b == y.b; }
bool GuidNul(const Guid& x) { return x.a == 0 && x.b == 0; }

// Tableau dynamique {ptr, compte u32} ; faux si illisible ou aberrant.
bool LireTableau(uintptr_t adr, uintptr_t& tab, int& n)
{
    tab = 0; n = 0;
    uint32_t c = 0;
    if (!SafeRead((void*)adr, &tab, 8) || !SafeRead((void*)(adr + 8), &c, 4)) return false;
    if (c == 0) return true;
    if (tab <= 0x10000 || c > MAX_ELEMENTS) { tab = 0; return false; }
    n = (int)c;
    return true;
}

// Texte 8 bits pointe par le qword en 'adr' (NameString ou cle de traduction).
void LireTexte(uintptr_t adr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adr, &p, 8) || p <= 0x10000) { strcpy_s(out, taille, "-"); return; }
    // Lecture par blocs de 16 : un bloc unique de 160 octets echouait quand la
    // chaine finissait pres d'une page non lisible (nom affiche "?").
    int i = 0;
    bool fin = false, illisible = false;
    while (!fin && i < taille - 1) {
        char bloc[16];
        if (!SafeRead((void*)(p + i), bloc, sizeof(bloc))) {
            // Fin de page : octet par octet.
            if (!SafeRead((void*)(p + i), bloc, 1)) { illisible = (i == 0); break; }
            if (!bloc[0]) break;
            out[i] = (bloc[0] >= 0x20 && bloc[0] < 0x7F) ? bloc[0] : '?';
            ++i;
            continue;
        }
        for (int k = 0; k < (int)sizeof(bloc) && i < taille - 1; ++k) {
            if (!bloc[k]) { fin = true; break; }
            out[i++] = (bloc[k] >= 0x20 && bloc[k] < 0x7F) ? bloc[k] : '?';
        }
    }
    out[i] = 0;
    if (illisible) strcpy_s(out, taille, "?");
}

void Ajouter(char* buf, int taille, const char* s)
{
    size_t l = strlen(buf);
    if ((int)l >= taille - 1) return;
    strncat_s(buf, taille, s, _TRUNCATE);
}

struct Loi {
    uintptr_t adr;
    Guid      guid;
    Guid      categorie;
    char      nom[96];
};

Loi g_lois[MAX_ELEMENTS];
int g_nLois = 0;

const char* NomDeLoi(const Guid& g)
{
    for (int i = 0; i < g_nLois; ++i)
        if (GuidEgal(g_lois[i].guid, g)) return g_lois[i].nom;
    return nullptr;
}

const char* NomDeLoiParAdresse(uintptr_t adr)
{
    for (int i = 0; i < g_nLois; ++i)
        if (g_lois[i].adr == adr) return g_lois[i].nom;
    return nullptr;
}

// Liste de liens {GUID 16 octets} vers des lois, en noms separes par ", ".
void ListeDeLiens(uintptr_t adrTableau, char* out, int taille)
{
    out[0] = 0;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrTableau, tab, n)) { strcpy_s(out, taille, "(illisible)"); return; }
    if (n == 0) { strcpy_s(out, taille, "(aucune)"); return; }
    for (int i = 0; i < n; ++i) {
        Guid g{};
        if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
        const char* nom = NomDeLoi(g);
        char inconnu[48];
        if (!nom) {
            sprintf_s(inconnu, "?%016llX", (unsigned long long)g.a);
            nom = inconnu;
        }
        if (i > 0) Ajouter(out, taille, ", ");
        Ajouter(out, taille, nom);
    }
}

bool NomPolitique(const Guid& g, char* out, int taille);
bool NomScenario(const Guid& g, char* out, int taille);
bool NomTechno(const Guid& g, char* out, int taille);   // releve des technos (plus bas)

// FrostpunkUnlocksConfig : instance statique inconnue ; on la cherche par RTTI
// autour de son gestionnaire de proprietes (base + 0x4082EC0), lecture seule.
uintptr_t g_configUnlocks = 0;

void TrouverConfigUnlocks(uintptr_t base)
{
    if (g_configUnlocks) return;
    for (uintptr_t a = base + OFF_UNLOCKS_PROPMAN - 0x8000; a < base + OFF_UNLOCKS_PROPMAN + 0x8000; a += 8) {
        uintptr_t vptr = 0;
        if (!SafeRead((void*)a, &vptr, 8) || vptr < base || vptr > base + 0x5000000) continue;
        char cls[64] = {};
        if (GetClassNameFP(a, cls, sizeof(cls)) && strcmp(cls, "FrostpunkUnlocksConfig") == 0) {
            g_configUnlocks = a;
            printf("[LOIS] FrostpunkUnlocksConfig trouvee a base + 0x%llX\n", (unsigned long long)(a - base));
            return;
        }
    }
    printf("[LOIS] FrostpunkUnlocksConfig introuvable autour de base + 0x%llX\n",
        (unsigned long long)OFF_UNLOCKS_PROPMAN);
}

// Nom d'une FrostpunkUnlockEntry par son GUID (+0x00 nom, +0x08 GUID).
bool NomUnlock(const Guid& g, char* out, int taille)
{
    if (!g_configUnlocks) return false;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(g_configUnlocks + CFG_UNLOCKS, tab, n)) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t u = 0; Guid gu{};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &u, 8) || u <= 0x10000) continue;
        if (LireGuid(u + UNLOCK_GUID, gu) && GuidEgal(gu, g)) {
            LireTexte(u + UNLOCK_NOM, out, taille);
            return true;
        }
    }
    return false;
}

// Description d'un prerequis, recursive pour les composites :
//   FrostpunkRequirementEntry (base, 0x30) : +0x18 lien d'erreur, +0x28 texte d'erreur
//   CompositeRequirement (0x50) : +0x30 Operator (eBooleanOperator), +0x38 Requirements
//   SociotechRequirement (0x40) : +0x30 lien vers une loi
//   SociotechExclusionRequirement (0x38) : +0x30 ExclusionGroup (NameString)
//   UnlockRequirement (0x40) : +0x30 lien vers une FrostpunkUnlockEntry
void DecrireExigence(uintptr_t req, char* out, int taille, int profondeur);

void DecrireListeExigences(uintptr_t adrTableau, char* out, int taille, int profondeur, const char* sep)
{
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrTableau, tab, n)) { Ajouter(out, taille, "(illisible)"); return; }
    if (n == 0) { Ajouter(out, taille, "(aucun)"); return; }
    for (int i = 0; i < n; ++i) {
        uintptr_t req = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &req, 8) || req <= 0x10000) continue;
        if (i > 0) Ajouter(out, taille, sep);
        DecrireExigence(req, out, taille, profondeur);
    }
}

void DecrireExigence(uintptr_t req, char* out, int taille, int profondeur)
{
    char cls[96] = "?";
    GetClassNameFP(req, cls, sizeof(cls));
    const char* court = strncmp(cls, "Frostpunk", 9) == 0 ? cls + 9 : cls;
    char elem[256];

    if (strcmp(court, "SociotechRequirement") == 0) {
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        const char* nom = NomDeLoi(g);
        sprintf_s(elem, "loi %s", nom ? nom : "?");
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "TechnologyRequirement") == 0) {
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        char nom[96];
        if (!NomTechno(g, nom, sizeof(nom)))
            sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        sprintf_s(elem, "techno %s", nom);
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "SociotechExclusionRequirement") == 0) {
        char groupe[64];
        LireTexte(req + REQ_LIEN, groupe, sizeof(groupe));
        sprintf_s(elem, "exclusion %s", groupe);
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "UnlockRequirement") == 0) {
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        char nom[96];
        if (!NomUnlock(g, nom, sizeof(nom)))
            sprintf_s(nom, "?%016llX%016llX", (unsigned long long)g.a, (unsigned long long)g.b);
        sprintf_s(elem, "unlock %s", nom);
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "PolicyRequirement") == 0) {
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        char nom[128];
        if (!NomPolitique(g, nom, sizeof(nom)))
            sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        sprintf_s(elem, "politique %s", nom);
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "ScenarioRequirement") == 0) {
        Guid g{}; uint8_t negate = 0;
        LireGuid(req + REQ_LIEN, g);
        SafeRead((void*)(req + 0x40), &negate, 1);
        char nom[96];
        if (!NomScenario(g, nom, sizeof(nom)))
            sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        sprintf_s(elem, "%sscenario %s", negate ? "PAS " : "", nom);
        Ajouter(out, taille, elem);
    } else if (strcmp(court, "CompositeRequirement") == 0) {
        int32_t op = -1;
        SafeRead((void*)(req + COMPOSITE_OP), &op, 4);
        sprintf_s(elem, "op%d(", op);
        Ajouter(out, taille, elem);
        if (profondeur < 4) DecrireListeExigences(req + COMPOSITE_LISTE, out, taille, profondeur + 1, ", ");
        else Ajouter(out, taille, "...");
        Ajouter(out, taille, ")");
    } else {
        // Autre type : classe + texte d'erreur (souvent parlant).
        char erreur[128];
        LireTexte(req + REQ_TEXTE_ERREUR, erreur, sizeof(erreur));
        sprintf_s(elem, "%s[%s]", court, erreur);
        Ajouter(out, taille, elem);
    }
}

// --- Evaluation des prerequis dans la partie courante -----------------------
// Resultat : 1 vrai, 0 faux, -1 inconnu. Les prerequis de loi et d'exclusion
// sont tenus pour vrais : on cherche ce qui EXISTE dans le scenario, pas ce
// qui est signable a l'instant.
uintptr_t g_unlocksActifs[MAX_ELEMENTS];
int       g_nUnlocksActifs = -1;   // -1 : etat des unlocks illisible

// Scenario courant (FrostpunkScenarioSystem +0x68 = FrostpunkScenarioEntry,
// +0x08 GUID) pour FrostpunkScenarioRequirement (0x48 : +0x30 lien scenario,
// +0x40 Negate).
Guid g_guidScenario{};
bool g_scenarioConnu = false;

// Politiques en vigueur : FrostpunkPolicySystem +0x68 = Map {EntryPtr
// d'enjeu, GUID de la politique courante} au pas de 0x18, compte +8.
// FrostpunkPolicyRequirement (0x40) : +0x30 lien vers une FrostpunkPolicyEntry
// (+0x00 nom, +0x08 GUID), rangees dans les enjeux de FrostpunkSocietyConfig
// +0x150 (FrostpunkPolicyIssueEntry : +0x00 nom, +0x18 Policies).
constexpr int CFG_ENJEUX      = 0x150;
constexpr int ENJEU_POLITIQUES= 0x18;
constexpr int SYS_POLITIQUES  = 0x68;
constexpr int SCNREQ_NEGATE   = 0x40;

Guid g_politiques[MAX_ELEMENTS];
int  g_nPolitiques = -1;            // -1 : etat des politiques illisible

bool PolitiqueActive(const Guid& g)
{
    for (int i = 0; i < g_nPolitiques; ++i)
        if (GuidEgal(g_politiques[i], g)) return true;
    return false;
}

// Nom d'une politique par son GUID : enjeux de la config de societe.
bool NomPolitique(const Guid& g, char* out, int taille)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(base + OFF_SOCIETY_CONFIG + CFG_ENJEUX, tab, n)) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t enjeu = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &enjeu, 8) || enjeu <= 0x10000) continue;
        uintptr_t tp = 0; int np = 0;
        if (!LireTableau(enjeu + ENJEU_POLITIQUES, tp, np)) continue;
        for (int k = 0; k < np; ++k) {
            uintptr_t pol = 0; Guid gp{};
            if (!SafeRead((void*)(tp + (uintptr_t)k * 8), &pol, 8) || pol <= 0x10000) continue;
            if (LireGuid(pol + 0x08, gp) && GuidEgal(gp, g)) {
                char nEnjeu[64], nPol[64];
                LireTexte(enjeu, nEnjeu, sizeof(nEnjeu));
                LireTexte(pol, nPol, sizeof(nPol));
                sprintf_s(out, taille, "%s=%s", nEnjeu, nPol);
                return true;
            }
        }
    }
    return false;
}

// Nom d'un scenario par son GUID (config des scenarios +0x88).
bool NomScenario(const Guid& g, char* out, int taille)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(base + OFF_SCENARIOS_CONFIG + CFG_SCENARIOS, tab, n)) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t s = 0; Guid gs{};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &s, 8) || s <= 0x10000) continue;
        if (LireGuid(s + 0x08, gs) && GuidEgal(gs, g)) { LireTexte(s, out, taille); return true; }
    }
    return false;
}

bool UnlockActif(const Guid& g)
{
    for (int i = 0; i < g_nUnlocksActifs; ++i) {
        Guid gu{};
        if (LireGuid(g_unlocksActifs[i] + UNLOCK_GUID, gu) && GuidEgal(gu, g)) return true;
    }
    return false;
}

int EvaluerExigence(uintptr_t req, int profondeur);

// Liste = ET de ses elements ; vide = vrai.
int EvaluerListe(uintptr_t adrTableau, int op, int profondeur)
{
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrTableau, tab, n)) return -1;
    int res = (op == 1) ? 0 : 1;
    if (op == 1 && n == 0) res = 1;
    for (int i = 0; i < n; ++i) {
        uintptr_t req = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &req, 8) || req <= 0x10000) { res = -1; continue; }
        int v = EvaluerExigence(req, profondeur);
        if (op == 1) { if (v == 1) return 1; if (v < 0) res = -1; }
        else         { if (v == 0) return 0; if (v < 0) res = -1; }
    }
    return res;
}

int EvaluerExigence(uintptr_t req, int profondeur)
{
    if (profondeur > 6) return -1;
    char cls[96] = "?";
    GetClassNameFP(req, cls, sizeof(cls));
    const char* court = strncmp(cls, "Frostpunk", 9) == 0 ? cls + 9 : cls;

    if (strcmp(court, "SociotechRequirement") == 0) return 1;
    if (strcmp(court, "SociotechExclusionRequirement") == 0) return 1;
    if (strcmp(court, "ScenarioRequirement") == 0) {
        if (!g_scenarioConnu) return -1;
        Guid g{}; uint8_t negate = 0;
        LireGuid(req + REQ_LIEN, g);
        SafeRead((void*)(req + SCNREQ_NEGATE), &negate, 1);
        bool egal = GuidEgal(g, g_guidScenario);
        return (egal != (negate != 0)) ? 1 : 0;
    }
    if (strcmp(court, "PolicyRequirement") == 0) {
        if (g_nPolitiques < 0) return -1;
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        return PolitiqueActive(g) ? 1 : 0;
    }
    if (strcmp(court, "UnlockRequirement") == 0) {
        if (g_nUnlocksActifs < 0) return -1;
        Guid g{};
        LireGuid(req + REQ_LIEN, g);
        return UnlockActif(g) ? 1 : 0;
    }
    if (strcmp(court, "CompositeRequirement") == 0) {
        int32_t op = -1;
        SafeRead((void*)(req + COMPOSITE_OP), &op, 4);
        if (op == 0 || op == 1) return EvaluerListe(req + COMPOSITE_LISTE, op, profondeur + 1);
        if (op == 2) {
            int v = EvaluerListe(req + COMPOSITE_LISTE, 0, profondeur + 1);
            return v < 0 ? -1 : !v;
        }
        return -1;
    }
    return -1;
}

const char* TexteEval(int v) { return v == 1 ? "oui" : (v == 0 ? "NON" : "?"); }

// Unlocks actifs de la partie : FrostpunkUnlocksSystem, trouve par son nom
// dans l'annuaire des systemes (id = 32 bits bas), +0x68.
void ChargerUnlocksActifs()
{
    g_nUnlocksActifs = -1;
    uintptr_t idBrut = FindSceneSystem("FrostpunkUnlocksSystem");
    uintptr_t sys = idBrut ? SystemeParId((int)(idBrut & 0xFFFFFFFF)) : 0;
    char cls[64] = "?";
    if (sys) GetClassNameFP(sys, cls, sizeof(cls));
    if (!sys || strcmp(cls, "FrostpunkUnlocksSystem") != 0) {
        printf("[LOIS] systeme des unlocks introuvable (id %llu, objet 0x%llX, classe %s)\n",
            (unsigned long long)(idBrut & 0xFFFFFFFF), (unsigned long long)sys, cls);
        return;
    }
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(sys + SYS_UNLOCKS_ACTIFS, tab, n)) {
        printf("[LOIS] unlocks actifs illisibles (systeme 0x%llX)\n", (unsigned long long)sys);
        return;
    }
    g_nUnlocksActifs = 0;
    char ligne[4096] = {};
    for (int i = 0; i < n && g_nUnlocksActifs < MAX_ELEMENTS; ++i) {
        uintptr_t u = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &u, 8) || u <= 0x10000) continue;
        g_unlocksActifs[g_nUnlocksActifs++] = u;
        char nom[96];
        LireTexte(u + UNLOCK_NOM, nom, sizeof(nom));
        if (ligne[0]) Ajouter(ligne, sizeof(ligne), ", ");
        Ajouter(ligne, sizeof(ligne), nom);
    }
    printf("[LOIS] %d unlocks actifs : %s\n", g_nUnlocksActifs, ligne[0] ? ligne : "(aucun)");
}

// Scenario courant et politiques en vigueur (pour ScenarioRequirement et
// PolicyRequirement).
void ChargerScenarioEtPolitiques()
{
    g_scenarioConnu = false;
    uintptr_t scnSys = SystemeParId(0x0C), scn = 0;
    if (scnSys && SafeRead((void*)(scnSys + 0x68), &scn, 8) && scn > 0x10000 &&
        LireGuid(scn + 0x08, g_guidScenario))
        g_scenarioConnu = true;

    g_nPolitiques = -1;
    uintptr_t idBrut = FindSceneSystem("FrostpunkPolicySystem");
    uintptr_t sys = idBrut ? SystemeParId((int)(idBrut & 0xFFFFFFFF)) : 0;
    char cls[64] = "?";
    if (sys) GetClassNameFP(sys, cls, sizeof(cls));
    uintptr_t tab = 0; int n = 0;
    if (!sys || strcmp(cls, "FrostpunkPolicySystem") != 0 || !LireTableau(sys + SYS_POLITIQUES, tab, n)) {
        printf("[LOIS] politiques illisibles (systeme 0x%llX, classe %s)\n", (unsigned long long)sys, cls);
        return;
    }
    g_nPolitiques = 0;
    char ligne[4096] = {};
    for (int i = 0; i < n && g_nPolitiques < MAX_ELEMENTS; ++i) {
        Guid g{};
        if (!LireGuid(tab + (uintptr_t)i * 0x18 + 8, g)) continue;
        g_politiques[g_nPolitiques++] = g;
        char nom[128];
        if (!NomPolitique(g, nom, sizeof(nom))) sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        if (ligne[0]) Ajouter(ligne, sizeof(ligne), ", ");
        Ajouter(ligne, sizeof(ligne), nom);
    }
    printf("[LOIS] %d politiques en vigueur : %s\n", g_nPolitiques, ligne[0] ? ligne : "(aucune)");
}

// Lois du scenario courant : pour chaque categorie et chaque loi, existe-t-elle
// dans cette partie ? (categorie + affichage evalues avec les unlocks actifs,
// lois verrouillees par le reglage du scenario).
void LoisDuScenarioCourant(uintptr_t base, uintptr_t tabCat, int nCat)
{
    char scenario[96] = "?";
    uintptr_t scnSys = SystemeParId(0x0C), scn = 0;
    if (scnSys && SafeRead((void*)(scnSys + 0x68), &scn, 8) && scn > 0x10000)
        LireTexte(scn, scenario, sizeof(scenario));

    // Reglage courant (lois de depart / verrouillees).
    uintptr_t p = 0, reglage = 0;
    SafeRead((void*)(base + OFF_REGLAGE_COURANT), &p, 8);
    if (p > 0x10000) SafeRead((void*)(p + REGLAGE_COURANT), &reglage, 8);
    char nomReglage[96] = "(aucun)";
    if (reglage > 0x10000) LireTexte(reglage + REG_NOM, nomReglage, sizeof(nomReglage));

    printf("[LOIS] ===== scenario courant %s (reglage %s) : lois presentes =====\n", scenario, nomReglage);
    if (g_nUnlocksActifs < 0)
        printf("[LOIS] (unlocks inconnus : les prerequis d'unlock sont marques ?)\n");

    for (int c = 0; c < nCat; ++c) {
        uintptr_t cat = 0;
        if (!SafeRead((void*)(tabCat + (uintptr_t)c * 8), &cat, 8) || cat <= 0x10000) continue;
        Guid gCat{};
        LireGuid(cat + CAT_GUID, gCat);
        char nom[96];
        LireTexte(cat + CAT_NOM, nom, sizeof(nom));
        // Un arbre peut etre ferme au moment du releve et s'ouvrir plus tard
        // (unlock donne par l'histoire : IdeologyTree, Labour tree...) : ses
        // lois sont evaluees quand meme, avec le prerequis de l'arbre en clair.
        int vCat = EvaluerListe(cat + CAT_PREREQUIS, 0, 0);
        char reqCat[1024] = {};
        DecrireListeExigences(cat + CAT_PREREQUIS, reqCat, sizeof(reqCat), 0, " ; ");
        printf("[LOIS] --- %s : arbre ouvert maintenant %s (requis : %s)\n", nom, TexteEval(vCat), reqCat);

        int nPres = 0;
        for (int i = 0; i < g_nLois; ++i) {
            const Loi& l = g_lois[i];
            if (!GuidEgal(l.categorie, gCat)) continue;
            uint8_t activee = 0;
            SafeRead((void*)(l.adr + LOI_ACTIVEE), &activee, 1);

            bool verrouillee = false;
            uintptr_t tab = 0; int n = 0;
            if (reglage > 0x10000 && LireTableau(reglage + REG_VERROUILLE, tab, n))
                for (int k = 0; k < n && !verrouillee; ++k) {
                    Guid g{};
                    if (LireGuid(tab + (uintptr_t)k * 0x10, g) && GuidEgal(g, l.guid)) verrouillee = true;
                }

            int vAff = EvaluerListe(l.adr + LOI_AFFICHAGE, 0, 0);
            int vReq = EvaluerListe(l.adr + LOI_PREREQUIS, 0, 0);

            const char* statut;
            if (!activee) statut = "DESACTIVEE";
            else if (verrouillee) statut = "VERROUILLEE (reglage)";
            else if (vAff == 0) statut = "CACHEE (affichage)";
            else if (vAff < 0) statut = "? (prerequis non evalue)";
            else if (vCat == 0) statut = "PRESENTE si l'arbre s'ouvre";
            else if (vReq == 0) statut = "PRESENTE, bloquee par un unlock";
            else statut = "PRESENTE";
            if (strncmp(statut, "PRESENTE", 8) == 0) nPres++;
            char cle[192];
            LireTexte(l.adr + LOI_NOM_AFFICHE, cle, sizeof(cle));
            const char* affiche = NomAfficheDe(cle);
            printf("[LOIS]   %-45s %-32s %s\n", l.nom,
                affiche ? affiche : "(nom affiche non vu)", statut);
        }
        printf("[LOIS]     -> %d lois presentes\n", nPres);
    }
}

void DecrireLoi(const Loi& l)
{
    char affiche[160], exclusion[96], prerequis[2048] = {};
    LireTexte(l.adr + LOI_NOM_AFFICHE, affiche, sizeof(affiche));
    LireTexte(l.adr + LOI_EXCLUSION, exclusion, sizeof(exclusion));

    int32_t grille[2] = {};
    uint8_t activee = 0, signable = 0, speciale = 0;
    SafeRead((void*)(l.adr + LOI_GRILLE), grille, 8);
    SafeRead((void*)(l.adr + LOI_ACTIVEE), &activee, 1);
    SafeRead((void*)(l.adr + LOI_SIGNABLE), &signable, 1);
    SafeRead((void*)(l.adr + LOI_SPECIALE), &speciale, 1);

    DecrireListeExigences(l.adr + LOI_PREREQUIS, prerequis, sizeof(prerequis), 0, " ; ");
    char affichage[1024] = {};
    DecrireListeExigences(l.adr + LOI_AFFICHAGE, affichage, sizeof(affichage), 0, " ; ");
    Ajouter(prerequis, sizeof(prerequis), " | affichage : ");
    Ajouter(prerequis, sizeof(prerequis), affichage);

    printf("[LOIS]   %-40s | %s | grille (%d,%d)%s%s%s%s%s | requis : %s\n",
        l.nom, affiche, grille[0], grille[1],
        activee ? "" : " | DESACTIVEE",
        signable ? "" : " | non signable",
        speciale ? " | noeud special" : "",
        exclusion[0] && strcmp(exclusion, "-") ? " | exclusion " : "",
        exclusion[0] && strcmp(exclusion, "-") ? exclusion : "",
        prerequis);
}

} // namespace

// --- Noms affiches, releves au passage dans hkTranslate ----------------------
// valeur = objet de traduction : +0x00 pointeur vers le texte UTF-16.
namespace {
constexpr int MAX_NOMS_AFFICHES = 256;
struct NomAffiche { char cle[160]; char texte[96]; };
NomAffiche g_nomsAffiches[MAX_NOMS_AFFICHES];
volatile int g_nNomsAffiches = 0;

const char* NomAfficheDe(const char* cle)
{
    if (cle[0] == '@') ++cle;
    for (int i = 0; i < g_nNomsAffiches; ++i)
        if (strcmp(g_nomsAffiches[i].cle, cle) == 0) return g_nomsAffiches[i].texte;
    return nullptr;
}
} // namespace

void LoisNoterTraduction(const char* cle, uintptr_t valeur)
{
    static const char PREFIXE[] = "Generated/SocietyConfig/Sociotechs/";
    if (strncmp(cle, PREFIXE, sizeof(PREFIXE) - 1) != 0) return;
    size_t l = strlen(cle);
    static const char SUFFIXE[] = "/Display name";
    if (l < sizeof(SUFFIXE) || strcmp(cle + l - (sizeof(SUFFIXE) - 1), SUFFIXE) != 0) return;
    if (l >= sizeof(NomAffiche::cle) || NomAfficheDe(cle)) return;
    if (g_nNomsAffiches >= MAX_NOMS_AFFICHES || valeur <= 0x10000) return;

    uintptr_t chars = 0;
    if (!SafeRead((void*)valeur, &chars, 8) || chars <= 0x10000) return;
    NomAffiche& n = g_nomsAffiches[g_nNomsAffiches];
    int i = 0;
    for (; i < (int)sizeof(n.texte) - 1; ++i) {
        wchar_t ch = 0;
        if (!SafeRead((void*)(chars + i * 2), &ch, 2) || ch == 0) break;
        // Tirets et apostrophes typographiques : leur equivalent ASCII (le
        // nom sert aussi aux popups AP, "Child Labour - Safe Jobs").
        if (ch == 0x2013 || ch == 0x2014) ch = L'-';
        else if (ch == 0x2018 || ch == 0x2019) ch = L'\'';
        n.texte[i] = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '?';
    }
    n.texte[i] = 0;
    if (i == 0) return;
    strcpy_s(n.cle, cle);
    g_nNomsAffiches = g_nNomsAffiches + 1;
}

bool LoisNomAffiche(uintptr_t loi, char* out, int taille)
{
    char cle[160];
    LireTexte(loi + LOI_NOM_AFFICHE, cle, sizeof(cle));
    const char* texte = NomAfficheDe(cle);
    if (!texte) return false;
    strcpy_s(out, taille, texte);
    return true;
}

// --- Qui donne quel unlock ? -------------------------------------------------
// Decompiles du 2026-10-30 :
//   FrostpunkAddUnlockEffect (0x28) : +0x18 lien vers la FrostpunkUnlockEntry ;
//   repertoires d'evenements : tableau base + 0x406DDA8 (compte + 0x406DDB0),
//     repertoire : +0x00 nom, +0x08 GUID, +0x18 evenements (compte +0x20) ;
//   FrostpunkGameEventEntry : +0x00 nom, +0x58 effets (pointeurs, compte +0x60) ;
//   effets de dilemme (Create...DilemmaEffect) : +0x18 lien vers le dilemme,
//     resolu par (*DAT_144069970)->vtable[0x88](coll, &guid) (thread du jeu) ;
//   FrostpunkDilemmaEntry : +0x48 effets initiaux, +0x60 effets ignores,
//     +0x78 choix (FrostpunkDilemmaChoiceEntry : +0x78 effets, +0x90 resultats) ;
//   FrostpunkScenarioEntry : +0xE8 liens "Events setups", +0x100 liens
//     "Unlocks setups" ; config des scenarios +0xC0 = reglages d'evenements
//     (FrostpunkScenarioGameEventsSetupEntry : +0x08 GUID, +0x18 liens de
//     repertoires), +0x248 = reglages d'unlocks (FrostpunkScenarioUnlocksSetup
//     Entry : +0x08 GUID, +0x18 liens "Starting unlocks").
namespace {
constexpr uintptr_t OFF_REPERTOIRES_EVT   = 0x406DDA8;
constexpr uintptr_t OFF_COLL_DILEMMES     = 0x4069970;
constexpr int REP_EVENEMENTS = 0x18;
constexpr int EVT_EFFETS     = 0x58;
constexpr int UNLOCKEFFET_LIEN = 0x18;
constexpr int DILEFFET_LIEN  = 0x18;
constexpr int SCN_EVENEMENTS = 0xE8;
constexpr int SCN_UNLOCKS    = 0x100;
constexpr int CFG_REGLAGES_EVT     = 0xC0;
constexpr int CFG_REGLAGES_UNLOCKS = 0x248;
constexpr int REGLAGE_LIENS  = 0x18;

volatile bool g_releveDemande = false;

// Objets deja parcourus : table de hachage a sondage lineaire (8192 en
// tableau lineaire saturait au 1er releve).
constexpr int TAILLE_VUS = 1 << 16;
uintptr_t g_vus[TAILLE_VUS];
int       g_nVus = 0;
int       g_nTrouves = 0;
int       g_nEffetsDilemme = 0, g_nGuidNuls = 0, g_nNonResolus = 0, g_nResolus = 0;

// L'ensemble est vide a chaque evenement (VidagesVus) : un effet partage par
// plusieurs evenements doit etre attribue a chacun (au 2e releve, l'effet
// IdeologyTree de "Sick from the dead" etait masque par un autre evenement).
uint32_t g_casesVus[TAILLE_VUS * 3 / 4];
int      g_nVusTotal = 0;
bool     g_evtDonneIdeologie = false;

// Parcours pour un autre module (LoisParcourirEvenements) : appele sur chaque
// objet "Frostpunk..." atteint ; les lignes [UNLK] sont alors tues.
VisiteurEffet_t g_visiteur = nullptr;
uintptr_t       g_evtCourant = 0;

bool DejaVu(uintptr_t p)
{
    if (g_nVus >= TAILLE_VUS * 3 / 4) return true;   // plein : on arrete de descendre
    uint32_t h = (uint32_t)((p >> 3) * 2654435761u) & (TAILLE_VUS - 1);
    for (int k = 0; k < TAILLE_VUS; ++k) {
        uint32_t i = (h + k) & (TAILLE_VUS - 1);
        if (g_vus[i] == p) return true;
        if (g_vus[i] == 0) { g_vus[i] = p; g_casesVus[g_nVus++] = i; g_nVusTotal++; return false; }
    }
    return true;
}

void ViderVus()
{
    for (int k = 0; k < g_nVus; ++k) g_vus[g_casesVus[k]] = 0;
    g_nVus = 0;
}

// Declencheurs et conditions d'un evenement : tout tableau d'objets RTTI des
// 0x100 premiers octets de l'entree (hors effets +0x58), decrit comme un
// prerequis (classe + texte d'erreur pour les types inconnus).
void DecrireEvenement(uintptr_t evt, const char* prefixe = "[UNLK]")
{
    uint8_t active = 0;
    SafeRead((void*)(evt + 0x20), &active, 1);
    printf("%s     evenement %s\n", prefixe, active ? "active" : "DESACTIVE");
    // Entree de 0x90 octets, entrees contigues : lire plus loin donnait les
    // declencheurs de l'evenement SUIVANT (2e releve v8).
    for (int off = 0x08; off + 0x10 <= 0x90; off += 8) {
        if (off == EVT_EFFETS) continue;
        uintptr_t tab = 0; uint32_t n = 0;
        if (!SafeRead((void*)(evt + off), &tab, 8) || !SafeRead((void*)(evt + off + 8), &n, 4)) continue;
        if (tab <= 0x10000 || n == 0 || n > 64) continue;
        uintptr_t premier = 0;
        char c2[96] = {};
        if (!SafeRead((void*)tab, &premier, 8) || premier <= 0x10000 ||
            !GetClassNameFP(premier, c2, sizeof(c2)) || strncmp(c2, "Frostpunk", 9) != 0) continue;
        char liste[2048] = {};
        DecrireListeExigences(evt + off, liste, sizeof(liste), 0, " ; ");
        printf("%s     +0x%02X (%u) : %s\n", prefixe, off, n, liste);
        off += 8;   // saute le compte
    }
}

typedef uintptr_t (*TrouverDilemme_t)(uintptr_t, const Guid*);

// Appel protege : une adresse fausse ne doit pas faire tomber le jeu.
uintptr_t AppelerTrouverDilemme(TrouverDilemme_t fn, uintptr_t coll, const Guid* g)
{
    __try { return fn(coll, g); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// DAT_144069970 est la collection elle-meme (objet statique, vtable en +0) :
// (*DAT_144069970)[0x88](&DAT_144069970, &guid). La 1re version lisait un
// niveau de trop et appelait une adresse prise dans le code : plantage.
uintptr_t ResoudreDilemme(const Guid& g)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t coll = base + OFF_COLL_DILEMMES, vt = 0, fn = 0;
    if (GuidNul(g)) return 0;
    if (!SafeRead((void*)coll, &vt, 8) || vt < base || vt > base + 0x5000000) return 0;
    if (!SafeRead((void*)(vt + 0x88), &fn, 8) || fn < base || fn > base + 0x2000000) return 0;
    static bool annonce = false;
    if (!annonce) {
        annonce = true;
        printf("[UNLK] resolution des dilemmes : vtable +0x%llX, fonction +0x%llX\n",
            (unsigned long long)(vt - base), (unsigned long long)(fn - base));
    }
    return AppelerTrouverDilemme((TrouverDilemme_t)fn, coll, &g);
}

void ParcourirObjet(uintptr_t obj, int prof, const char* evt, const char* rep);

// Taille reelle d'un objet de reflexion : methode [3] (vtable +0x18) = sa fiche,
// fiche +0x88 = taille (FUN_140e4e520). Sans elle, le parcours generique lisait
// 0x100 octets et debordait dans les objets voisins (attributions fausses).
typedef uintptr_t (*Fiche_t)(uintptr_t);

uintptr_t AppelerFiche(Fiche_t fn, uintptr_t obj)
{
    __try { return fn(obj); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

int TailleObjet(uintptr_t obj)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t vt = 0, fn = 0, fiche = 0;
    uint64_t taille = 0;
    if (!SafeRead((void*)obj, &vt, 8) || vt < base || vt > base + 0x5000000) return 0;
    if (!SafeRead((void*)(vt + 0x18), &fn, 8) || fn < base || fn > base + 0x2000000) return 0;
    fiche = AppelerFiche((Fiche_t)fn, obj);
    if (fiche <= 0x10000 || !SafeRead((void*)(fiche + 0x88), &taille, 8)) return 0;
    if (taille < 0x10 || taille > 0x1000) return 0;
    return (int)taille;
}

// Tableau {ptr, compte} d'objets RTTI "Frostpunk..." : on parcourt chacun.
void ParcourirTableau(uintptr_t adr, int prof, const char* evt, const char* rep)
{
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adr, tab, n) || n == 0 || n > 256) return;
    for (int i = 0; i < n; ++i) {
        uintptr_t o = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &o, 8) && o > 0x10000)
            ParcourirObjet(o, prof, evt, rep);
    }
}

void ParcourirDilemme(uintptr_t dil, int prof, const char* evt, const char* rep)
{
    if (!dil || prof > 10 || DejaVu(dil)) return;
    ParcourirTableau(dil + 0x48, prof + 1, evt, rep);
    ParcourirTableau(dil + 0x60, prof + 1, evt, rep);
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(dil + 0x78, tab, n)) return;
    for (int i = 0; i < n && i < 8; ++i) {
        uintptr_t ch = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &ch, 8) || ch <= 0x10000) continue;
        ParcourirTableau(ch + 0x78, prof + 1, evt, rep);
        ParcourirTableau(ch + 0x90, prof + 1, evt, rep);
    }
}

void ParcourirObjet(uintptr_t obj, int prof, const char* evt, const char* rep)
{
    if (prof > 10 || DejaVu(obj)) return;
    char cls[96] = {};
    if (!GetClassNameFP(obj, cls, sizeof(cls)) || strncmp(cls, "Frostpunk", 9) != 0) return;

    if (g_visiteur) g_visiteur(obj, cls, g_evtCourant, evt, rep);
    if (!g_visiteur && strcmp(cls, "FrostpunkAddUnlockEffect") == 0) {
        Guid g{};
        LireGuid(obj + UNLOCKEFFET_LIEN, g);
        char nom[96];
        if (!NomUnlock(g, nom, sizeof(nom))) sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        printf("[UNLK] %-45s <- evenement \"%s\" (repertoire %s)\n", nom, evt, rep);
        if (strcmp(nom, "IdeologyTree") == 0) g_evtDonneIdeologie = true;
        g_nTrouves++;
        return;
    }
    if (strstr(cls, "DilemmaEffect")) {
        Guid g{};
        LireGuid(obj + DILEFFET_LIEN, g);
        g_nEffetsDilemme++;
        if (GuidNul(g)) {
            if (g_nGuidNuls++ < 3) printf("[UNLK] %s sans lien (+0x18 nul) dans \"%s\"\n", cls, evt);
        } else {
            uintptr_t dil = ResoudreDilemme(g);
            if (!dil) {
                if (g_nNonResolus++ < 3)
                    printf("[UNLK] %s : dilemme %016llX non resolu (\"%s\")\n", cls, (unsigned long long)g.a, evt);
            } else {
                g_nResolus++;
                ParcourirDilemme(dil, prof + 1, evt, rep);
            }
        }
    }
    // Generique : tout tableau d'objets "Frostpunk..." DANS l'objet (taille de
    // sa fiche de reflexion ; effets imbriques des Select / Switch / Timed /
    // Randomized...). Taille inconnue : on ne descend pas.
    int taille = TailleObjet(obj);
    for (int off = 0x08; off + 0x10 <= taille; off += 8) {
        uintptr_t tab = 0; uint32_t n = 0;
        if (!SafeRead((void*)(obj + off), &tab, 8) || !SafeRead((void*)(obj + off + 8), &n, 4)) continue;
        if (tab <= 0x10000 || n == 0 || n > 256) continue;
        uintptr_t premier = 0;
        if (!SafeRead((void*)tab, &premier, 8) || premier <= 0x10000) continue;
        char c2[96] = {};
        if (GetClassNameFP(premier, c2, sizeof(c2))) {
            if (strncmp(c2, "Frostpunk", 9) == 0) ParcourirTableau(obj + off, prof + 1, evt, rep);
            continue;
        }
        // Pointeurs vers des structures intégrées SANS vtable (ex.
        // FrostpunkSwitchEffect +0x18 Cases -> FrostpunkCaseEntry, 0x38 :
        // +0x08 conditions, +0x20 effets) : chaque élément est fouillé à la
        // recherche de ses propres tableaux d'objets "Frostpunk...".
        for (uint32_t i = 0; i < n && i < 64; ++i) {
            uintptr_t elt = 0;
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &elt, 8) || elt <= 0x10000 || DejaVu(elt)) continue;
            for (int o2 = 0; o2 + 0x10 <= 0x80; o2 += 8) {
                uintptr_t t2 = 0, p2 = 0; uint32_t n2 = 0;
                if (!SafeRead((void*)(elt + o2), &t2, 8) || !SafeRead((void*)(elt + o2 + 8), &n2, 4)) continue;
                if (t2 <= 0x10000 || n2 == 0 || n2 > 256 || !SafeRead((void*)t2, &p2, 8) || p2 <= 0x10000) continue;
                char c3[96] = {};
                if (GetClassNameFP(p2, c3, sizeof(c3)) && strncmp(c3, "Frostpunk", 9) == 0)
                    ParcourirTableau(elt + o2, prof + 1, evt, rep);
            }
        }
    }
}

// Liens {GUID} d'un reglage vers des entrees d'un tableau de config ; ecrit
// les noms (+0x00) des entrees de 'tabCible' dont le GUID (+0x08) correspond.
void NomsDesLiens(uintptr_t adrLiens, uintptr_t adrTabCible, char* out, int taille)
{
    out[0] = 0;
    uintptr_t liens = 0, cible = 0; int nl = 0, nc = 0;
    if (!LireTableau(adrLiens, liens, nl) || !LireTableau(adrTabCible, cible, nc)) return;
    for (int i = 0; i < nl; ++i) {
        Guid g{};
        if (!LireGuid(liens + (uintptr_t)i * 0x10, g)) continue;
        for (int k = 0; k < nc; ++k) {
            uintptr_t e = 0; Guid ge{};
            if (SafeRead((void*)(cible + (uintptr_t)k * 8), &e, 8) && e > 0x10000 &&
                LireGuid(e + 0x08, ge) && GuidEgal(ge, g)) {
                char nom[96];
                LireTexte(e, nom, sizeof(nom));
                if (out[0]) Ajouter(out, taille, ", ");
                Ajouter(out, taille, nom);
                break;
            }
        }
    }
}

void DiagUnlocks()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t cfgScn = base + OFF_SCENARIOS_CONFIG;
    memset(g_vus, 0, sizeof(g_vus));
    g_nVus = 0;
    g_nVusTotal = 0;
    g_nTrouves = 0;
    g_nEffetsDilemme = g_nGuidNuls = g_nNonResolus = g_nResolus = 0;

    // 1) Tous les evenements de tous les repertoires.
    uintptr_t reps = 0; int nReps = 0;
    if (!LireTableau(base + OFF_REPERTOIRES_EVT, reps, nReps)) {
        printf("[UNLK] repertoires d'evenements illisibles\n");
        return;
    }
    printf("[UNLK] ===== %d repertoires d'evenements =====\n", nReps);
    for (int r = 0; r < nReps; ++r) {
        uintptr_t rep = 0;
        if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000) continue;
        char nomRep[96];
        LireTexte(rep, nomRep, sizeof(nomRep));
        uintptr_t evts = 0; int nEvts = 0;
        if (!LireTableau(rep + REP_EVENEMENTS, evts, nEvts)) continue;
        for (int e = 0; e < nEvts; ++e) {
            uintptr_t evt = 0;
            if (!SafeRead((void*)(evts + (uintptr_t)e * 8), &evt, 8) || evt <= 0x10000) continue;
            char nomEvt[128];
            LireTexte(evt, nomEvt, sizeof(nomEvt));
            ViderVus();
            g_evtDonneIdeologie = false;
            ParcourirTableau(evt + EVT_EFFETS, 0, nomEvt, nomRep);
            if (g_evtDonneIdeologie) DecrireEvenement(evt);
        }
    }
    printf("[UNLK] %d effets AddUnlock trouves (%d objets parcourus)\n", g_nTrouves, g_nVusTotal);
    printf("[UNLK] effets de dilemme : %d (lien nul %d, non resolus %d, resolus %d)\n",
        g_nEffetsDilemme, g_nGuidNuls, g_nNonResolus, g_nResolus);

    // Repertoires actifs dans la partie : systeme d'evenements
    // (*(base + 0x40935C8)) +0x2128, lu comme un tableau de pointeurs de
    // repertoires (format suppose : verifie par le nom de chaque element).
    uintptr_t sysEvt = 0;
    SafeRead((void*)(base + 0x40935C8), &sysEvt, 8);
    uintptr_t actifs = 0; int nActifs = 0;
    if (sysEvt > 0x10000 && LireTableau(sysEvt + 0x2128, actifs, nActifs)) {
        char liste[4096] = {};
        for (int i = 0; i < nActifs; ++i) {
            uintptr_t rep = 0;
            if (!SafeRead((void*)(actifs + (uintptr_t)i * 8), &rep, 8) || rep <= 0x10000) continue;
            char nom[96];
            LireTexte(rep, nom, sizeof(nom));
            if (liste[0]) Ajouter(liste, sizeof(liste), ", ");
            Ajouter(liste, sizeof(liste), nom);
        }
        printf("[UNLK] %d repertoires actifs dans cette partie : %s\n", nActifs, liste);
    } else {
        printf("[UNLK] repertoires actifs illisibles (systeme d'evenements 0x%llX)\n", (unsigned long long)sysEvt);
    }

    // 2) Repertoires d'evenements et unlocks de depart de chaque scenario.
    uintptr_t tabScn = 0; int nScn = 0;
    LireTableau(cfgScn + CFG_SCENARIOS, tabScn, nScn);
    for (int s = 0; s < nScn; ++s) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(tabScn + (uintptr_t)s * 8), &scn, 8) || scn <= 0x10000) continue;
        char nom[96], reglagesEvt[512], reglagesUnl[512];
        LireTexte(scn, nom, sizeof(nom));
        NomsDesLiens(scn + SCN_EVENEMENTS, cfgScn + CFG_REGLAGES_EVT, reglagesEvt, sizeof(reglagesEvt));
        NomsDesLiens(scn + SCN_UNLOCKS, cfgScn + CFG_REGLAGES_UNLOCKS, reglagesUnl, sizeof(reglagesUnl));
        printf("[UNLK] scenario %s : evenements [%s] ; unlocks [%s]\n", nom, reglagesEvt, reglagesUnl);
    }
    // Detail des reglages : repertoires et unlocks de depart.
    uintptr_t tabRe = 0; int nRe = 0;
    if (LireTableau(cfgScn + CFG_REGLAGES_EVT, tabRe, nRe))
        for (int i = 0; i < nRe; ++i) {
            uintptr_t e = 0;
            if (!SafeRead((void*)(tabRe + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
            char nom[96], liste[2048];
            LireTexte(e, nom, sizeof(nom));
            NomsDesLiens(e + REGLAGE_LIENS, base + OFF_REPERTOIRES_EVT, liste, sizeof(liste));
            printf("[UNLK] reglage d'evenements %s : repertoires %s\n", nom, liste);
        }
    uintptr_t tabRu = 0; int nRu = 0;
    if (LireTableau(cfgScn + CFG_REGLAGES_UNLOCKS, tabRu, nRu))
        for (int i = 0; i < nRu; ++i) {
            uintptr_t e = 0;
            if (!SafeRead((void*)(tabRu + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
            char nom[96], liste[2048] = {};
            LireTexte(e, nom, sizeof(nom));
            uintptr_t liens = 0; int nl = 0;
            if (LireTableau(e + REGLAGE_LIENS, liens, nl))
                for (int k = 0; k < nl; ++k) {
                    Guid g{}; char nu[96];
                    if (!LireGuid(liens + (uintptr_t)k * 0x10, g)) continue;
                    if (!NomUnlock(g, nu, sizeof(nu))) sprintf_s(nu, "?");
                    if (liste[0]) Ajouter(liste, sizeof(liste), ", ");
                    Ajouter(liste, sizeof(liste), nu);
                }
            printf("[UNLK] reglage d'unlocks %s : depart %s\n", nom, liste[0] ? liste : "(aucun)");
        }
}

// --- Releve des technologies (F4, 2026-11-03) ---------------------------------
// But : palier, arbre et prerequis de chaque techno, pour la logique de
// l'apworld. Decompiles du 2026-11-03 :
//   FrostpunkTechnologiesConfig (instance statique base + 0x4074430 ; sa
//     collection de technos, en +0x90, est rendue par FUN_14145b680)
//     : +0x78 categories (onglets), +0xB0 technos,
//     +0x100 ensembles de technos (tableaux de pointeurs, compte +8)
//   FrostpunkTechnologyEntry (0x128, base FrostpunkProgressTreeEntry) :
//     +0x18 nom, +0x20 GUID (hash du mod = 8 premiers octets), +0x30 Enabled,
//     +0x38 Requirements, +0x50 Show requirements, +0x68 Display name,
//     +0x88 Grid position (2 x int32), +0x90 Category (lien GUID),
//     +0x10C Display category (enum), +0x110 Tree entry type (NameString)
//   FrostpunkTechnologyRequirement (0x40) : +0x30 lien GUID vers la techno
//     (verifie par FUN_1416a71f0, cas 4 : techno a l'etat 2)
//   FrostpunkTechnologyCategoryEntry (0x38) : +0x00 nom, +0x08 GUID,
//     +0x18 Display name
//   FrostpunkTechnologySetEntry (0x30) : +0x00 nom, +0x08 GUID, +0x18 Technologies
//   FrostpunkScenariosConfig +0x1D8 : FrostpunkScenarioTechnologySetupEntry
//     (0x50) : +0x00 nom, +0x08 GUID, +0x18 Starting technologies,
//     +0x30 Technology set (lien)
//   FrostpunkScenarioEntry +0x138 : Technology setups (liens de 0x10, compte +8)
// Lignes a format fixe, relues par outils\technos_table.py.
constexpr uintptr_t OFF_TECH_CONFIG = 0x4074430;
constexpr int TCFG_CATEGORIES = 0x78;
constexpr int TCFG_TECHNOS    = 0xB0;
constexpr int TCFG_ENSEMBLES  = 0x100;
constexpr int TECH_NOM        = 0x18;
constexpr int TECH_GUID       = 0x20;
constexpr int TECH_ACTIVE     = 0x30;
constexpr int TECH_PREREQUIS  = 0x38;
constexpr int TECH_AFFICHAGE  = 0x50;
constexpr int TECH_GRILLE     = 0x88;
constexpr int TECH_CATEGORIE  = 0x90;
constexpr int TECH_CAT_AFF    = 0x10C;
constexpr int TECH_TYPE_ARBRE = 0x110;
constexpr int TCAT_NOM        = 0x00;
constexpr int TCAT_GUID       = 0x08;
constexpr int TSET_NOM        = 0x00;
constexpr int TSET_GUID       = 0x08;
constexpr int TSET_TECHNOS    = 0x18;
constexpr int CFG_REGLAGES_TECH = 0x1D8;
constexpr int TREG_NOM        = 0x00;
constexpr int TREG_GUID       = 0x08;
constexpr int TREG_DEPART     = 0x18;
constexpr int TREG_ENSEMBLE   = 0x30;
constexpr int SCN_REGLAGES_TECH = 0x138;

// 1er test (2026-11-03) : base + 0x40744C0 = EntryCollection<FrostpunkTechnologyEntry>,
// membre de la config en +0x90 (constructeur FUN_14145a4c0) -> config en
// base + 0x4074430. Repli : balayage RTTI autour, comme FrostpunkUnlocksConfig.
uintptr_t ConfigTechnos()
{
    static uintptr_t trouvee = 0;
    if (trouvee) return trouvee;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    for (uintptr_t a = base + OFF_TECH_CONFIG - 0x1000; a < base + OFF_TECH_CONFIG + 0x1000; a += 8) {
        uintptr_t vptr = 0;
        if (!SafeRead((void*)a, &vptr, 8) || vptr < base || vptr > base + 0x5000000) continue;
        char cls[64] = {};
        if (GetClassNameFP(a, cls, sizeof(cls)) && strcmp(cls, "FrostpunkTechnologiesConfig") == 0) {
            trouvee = a;
            printf("[TECHNO] FrostpunkTechnologiesConfig trouvee a base + 0x%llX\n", (unsigned long long)(a - base));
            return a;
        }
    }
    return 0;
}

bool NomTechno(const Guid& g, char* out, int taille)
{
    uintptr_t cfg = ConfigTechnos();
    uintptr_t tab = 0; int n = 0;
    if (!cfg || !LireTableau(cfg + TCFG_TECHNOS, tab, n)) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t t = 0; Guid gt{};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &t, 8) || t <= 0x10000) continue;
        if (LireGuid(t + TECH_GUID, gt) && GuidEgal(gt, g)) { LireTexte(t + TECH_NOM, out, taille); return true; }
    }
    return false;
}

// Nom d'une entree (+0x00 nom, +0x08 GUID) d'un tableau de pointeurs.
bool NomEntreeParGuid(uintptr_t adrTableau, const Guid& g, char* out, int taille)
{
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrTableau, tab, n)) return false;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0; Guid ge{};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
        if (LireGuid(e + 0x08, ge) && GuidEgal(ge, g)) { LireTexte(e, out, taille); return true; }
    }
    return false;
}

// Liste de liens GUID (pas de 0x10) vers des technos, en noms.
void ListeDeTechnos(uintptr_t adrTableau, char* out, int taille)
{
    out[0] = 0;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrTableau, tab, n)) { strcpy_s(out, taille, "(illisible)"); return; }
    if (n == 0) { strcpy_s(out, taille, "(aucune)"); return; }
    for (int i = 0; i < n; ++i) {
        Guid g{}; char nom[96];
        if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
        if (!NomTechno(g, nom, sizeof(nom))) sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        if (i > 0) Ajouter(out, taille, ", ");
        Ajouter(out, taille, nom);
    }
}

void DiagTechnos()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t cfg = ConfigTechnos();
    if (!cfg) {
        char cls[64] = "?";
        GetClassNameFP(base + OFF_TECH_CONFIG, cls, sizeof(cls));
        printf("[TECHNO] config des technos introuvable autour de base + 0x%llX (classe %s)\n",
            (unsigned long long)OFF_TECH_CONFIG, cls);
        return;
    }

    // --- Onglets --------------------------------------------------------------
    uintptr_t tabCat = 0; int nCat = 0;
    LireTableau(cfg + TCFG_CATEGORIES, tabCat, nCat);
    uintptr_t tabT = 0; int nT = 0;
    LireTableau(cfg + TCFG_TECHNOS, tabT, nT);
    uintptr_t tabSet = 0; int nSet = 0;
    LireTableau(cfg + TCFG_ENSEMBLES, tabSet, nSet);
    printf("[TECHNO] ===== %d technos, %d onglets, %d ensembles (config 0x%llX) =====\n",
        nT, nCat, nSet, (unsigned long long)cfg);
    for (int c = 0; c < nCat; ++c) {
        uintptr_t cat = 0; Guid g{};
        if (!SafeRead((void*)(tabCat + (uintptr_t)c * 8), &cat, 8) || cat <= 0x10000) continue;
        char nom[96];
        LireTexte(cat + TCAT_NOM, nom, sizeof(nom));
        LireGuid(cat + TCAT_GUID, g);
        printf("[TECHNO] CAT | %s | %016llX\n", nom, (unsigned long long)g.a);
    }

    // --- Technos ----------------------------------------------------------------
    // T | hash | nom | onglet | colonne | ligne | type d'entree | categorie d'affichage | active
    // REQ | hash | prerequis        VOIR | hash | prerequis d'affichage
    for (int i = 0; i < nT; ++i) {
        uintptr_t t = 0;
        if (!SafeRead((void*)(tabT + (uintptr_t)i * 8), &t, 8) || t <= 0x10000) continue;
        char nom[96], onglet[96], type[64];
        Guid g{}, gCat{};
        int32_t grille[2] = { -1, -1 }, catAff = -1;
        uint8_t active = 0;
        LireTexte(t + TECH_NOM, nom, sizeof(nom));
        LireGuid(t + TECH_GUID, g);
        LireGuid(t + TECH_CATEGORIE, gCat);
        SafeRead((void*)(t + TECH_GRILLE), grille, sizeof(grille));
        SafeRead((void*)(t + TECH_CAT_AFF), &catAff, 4);
        SafeRead((void*)(t + TECH_ACTIVE), &active, 1);
        LireTexte(t + TECH_TYPE_ARBRE, type, sizeof(type));
        if (GuidNul(gCat)) strcpy_s(onglet, "-");
        else if (!NomEntreeParGuid(cfg + TCFG_CATEGORIES, gCat, onglet, sizeof(onglet)))
            sprintf_s(onglet, "?%016llX", (unsigned long long)gCat.a);
        printf("[TECHNO] T | %016llX | %s | %s | %d | %d | %s | %d | %d\n",
            (unsigned long long)g.a, nom, onglet, grille[0], grille[1], type, catAff, active);
        char req[2048] = {}, voir[2048] = {};
        DecrireListeExigences(t + TECH_PREREQUIS, req, sizeof(req), 0, " ; ");
        DecrireListeExigences(t + TECH_AFFICHAGE, voir, sizeof(voir), 0, " ; ");
        printf("[TECHNO] REQ | %016llX | %s\n", (unsigned long long)g.a, req);
        printf("[TECHNO] VOIR | %016llX | %s\n", (unsigned long long)g.a, voir);
    }

    // --- Ensembles (technos presentes dans un scenario) ------------------------
    for (int s = 0; s < nSet; ++s) {
        uintptr_t e = 0;
        if (!SafeRead((void*)(tabSet + (uintptr_t)s * 8), &e, 8) || e <= 0x10000) continue;
        char nom[96];
        LireTexte(e + TSET_NOM, nom, sizeof(nom));
        uintptr_t liens = 0; int nl = 0;
        LireTableau(e + TSET_TECHNOS, liens, nl);
        printf("[TECHNO] SET | %s | %d\n", nom, nl);
        for (int k = 0; k < nl; ++k) {
            Guid g{};
            if (!LireGuid(liens + (uintptr_t)k * 0x10, g)) continue;
            printf("[TECHNO] SETT | %s | %016llX\n", nom, (unsigned long long)g.a);
        }
    }

    // --- Reglages de technos et scenarios -----------------------------------
    uintptr_t scenarios = base + OFF_SCENARIOS_CONFIG;
    uintptr_t tabReg = 0; int nReg = 0;
    LireTableau(scenarios + CFG_REGLAGES_TECH, tabReg, nReg);
    for (int r = 0; r < nReg; ++r) {
        uintptr_t reg = 0;
        if (!SafeRead((void*)(tabReg + (uintptr_t)r * 8), &reg, 8) || reg <= 0x10000) continue;
        char nom[96], ens[96], depart[2048];
        Guid gEns{};
        LireTexte(reg + TREG_NOM, nom, sizeof(nom));
        LireGuid(reg + TREG_ENSEMBLE, gEns);
        if (GuidNul(gEns)) strcpy_s(ens, "-");
        else if (!NomEntreeParGuid(cfg + TCFG_ENSEMBLES, gEns, ens, sizeof(ens)))
            sprintf_s(ens, "?%016llX", (unsigned long long)gEns.a);
        ListeDeTechnos(reg + TREG_DEPART, depart, sizeof(depart));
        printf("[TECHNO] REG | %s | %s | %s\n", nom, ens, depart);
    }
    uintptr_t tabScn = 0; int nScn = 0;
    LireTableau(scenarios + CFG_SCENARIOS, tabScn, nScn);
    for (int s = 0; s < nScn; ++s) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(tabScn + (uintptr_t)s * 8), &scn, 8) || scn <= 0x10000) continue;
        char nom[96], liste[1024] = {};
        LireTexte(scn + SCN_NOM, nom, sizeof(nom));
        uintptr_t tab = 0; int n = 0;
        LireTableau(scn + SCN_REGLAGES_TECH, tab, n);
        for (int i = 0; i < n; ++i) {
            Guid g{}; char trouve[96];
            if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
            if (!NomEntreeParGuid(scenarios + CFG_REGLAGES_TECH, g, trouve, sizeof(trouve)))
                sprintf_s(trouve, "?%016llX", (unsigned long long)g.a);
            if (i > 0) Ajouter(liste, sizeof(liste), ", ");
            Ajouter(liste, sizeof(liste), trouve);
        }
        printf("[TECHNO] SCN | %s | %s\n", nom, n ? liste : "-");
    }
    printf("[TECHNO] releve termine\n");
}

// Releve pour la table des checks de lois (outils\lois_table.py), une ligne
// par objet, champs separes par " | " :
//   CAT | nom | prerequis de l'arbre
//   L | guid | nom | nom affiche (- si non vu) | categorie | activee | groupe
//     d'exclusion (-) | arbre AP (-) | poids | grille x | grille y |
//     prerequis | prerequis d'affichage
//   REG | reglage | lois de depart | lois verrouillees
//   SCN | scenario | reglages
// Apres DiagLois (g_lois rempli). Thread du jeu (arbres de lois_ap).
void DiagTableLois(uintptr_t base)
{
    uintptr_t societe = base + OFF_SOCIETY_CONFIG;
    uintptr_t scenarios = base + OFF_SCENARIOS_CONFIG;
    printf("[LOITAB] ===== table des lois : %d lois =====\n", g_nLois);

    uintptr_t tabCat = 0; int nCat = 0;
    LireTableau(societe + CFG_CATEGORIES, tabCat, nCat);
    for (int c = 0; c < nCat; ++c) {
        uintptr_t cat = 0;
        if (!SafeRead((void*)(tabCat + (uintptr_t)c * 8), &cat, 8) || cat <= 0x10000) continue;
        char nom[96], req[1024] = {};
        LireTexte(cat + CAT_NOM, nom, sizeof(nom));
        DecrireListeExigences(cat + CAT_PREREQUIS, req, sizeof(req), 0, " ; ");
        printf("[LOITAB] CAT | %s | %s\n", nom, req);
    }

    for (int i = 0; i < g_nLois; ++i) {
        const Loi& l = g_lois[i];
        char cle[192], exclusion[96], categorie[96] = "?", arbre[96];
        LireTexte(l.adr + LOI_NOM_AFFICHE, cle, sizeof(cle));
        const char* affiche = NomAfficheDe(cle);
        LireTexte(l.adr + LOI_EXCLUSION, exclusion, sizeof(exclusion));
        if (!exclusion[0]) strcpy_s(exclusion, "-");
        for (int c = 0; c < nCat; ++c) {
            uintptr_t cat = 0; Guid g{};
            if (SafeRead((void*)(tabCat + (uintptr_t)c * 8), &cat, 8) && cat > 0x10000 &&
                LireGuid(cat + CAT_GUID, g) && GuidEgal(g, l.categorie)) {
                LireTexte(cat + CAT_NOM, categorie, sizeof(categorie));
                break;
            }
        }
        int poids = 0;
        if (!LoisAPArbreDeLoi(l.adr, arbre, sizeof(arbre), &poids)) strcpy_s(arbre, "-");
        uint8_t activee = 0;
        int32_t grille[2] = {};
        SafeRead((void*)(l.adr + LOI_ACTIVEE), &activee, 1);
        SafeRead((void*)(l.adr + LOI_GRILLE), grille, 8);
        char req[2048] = {}, voir[1024] = {};
        DecrireListeExigences(l.adr + LOI_PREREQUIS, req, sizeof(req), 0, " ; ");
        DecrireListeExigences(l.adr + LOI_AFFICHAGE, voir, sizeof(voir), 0, " ; ");
        printf("[LOITAB] L | %016llX | %s | %s | %s | %d | %s | %s | %d | %d | %d | %s | %s\n",
            (unsigned long long)l.guid.a, l.nom, affiche ? affiche : "-", categorie, activee,
            exclusion, arbre, poids, grille[0], grille[1], req, voir);
    }

    uintptr_t tabReg = 0; int nReg = 0;
    LireTableau(scenarios + CFG_REGLAGES, tabReg, nReg);
    for (int r = 0; r < nReg; ++r) {
        uintptr_t reg = 0;
        if (!SafeRead((void*)(tabReg + (uintptr_t)r * 8), &reg, 8) || reg <= 0x10000) continue;
        char nom[96], depart[2048], verr[2048];
        LireTexte(reg + REG_NOM, nom, sizeof(nom));
        ListeDeLiens(reg + REG_DEPART, depart, sizeof(depart));
        ListeDeLiens(reg + REG_VERROUILLE, verr, sizeof(verr));
        printf("[LOITAB] REG | %s | %s | %s\n", nom, depart, verr);
    }

    uintptr_t tabScn = 0; int nScn = 0;
    LireTableau(scenarios + CFG_SCENARIOS, tabScn, nScn);
    for (int s = 0; s < nScn; ++s) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(tabScn + (uintptr_t)s * 8), &scn, 8) || scn <= 0x10000) continue;
        char nom[96], liste[1024] = {};
        LireTexte(scn + SCN_NOM, nom, sizeof(nom));
        uintptr_t tab = 0; int n = 0;
        LireTableau(scn + SCN_REGLAGES, tab, n);
        for (int i = 0; i < n; ++i) {
            Guid g{}; char trouve[96] = "?";
            if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
            for (int r = 0; r < nReg; ++r) {
                uintptr_t reg = 0; Guid gr{};
                if (SafeRead((void*)(tabReg + (uintptr_t)r * 8), &reg, 8) && reg > 0x10000 &&
                    LireGuid(reg + REG_GUID, gr) && GuidEgal(gr, g)) {
                    LireTexte(reg + REG_NOM, trouve, sizeof(trouve));
                    break;
                }
            }
            if (i > 0) Ajouter(liste, sizeof(liste), ", ");
            Ajouter(liste, sizeof(liste), trouve);
        }
        printf("[LOITAB] SCN | %s | %s\n", nom, n ? liste : "-");
    }
    printf("[LOITAB] releve termine\n");
}
} // namespace

void LoisParcourirEvenements(VisiteurEffet_t visiteur)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    memset(g_vus, 0, sizeof(g_vus));
    g_nVus = 0;
    g_nVusTotal = 0;
    uintptr_t reps = 0; int nReps = 0;
    if (!visiteur || !LireTableau(base + OFF_REPERTOIRES_EVT, reps, nReps)) return;
    g_visiteur = visiteur;
    for (int r = 0; r < nReps; ++r) {
        uintptr_t rep = 0;
        if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000) continue;
        char nomRep[96];
        LireTexte(rep, nomRep, sizeof(nomRep));
        uintptr_t evts = 0; int nEvts = 0;
        if (!LireTableau(rep + REP_EVENEMENTS, evts, nEvts)) continue;
        for (int e = 0; e < nEvts; ++e) {
            uintptr_t evt = 0;
            if (!SafeRead((void*)(evts + (uintptr_t)e * 8), &evt, 8) || evt <= 0x10000) continue;
            char nomEvt[128];
            LireTexte(evt, nomEvt, sizeof(nomEvt));
            ViderVus();
            g_evtCourant = evt;
            ParcourirTableau(evt + EVT_EFFETS, 0, nomEvt, nomRep);
        }
    }
    g_visiteur = nullptr;
    g_evtCourant = 0;
}

void LoisParcourirEffets(uintptr_t adrTableau, VisiteurEffet_t visiteur, const char* nom, const char* rep)
{
    if (!visiteur) return;
    g_visiteur = visiteur;
    g_evtCourant = 0;
    ViderVus();
    ParcourirTableau(adrTableau, 0, nom, rep);
    g_visiteur = nullptr;
}

void LoisDecrireEvenement(uintptr_t evt, const char* prefixe)
{
    DecrireEvenement(evt, prefixe);
}

void LoisDecrireExigences(uintptr_t adrTableau, char* out, int taille)
{
    out[0] = 0;
    DecrireListeExigences(adrTableau, out, taille, 0, " ; ");
}

void LoisDemanderReleve()
{
    g_releveDemande = true;
    printf("[LOIS] releve demande : il s'execute sur le thread du jeu (jeu hors pause)\n");
}

// Debug LOIS_SANS_DELAI (remise a zero continue du delai entre deux lois par
// FUN_141700760 = ResetSociotechCooldown) retire le 2026-11-04 a la demande
// de l'auteur : le delai du jeu s'applique.
void LoisPompe()
{
    if (g_releveDemande) {
        g_releveDemande = false;
        DiagLois();
        DiagTableLois((uintptr_t)GetModuleHandleW(NULL));   // table des checks de lois
        DiagUnlocks();
        printf("[LOIS] releve termine\n");
        DiagTechnos();   // releve des technos (palier, arbre, prerequis)
    }
}

void DiagLois()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t societe = base + OFF_SOCIETY_CONFIG;
    uintptr_t scenarios = base + OFF_SCENARIOS_CONFIG;

    TrouverConfigUnlocks(base);
    ChargerUnlocksActifs();
    ChargerScenarioEtPolitiques();

    // --- Lois ---------------------------------------------------------------
    g_nLois = 0;
    uintptr_t tabLois = 0; int nLois = 0;
    if (!LireTableau(societe + CFG_LOIS, tabLois, nLois) || nLois == 0) {
        printf("[LOIS] liste des lois illisible (config 0x%llX)\n", (unsigned long long)societe);
        return;
    }
    for (int i = 0; i < nLois && g_nLois < MAX_ELEMENTS; ++i) {
        uintptr_t e = 0;
        if (!SafeRead((void*)(tabLois + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
        Loi& l = g_lois[g_nLois];
        l.adr = e;
        if (!LireGuid(e + LOI_GUID, l.guid) || !LireGuid(e + LOI_CATEGORIE, l.categorie)) continue;
        LireTexte(e + LOI_NOM, l.nom, sizeof(l.nom));
        g_nLois++;
    }

    uintptr_t tabCat = 0; int nCat = 0;
    LireTableau(societe + CFG_CATEGORIES, tabCat, nCat);
    printf("[LOIS] ===== %d lois, %d categories (config 0x%llX) =====\n",
        g_nLois, nCat, (unsigned long long)societe);

    // --- Par categorie (arbre) ------------------------------------------------
    static bool classee[MAX_ELEMENTS];
    memset(classee, 0, sizeof(classee));
    for (int c = 0; c < nCat; ++c) {
        uintptr_t cat = 0;
        if (!SafeRead((void*)(tabCat + (uintptr_t)c * 8), &cat, 8) || cat <= 0x10000) continue;
        Guid gCat{};
        LireGuid(cat + CAT_GUID, gCat);
        char nom[96], affiche[160];
        LireTexte(cat + CAT_NOM, nom, sizeof(nom));
        LireTexte(cat + CAT_NOM_AFFICHE, affiche, sizeof(affiche));
        int n = 0;
        for (int i = 0; i < g_nLois; ++i) if (GuidEgal(g_lois[i].categorie, gCat)) n++;
        printf("[LOIS] --- Categorie %d : %s (%s), %d lois\n", c, nom, affiche, n);
        char reqCat[2048] = {};
        DecrireListeExigences(cat + CAT_PREREQUIS, reqCat, sizeof(reqCat), 0, " ; ");
        printf("[LOIS]     requis de la categorie : %s\n", reqCat);
        for (int i = 0; i < g_nLois; ++i)
            if (GuidEgal(g_lois[i].categorie, gCat)) { DecrireLoi(g_lois[i]); classee[i] = true; }
    }
    int nOrph = 0;
    for (int i = 0; i < g_nLois; ++i) if (!classee[i]) nOrph++;
    if (nOrph) {
        printf("[LOIS] --- Sans categorie connue : %d lois\n", nOrph);
        for (int i = 0; i < g_nLois; ++i)
            if (!classee[i]) {
                printf("[LOIS]   (categorie %s)\n", GuidNul(g_lois[i].categorie) ? "nulle" : "inconnue");
                DecrireLoi(g_lois[i]);
            }
    }

    // --- Reglages par scenario ------------------------------------------------
    uintptr_t tabReg = 0; int nReg = 0;
    LireTableau(scenarios + CFG_REGLAGES, tabReg, nReg);
    printf("[LOIS] ===== %d reglages de lois (config des scenarios 0x%llX) =====\n",
        nReg, (unsigned long long)scenarios);
    for (int r = 0; r < nReg; ++r) {
        uintptr_t reg = 0;
        if (!SafeRead((void*)(tabReg + (uintptr_t)r * 8), &reg, 8) || reg <= 0x10000) continue;
        char nom[96], depart[2048], verr[2048];
        LireTexte(reg + REG_NOM, nom, sizeof(nom));
        ListeDeLiens(reg + REG_DEPART, depart, sizeof(depart));
        ListeDeLiens(reg + REG_VERROUILLE, verr, sizeof(verr));
        printf("[LOIS] reglage %s\n", nom);
        printf("[LOIS]     de depart : %s\n", depart);
        printf("[LOIS]     verrouillees : %s\n", verr);
    }

    uintptr_t tabScn = 0; int nScn = 0;
    LireTableau(scenarios + CFG_SCENARIOS, tabScn, nScn);
    printf("[LOIS] ===== %d scenarios =====\n", nScn);
    for (int s = 0; s < nScn; ++s) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(tabScn + (uintptr_t)s * 8), &scn, 8) || scn <= 0x10000) continue;
        char nom[96], liste[1024] = {};
        LireTexte(scn + SCN_NOM, nom, sizeof(nom));
        uintptr_t tab = 0; int n = 0;
        LireTableau(scn + SCN_REGLAGES, tab, n);
        for (int i = 0; i < n; ++i) {
            Guid g{};
            if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
            char trouve[96] = "?";
            for (int r = 0; r < nReg; ++r) {
                uintptr_t reg = 0; Guid gr{};
                if (SafeRead((void*)(tabReg + (uintptr_t)r * 8), &reg, 8) && reg > 0x10000 &&
                    LireGuid(reg + REG_GUID, gr) && GuidEgal(gr, g)) {
                    LireTexte(reg + REG_NOM, trouve, sizeof(trouve));
                    break;
                }
            }
            if (i > 0) Ajouter(liste, sizeof(liste), ", ");
            Ajouter(liste, sizeof(liste), trouve);
        }
        printf("[LOIS] scenario %-40s reglages : %s\n", nom, n ? liste : "(aucun)");
    }

    // --- Lois presentes dans le scenario courant ------------------------------
    LoisDuScenarioCourant(base, tabCat, nCat);

    // --- Etat courant (partie chargee) ----------------------------------------
    uintptr_t sys = 0;
    SafeRead((void*)(base + OFF_SYSTEME_LOIS), &sys, 8);
    uintptr_t tabEtat = 0; int nEtat = 0;
    if (sys <= 0x10000 || !LireTableau(sys + SYS_ETATS, tabEtat, nEtat)) {
        printf("[LOIS] systeme des lois illisible (pas en partie ?)\n");
        return;
    }
    char cls[64] = "?";
    GetClassNameFP(sys, cls, sizeof(cls));
    printf("[LOIS] ===== etat courant : systeme 0x%llX (%s), %d lois =====\n",
        (unsigned long long)sys, cls, nEtat);
    char ligne[4096] = {};
    for (int i = 0; i < nEtat; ++i) {
        uintptr_t loi = 0; int32_t etat = 0;
        if (!SafeRead((void*)(tabEtat + (uintptr_t)i * 0x18), &loi, 8)) continue;
        SafeRead((void*)(tabEtat + (uintptr_t)i * 0x18 + 0x10), &etat, 4);
        if (etat == 0) continue;
        const char* nom = NomDeLoiParAdresse(loi);
        char elem[128];
        sprintf_s(elem, "%s=%d", nom ? nom : "?", etat);
        if (ligne[0]) Ajouter(ligne, sizeof(ligne), ", ");
        Ajouter(ligne, sizeof(ligne), elem);
    }
    printf("[LOIS] etats non nuls : %s\n", ligne[0] ? ligne : "(aucun)");
}
