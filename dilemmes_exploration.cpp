// dilemmes_exploration.cpp
// ---------------------------------------------------------------------------
// Dilemmes d'exploration a plusieurs choix : choix unique "Archipelago
// Prevail" (2026-11-10, regle de l'auteur : gain maximal, effets du jeu
// inchanges ; table et motifs dans docs/chantiers/dilemmes_exploration.md).
//
// Principe : on modifie SUR PLACE la definition du dilemme (FrostpunkDilemma
// Entry, configuration base + 0x4069880, repertoires +0x130) : son tableau
// de choix (+0x78 : pointeur, +0x80 compte, +0x84 capacite) est remplace par
// un tableau a nous (memoire du moteur) qui ne contient qu'une COPIE du choix
// retenu (FrostpunkDilemmaChoiceEntry, 0x150 octets) dont seule la cle du
// texte (+0xA8) change ("@AP/Prevail", traduite par hkTranslate). Effets
// (+0x78), resultats (+0x90), description et texte d'effet restent ceux du
// jeu (tableaux partages, jamais liberes). La bulle creee par le jeu, ou
// relue d'une sauvegarde, lit la definition : ni clone ni GUID a recoller.
//
// Les tableaux d'origine ne sont jamais liberes (l'interface peut encore les
// tenir) ; ils sont remis en place si la partie suivante est un autre
// scenario (definitions partagees, ex. "[Frostland] 015 Coal Mine").
// ---------------------------------------------------------------------------
#include "dilemmes_exploration.h"
#include "mod_common.h"
#include "scenario.h"
#include "groupes.h"     // CleMoteur
#include <cstdio>
#include <cstring>

namespace {

static const uintptr_t OFF_CONFIG_DILEMMES = Adr(0x4069880);   // FrostpunkDilemmasConfig
constexpr int CFG_DIL_REPERTOIRES = 0x130;   // EntryDirectory* (compte +0x138)
constexpr int REP_ENTREES         = 0x18;    // entrees du repertoire (compte +0x20)
constexpr int DIL_CHOIX           = 0x78;    // tableau des choix : ptr, +8 compte, +0xC capacite
constexpr int TAILLE_CHOIX        = 0x150;
constexpr int CHOIX_TEXTE         = 0xA8;
const char* const CLE_PREVAIL     = "@AP/Prevail";

// Choix force par dilemme (numero 1..N du choix dans le jeu). Scenario =
// nom interne (g_run.scenario).
// 2026-11-11 (demande de l'auteur) : jamais de choix portant la tache
// "Dismantle outpost" (13) : il detruit le site ou l'on peut construire un
// avant-poste (ex. Coal Mine de Refugees). Coal Mine et Fishermen Village
// (A New Home, Refugees) : choix 1 (Retrieve base resources) au lieu de 2.
struct ChoixForce { const char* scenario; const char* dilemme; int choix; };
const ChoixForce CHOIX_FORCES[] = {
    // A New Home
    { "DEFAULT", "[Frostland] C1. Camp Compass", 1 },
    { "DEFAULT", "[Frostland] 013 Observatory Ruins", 1 },
    { "DEFAULT", "[Frostland] 021 Rebels Colony", 1 },
    { "DEFAULT", "[Frostland] 016 Tesla City", 1 },
    { "DEFAULT", "[Frostland] 022 Steel Bridge", 1 },
    { "DEFAULT", "[Frostland] 014 Workers Convoy", 1 },
    { "DEFAULT", "[Frostland] A0. Ice Crevasse", 1 },
    { "DEFAULT", "[Frostland] C0. Deep Hollow", 1 },
    { "DEFAULT", "[Frostland] 017 Inventors End", 1 },
    { "DEFAULT", "[Frostland] A1. Camp Vulcan", 1 },
    { "DEFAULT", "[Frostland] 018 Fishermen Village", 1 },   // 2 = Dismantle outpost
    { "DEFAULT", "[Frostland] 010 Children Hideout", 1 },
    { "DEFAULT", "[Frostland] B0. Snow Burrows", 1 },
    { "DEFAULT", "[Frostland] 015 Coal Mine", 1 },           // 2 = Dismantle outpost
    { "DEFAULT", "[Frostland] 001 Lost Expedition", 1 },
    { "DEFAULT", "[Frostland] 012gc Bear Cave", 2 },
    { "DEFAULT", "[Frostland] 007 Winterhome Ruins", 1 },
    { "DEFAULT", "[Frostland] 009 Freshwater Springs", 1 },
    // The Arks
    { "TechHaven", "[Frostland] New Manchester", 1 },
    { "TechHaven", "[NM] NoHelp - 01 begging for help - survivor alive", 1 },
    { "TechHaven", "[NM] NoHelp - 01 begging for help - survivor dead", 1 },
    // The Refugees
    { "REFUGEES", "[Lore] Iron Bank", 2 },
    { "REFUGEES", "[Survivors] Red Flag Camp", 2 },
    { "REFUGEES", "[Frostland] 015 Coal Mine", 1 },          // 2 = Dismantle outpost
    { "REFUGEES", "White Pass", 1 },
    { "REFUGEES", "[Lore] Hangman's Hill", 2 },
    { "REFUGEES", "[Survivors] Trail Bands", 1 },
    { "REFUGEES", "[Survivors] Black Spire", 1 },
    { "REFUGEES", "[Outpost] Fishermen Village", 1 },        // 2 = Dismantle outpost
    // Fall of Winterhome
    { "WINTERHOME", "[lore03] Steel Bridge", 1 },
    { "WINTERHOME", "[lore04] Storm Shelter", 2 },
    { "WINTERHOME", "[lore02] Coal Mine", 2 },
    { "WINTERHOME", "[lore01] Weather Station", 2 },
    { "WINTERHOME", "[lore12] Last Stand", 2 },
    { "WINTERHOME", "[lore05] Trail South", 2 },
    // The Last Autumn
    { "BUILDERS", "61. Workers Camp", 2 },
    { "BUILDERS", "66. Polar Bear Cave", 1 },
    { "BUILDERS", "46. EIC Steam Cores Store", 1 },
    { "BUILDERS", "53. Goods Station", 1 },
    { "BUILDERS", "62. Transport Lost", 1 },
    { "BUILDERS", "65. Generator Gamma", 1 },
    { "BUILDERS", "63. Convoy in Distress", 2 },
    { "BUILDERS", "63. Convoy in Distress (Winter)", 2 },
    // On the Edge
    { "Reconquest", "31. Ghost Ship", 1 },
    { "Reconquest", "12. City of Rats", 1 },
    { "Reconquest", "22. Fortified Hut", 1 },
    { "Reconquest", "06. Storm Watch", 1 },
    { "Reconquest", "15. Distant Colony", 1 },
    { "Reconquest", "27. Steel Bridge", 3 },
};
constexpr int NB_FORCES = sizeof(CHOIX_FORCES) / sizeof(CHOIX_FORCES[0]);

// Definition modifiee : tableau d'origine (remis en place au besoin) et le
// notre (garde pour la partie suivante du meme scenario).
struct DefModifiee {
    uintptr_t def;
    uintptr_t tabOrig;
    int32_t   nOrig, capOrig;
    uintptr_t tabNotre;
    int       choix;      // numero force (1..N) pour lequel tabNotre a ete fait
};
constexpr int MAX_DEFS = 128;
DefModifiee g_defs[MAX_DEFS] = {};
int         g_nDefs = 0;

DefModifiee* DefConnue(uintptr_t def)
{
    for (int i = 0; i < g_nDefs; ++i) if (g_defs[i].def == def) return &g_defs[i];
    return nullptr;
}

// Choix force pour ce dilemme dans le scenario courant, 0 sinon. 'connu' :
// le dilemme figure dans la table (n'importe quel scenario).
int ChoixPour(const char* nom, const char* scenario, bool& connu)
{
    connu = false;
    int choix = 0;
    for (int i = 0; i < NB_FORCES; ++i) {
        if (strcmp(CHOIX_FORCES[i].dilemme, nom) != 0) continue;
        connu = true;
        if (scenario && _stricmp(CHOIX_FORCES[i].scenario, scenario) == 0) choix = CHOIX_FORCES[i].choix;
    }
    return choix;
}

void LireTableau(uintptr_t def, uintptr_t& tab, int32_t& n, int32_t& cap)
{
    tab = 0; n = cap = 0;
    SafeRead((void*)(def + DIL_CHOIX), &tab, 8);
    SafeRead((void*)(def + DIL_CHOIX + 8), &n, 4);
    SafeRead((void*)(def + DIL_CHOIX + 0xC), &cap, 4);
}

void EcrireTableau(uintptr_t def, uintptr_t tab, int32_t n, int32_t cap)
{
    SafeWrite((void*)(def + DIL_CHOIX), &tab, 8);
    SafeWrite((void*)(def + DIL_CHOIX + 8), &n, 4);
    SafeWrite((void*)(def + DIL_CHOIX + 0xC), &cap, 4);
}

// Notre tableau d'un seul choix : copie du choix 'choix' (1..N) de l'original,
// texte "@AP/Prevail". 0 en cas d'echec.
uintptr_t FaireTableauPrevail(uintptr_t tabOrig, int32_t nOrig, int choix, const char* nom)
{
    uintptr_t c = 0, modele = 0;
    if (!MoteurAlloc || choix < 1 || choix > nOrig ||
        !SafeRead((void*)(tabOrig + (uintptr_t)(choix - 1) * 8), &c, 8) || c <= 0x10000 ||
        !SafeRead((void*)(c + CHOIX_TEXTE), &modele, 8)) {
        printf("[DILEXP] \"%s\" : choix %d illisible (%d choix)\n", nom, choix, nOrig);
        return 0;
    }
    uintptr_t cle = CleMoteur(CLE_PREVAIL, modele);
    uintptr_t copie = (uintptr_t)MoteurAlloc(TAILLE_CHOIX);
    uintptr_t tab = (uintptr_t)MoteurAlloc(8);
    if (!cle || !copie || !tab || !SafeRead((void*)c, (void*)copie, TAILLE_CHOIX)) {
        printf("[DILEXP] \"%s\" : copie du choix %d impossible (cle 0x%llX)\n", nom, choix, (unsigned long long)cle);
        return 0;
    }
    SafeWrite((void*)(copie + CHOIX_TEXTE), &cle, 8);
    SafeWrite((void*)tab, &copie, 8);
    return tab;
}

// Une definition de la configuration : modifiee, remise en place ou laissee.
void TraiterDefinition(uintptr_t def, const char* nom, const char* scenario, int& nModif, int& nRest)
{
    bool connu = false;
    int choix = ChoixPour(nom, scenario, connu);
    if (!connu) return;

    uintptr_t tab; int32_t n, cap;
    LireTableau(def, tab, n, cap);
    DefModifiee* d = DefConnue(def);
    // Definition relue par le jeu (ni notre tableau ni l'original connu) :
    // l'ancienne fiche ne vaut plus rien.
    if (d && tab != d->tabNotre && tab != d->tabOrig) {
        printf("[DILEXP] \"%s\" : definition relue par le jeu, fiche oubliee\n", nom);
        *d = g_defs[--g_nDefs];
        d = nullptr;
    }

    if (choix == 0) {
        // Autre scenario : choix d'origine.
        if (d && tab == d->tabNotre) {
            EcrireTableau(def, d->tabOrig, d->nOrig, d->capOrig);
            ++nRest;
        }
        return;
    }

    if (!d) {
        if (tab <= 0x10000 || n < 2 || n > 8) {
            printf("[DILEXP] \"%s\" : %d choix, laisse tel quel\n", nom, n);
            return;
        }
        if (g_nDefs >= MAX_DEFS) return;
        d = &g_defs[g_nDefs++];
        *d = { def, tab, n, cap, 0, 0 };
    }
    if (!d->tabNotre || d->choix != choix) {
        d->tabNotre = FaireTableauPrevail(d->tabOrig, d->nOrig, choix, nom);
        d->choix = choix;
        if (!d->tabNotre) return;
    }
    if (tab != d->tabNotre) {
        EcrireTableau(def, d->tabNotre, 1, 1);
        ++nModif;
        printf("[DILEXP] \"%s\" : choix %d/%d -> Archipelago Prevail\n", nom, choix, d->nOrig);
    }
}

void Appliquer()
{
    const char* scenario = g_run.valid && g_run.scenario[0] ? g_run.scenario : nullptr;
    uintptr_t cfg = (uintptr_t)GetModuleHandleW(nullptr) + OFF_CONFIG_DILEMMES, reps = 0;
    int32_t nr = 0;
    if (!SafeRead((void*)(cfg + CFG_DIL_REPERTOIRES), &reps, 8) ||
        !SafeRead((void*)(cfg + CFG_DIL_REPERTOIRES + 8), &nr, 4) || reps <= 0x10000 || nr <= 0 || nr > 1024) {
        printf("[DILEXP] repertoires des dilemmes illisibles\n");
        return;
    }
    int nModif = 0, nRest = 0;
    for (int r = 0; r < nr; ++r) {
        uintptr_t rep = 0, tab = 0;
        int32_t ne = 0;
        if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000 ||
            !SafeRead((void*)(rep + REP_ENTREES), &tab, 8) || !SafeRead((void*)(rep + REP_ENTREES + 8), &ne, 4) ||
            tab <= 0x10000 || ne <= 0 || ne > 4096) continue;
        for (int i = 0; i < ne; ++i) {
            uintptr_t def = 0, p = 0;
            char nom[160] = "";
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &def, 8) || def <= 0x10000 ||
                !SafeRead((void*)def, &p, 8) || p <= 0x10000 || !ReadAscii(p, nom, sizeof(nom))) continue;
            TraiterDefinition(def, nom, scenario, nModif, nRest);
        }
    }
    printf("[DILEXP] scenario %s : %d dilemme(s) a choix unique, %d remis d'origine\n",
        scenario ? scenario : "?", nModif, nRest);
}

} // namespace

void DilemmesExplorationPompe()
{
    static unsigned generationVue = 0;
    if (!PartiePrete() || PartieGeneration() == generationVue) return;
    generationVue = PartieGeneration();
    Appliquer();
}
