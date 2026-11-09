// ---- temperature.cpp ----
// Releve des variations de temperature scriptees (F9). Decompiles du
// 2026-11-01 (adresses Ghidra) :
//   FrostpunkEnvironmentConfig (base + 0x406B4D0, reflexion FUN_1413f5320) :
//     +0x58 Default temperature level (lien), +0x68 Default celsius degrees
//     (float, -10 par defaut), +0x6C Celsius degrees per level (float, 10),
//     +0x90 Temperature levels (pointeurs, compte +0x98), +0xF8 Weathers
//   FrostpunkTemperatureLevelEntry (0x68, FUN_1413f6430) : +0x00 nom, +0x08 GUID,
//     +0x40 Display name
//   FrostpunkWeatherEntry (FUN_1413f6dc0) : +0x00 nom, +0x08 GUID, +0x24 Storm,
//     +0x54 Temperature offset from default level (int), +0xB8 Display name
//   FrostpunkScenarioWeatherTimelineEntry (0x40, polymorphe) : +0x18 nom,
//     +0x20 GUID, +0x30 Display name
//     Simple (0x58) : +0x40 Weather changes (FrostpunkWeatherChangeEntry, pas
//       0x20 : +0x00 jour u16, +0x02 heure u8, +0x03 minute u8, +0x04 lien meteo)
//     Composite (0x60) : +0x40 Components (pas 0x18 : liens de chronologies en
//       +0x00), +0x58 Loop last component
//   FrostpunkScenarioEntry : +0xC8 Game acts setup (lien), +0x118 Weather
//     timelines (liens)
//   FrostpunkScenarioGameActsSetupEntry (0x38) : +0x18 actes (pointeurs),
//     +0x30 Start first act on game start
//   FrostpunkGameActEntry (0x30) : +0x00 nom, +0x08 GUID, +0x18 Weather timelines
//   FrostpunkStartGameActEffect (0x28) : +0x18 lien vers l'acte
//   FrostpunkDifficultyEntry (0x38) : +0x18 Default, +0x20 Modifications
//   FrostpunkDifficultyModificationEntry (0x50) : +0x30 Required scenario,
//     +0x40 Not allowed scenario
//     GameAct (0x78) : +0x50 acte, +0x60 Weather timelines override
//     Scenario (0x78) : +0x50 scenario, +0x60 Weather timelines override
//     TemperatureLevel (0x68) : +0x50 niveau de temperature, +0x60 Illness
//       chance modification (maladie a ce niveau : pas une variation)
//   FrostpunkWeatherSystem (*(base + 0x4062D48), FUN_14132de80) : +0x118
//     WeatherTimeline {FrostpunkGameTime, EntryPtr meteo} (pas 0x10), +0x130
//     SelectedWeatherTimelineEntries, +0x148 index, +0x150 meteo courante,
//     +0x158 precedente
//
// Montees de temperature Archipelago (decision de l'auteur, 2026-11-01) :
// toute montee scriptee est retiree, les baisses restent vanilla, donc le
// decalage se cumule ; chaque item "Temperature rise" rend des niveaux.
//   FrostpunkTemperatureSystem (reflexion FUN_141813280) : +0x21A0
//     CurrentTemperature (int, en niveaux), +0x20C0 evenement
//     GlobalTemperatureLevelChanged (ecouteurs +0x20D8, compte +0x20E0)
//   FUN_141812c10(sys) = recalcul de la temperature globale : indice du niveau
//     par defaut (config d'environnement +0x58 dans +0x90) + decalage de la
//     meteo courante (+0x54) ; si la valeur change, +0x21A0 puis evenements.
//     Seul appelant : l'ecouteur 0x141811540 (objet = sys + 0x20A8), inscrit
//     par FUN_141812500 sur l'evenement "meteo changee" (meteo +0x30), appele
//     par FUN_14132f260 apres la pose de +0x150 / +0x158 / +0x160.
//   Temperature d'une entite (FUN_141811e10) = +0x21A0 + chaleur locale.
#include "temperature.h"
#include "mod_common.h"
#include "lois.h"
#include "scenario.h"   // FindSceneSystem
#include "archipelago.h"  // SendTemperatureCheck
#include "MinHook.h"
#include <cstring>

namespace {

static const uintptr_t OFF_ENV_CONFIG       = Adr(0x406B4D0);
static const uintptr_t OFF_SCENARIOS_CONFIG = Adr(0x2C34490);
static const uintptr_t OFF_SYSTEME_METEO    = Adr(0x4062D48);
static const uintptr_t OFF_HORLOGE          = Adr(0x2C32A40);   // *(...) + 0xB0 = date courante
static const uintptr_t OFF_SECONDES_HEURE   = Adr(0x2C38FF0);   // int : secondes par heure de jeu

constexpr int TAILLE_CFG_SCENARIOS = 0x540;

constexpr int ENV_NIVEAU_DEFAUT    = 0x58;
constexpr int ENV_CELSIUS_DEFAUT   = 0x68;
constexpr int ENV_CELSIUS_NIVEAU   = 0x6C;
constexpr int ENV_NIVEAUX          = 0x90;
constexpr int ENV_METEOS           = 0xF8;

constexpr int NIV_NOM_AFFICHE      = 0x40;
constexpr int MET_TEMPETE          = 0x24;
constexpr int MET_DECALAGE         = 0x54;
constexpr int MET_NOM_AFFICHE      = 0xB8;

constexpr int CFG_SCENARIOS        = 0x88;
constexpr int CFG_CHRONOS          = 0x1A0;
constexpr int SCN_ACTES            = 0xC8;
constexpr int SCN_CHRONOS          = 0x118;

constexpr int CHR_NOM              = 0x18;
constexpr int CHR_GUID             = 0x20;
constexpr int CHR_LISTE            = 0x40;
constexpr int CHR_BOUCLE           = 0x58;
constexpr int CHG_PAS              = 0x20;
constexpr int CMP_PAS              = 0x18;

constexpr int REG_ACTES            = 0x18;
constexpr int REG_PREMIER_ACTE     = 0x30;
constexpr int ACTE_CHRONOS         = 0x18;

constexpr int DIF_DEFAUT           = 0x18;
constexpr int DIF_MODIFS           = 0x20;
constexpr int MOD_SCN_REQUIS       = 0x30;
constexpr int MOD_SCN_INTERDIT     = 0x40;
constexpr int MOD_CIBLE            = 0x50;
constexpr int MOD_REMPLACEMENT     = 0x60;

constexpr int SYS_CHRONOLOGIE      = 0x118;
constexpr int SYS_CHRONOS_CHOISIES = 0x130;
constexpr int SYS_INDEX            = 0x148;
constexpr int SYS_METEO_COURANTE   = 0x150;
constexpr int SYS_METEO_PRECEDENTE = 0x158;

constexpr int MAX_ELEMENTS = 1024;

volatile bool g_releveDemande = false;

struct Guid { uint64_t a, b; };

bool LireGuid(uintptr_t adr, Guid& g) { return SafeRead((void*)adr, &g, sizeof(g)); }
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
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        out[i] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }
    out[i] = 0;
    if (i == 0) strcpy_s(out, taille, "?");
}

void Ajouter(char* buf, int taille, const char* s)
{
    if ((int)strlen(buf) >= taille - 1) return;
    strncat_s(buf, taille, s, _TRUNCATE);
}

// Element d'un tableau de pointeurs dont le GUID (en +offGuid) vaut 'g'.
uintptr_t ParGuid(uintptr_t adrTableau, int offGuid, const Guid& g)
{
    uintptr_t tab = 0; int n = 0;
    if (GuidNul(g) || !LireTableau(adrTableau, tab, n)) return 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0; Guid ge{};
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) && e > 0x10000 &&
            LireGuid(e + offGuid, ge) && GuidEgal(ge, g)) return e;
    }
    return 0;
}

// Cherche 'g' dans tous les tableaux de pointeurs d'un objet de config (les
// collections dont on ne connait pas l'offset, ex. les reglages d'actes).
uintptr_t ParGuidDansObjet(uintptr_t obj, int taille, int offGuid, const Guid& g)
{
    for (int off = 0; off + 0x10 <= taille; off += 8) {
        uintptr_t e = ParGuid(obj + off, offGuid, g);
        if (e) return e;
    }
    return 0;
}

uintptr_t Base() { return (uintptr_t)GetModuleHandleW(NULL); }
uintptr_t EnvConfig() { return Base() + OFF_ENV_CONFIG; }
uintptr_t CfgScenarios() { return Base() + OFF_SCENARIOS_CONFIG; }

uintptr_t MeteoParGuid(const Guid& g) { return ParGuid(EnvConfig() + ENV_METEOS, 0x08, g); }
uintptr_t NiveauParGuid(const Guid& g) { return ParGuid(EnvConfig() + ENV_NIVEAUX, 0x08, g); }
uintptr_t ChronoParGuid(const Guid& g) { return ParGuid(CfgScenarios() + CFG_CHRONOS, CHR_GUID, g); }
uintptr_t ScenarioParGuid(const Guid& g) { return ParGuid(CfgScenarios() + CFG_SCENARIOS, 0x08, g); }
uintptr_t ReglageActesParGuid(const Guid& g)
{
    return ParGuidDansObjet(CfgScenarios(), TAILLE_CFG_SCENARIOS, 0x08, g);
}

// Acte par GUID : parcourt les reglages d'actes de tous les scenarios.
uintptr_t ActeParGuid(const Guid& g, uintptr_t* reglage = nullptr)
{
    uintptr_t tab = 0; int n = 0;
    if (GuidNul(g) || !LireTableau(CfgScenarios() + CFG_SCENARIOS, tab, n)) return 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t scn = 0; Guid gr{};
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &scn, 8) || scn <= 0x10000) continue;
        if (!LireGuid(scn + SCN_ACTES, gr)) continue;
        uintptr_t reg = ReglageActesParGuid(gr);
        if (!reg) continue;
        uintptr_t acte = ParGuid(reg + REG_ACTES, 0x08, g);
        if (acte) { if (reglage) *reglage = reg; return acte; }
    }
    return 0;
}

// Indice d'un niveau de temperature dans la liste de la config (-1 sinon).
int IndiceNiveau(uintptr_t niv)
{
    uintptr_t tab = 0; int n = 0;
    if (!niv || !LireTableau(EnvConfig() + ENV_NIVEAUX, tab, n)) return -1;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) && e == niv) return i;
    }
    return -1;
}

void NomDe(uintptr_t entree, int offNom, char* out, int taille)
{
    if (!entree) { strcpy_s(out, taille, "(introuvable)"); return; }
    LireTexte(entree + offNom, out, taille);
}

// "Nom (decalage +n, TEMPETE)" d'une FrostpunkWeatherEntry.
void DecrireMeteo(uintptr_t m, char* out, int taille)
{
    if (!m) { strcpy_s(out, taille, "(meteo introuvable)"); return; }
    char nom[96];
    LireTexte(m, nom, sizeof(nom));
    int32_t dec = 0; uint8_t tempete = 0;
    SafeRead((void*)(m + MET_DECALAGE), &dec, 4);
    SafeRead((void*)(m + MET_TEMPETE), &tempete, 1);
    sprintf_s(out, taille, "%s (decalage %+d niveau(x)%s)", nom, dec, tempete ? ", TEMPETE" : "");
}

// Noms des chronologies d'une liste de liens.
void NomsChronos(uintptr_t adrLiens, char* out, int taille)
{
    out[0] = 0;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adrLiens, tab, n)) { strcpy_s(out, taille, "(illisible)"); return; }
    if (n == 0) { strcpy_s(out, taille, "(aucune)"); return; }
    for (int i = 0; i < n; ++i) {
        Guid g{}; char nom[96];
        if (!LireGuid(tab + (uintptr_t)i * 0x10, g)) continue;
        uintptr_t c = ChronoParGuid(g);
        if (c) LireTexte(c + CHR_NOM, nom, sizeof(nom));
        else sprintf_s(nom, "?%016llX", (unsigned long long)g.a);
        if (i > 0) Ajouter(out, taille, ", ");
        Ajouter(out, taille, nom);
    }
}

void DecrireChrono(uintptr_t c)
{
    char nom[96], cls[96] = "?";
    LireTexte(c + CHR_NOM, nom, sizeof(nom));
    GetClassNameFP(c, cls, sizeof(cls));
    printf("[TEMP] chronologie \"%s\" (%s)\n", nom, cls);
    if (strcmp(cls, "FrostpunkScenarioSimpleWeatherTimeline") == 0) {
        uintptr_t tab = 0; uint32_t n = 0;
        SafeRead((void*)(c + CHR_LISTE), &tab, 8);
        SafeRead((void*)(c + CHR_LISTE + 8), &n, 4);
        if (n > 256 || (n && tab <= 0x10000)) { printf("[TEMP]   changements illisibles\n"); return; }
        for (uint32_t i = 0; i < n; ++i) {
            uintptr_t e = tab + (uintptr_t)i * CHG_PAS;
            uint16_t jour = 0; uint8_t heure = 0, minute = 0; Guid g{};
            SafeRead((void*)e, &jour, 2);
            SafeRead((void*)(e + 2), &heure, 1);
            SafeRead((void*)(e + 3), &minute, 1);
            LireGuid(e + 4, g);
            char met[160];
            DecrireMeteo(MeteoParGuid(g), met, sizeof(met));
            printf("[TEMP]   jour %u %02u:%02u -> %s\n", jour, heure, minute, met);
        }
    } else if (strcmp(cls, "FrostpunkScenarioCompositeWeatherTimeline") == 0) {
        uintptr_t tab = 0; uint32_t n = 0; uint8_t boucle = 0;
        SafeRead((void*)(c + CHR_LISTE), &tab, 8);
        SafeRead((void*)(c + CHR_LISTE + 8), &n, 4);
        SafeRead((void*)(c + CHR_BOUCLE), &boucle, 1);
        if (n > 64 || (n && tab <= 0x10000)) { printf("[TEMP]   composantes illisibles\n"); return; }
        printf("[TEMP]   %u composante(s), boucle sur la derniere : %s\n", n, boucle ? "oui" : "non");
        for (uint32_t i = 0; i < n; ++i) {
            char liste[1024];
            NomsChronos(tab + (uintptr_t)i * CMP_PAS, liste, sizeof(liste));
            printf("[TEMP]   composante %u : %s\n", i, liste);
        }
    }
}

// --- 1) Config d'environnement ----------------------------------------------
void ReleverEnvironnement()
{
    uintptr_t env = EnvConfig();
    char cls[64] = "?";
    GetClassNameFP(env, cls, sizeof(cls));
    float defaut = 0, parNiveau = 0;
    SafeRead((void*)(env + ENV_CELSIUS_DEFAUT), &defaut, 4);
    SafeRead((void*)(env + ENV_CELSIUS_NIVEAU), &parNiveau, 4);
    Guid gd{};
    LireGuid(env + ENV_NIVEAU_DEFAUT, gd);
    uintptr_t nivDefaut = NiveauParGuid(gd);
    char nomDefaut[96];
    NomDe(nivDefaut, 0, nomDefaut, sizeof(nomDefaut));
    printf("[TEMP] ===== config d'environnement (%s) =====\n", cls);
    printf("[TEMP] degres par defaut %.1f, degres par niveau %.1f, niveau par defaut %s (indice %d)\n",
        defaut, parNiveau, nomDefaut, IndiceNiveau(nivDefaut));

    uintptr_t tab = 0; int n = 0;
    LireTableau(env + ENV_NIVEAUX, tab, n);
    printf("[TEMP] %d niveaux de temperature :\n", n);
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
        char nom[96], cle[128];
        LireTexte(e, nom, sizeof(nom));
        LireTexte(e + NIV_NOM_AFFICHE, cle, sizeof(cle));
        printf("[TEMP]   [%d] %s  %s\n", i, nom, cle);
    }

    LireTableau(env + ENV_METEOS, tab, n);
    printf("[TEMP] %d meteos :\n", n);
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000) continue;
        char met[160], cle[128];
        DecrireMeteo(e, met, sizeof(met));
        LireTexte(e + MET_NOM_AFFICHE, cle, sizeof(cle));
        printf("[TEMP]   %s  %s\n", met, cle);
    }
}

// --- 2) Toutes les chronologies ---------------------------------------------
void ReleverChronologies()
{
    uintptr_t tab = 0; int n = 0;
    LireTableau(CfgScenarios() + CFG_CHRONOS, tab, n);
    printf("[TEMP] ===== %d chronologies meteo =====\n", n);
    for (int i = 0; i < n; ++i) {
        uintptr_t c = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &c, 8) && c > 0x10000) DecrireChrono(c);
    }
}

// --- 3) Scenarios : chronologies et actes -----------------------------------
void ReleverScenarios()
{
    uintptr_t tab = 0; int n = 0;
    LireTableau(CfgScenarios() + CFG_SCENARIOS, tab, n);
    printf("[TEMP] ===== %d scenarios =====\n", n);
    for (int i = 0; i < n; ++i) {
        uintptr_t scn = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &scn, 8) || scn <= 0x10000) continue;
        char nom[96], chronos[1024];
        LireTexte(scn, nom, sizeof(nom));
        NomsChronos(scn + SCN_CHRONOS, chronos, sizeof(chronos));
        printf("[TEMP] scenario %s : chronologies %s\n", nom, chronos);

        Guid gr{};
        LireGuid(scn + SCN_ACTES, gr);
        if (GuidNul(gr)) { printf("[TEMP]   pas de reglage d'actes\n"); continue; }
        uintptr_t reg = ReglageActesParGuid(gr);
        if (!reg) { printf("[TEMP]   reglage d'actes %016llX introuvable\n", (unsigned long long)gr.a); continue; }
        char nomReg[96]; uint8_t premier = 0;
        LireTexte(reg, nomReg, sizeof(nomReg));
        SafeRead((void*)(reg + REG_PREMIER_ACTE), &premier, 1);
        uintptr_t actes = 0; int na = 0;
        LireTableau(reg + REG_ACTES, actes, na);
        printf("[TEMP]   actes \"%s\" (%d, 1er acte au depart : %s)\n", nomReg, na, premier ? "oui" : "non");
        for (int a = 0; a < na; ++a) {
            uintptr_t acte = 0;
            if (!SafeRead((void*)(actes + (uintptr_t)a * 8), &acte, 8) || acte <= 0x10000) continue;
            char nomActe[96], ch[1024];
            LireTexte(acte, nomActe, sizeof(nomActe));
            NomsChronos(acte + ACTE_CHRONOS, ch, sizeof(ch));
            printf("[TEMP]   acte %d \"%s\" : chronologies %s\n", a, nomActe, ch);
        }
    }
}

// --- 4) Difficultes ---------------------------------------------------------
void NomScenarioLien(uintptr_t adrLien, char* out, int taille)
{
    Guid g{};
    LireGuid(adrLien, g);
    if (GuidNul(g)) { strcpy_s(out, taille, "-"); return; }
    NomDe(ScenarioParGuid(g), 0, out, taille);
}

void ReleverDifficultes()
{
    uintptr_t sys = SystemeParId(IdSysteme::Difficulty);
    if (!sys || !ClasseExacte(sys, "FrostpunkDifficultySystem")) {
        printf("[TEMP] systeme des difficultes absent (hors partie ?)\n");
        return;
    }
    uintptr_t arr = 0; uint32_t cnt = 0;
    SafeRead((void*)(sys + 0x30), &arr, 8);
    SafeRead((void*)(sys + 0x38), &cnt, 4);
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) { printf("[TEMP] reglages de difficulte illisibles\n"); return; }
    printf("[TEMP] ===== %u reglages de difficulte =====\n", cnt);
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t def = 0, choisi = 0;
        SafeRead((void*)(arr + (uintptr_t)i * 0x10), &def, 8);
        SafeRead((void*)(arr + (uintptr_t)i * 0x10 + 8), &choisi, 8);
        if (def <= 0x10000) continue;
        char nomReg[96];
        LireTexte(def, nomReg, sizeof(nomReg));
        uintptr_t difs = 0; int nd = 0;
        LireTableau(def + 0x18, difs, nd);
        for (int d = 0; d < nd; ++d) {
            uintptr_t dif = 0;
            if (!SafeRead((void*)(difs + (uintptr_t)d * 8), &dif, 8) || dif <= 0x10000) continue;
            char nomDif[96]; uint8_t defaut = 0;
            LireTexte(dif, nomDif, sizeof(nomDif));
            SafeRead((void*)(dif + DIF_DEFAUT), &defaut, 1);
            uintptr_t mods = 0; int nm = 0;
            LireTableau(dif + DIF_MODIFS, mods, nm);
            int autres = 0;
            bool entete = false;
            for (int m = 0; m < nm; ++m) {
                uintptr_t mod = 0;
                if (!SafeRead((void*)(mods + (uintptr_t)m * 8), &mod, 8) || mod <= 0x10000) continue;
                char cls[96] = "?";
                GetClassNameFP(mod, cls, sizeof(cls));
                bool acte = strcmp(cls, "FrostpunkGameActDifficultyModification") == 0;
                bool scn  = strcmp(cls, "FrostpunkScenarioDifficultyModification") == 0;
                bool niv  = strcmp(cls, "FrostpunkTemperatureLevelDifficultyModification") == 0;
                if (!acte && !scn && !niv) { ++autres; continue; }
                if (!entete) {
                    entete = true;
                    printf("[TEMP] %s / %s%s%s :\n", nomReg, nomDif, defaut ? " (defaut)" : "",
                        dif == choisi ? " [CHOISIE]" : "");
                }
                char requis[96], interdit[96];
                NomScenarioLien(mod + MOD_SCN_REQUIS, requis, sizeof(requis));
                NomScenarioLien(mod + MOD_SCN_INTERDIT, interdit, sizeof(interdit));
                Guid g{};
                LireGuid(mod + MOD_CIBLE, g);
                char cible[96], remp[1024] = "";
                if (acte) NomDe(ActeParGuid(g), 0, cible, sizeof(cible));
                else if (scn) NomDe(ScenarioParGuid(g), 0, cible, sizeof(cible));
                else {
                    uintptr_t nv = NiveauParGuid(g);
                    NomDe(nv, 0, cible, sizeof(cible));
                    char ind[24];
                    sprintf_s(ind, " (indice %d)", IndiceNiveau(nv));
                    Ajouter(cible, sizeof(cible), ind);
                }
                if (!niv) NomsChronos(mod + MOD_REMPLACEMENT, remp, sizeof(remp));
                // TemperatureLevel = chance de maladie a ce niveau (+0x60), pas
                // une variation de temperature (releve du 2026-11-01, "Health").
                printf("[TEMP]   %s %s%s%s (scenario requis %s, interdit %s)\n",
                    acte ? "acte" : (scn ? "scenario" : "maladie au niveau"),
                    cible, niv ? "" : " -> chronologies ", remp, requis, interdit);
            }
            if (entete && autres) printf("[TEMP]   (+%d autres modifications sans rapport)\n", autres);
            if (!entete) printf("[TEMP] %s / %s%s : aucune modification meteo (%d autres)\n",
                nomReg, nomDif, dif == choisi ? " [CHOISIE]" : "", autres);
        }
    }
}

// --- 5) Evenements : changement d'acte, effets meteo ------------------------
uintptr_t g_dernierEvtDecrit = 0;
int       g_nEffetsVus = 0;

void VisiterEffet(uintptr_t obj, const char* cls, uintptr_t evt, const char* nomEvt, const char* nomRep)
{
    bool acte = strcmp(cls, "FrostpunkStartGameActEffect") == 0;
    bool meteo = strstr(cls, "Weather") || strstr(cls, "Temperature");
    if (!acte && !meteo) return;
    ++g_nEffetsVus;
    char detail[256] = "";
    if (acte) {
        Guid g{};
        LireGuid(obj + 0x18, g);
        uintptr_t reg = 0;
        uintptr_t a = ActeParGuid(g, &reg);
        char nomActe[96], nomReg[96] = "?";
        NomDe(a, 0, nomActe, sizeof(nomActe));
        if (reg) LireTexte(reg, nomReg, sizeof(nomReg));
        sprintf_s(detail, "acte \"%s\" (reglage %s)", nomActe, nomReg);
    } else {
        // Classe meteo inconnue : premiers qwords bruts.
        uint64_t q[6] = {};
        SafeRead((void*)(obj + 0x18), q, sizeof(q));
        sprintf_s(detail, "+0x18 : %016llX %016llX %016llX %016llX %016llX %016llX",
            (unsigned long long)q[0], (unsigned long long)q[1], (unsigned long long)q[2],
            (unsigned long long)q[3], (unsigned long long)q[4], (unsigned long long)q[5]);
    }
    printf("[TEMP] %s : %s <- evenement \"%s\" (repertoire %s)\n", cls, detail, nomEvt, nomRep);
    if (evt && evt != g_dernierEvtDecrit) {
        g_dernierEvtDecrit = evt;
        LoisDecrireEvenement(evt, "[TEMP]");
    }
}

void ReleverEvenements()
{
    printf("[TEMP] ===== evenements (changement d'acte, effets meteo) =====\n");
    g_dernierEvtDecrit = 0;
    g_nEffetsVus = 0;
    LoisParcourirEvenements(VisiterEffet);
    printf("[TEMP] %d effet(s) trouve(s)\n", g_nEffetsVus);
}

// --- 6) Partie en cours : chronologie calculee par le jeu --------------------
void HeuresDe(uint64_t date, char* out, int taille)
{
    int32_t secHeure = 0;
    SafeRead((void*)(Base() + OFF_SECONDES_HEURE), &secHeure, 4);
    double sec = (double)date / 2147483648.0;
    if (secHeure <= 0) { sprintf_s(out, taille, "%.0f s", sec); return; }
    double h = sec / secHeure;
    int jour = (int)(h / 24.0);
    double hj = h - jour * 24.0;
    sprintf_s(out, taille, "%.2f h (jour %d + %02d:%02d)", h, jour, (int)hj, (int)((hj - (int)hj) * 60));
}

void ReleverPartie()
{
    uintptr_t base = Base();
    uintptr_t ws = 0;
    SafeRead((void*)(base + OFF_SYSTEME_METEO), &ws, 8);
    if (ws <= 0x10000 || !ClasseExacte(ws, "FrostpunkWeatherSystem")) {
        printf("[TEMP] systeme meteo absent (0x%llX) : pas de partie ?\n", (unsigned long long)ws);
        return;
    }
    char scnNom[96] = "?";
    uintptr_t sysScn = SystemeParId(IdSysteme::Scenario), scn = 0;
    if (sysScn && SafeRead((void*)(sysScn + 0x68), &scn, 8) && scn > 0x10000) LireTexte(scn, scnNom, sizeof(scnNom));

    uintptr_t horloge = 0; uint64_t maintenant = 0;
    SafeRead((void*)(base + OFF_HORLOGE), &horloge, 8);
    if (horloge > 0x10000) SafeRead((void*)(horloge + 0xB0), &maintenant, 8);
    char tNow[96];
    HeuresDe(maintenant, tNow, sizeof(tNow));
    printf("[TEMP] ===== partie en cours : scenario %s, date %s =====\n", scnNom, tNow);

    uintptr_t cour = 0, prec = 0; int32_t index = 0;
    SafeRead((void*)(ws + SYS_METEO_COURANTE), &cour, 8);
    SafeRead((void*)(ws + SYS_METEO_PRECEDENTE), &prec, 8);
    SafeRead((void*)(ws + SYS_INDEX), &index, 4);
    char mc[160], mp[160];
    DecrireMeteo(cour, mc, sizeof(mc));
    DecrireMeteo(prec, mp, sizeof(mp));
    printf("[TEMP] meteo courante %s, precedente %s, index %d\n", mc, cour ? mp : "-", index);

    uintptr_t tab = 0; int n = 0;
    LireTableau(ws + SYS_CHRONOS_CHOISIES, tab, n);
    char liste[1024] = "";
    for (int i = 0; i < n; ++i) {
        uintptr_t c = 0; char nom[96];
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &c, 8) || c <= 0x10000) continue;
        LireTexte(c + CHR_NOM, nom, sizeof(nom));
        if (liste[0]) Ajouter(liste, sizeof(liste), ", ");
        Ajouter(liste, sizeof(liste), nom);
    }
    printf("[TEMP] chronologies retenues (%d) : %s\n", n, liste[0] ? liste : "(aucune)");

    LireTableau(ws + SYS_CHRONOLOGIE, tab, n);
    printf("[TEMP] chronologie calculee : %d changement(s)\n", n);
    for (int i = 0; i < n; ++i) {
        uint64_t date = 0; uintptr_t m = 0;
        SafeRead((void*)(tab + (uintptr_t)i * 0x10), &date, 8);
        SafeRead((void*)(tab + (uintptr_t)i * 0x10 + 8), &m, 8);
        char t[96], met[160];
        HeuresDe(date, t, sizeof(t));
        DecrireMeteo(m, met, sizeof(met));
        printf("[TEMP]   %s%s -> %s\n", i == index ? "> " : "  ", t, met);
    }
}

// --- 7) Montees de temperature Archipelago ------------------------------------
static const uintptr_t OFF_RECALCUL_TEMPERATURE = Adr(0x1812C10);
constexpr int SYS_TEMP_COURANTE    = 0x21A0;
constexpr int SYS_DATE_CHANGEMENT  = 0x160;    // systeme meteo : LastWeatherChangeTime
constexpr int MAX_RETIREES         = 256;

typedef uintptr_t (*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
Fonction4_t oRecalcul = nullptr;

// Une montee retiree : date du changement de meteo (date interne du jeu) et
// nombre de niveaux retires. Une sauvegarde plus ancienne ne compte que les
// montees dont la date est passee (le calendrier est deterministe : une
// montee rejouee retombe sur la meme date et n'est pas comptee deux fois).
struct Retiree { uint64_t date; int niveaux; };
Retiree   g_retirees[MAX_RETIREES];
int       g_nRetirees = 0;
int       g_recus = 0;                 // niveaux rendus par les items (cumulatif)
uint64_t  g_partie[2] = {};
bool      g_partieLue = false;
uintptr_t g_sysTemp = 0;
volatile LONG g_niveauxEnAttente = 0;  // items recus hors thread du jeu
bool      t_recalculForce = false;     // recalcul demande par le mod (pas un changement de meteo)
unsigned  g_generationVue = 0;
// Meteo VANILLA (2026-11-06, demande de l'auteur : "le temps change" a l'ecran
// lors d'une montee retiree). Une montee retiree n'est plus appliquee du tout
// au systeme meteo (hkPoserMeteo) : la meteo affichee reste la plus froide, et
// la meteo que le jeu voulait est gardee ici (et dans la sauvegarde locale,
// ligne "V"). La temperature = decalage de la meteo vanilla - montees
// retirees + niveaux recus (hkRecalcul).
uintptr_t g_meteoVanilla = 0;

uint64_t Maintenant()
{
    uintptr_t horloge = 0; uint64_t t = 0;
    SafeRead((void*)(Base() + OFF_HORLOGE), &horloge, 8);
    if (horloge > 0x10000) SafeRead((void*)(horloge + 0xB0), &t, 8);
    return t;
}

bool CheminSauvegarde(char* out, size_t taille)
{
    if (!g_partie[0] && !g_partie[1]) return false;
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_temperature_%016llX%016llX.txt", base,
        (unsigned long long)g_partie[1], (unsigned long long)g_partie[0]);
    return true;
}

// mods\ap_temperature_<GameId>.txt : "A <niveaux recus>", une ligne
// "M <date hex> <niveaux>" par montee retiree, et "V <guid a> <guid b>" =
// meteo vanilla (si elle differe de la meteo affichee). Reecrit en entier.
void Sauver()
{
    char chemin[MAX_PATH];
    if (!g_partieLue || !CheminSauvegarde(chemin, sizeof(chemin))) return;
    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "w") != 0 || !f) {
        printf("[TEMPAP] sauvegarde impossible : %s\n", chemin);
        return;
    }
    fprintf(f, "A %d\n", g_recus);
    for (int i = 0; i < g_nRetirees; ++i)
        fprintf(f, "M %llX %d\n", (unsigned long long)g_retirees[i].date, g_retirees[i].niveaux);
    Guid g{};
    if (g_meteoVanilla > 0x10000 && LireGuid(g_meteoVanilla + 0x08, g))
        fprintf(f, "V %llX %llX\n", (unsigned long long)g.a, (unsigned long long)g.b);
    fclose(f);
}

void Charger()
{
    g_nRetirees = 0;
    g_recus = 0;
    g_meteoVanilla = 0;
    char chemin[MAX_PATH];
    if (!CheminSauvegarde(chemin, sizeof(chemin))) return;
    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "r") != 0 || !f) return;
    char ligne[128];
    while (fgets(ligne, sizeof(ligne), f)) {
        unsigned long long d = 0, e = 0; int n = 0;
        if (ligne[0] == 'A') sscanf_s(ligne + 1, "%d", &g_recus);
        else if (ligne[0] == 'M' && g_nRetirees < MAX_RETIREES &&
                 sscanf_s(ligne + 1, "%llX %d", &d, &n) == 2)
            g_retirees[g_nRetirees++] = { (uint64_t)d, n };
        else if (ligne[0] == 'V' && sscanf_s(ligne + 1, "%llX %llX", &d, &e) == 2)
            g_meteoVanilla = MeteoParGuid(Guid{ (uint64_t)d, (uint64_t)e });
    }
    fclose(f);
}

// Relit la sauvegarde locale quand le GameId change.
bool AssurerPartie()
{
    uint64_t gid[2] = {};
    uintptr_t scn = SystemeParId(IdSysteme::Scenario);
    if (!scn || !SafeRead((void*)(scn + 0x110), gid, sizeof(gid)) || (!gid[0] && !gid[1])) return false;
    if (g_partieLue && gid[0] == g_partie[0] && gid[1] == g_partie[1]) return true;
    g_partie[0] = gid[0];
    g_partie[1] = gid[1];
    g_partieLue = true;
    Charger();
    printf("[TEMPAP] partie %016llX%016llX : %d montee(s) retiree(s) notee(s), %d niveau(x) recu(s)\n",
        (unsigned long long)gid[1], (unsigned long long)gid[0], g_nRetirees, g_recus);
    return true;
}

// Decalage a ajouter a la meteo : niveaux recus moins montees retirees deja
// passees.
int Decalage()
{
    uint64_t t = Maintenant();
    int retire = 0;
    for (int i = 0; i < g_nRetirees; ++i)
        if (g_retirees[i].date <= t) retire += g_retirees[i].niveaux;
    return g_recus - retire;
}

// Montees de fin de partie laissees au jeu (decision de l'auteur,
// 2026-11-01) : fin de la tempete de A New Home (meteo "-00") et J45 de
// Last Autumn (retour a "-20_NoFog" dans BUILDERS).
bool MonteeExclue(uintptr_t meteo)
{
    char nom[96];
    LireTexte(meteo, nom, sizeof(nom));
    if (strcmp(nom, "-00") == 0) return true;
    if (strcmp(nom, "-20_NoFog") == 0) {
        char scnNom[96] = "?";
        uintptr_t sysScn = SystemeParId(IdSysteme::Scenario), scn = 0;
        if (sysScn && SafeRead((void*)(sysScn + 0x68), &scn, 8) && scn > 0x10000) LireTexte(scn, scnNom, sizeof(scnNom));
        if (strcmp(scnNom, "BUILDERS") == 0) return true;
    }
    return false;
}

// Le changement de 'avant' a 'apres' est-il une montee retiree ?
bool MonteeRetiree(uintptr_t avant, uintptr_t apres)
{
    if (avant <= 0x10000 || apres <= 0x10000) return false;
    int32_t da = 0, dp = 0;
    if (!SafeRead((void*)(avant + MET_DECALAGE), &da, 4) || !SafeRead((void*)(apres + MET_DECALAGE), &dp, 4)) return false;
    return dp > da && !MonteeExclue(apres);
}

// --- Previsions de l'interface (decompiles du 2026-11-01) ---------------------
// FUN_141559730(panneau, i, &chronologie[i], &chronologie[i-1], p5) : icone de
// la frise meteo pour le changement i (WEATHER_CHANGE_ICON, case montee /
// baisse selon l'ecart des decalages, infobulle) ; ecart nul -> pas d'icone,
// renvoie 1 (la boucle de FUN_141559f10 / FUN_14155a620 / FUN_14155acf0
// continue). FUN_141bcc1a0(panneau, meteo a venir) : icone du panneau de
// temperature (TEMPERATURE_UP / TEMPERATURE_DOWN / STORM, ecart nul : rien),
// ecouteur 0x141bcb540. Le message "WeatherChangeApproaching" (0x1B) n'est
// programme (FUN_141680800) que vers une meteo plus froide : rien a faire.
static const uintptr_t OFF_ICONE_FRISE   = Adr(0x1559730);
static const uintptr_t OFF_ICONE_PANNEAU = Adr(0x1BCC1A0);
typedef uintptr_t (*Fonction5_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
Fonction5_t oIconeFrise = nullptr;
Fonction4_t oIconePanneau = nullptr;

uintptr_t hkIconeFrise(uintptr_t panneau, uintptr_t i, uintptr_t entree, uintptr_t precedente, uintptr_t p5)
{
    uintptr_t apres = 0, avant = 0;
    if (entree > 0x10000 && precedente > 0x10000 &&
        SafeRead((void*)(entree + 8), &apres, 8) && SafeRead((void*)(precedente + 8), &avant, 8) &&
        MonteeRetiree(avant, apres)) {
        static int nJournal = 0;
        if (nJournal < 20) {
            ++nJournal;
            char ma[160], mp[160];
            DecrireMeteo(avant, ma, sizeof(ma));
            DecrireMeteo(apres, mp, sizeof(mp));
            printf("[TEMPAP] frise : icone de montee masquee (changement %d : %s -> %s)\n", (int)i, ma, mp);
        }
        return 1;                    // comme un ecart nul : pas d'icone, la boucle continue
    }
    return oIconeFrise(panneau, i, entree, precedente, p5);
}

uintptr_t hkIconePanneau(uintptr_t panneau, uintptr_t aVenir, uintptr_t p3, uintptr_t p4)
{
    uintptr_t ws = 0, cour = 0;
    SafeRead((void*)(Base() + OFF_SYSTEME_METEO), &ws, 8);
    if (ws > 0x10000) SafeRead((void*)(ws + SYS_METEO_COURANTE), &cour, 8);
    if (MonteeRetiree(cour, aVenir)) {
        static int nJournal = 0;
        if (nJournal < 20) {
            ++nJournal;
            printf("[TEMPAP] panneau : TEMPERATURE_UP masque\n");
        }
        return oIconePanneau(panneau, cour, p3, p4);   // meteo identique : aucun etat joue
    }
    return oIconePanneau(panneau, aVenir, p3, p4);
}

// Changement de meteo VANILLA de 'prec' a 'cour' (hkPoserMeteo, 2026-11-06 ;
// avant : vu par le recalcul sur la meteo du systeme) : si c'est une montee,
// elle est retiree (une seule fois par date de changement).
void NoterChangement(uintptr_t prec, uintptr_t cour, uint64_t date)
{
    if (!cour || !prec) return;                          // meteo de depart
    int32_t dc = 0, dp = 0;
    SafeRead((void*)(cour + MET_DECALAGE), &dc, 4);
    SafeRead((void*)(prec + MET_DECALAGE), &dp, 4);
    char mc[160], mp[160];
    DecrireMeteo(cour, mc, sizeof(mc));
    DecrireMeteo(prec, mp, sizeof(mp));
    if (dc <= dp) {
        printf("[TEMPAP] changement de meteo %s -> %s : baisse ou palier, laisse au jeu\n", mp, mc);
        return;
    }
    if (MonteeExclue(cour)) {
        printf("[TEMPAP] changement de meteo %s -> %s : montee de fin de partie, laissee au jeu\n", mp, mc);
        return;
    }
    for (int i = 0; i < g_nRetirees; ++i)
        if (g_retirees[i].date == date) return;          // deja comptee (sauvegarde plus ancienne)
    if (g_nRetirees >= MAX_RETIREES) return;
    // Un check par NIVEAU retire (decision de l'auteur, 2026-11-03) : une
    // montee de N niveaux envoie les checks k+1..k+N, k = niveaux des
    // montees retirees plus anciennes de la partie. Un item rend 1 niveau :
    // tous les items recus = la temperature vanilla.
    int deja = 0;
    for (int i = 0; i < g_nRetirees; ++i)
        if (g_retirees[i].date < date) deja += g_retirees[i].niveaux;
    g_retirees[g_nRetirees++] = { date, dc - dp };
    Sauver();
    printf("[TEMPAP] changement de meteo %s -> %s : MONTEE de %d niveau(x) retiree (checks %d a %d)\n",
        mp, mc, dc - dp, deja + 1, deja + dc - dp);
    for (int k = deja + 1; k <= deja + dc - dp; ++k)
        SendTemperatureCheck(k);
}

// Hook de FUN_141812c10 : la meteo courante porte, le temps de l'appel, le
// decalage Archipelago en plus du sien ; le jeu calcule et diffuse lui-meme.
uintptr_t hkRecalcul(uintptr_t sys, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    g_sysTemp = sys;
    uintptr_t ws = 0, cour = 0;
    SafeRead((void*)(Base() + OFF_SYSTEME_METEO), &ws, 8);
    if (ws > 0x10000) SafeRead((void*)(ws + SYS_METEO_COURANTE), &cour, 8);
    if (!AssurerPartie() || !cour) return oRecalcul(sys, p2, p3, p4);
    // Les montees sont notees par hkPoserMeteo (meteo vanilla). Ici : la
    // meteo affichee peut etre plus froide que la vanilla (montee non
    // appliquee) -> l'ecart de leurs decalages est ajoute.
    if (!g_meteoVanilla) g_meteoVanilla = cour;

    int dec = Decalage();
    int32_t decVanilla = 0, decAffiche = 0;
    if (g_meteoVanilla != cour && SafeRead((void*)(g_meteoVanilla + MET_DECALAGE), &decVanilla, 4) &&
        SafeRead((void*)(cour + MET_DECALAGE), &decAffiche, 4))
        dec += decVanilla - decAffiche;
    int32_t avant = 0, origine = 0;
    SafeRead((void*)(sys + SYS_TEMP_COURANTE), &avant, 4);
    SafeRead((void*)(cour + MET_DECALAGE), &origine, 4);
    uintptr_t res;
    if (dec != 0) {
        int32_t modifie = origine + dec;
        SafeWrite((void*)(cour + MET_DECALAGE), &modifie, 4);
        res = oRecalcul(sys, p2, p3, p4);
        SafeWrite((void*)(cour + MET_DECALAGE), &origine, 4);
    } else {
        res = oRecalcul(sys, p2, p3, p4);
    }
    int32_t apres = 0;
    SafeRead((void*)(sys + SYS_TEMP_COURANTE), &apres, 4);
    printf("[TEMPAP] recalcul%s : meteo %+d, decalage AP %+d -> temperature %d -> %d (niveaux)\n",
        t_recalculForce ? " force" : "", origine, dec, avant, apres);
    return res;
}

uintptr_t SystemeTemperature()
{
    if (g_sysTemp && ClasseExacte(g_sysTemp, "FrostpunkTemperatureSystem")) return g_sysTemp;
    uintptr_t idBrut = FindSceneSystem("FrostpunkTemperatureSystem");
    uintptr_t sys = idBrut ? SystemeParId((int)(idBrut & 0xFFFFFFFF)) : 0;
    if (sys && ClasseExacte(sys, "FrostpunkTemperatureSystem")) return g_sysTemp = sys;
    return 0;
}

void RecalculForce(const char* raison)
{
    uintptr_t sys = SystemeTemperature();
    if (!sys || !oRecalcul) {
        printf("[TEMPAP] recalcul (%s) impossible : systeme de temperature introuvable\n", raison);
        return;
    }
    t_recalculForce = true;
    hkRecalcul(sys, 0, 0, 0);
    t_recalculForce = false;
}

// --- Pose de la meteo (decompile du 2026-11-06) --------------------------------
// FUN_14132f260(systeme meteo, meteo) : si differente de la courante (+0x150),
// retire les effets visuels (calques SFX) de l'ancienne, pose +0x158 =
// ancienne, +0x150 = nouvelle, +0x160 = date, applique les visuels de la
// nouvelle (FUN_14132b790, globaux de rendu) et diffuse "meteo changee"
// (ecouteurs +0x48 : dont le recalcul de la temperature).
// Une montee retiree n'est plus posee : on note la meteo vanilla, la montee
// (NoterChangement), et on force le recalcul ; l'ecran garde la meteo froide.
// Pendant un chargement (partie pas prete) : laisse au jeu (meteo restauree).
static const uintptr_t OFF_POSER_METEO = Adr(0x132F260);
Fonction4_t oPoserMeteo = nullptr;

uintptr_t hkPoserMeteo(uintptr_t ws, uintptr_t nouvelle, uintptr_t p3, uintptr_t p4)
{
    uintptr_t affichee = 0;
    SafeRead((void*)(ws + SYS_METEO_COURANTE), &affichee, 8);
    if (!nouvelle || nouvelle == affichee || !PartiePrete() || !AssurerPartie())
        return oPoserMeteo(ws, nouvelle, p3, p4);

    uintptr_t vanillaPrec = g_meteoVanilla ? g_meteoVanilla : affichee;
    g_meteoVanilla = nouvelle;
    NoterChangement(vanillaPrec, nouvelle, Maintenant());   // montee vanilla -> retiree
    Sauver();

    if (affichee && MonteeRetiree(affichee, nouvelle)) {
        char ma[160], mn[160];
        DecrireMeteo(affichee, ma, sizeof(ma));
        DecrireMeteo(nouvelle, mn, sizeof(mn));
        printf("[TEMPAP] meteo %s non posee (montee retiree) : l'ecran garde %s\n", mn, ma);
        RecalculForce("meteo vanilla changee");
        return 0;
    }
    return oPoserMeteo(ws, nouvelle, p3, p4);
}

} // namespace

void InstallHooksTemperature(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_RECALCUL_TEMPERATURE, (void*)&hkRecalcul,     (void**)&oRecalcul,     "recalcul de la temperature" },
        { OFF_ICONE_FRISE,          (void*)&hkIconeFrise,   (void**)&oIconeFrise,   "icone de la frise meteo" },
        { OFF_ICONE_PANNEAU,        (void*)&hkIconePanneau, (void**)&oIconePanneau, "icone du panneau de temperature" },
        { OFF_POSER_METEO,          (void*)&hkPoserMeteo,   (void**)&oPoserMeteo,   "pose de la meteo (visuels)" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[TEMPAP] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}

bool TemperatureNomActe(const uint64_t guid[2], char* out, int taille)
{
    Guid g{ guid[0], guid[1] };
    uintptr_t reg = 0;
    uintptr_t a = ActeParGuid(g, &reg);
    if (!a) { strcpy_s(out, taille, "?"); return false; }
    char nomActe[96], nomReg[96] = "?";
    LireTexte(a, nomActe, sizeof(nomActe));
    if (reg) LireTexte(reg, nomReg, sizeof(nomReg));
    sprintf_s(out, taille, "%s (reglage %s)", nomActe, nomReg);
    return true;
}

void TemperatureRecevoirMontee(int niveaux)
{
    InterlockedExchangeAdd(&g_niveauxEnAttente, niveaux);
}

int TemperatureItemsRecus(const uint64_t gid[2])
{
    if (!g_partieLue || gid[0] != g_partie[0] || gid[1] != g_partie[1]) return -1;
    return g_recus + (int)g_niveauxEnAttente;
}

void TemperatureDemanderReleve()
{
    g_releveDemande = true;
    printf("[TEMP] releve demande : il s'execute sur le thread du jeu (jeu hors pause)\n");
}

void TemperaturePompe()
{
    if (g_releveDemande) {
        g_releveDemande = false;
        ReleverEnvironnement();
        ReleverChronologies();
        ReleverScenarios();
        ReleverDifficultes();
        ReleverEvenements();
        ReleverPartie();
        printf("[TEMP] releve termine\n");
    }
    if (!PartiePrete() || !AssurerPartie()) return;

    // Partie (re)chargee : la temperature sauvegardee par le jeu peut dater
    // d'avant des items recus depuis (le fichier local est cumulatif).
    if (g_generationVue != PartieGeneration()) {
        g_generationVue = PartieGeneration();
        // Meteo vanilla de la partie chargee (la sauvegarde du jeu porte la
        // meteo AFFICHEE) : derniere entree de la chronologie calculee
        // (systeme meteo +0x118, {date, meteo} au pas de 0x10) deja passee.
        uintptr_t ws = 0, tab = 0;
        int n = 0;
        SafeRead((void*)(Base() + OFF_SYSTEME_METEO), &ws, 8);
        if (ws > 0x10000 && LireTableau(ws + SYS_CHRONOLOGIE, tab, n)) {
            uint64_t t = Maintenant();
            uintptr_t v = 0;
            for (int i = 0; i < n; ++i) {
                uint64_t d = 0; uintptr_t m = 0;
                if (!SafeRead((void*)(tab + (uintptr_t)i * 0x10), &d, 8) || !SafeRead((void*)(tab + (uintptr_t)i * 0x10 + 8), &m, 8)) break;
                if (d > t) break;
                v = m;
            }
            if (v > 0x10000) {
                g_meteoVanilla = v;
                char mv[160];
                DecrireMeteo(v, mv, sizeof(mv));
                printf("[TEMPAP] partie chargee : meteo vanilla %s (chronologie)\n", mv);
            }
        }
        RecalculForce("partie chargee");
    }

    LONG n = InterlockedExchange(&g_niveauxEnAttente, 0);
    if (n != 0) {
        g_recus += (int)n;
        Sauver();
        printf("[TEMPAP] %+ld niveau(x) recu(s), total %d\n", (long)n, g_recus);
        RecalculForce("item recu");
    }
}
