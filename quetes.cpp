// ---- quetes.cpp ----
// Releve des quetes (F9). Decompiles du 2026-11-01 (adresses Ghidra) :
//   FrostpunkQuestsConfig = base + 0x407AE80 (FUN_14148d280) : +0x58
//     HierarchicalEntryCollection (FUN_14148c7f0) dont la collection des
//     repertoires est a +0x58 + 0x20 : tableau de pointeurs en +0x98, compte +0xA0
//   EntryDirectory<FrostpunkQuestEntry> (0x30, FUN_14149fa10) : +0x00 nom,
//     +0x08 GUID, +0x18 Entries (pointeurs de quetes)
//   FrostpunkQuestEntry (0xF0, FUN_1414a5930) : +0x00 nom, +0x08 GUID,
//     +0x18 Timeout, +0x20 Complete on timeout, +0x24 / +0x28 occurrences,
//     +0x30 Objectives (FrostpunkQuestObjectiveEntry polymorphes), +0x48
//     Initial effects, +0x60 Success effects, +0x78 Failure effects, +0x90
//     Social problem, +0xA0 Displayed name, +0xA8 Description, +0xB0 Time
//     left text, +0xD8 Quest type (eQuestType)
//   FrostpunkQuestObjectiveEntry (0x88, polymorphe, FUN_1414a61e0) : +0x18
//     Conditions (BooleanExpression), +0x30 Update conditions after start,
//     +0x31 Hide progress, +0x32 Display remaining progress, +0x33 Mandatory,
//     +0x38 icone, +0x50 texte, +0x58 Description, +0x60 GUID, +0x70
//     Highlighted entities ; champs propres a la sous-classe au-dela de 0x88
//   Effets a lien (StartQuestEffect, FinishQuestEffect, StartGameActEffect,
//     ...) : lien presume en +0x18 (GUID de la quete / de l'acte).
//   Scenarios : FrostpunkScenariosConfig (base + 0x2C34490) +0x88 scenarios,
//     scenario +0xE8 "Events setups" (liens), config +0xC0 reglages
//     d'evenements (+0x18 liens vers les repertoires d'evenements, rangés a
//     base + 0x406DDA8 : +0x00 nom, +0x08 GUID).
#include "quetes.h"
#include "mod_common.h"
#include "lois.h"          // LoisParcourirEvenements, LoisParcourirEffets, LoisDecrireEvenement
#include "temperature.h"   // TemperatureNomActe
#include "evenements.h"    // g_nomEvtEnCours
#include "archipelago.h"   // SendQuestCheck
#include "scooting.h"      // ScootingReleverSites
#include "sites_contenu.h" // SitesContenuReleve
#include "recettes.h"      // RecettesReleve
#include "known_quests.h"  // KNOWN_QUESTS (genere par outils\quetes_excel.py)
#include "MinHook.h"
#include <cstring>

namespace {

constexpr uintptr_t OFF_QUESTS_CONFIG    = 0x407AE80;
constexpr uintptr_t OFF_SCENARIOS_CONFIG = 0x2C34490;
constexpr uintptr_t OFF_REPERTOIRES_EVT  = 0x406DDA8;
constexpr int CFG_REPERTOIRES = 0x98;
constexpr int REP_ENTREES     = 0x18;
constexpr int CFG_SCENARIOS   = 0x88;
constexpr int CFG_REGLAGES_EVT= 0xC0;
constexpr int SCN_EVENEMENTS  = 0xE8;
constexpr int REGLAGE_LIENS   = 0x18;

constexpr int QUE_OBJECTIFS   = 0x30;
constexpr int QUE_INITIAUX    = 0x48;
constexpr int QUE_SUCCES      = 0x60;
constexpr int QUE_ECHEC       = 0x78;
constexpr int QUE_NOM_AFFICHE = 0xA0;
constexpr int QUE_DESCRIPTION = 0xA8;
constexpr int QUE_TYPE        = 0xD8;

constexpr int OBJ_CONDITIONS  = 0x18;
constexpr int OBJ_OBLIGATOIRE = 0x33;
constexpr int OBJ_TEXTE       = 0x50;
constexpr int OBJ_DESCRIPTION = 0x58;
constexpr int OBJ_TAILLE_BASE = 0x88;

constexpr int MAX_ELEMENTS = 4096;
constexpr int MAX_QUETES   = 1024;
constexpr int MAX_REPS     = 128;
constexpr int MAX_LIENS    = 4096;

volatile bool g_releveDemande = false;

struct Guid { uint64_t a, b; };
bool LireGuid(uintptr_t adr, Guid& g) { return SafeRead((void*)adr, &g, sizeof(g)); }
bool GuidEgal(const Guid& x, const Guid& y) { return x.a == y.a && x.b == y.b; }

uintptr_t Base() { return (uintptr_t)GetModuleHandleW(NULL); }

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

// Texte 8 bits pointe par le qword en 'adr' ; false si ce n'est pas un texte.
bool LireTexte(uintptr_t adr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adr, &p, 8) || p <= 0x10000) { strcpy_s(out, taille, "-"); return false; }
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        if (c < 0x20 || c >= 0x7F) { strcpy_s(out, taille, "?"); return false; }
        out[i] = c;
    }
    out[i] = 0;
    if (i < 2) { strcpy_s(out, taille, "?"); return false; }
    return true;
}

typedef uintptr_t (*Fiche_t)(uintptr_t);
uintptr_t AppelerFiche(Fiche_t fn, uintptr_t obj)
{
    __try { return fn(obj); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// Taille d'un objet de reflexion : vtable [3] = fiche, fiche +0x88 = taille.
int TailleObjet(uintptr_t obj)
{
    uintptr_t base = Base(), vt = 0, fn = 0, fiche = 0;
    uint64_t taille = 0;
    if (!SafeRead((void*)obj, &vt, 8) || vt < base || vt > base + 0x5000000) return 0;
    if (!SafeRead((void*)(vt + 0x18), &fn, 8) || fn < base || fn > base + 0x2000000) return 0;
    fiche = AppelerFiche((Fiche_t)fn, obj);
    if (fiche <= 0x10000 || !SafeRead((void*)(fiche + 0x88), &taille, 8)) return 0;
    if (taille < 0x10 || taille > 0x1000) return 0;
    return (int)taille;
}

const char* SansPrefixe(const char* cls) { return strncmp(cls, "Frostpunk", 9) == 0 ? cls + 9 : cls; }

// --- Traduction d'une cle (export Excel, demande de l'auteur du 2026-11-01) ---
// FUN_140feb760(gestionnaire *(base + 0x2A28D80), &cle, &GUID nul
// DAT_142a25a30 (langue courante), sortie 0, repli 1, avertir 0) -> objet
// texte dont le qword +0 pointe sur les caracteres UTF-16 (meme appel que
// FUN_14116b850). Thread du jeu. 'adrCle' = adresse du qword qui porte la cle.
typedef uintptr_t (*TraduireCle_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint8_t, uint8_t);
uintptr_t AppelerTraduire(TraduireCle_t fn, uintptr_t mgr, uintptr_t adrCle, uintptr_t langue)
{
    __try { return fn(mgr, adrCle, langue, 0, 1, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// Texte traduit en UTF-8 sur une ligne : tabulations et retours en " / ",
// tirets et apostrophes typographiques en ASCII, autres caracteres non ASCII
// en \uXXXX (decodes par le script Excel ; pas de non-ASCII dans printf).
void Traduire(uintptr_t adrCle, char* out, int taille)
{
    strcpy_s(out, taille, "-");
    uintptr_t cle = 0, mgr = 0;
    if (!SafeRead((void*)adrCle, &cle, 8) || cle <= 0x10000) return;
    if (!SafeRead((void*)(Base() + 0x2A28D80), &mgr, 8) || mgr <= 0x10000) { strcpy_s(out, taille, "(gestionnaire ?)"); return; }
    // Les cles des donnees commencent par '@' : l'appelant du jeu le saute
    // (FUN_141ce5a40) avant de traduire. 1er essai du 2026-11-01 avec le '@' :
    // tout "(non traduit)".
    char premier = 0;
    SafeRead((void*)cle, &premier, 1);
    uintptr_t sansArobase = premier == '@' ? cle + 1 : cle;
    uintptr_t res = AppelerTraduire((TraduireCle_t)(Base() + 0xFEB760), mgr, (uintptr_t)&sansArobase, Base() + 0x2A25A30);
    uintptr_t chars = 0;
    if (res <= 0x10000 || !SafeRead((void*)res, &chars, 8) || chars <= 0x10000) { strcpy_s(out, taille, "(non traduit)"); return; }
    int o = 0;
    for (int i = 0; i < 2048 && o < taille - 8; ++i) {
        wchar_t ch = 0;
        if (!SafeRead((void*)(chars + i * 2), &ch, 2) || ch == 0) break;
        if (ch == 0x2013 || ch == 0x2014) ch = L'-';
        else if (ch == 0x2018 || ch == 0x2019) ch = L'\'';
        else if (ch == 0x201C || ch == 0x201D) ch = L'"';
        if (ch == L'\t' || ch == L'\n') { if (o + 3 < taille) { out[o++] = ' '; out[o++] = '/'; out[o++] = ' '; } continue; }
        if (ch == L'\r') continue;
        if (ch >= 0x20 && ch < 0x7F) out[o++] = (char)ch;
        else o += sprintf_s(out + o, taille - o, "\\u%04X", (unsigned)ch);
    }
    out[o] = 0;
    if (o == 0) strcpy_s(out, taille, "(vide)");
}

// Classes RTTI d'un tableau de pointeurs d'objets polymorphes.
void ClassesDuTableau(uintptr_t adr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t tab = 0; int n = 0;
    if (!LireTableau(adr, tab, n) || n == 0) { strcpy_s(out, taille, "(aucun)"); return; }
    for (int i = 0; i < n && i < 32; ++i) {
        uintptr_t o = 0; char cls[96] = "?";
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &o, 8) && o > 0x10000) GetClassNameFP(o, cls, sizeof(cls));
        if (i > 0) strncat_s(out, taille, ", ", _TRUNCATE);
        strncat_s(out, taille, SansPrefixe(cls), _TRUNCATE);
    }
}

// Champs propres a une sous-classe d'objectif : chaque dword de 0x88 a la fin,
// en clair s'il s'agit d'un texte, sinon en entier.
void ChampsPropres(uintptr_t obj, char* out, int taille)
{
    out[0] = 0;
    int t = TailleObjet(obj);
    if (t <= OBJ_TAILLE_BASE) { strcpy_s(out, taille, t ? "(aucun)" : "(taille inconnue)"); return; }
    for (int off = OBJ_TAILLE_BASE; off + 4 <= t && off < 0x140; off += 4) {
        char texte[96], item[128];
        if ((off & 7) == 0 && off + 8 <= t && LireTexte(obj + off, texte, sizeof(texte))) {
            sprintf_s(item, "+%X \"%s\" ", off, texte);
            off += 4;
        } else {
            int32_t v = 0;
            SafeRead((void*)(obj + off), &v, 4);
            if (v == 0) continue;
            sprintf_s(item, "+%X=%d ", off, v);
        }
        strncat_s(out, taille, item, _TRUNCATE);
    }
    if (!out[0]) strcpy_s(out, taille, "(tout a zero)");
}

// --- Recensement des quetes ---------------------------------------------------
struct Quete {
    uintptr_t adr;
    Guid      guid;
    char      nom[128];
    char      rep[48];
    int       type;
};
Quete g_quetes[MAX_QUETES];
int   g_nQuetes = 0;

int IndiceQuete(const Guid& g)
{
    for (int i = 0; i < g_nQuetes; ++i) if (GuidEgal(g_quetes[i].guid, g)) return i;
    return -1;
}

void RecenserQuetes()
{
    g_nQuetes = 0;
    uintptr_t reps = 0; int nr = 0;
    if (!LireTableau(Base() + OFF_QUESTS_CONFIG + CFG_REPERTOIRES, reps, nr)) return;
    for (int r = 0; r < nr; ++r) {
        uintptr_t rep = 0, ent = 0; int ne = 0;
        if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000) continue;
        char nomRep[48];
        LireTexte(rep, nomRep, sizeof(nomRep));
        if (!LireTableau(rep + REP_ENTREES, ent, ne)) continue;
        for (int e = 0; e < ne && g_nQuetes < MAX_QUETES; ++e) {
            uintptr_t q = 0;
            if (!SafeRead((void*)(ent + (uintptr_t)e * 8), &q, 8) || q <= 0x10000) continue;
            Quete& x = g_quetes[g_nQuetes++];
            x.adr = q;
            LireGuid(q + 8, x.guid);
            LireTexte(q, x.nom, sizeof(x.nom));
            strcpy_s(x.rep, nomRep);
            x.type = 0;
            SafeRead((void*)(q + QUE_TYPE), &x.type, 4);
        }
    }
}

// --- Liens : qui lance quoi ---------------------------------------------------
// source : evenement (repertoire d'evenements + nom) ou quete (indice, moment)
// cible  : quete (indice) ou acte (nom) ; genre : 'S' lance, 'F' termine, 'A' acte
struct Lien {
    char genre;
    int  cible;            // indice de quete, -1 pour un acte
    char acte[96];
    int  srcQuete;         // -1 si la source est un evenement
    char srcQuand[12];     // "initiaux" / "succes" / "echec"
    char srcRep[64];       // repertoire d'evenements
    char srcEvt[96];       // nom de l'evenement
    uintptr_t srcEvtAdr;   // FrostpunkGameEventEntry (conditions de scenario)
};
Lien g_liens[MAX_LIENS];
int  g_nLiens = 0;
int  g_queteCourante = -1;
const char* g_quandCourant = "";
uintptr_t g_dernierEvt = 0;

void AjouterLien(char genre, int cible, const char* acte, uintptr_t evt, const char* nomEvt, const char* nomRep)
{
    if (g_nLiens >= MAX_LIENS) return;
    Lien& l = g_liens[g_nLiens];
    l.genre = genre;
    l.cible = cible;
    strcpy_s(l.acte, acte ? acte : "");
    l.srcEvtAdr = evt;
    if (evt) {
        l.srcQuete = -1;
        l.srcQuand[0] = 0;
        strncpy_s(l.srcRep, nomRep ? nomRep : "?", _TRUNCATE);
        strncpy_s(l.srcEvt, nomEvt ? nomEvt : "?", _TRUNCATE);
    } else {
        l.srcQuete = g_queteCourante;
        strcpy_s(l.srcQuand, g_quandCourant);
        l.srcRep[0] = 0;
        l.srcEvt[0] = 0;
    }
    // Doublons (un meme effet atteint par plusieurs chemins) : ignores.
    for (int i = 0; i < g_nLiens; ++i) {
        const Lien& o = g_liens[i];
        if (o.genre == l.genre && o.cible == l.cible && strcmp(o.acte, l.acte) == 0 &&
            o.srcQuete == l.srcQuete && strcmp(o.srcQuand, l.srcQuand) == 0 &&
            strcmp(o.srcRep, l.srcRep) == 0 && strcmp(o.srcEvt, l.srcEvt) == 0) return;
    }
    ++g_nLiens;
}

// Effet atteint par le parcours d'un evenement (evt != 0) ou des effets d'une
// quete (evt == 0, quete g_queteCourante, moment g_quandCourant).
// Loi dont les effets sont en cours de parcours (source "Lois") : trace du
// 2026-11-01, "[Sociotech Related] Burials" lancee par les effets de la loi
// Cemetery (FUN_1416fdbc0).
const char* g_loiCourante = nullptr;
uintptr_t   g_adrLoiCourante = 0;
// Dilemme dont les effets sont en cours de parcours (source "Dilemmes/<rep>") :
// 005 LONDONERS est lancee par les effets du dilemme "Londoners appear",
// cree par une sequence et non par un evenement (trace du 2026-11-01).
const char* g_dilCourant = nullptr;
const char* g_repDilCourant = nullptr;
uintptr_t   g_adrDilCourant = 0;

// Effets qui revelent des sites ou des entites de la carte (2026-11-03, pour
// rattacher les sites aux regions de l'apworld) : FrostpunkRevealWorldMap
// EntityEffect (sans champ : entite du contexte), FrostpunkAddSiteReveals
// Effect (+0x18 GUID de gabarits de sites, pas 0x10, compte +0x20),
// FrostpunkCreateWorldMapEntityEffect. Ligne "[SREV]\tE\t<classe>\t<rep>\t
// <source>\t<moment>\t<cibles>", puis les declencheurs de l'evenement.
uintptr_t g_dernierEvtRevele = 0;

void NoterEffetRevelation(uintptr_t obj, const char* cls, uintptr_t evt, const char* nomEvt, const char* nomRep)
{
    const char* moment = "";
    if (g_loiCourante)            { nomEvt = g_loiCourante; nomRep = "Lois"; }
    else if (g_dilCourant)        { nomEvt = g_dilCourant; nomRep = g_repDilCourant; }
    else if (g_queteCourante >= 0) moment = g_quandCourant;
    char cibles[1024] = "";
    if (strstr(cls, "AddSiteRevealsEffect")) {
        uintptr_t tab = 0; int n = 0;
        if (LireTableau(obj + 0x18, tab, n) && n <= 64)
            for (int k = 0; k < n; ++k) {
                uint8_t g[16] = {};
                char nom[128];
                SafeRead((void*)(tab + (uintptr_t)k * 0x10), g, 16);
                ScootingNomGabaritParGuid(g, nom, sizeof(nom));
                if (cibles[0]) strncat_s(cibles, ", ", _TRUNCATE);
                strncat_s(cibles, nom, _TRUNCATE);
            }
    }
    printf("[SREV]\tE\t%s\t%s\t%s\t%s\t%s\n", cls, nomRep ? nomRep : "?", nomEvt ? nomEvt : "?", moment, cibles);
    if (evt && !g_loiCourante && !g_dilCourant && evt != g_dernierEvtRevele) {
        g_dernierEvtRevele = evt;
        printf("[SREV] declencheurs de l'evenement \"%s\" (repertoire %s) :\n", nomEvt, nomRep);
        LoisDecrireEvenement(evt, "[SREV]");
    }
}

// Cible des revelations (2026-11-03) : RevealWorldMapEntityEffect revele
// l'entite du CONTEXTE, posee par un effet parent a selecteur (ex.
// FrostpunkSelectEffect : +0x18 Selector, +0x50 Effects). Selecteurs
// (frostpunkgamecontextselectorsubclasses.h) : FrostpunkExpeditionSites
// Selector (FUN_1415f8ed0) +0x18 GUID du gabarit de site ; FrostpunkEntities
// ByNameSelector (FUN_1415f8ac0) +0x18 "Entity name" ; FrostpunkEntitiesWith
// TagSelector +0x18 ; FrostpunkUnionSelector (composants). Ligne
// "[SREV]\tC\t<rep>\t<source>\t<moment>\t<classe de l'effet>\t<selecteur>".
bool ClasseFrostpunk(uintptr_t p, char* cls, int taille)
{
    cls[0] = 0;
    return p > 0x10000 && GetClassNameFP(p, cls, taille) && strncmp(cls, "Frostpunk", 9) == 0;
}

bool ContientRevelation(uintptr_t eff, int prof)
{
    if (prof > 4) return false;
    for (int off = 0x18; off <= 0x78; off += 8) {
        uintptr_t tab = 0; uint32_t n = 0;
        if (!SafeRead((void*)(eff + off), &tab, 8) || !SafeRead((void*)(eff + off + 8), &n, 4)) continue;
        if (tab <= 0x10000 || n == 0 || n > 64) continue;
        for (uint32_t i = 0; i < n; ++i) {
            uintptr_t p = 0;
            char c[96];
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &p, 8) || !ClasseFrostpunk(p, c, sizeof(c))) continue;
            if (strstr(c, "RevealWorldMapEntityEffect") || strstr(c, "AddSiteRevealsEffect")) return true;
            if (ContientRevelation(p, prof + 1)) return true;
        }
    }
    return false;
}

// Texte en 'adr' : 8 bits, sinon UTF-16 (ASCII seulement).
void TexteSelecteur(uintptr_t adr, char* out, int taille)
{
    if (LireTexte(adr, out, taille) && out[0]) return;
    uintptr_t p = 0;
    wchar_t w[128] = {};
    out[0] = 0;
    if (!SafeRead((void*)adr, &p, 8) || p <= 0x10000 || !SafeRead((void*)p, w, sizeof(w) - 2)) return;
    int k = 0;
    for (; k < taille - 1 && k < 127 && w[k] >= 0x20 && w[k] < 0x7F; ++k) out[k] = (char)w[k];
    out[k] = 0;
}

void DecrireSelecteur(uintptr_t sel, char* out, int taille, int prof)
{
    char cls[96], t[160] = "";
    if (prof > 3 || !ClasseFrostpunk(sel, cls, sizeof(cls))) return;
    if (strstr(cls, "ExpeditionSitesSelector")) {
        uint8_t g[16] = {};
        SafeRead((void*)(sel + 0x18), g, 16);
        ScootingNomGabaritParGuid(g, t, sizeof(t));
    } else if (strstr(cls, "ByNameSelector") || strstr(cls, "WithTagSelector")) {
        TexteSelecteur(sel + 0x18, t, sizeof(t));
    }
    // Nom d'entite -> site(s) de la partie courante (emplacement de tirage).
    char sites[400] = "";
    if (strstr(cls, "ByNameSelector")) {
        char s[380];
        ScootingSitesDuNom(sel + 0x18, s, sizeof(s));
        sprintf_s(sites, " -> %s", s);
    }
    char item[800];
    sprintf_s(item, "%s%s%s%s%s", out[0] ? " + " : "", cls + 9, t[0] ? "=" : "", t, sites);
    strncat_s(out, taille, item, _TRUNCATE);
    // Selecteurs imbriques (union...) : pointeurs directs et tableaux.
    for (int off = 0x18; off <= 0x60; off += 8) {
        uintptr_t p = 0;
        char c[96];
        if (!SafeRead((void*)(sel + off), &p, 8)) continue;
        if (ClasseFrostpunk(p, c, sizeof(c)) && strstr(c, "Selector")) { DecrireSelecteur(p, out, taille, prof + 1); continue; }
        uint32_t n = 0;
        if (p <= 0x10000 || !SafeRead((void*)(sel + off + 8), &n, 4) || n == 0 || n > 16) continue;
        for (uint32_t i = 0; i < n; ++i) {
            // Elements : pointeurs, ou composants embarques (pointeur en +0x00
            // ou +0x08 de chaque element de 0x10..0x20 : on essaie le 1er qword).
            uintptr_t q = 0;
            if (SafeRead((void*)(p + (uintptr_t)i * 8), &q, 8) && ClasseFrostpunk(q, c, sizeof(c)) && strstr(c, "Selector"))
                DecrireSelecteur(q, out, taille, prof + 1);
        }
    }
}

void NoterCibleRevelation(uintptr_t obj, const char* cls, const char* nomEvt, const char* nomRep)
{
    uintptr_t sel = 0;
    char c[96];
    for (int off = 0x18; off <= 0x78 && !sel; off += 8) {
        uintptr_t p = 0;
        if (SafeRead((void*)(obj + off), &p, 8) && ClasseFrostpunk(p, c, sizeof(c)) && strstr(c, "Selector")) sel = p;
    }
    if (!sel || !ContientRevelation(obj, 0)) return;
    const char* moment = "";
    if (g_loiCourante)             { nomEvt = g_loiCourante; nomRep = "Lois"; }
    else if (g_dilCourant)         { nomEvt = g_dilCourant; nomRep = g_repDilCourant; }
    else if (g_queteCourante >= 0) moment = g_quandCourant;
    static char desc[1024];
    desc[0] = 0;
    DecrireSelecteur(sel, desc, sizeof(desc), 0);
    printf("[SREV]\tC\t%s\t%s\t%s\t%s\t%s\n", nomRep ? nomRep : "?", nomEvt ? nomEvt : "?", moment, cls, desc);
}

void VisiterEffet(uintptr_t obj, const char* cls, uintptr_t evt, const char* nomEvt, const char* nomRep)
{
    if (strstr(cls, "RevealWorldMapEntityEffect") || strstr(cls, "AddSiteRevealsEffect") ||
        strstr(cls, "CreateWorldMapEntityEffect"))
        NoterEffetRevelation(obj, cls, evt, nomEvt, nomRep);
    else if (strstr(cls, "Effect"))
        NoterCibleRevelation(obj, cls, nomEvt, nomRep);
    bool acte = strstr(cls, "StartGameActEffect") != nullptr;
    bool lance = strstr(cls, "StartQuestEffect") != nullptr;
    bool termine = strstr(cls, "FinishQuestEffect") != nullptr;
    if (!acte && !lance && !termine) return;
    uintptr_t src = evt;
    if (g_loiCourante) { src = g_adrLoiCourante; nomEvt = g_loiCourante; nomRep = "Lois"; }
    if (g_dilCourant)  { src = g_adrDilCourant; nomEvt = g_dilCourant; nomRep = g_repDilCourant; }
    Guid g{};
    LireGuid(obj + 0x18, g);
    if (acte) {
        uint64_t gg[2] = { g.a, g.b };
        char nomActe[96];
        TemperatureNomActe(gg, nomActe, sizeof(nomActe));
        AjouterLien('A', -1, nomActe, src, nomEvt, nomRep);
    } else {
        int i = IndiceQuete(g);
        if (i < 0) return;
        AjouterLien(lance ? 'S' : 'F', i, nullptr, src, nomEvt, nomRep);
    }
    if (evt && !g_loiCourante && !g_dilCourant && evt != g_dernierEvt) {
        g_dernierEvt = evt;
        printf("[QUETE] declencheurs de l'evenement \"%s\" (repertoire %s) :\n", nomEvt, nomRep);
        LoisDecrireEvenement(evt, "[QUETE]");
    }
}

void DescriptionSource(const Lien& l, char* out, int taille)
{
    if (l.srcQuete >= 0) sprintf_s(out, taille, "%s de \"%s\"", l.srcQuand, g_quetes[l.srcQuete].nom);
    else                 sprintf_s(out, taille, "evenement \"%s\" (%s)", l.srcEvt, l.srcRep);
}

// --- Description d'une quete ---------------------------------------------------
void DecrireQuete(int iq)
{
    uintptr_t q = g_quetes[iq].adr;
    char affiche[160], desc[160], ini[512], suc[512], ech[512];
    LireTexte(q + QUE_NOM_AFFICHE, affiche, sizeof(affiche));
    LireTexte(q + QUE_DESCRIPTION, desc, sizeof(desc));
    ClassesDuTableau(q + QUE_INITIAUX, ini, sizeof(ini));
    ClassesDuTableau(q + QUE_SUCCES, suc, sizeof(suc));
    ClassesDuTableau(q + QUE_ECHEC, ech, sizeof(ech));
    printf("[QUETE] quete \"%s\" (repertoire %s, type %d)\n", g_quetes[iq].nom, g_quetes[iq].rep, g_quetes[iq].type);
    printf("[QUETE]   nom %s ; description %s\n", affiche, desc);
    printf("[QUETE]   effets : initiaux [%s] ; succes [%s] ; echec [%s]\n", ini, suc, ech);
    {
        char trNom[1024], trDesc[2048];
        Traduire(q + QUE_NOM_AFFICHE, trNom, sizeof(trNom));
        Traduire(q + QUE_DESCRIPTION, trDesc, sizeof(trDesc));
        printf("[QXLS]\tQ\t%s\t%s\t%d\t%016llX%016llX\t%s\t%s\n", g_quetes[iq].nom, g_quetes[iq].rep,
            g_quetes[iq].type, (unsigned long long)g_quetes[iq].guid.a, (unsigned long long)g_quetes[iq].guid.b,
            trNom, trDesc);
    }
    uintptr_t tab = 0; int n = 0;
    LireTableau(q + QUE_OBJECTIFS, tab, n);
    for (int i = 0; i < n; ++i) {
        uintptr_t o = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &o, 8) || o <= 0x10000) continue;
        char cls[96] = "?", texte[192], d[160], cond[512], champs[1024];
        GetClassNameFP(o, cls, sizeof(cls));
        LireTexte(o + OBJ_TEXTE, texte, sizeof(texte));
        LireTexte(o + OBJ_DESCRIPTION, d, sizeof(d));
        ClassesDuTableau(o + OBJ_CONDITIONS, cond, sizeof(cond));
        ChampsPropres(o, champs, sizeof(champs));
        uint8_t obligatoire = 0;
        SafeRead((void*)(o + OBJ_OBLIGATOIRE), &obligatoire, 1);
        printf("[QUETE]   objectif %d %s%s : %s ; %s\n", i, SansPrefixe(cls),
            obligatoire ? " (obligatoire)" : "", texte, d);
        printf("[QUETE]     conditions [%s] ; champs %s\n", cond, champs);
        // Export Excel : nom interne = segment de la cle apres "/Objectives/".
        char nomObj[96] = "-", trTexte[1024], trDesc[1024];
        const char* p = strstr(texte, "/Objectives/");
        if (p) {
            strncpy_s(nomObj, p + 12, _TRUNCATE);
            char* fin = strchr(nomObj, '/');
            if (fin) *fin = 0;
        }
        Traduire(o + OBJ_TEXTE, trTexte, sizeof(trTexte));
        Traduire(o + OBJ_DESCRIPTION, trDesc, sizeof(trDesc));
        printf("[QXLS]\tO\t%016llX%016llX\t%d\t%s\t%s\t%d\t%s\t%s\n", (unsigned long long)g_quetes[iq].guid.a,
            (unsigned long long)g_quetes[iq].guid.b, i, nomObj, SansPrefixe(cls),
            obligatoire ? 1 : 0, trTexte, trDesc);
    }
    // Liens portes par cette quete et vers elle.
    for (int k = 0; k < g_nLiens; ++k) {
        const Lien& l = g_liens[k];
        char src[256];
        if (l.srcQuete == iq) {
            if (l.genre == 'A') printf("[QUETE]   -> lance l'acte \"%s\" (effets %s)\n", l.acte, l.srcQuand);
            else printf("[QUETE]   -> %s \"%s\" (effets %s)\n", l.genre == 'S' ? "lance" : "termine",
                g_quetes[l.cible].nom, l.srcQuand);
        }
        if (l.cible == iq) {
            DescriptionSource(l, src, sizeof(src));
            printf("[QUETE]   <- %s par %s\n", l.genre == 'S' ? "lancee" : "terminee", src);
        }
    }
}

// --- Par scenario --------------------------------------------------------------
struct RepEvt { char nom[64]; Guid guid; bool dansUnReglage; };
RepEvt g_reps[MAX_REPS];
int    g_nReps = 0;

void RecenserRepertoiresEvt()
{
    g_nReps = 0;
    uintptr_t reps = 0; int nr = 0;
    if (!LireTableau(Base() + OFF_REPERTOIRES_EVT, reps, nr)) return;
    for (int r = 0; r < nr && g_nReps < MAX_REPS; ++r) {
        uintptr_t rep = 0;
        if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000) continue;
        RepEvt& x = g_reps[g_nReps++];
        LireTexte(rep, x.nom, sizeof(x.nom));
        LireGuid(rep + 8, x.guid);
        x.dansUnReglage = false;
    }
    // Repertoires cites par au moins un reglage d'evenements.
    uintptr_t regs = 0; int ng = 0;
    if (!LireTableau(Base() + OFF_SCENARIOS_CONFIG + CFG_REGLAGES_EVT, regs, ng)) return;
    for (int i = 0; i < ng; ++i) {
        uintptr_t reg = 0, liens = 0; int nl = 0;
        if (!SafeRead((void*)(regs + (uintptr_t)i * 8), &reg, 8) || reg <= 0x10000) continue;
        if (!LireTableau(reg + REGLAGE_LIENS, liens, nl)) continue;
        for (int k = 0; k < nl; ++k) {
            Guid g{};
            if (!LireGuid(liens + (uintptr_t)k * 0x10, g)) continue;
            for (int r = 0; r < g_nReps; ++r) if (GuidEgal(g_reps[r].guid, g)) g_reps[r].dansUnReglage = true;
        }
    }
}

// Repertoires d'evenements d'un scenario : ceux de ses reglages, plus ceux
// qu'aucun reglage ne cite (charges partout : Health, Sequences, Utility...,
// constate sur The Arks et On the Edge le 2026-10-30).
void RepertoiresDuScenario(uintptr_t scn, bool* actif)
{
    for (int r = 0; r < g_nReps; ++r) actif[r] = !g_reps[r].dansUnReglage;
    uintptr_t liensScn = 0; int ns = 0;
    uintptr_t regs = 0; int ng = 0;
    if (!LireTableau(scn + SCN_EVENEMENTS, liensScn, ns) ||
        !LireTableau(Base() + OFF_SCENARIOS_CONFIG + CFG_REGLAGES_EVT, regs, ng)) return;
    for (int s = 0; s < ns; ++s) {
        Guid gs{};
        if (!LireGuid(liensScn + (uintptr_t)s * 0x10, gs)) continue;
        for (int i = 0; i < ng; ++i) {
            uintptr_t reg = 0, liens = 0; int nl = 0; Guid gr{};
            if (!SafeRead((void*)(regs + (uintptr_t)i * 8), &reg, 8) || reg <= 0x10000) continue;
            if (!LireGuid(reg + 8, gr) || !GuidEgal(gr, gs) || !LireTableau(reg + REGLAGE_LIENS, liens, nl)) continue;
            for (int k = 0; k < nl; ++k) {
                Guid g{};
                if (!LireGuid(liens + (uintptr_t)k * 0x10, g)) continue;
                for (int r = 0; r < g_nReps; ++r) if (GuidEgal(g_reps[r].guid, g)) actif[r] = true;
            }
        }
    }
}

// Repertoires exclus comme sources : commandes console, Endless, succes.
bool RepExclu(const char* nom)
{
    return strcmp(nom, "Debug") == 0 || strcmp(nom, "CustomScenarioEvents") == 0 ||
           strcmp(nom, "Achivments") == 0 || strncmp(nom, "Scenario Builders Debug", 23) == 0;
}

bool RepActif(const char* nom, const bool* actif)
{
    if (RepExclu(nom)) return false;
    for (int r = 0; r < g_nReps; ++r) if (actif[r] && strcmp(g_reps[r].nom, nom) == 0) return true;
    return false;
}

// FrostpunkCurrentScenarioCheckExpression (0x30, FUN_1415e12f0) : +0x18 lien
// vers le scenario, +0x28 Negate. Recherche recursive dans les conditions
// d'un evenement (+0x40) et dans les tableaux d'expressions imbriquees
// (composites) : les scenarios cites sans Negate forment l'ensemble
// autorise, ceux cites avec Negate l'ensemble interdit.
void CollecterScenarios(uintptr_t adrTableau, Guid* oui, int& nOui, Guid* non, int& nNon, int prof)
{
    uintptr_t tab = 0; int n = 0;
    if (prof > 4 || !LireTableau(adrTableau, tab, n) || n > 64) return;
    for (int i = 0; i < n; ++i) {
        uintptr_t e = 0; char cls[96] = "";
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000 || !GetClassNameFP(e, cls, sizeof(cls))) continue;
        if (strcmp(cls, "FrostpunkCurrentScenarioCheckExpression") == 0) {
            Guid g{}; uint8_t negate = 0;
            LireGuid(e + 0x18, g);
            SafeRead((void*)(e + 0x28), &negate, 1);
            if (negate) { if (nNon < 16) non[nNon++] = g; }
            else        { if (nOui < 16) oui[nOui++] = g; }
            continue;
        }
        int t = TailleObjet(e);
        for (int off = 0x08; off + 0x10 <= t; off += 8) CollecterScenarios(e + off, oui, nOui, non, nNon, prof + 1);
    }
}

bool EvenementPourScenario(uintptr_t evt, const Guid& scn)
{
    if (!evt) return true;
    Guid oui[16], non[16]; int nOui = 0, nNon = 0;
    CollecterScenarios(evt + 0x40, oui, nOui, non, nNon, 0);
    for (int i = 0; i < nNon; ++i) if (GuidEgal(non[i], scn)) return false;
    if (nOui == 0) return true;
    for (int i = 0; i < nOui; ++i) if (GuidEgal(oui[i], scn)) return true;
    return false;
}

// Etiquette de scenario portee par un nom d'evenement ou de quete. Les
// evenements des repertoires communs (Sequences, Utility - General...) sont
// propres a un scenario sans condition de scenario (signaux de sequence) :
// releve v4 du 2026-11-01. Renvoie le nom interne du scenario, ou nullptr.
const char* ScenarioDeLEtiquette(const char* s)
{
    struct { const char* etiquette; const char* scenario; } t[] = {
        { "[Refugees", "REFUGEES" }, { "Refugees-Scenario", "REFUGEES" }, { "Endlog-refugees", "REFUGEES" },
        { "[Builders", "BUILDERS" }, { "Builders [", "BUILDERS" }, { "- Endless]", "Endless" },
        { "[Winterhome", "WINTERHOME" },
        { "[TH]", "TechHaven" }, { "[TH_", "TechHaven" }, { "[TechHeaven", "TechHaven" }, { "[NM]", "TechHaven" },
        { "[Reconquest", "Reconquest" }, { "[DLC3", "Reconquest" }, { "[aDLC3", "Reconquest" },
        // Fil principal de A New Home : le repertoire de dilemmes "Londoners"
        // est actif aussi dans The Arks (releve v2 du 2026-11-01, 005 / 006
        // et la suite ENDGAME y etaient rattachees a tort).
        { "[Main Narrative Line", "DEFAULT" }, { "[ENDGAME]", "DEFAULT" },
    };
    for (auto& x : t) if (strstr(s, x.etiquette)) return x.scenario;
    return nullptr;
}

bool EtiquetteCompatible(const char* s, const char* nomScenario)
{
    const char* e = ScenarioDeLEtiquette(s);
    return !e || strcmp(e, nomScenario) == 0;
}

// Scenario d'un repertoire de dilemmes : nom interne, "*" = commun a tous
// (filtre ensuite par l'etiquette du dilemme), nullptr = ecarte (debug,
// Endless, inutilise). Un repertoire homonyme d'un repertoire d'evenements
// actif (Londoners, Endgame, Sociotech Hints) suit celui-ci.
const char* ScenarioDuRepDilemmes(const char* rep, const bool* actif)
{
    if (RepActif(rep, actif)) return "*";
    struct { const char* debut; const char* scenario; } t[] = {
        { "Debug", nullptr }, { "CustomScenario", nullptr }, { "Reconquest Endless", nullptr },
        { "Holiday Gimmick", nullptr }, { "SeasonPass", nullptr }, { "Refugees Scenario - unused", nullptr },
        { "Winterhome Scenario (backup)", nullptr }, { "Winterhome Scenario [WIP", nullptr },
        { "Buidlers Consequences [temp]", nullptr }, { "Builders TEMP", nullptr },
        { "Main Scenario", "DEFAULT" }, { "Endgame", "DEFAULT" }, { "Londoners", nullptr },
        { "Refugees", "REFUGEES" }, { "TechHaven", "TechHaven" }, { "Winterhome", "WINTERHOME" },
        { "Builders", "BUILDERS" }, { "Reconquest", "Reconquest" },
    };
    for (auto& x : t) if (strncmp(rep, x.debut, strlen(x.debut)) == 0) return x.scenario;
    return "*";
}

// Lien utilisable comme point d'entree pour ce scenario : evenement d'un
// repertoire actif, conditions et etiquette compatibles ; ou effet d'une loi
// (srcRep "Lois", la quete visee filtre par son etiquette).
bool SourceValide(const Lien& l, const bool* actif, const Guid& scn, const char* nomScenario)
{
    if (l.srcQuete >= 0) return false;
    // Lois : etiquette du nom de la loi ("Builders [..." = Last Autumn,
    // "[... - Endless]" = Endless).
    if (strcmp(l.srcRep, "Lois") == 0) return EtiquetteCompatible(l.srcEvt, nomScenario);
    // Dilemmes : repertoire de dilemmes rattache par son nom (releve du
    // 2026-11-01 : 49 repertoires, presque aucun homonyme cote evenements).
    if (strncmp(l.srcRep, "Dilemmes/", 9) == 0) {
        const char* s = ScenarioDuRepDilemmes(l.srcRep + 9, actif);
        if (!s) return false;
        if (strcmp(s, "*") != 0 && strcmp(s, nomScenario) != 0) return false;
        return EtiquetteCompatible(l.srcEvt, nomScenario);
    }
    return RepActif(l.srcRep, actif) && EvenementPourScenario(l.srcEvtAdr, scn) &&
           EtiquetteCompatible(l.srcEvt, nomScenario);
}

void ReleverScenario(uintptr_t scn)
{
    char nom[96];
    LireTexte(scn, nom, sizeof(nom));
    if (strncmp(nom, "Endless", 7) == 0 || strncmp(nom, "Debug", 5) == 0 || strcmp(nom, "Scenario") == 0) return;

    static bool actif[MAX_REPS];
    RepertoiresDuScenario(scn, actif);
    char liste[2048] = "";
    for (int r = 0; r < g_nReps; ++r) {
        if (!actif[r]) continue;
        if (liste[0]) strncat_s(liste, ", ", _TRUNCATE);
        strncat_s(liste, g_reps[r].nom, _TRUNCATE);
        if (!g_reps[r].dansUnReglage) strncat_s(liste, "*", _TRUNCATE);
    }
    Guid gScn{};
    LireGuid(scn + 8, gScn);
    printf("[QUETE] ===== scenario %s =====\n", nom);
    printf("[QUETE] repertoires d'evenements (* = commun, cite par aucun reglage ; Debug, "
        "CustomScenarioEvents, Achivments exclus) : %s\n", liste);

    // Atteignables : lancees par un evenement d'un repertoire actif dont les
    // conditions de scenario admettent celui-ci, puis fermeture par les liens
    // quete -> quete.
    static int ordre[MAX_QUETES];
    static bool vu[MAX_QUETES];
    memset(vu, 0, sizeof(vu));
    int n = 0;
    for (int k = 0; k < g_nLiens; ++k) {
        const Lien& l = g_liens[k];
        if (l.genre == 'S' && SourceValide(l, actif, gScn, nom) && !vu[l.cible] &&
            EtiquetteCompatible(g_quetes[l.cible].nom, nom)) {
            vu[l.cible] = true;
            ordre[n++] = l.cible;
        }
    }
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < g_nLiens; ++k) {
            const Lien& l = g_liens[k];
            if (l.genre == 'S' && l.srcQuete == ordre[i] && !vu[l.cible] &&
                EtiquetteCompatible(g_quetes[l.cible].nom, nom)) {
                vu[l.cible] = true;
                ordre[n++] = l.cible;
            }
        }
    printf("[QUETE] %d quete(s) atteignable(s) :\n", n);
    for (int i = 0; i < n; ++i) {
        int q = ordre[i];
        char sources[1024] = "";
        for (int k = 0; k < g_nLiens; ++k) {
            const Lien& l = g_liens[k];
            if (l.genre != 'S' || l.cible != q) continue;
            if (l.srcQuete < 0 && !SourceValide(l, actif, gScn, nom)) continue;
            if (l.srcQuete >= 0 && !vu[l.srcQuete]) continue;
            char src[256];
            DescriptionSource(l, src, sizeof(src));
            if (sources[0]) strncat_s(sources, " | ", _TRUNCATE);
            strncat_s(sources, src, _TRUNCATE);
        }
        printf("[QUETE]   [%s, type %d] %s  <- %s\n", g_quetes[q].rep, g_quetes[q].type, g_quetes[q].nom, sources);
        // Export Excel : sources directes de la quete (L = loi, E = evenement,
        // Q = autre quete), pour classer "lancee par une loi".
        char genres[8] = "";
        for (int k = 0; k < g_nLiens; ++k) {
            const Lien& l = g_liens[k];
            if (l.genre != 'S' || l.cible != q) continue;
            if (l.srcQuete < 0 && !SourceValide(l, actif, gScn, nom)) continue;
            if (l.srcQuete >= 0 && !vu[l.srcQuete]) continue;
            char g = l.srcQuete >= 0 ? 'Q' : strcmp(l.srcRep, "Lois") == 0 ? 'L' :
                     strncmp(l.srcRep, "Dilemmes/", 9) == 0 ? 'D' : 'E';
            if (!strchr(genres, g)) { size_t z = strlen(genres); genres[z] = g; genres[z + 1] = 0; }
        }
        printf("[QXLS]\tS\t%s\t%d\t%016llX%016llX\t%s\t%s\n", nom, i, (unsigned long long)g_quetes[q].guid.a,
            (unsigned long long)g_quetes[q].guid.b, genres, sources);
        for (int k = 0; k < g_nLiens; ++k)
            if (g_liens[k].genre == 'A' && g_liens[k].srcQuete == q)
                printf("[QUETE]     -> acte \"%s\" (effets %s)\n", g_liens[k].acte, g_liens[k].srcQuand);
    }
    for (int k = 0; k < g_nLiens; ++k)
        if (g_liens[k].genre == 'A' && SourceValide(g_liens[k], actif, gScn, nom))
            printf("[QUETE]   acte \"%s\" <- evenement \"%s\" (%s)\n", g_liens[k].acte, g_liens[k].srcEvt, g_liens[k].srcRep);
}

void Releve()
{
    RecenserQuetes();
    RecenserRepertoiresEvt();
    printf("[QUETE] ===== %d quetes, %d repertoires d'evenements =====\n", g_nQuetes, g_nReps);

    // 1) Liens depuis les evenements (dilemmes et branches compris).
    g_nLiens = 0;
    g_dernierEvt = 0;
    g_dernierEvtRevele = 0;
    LoisParcourirEvenements(VisiterEffet);
    int nEvt = g_nLiens;
    // 2) Liens depuis les effets des quetes.
    struct { int off; const char* quand; } t[] = {
        { QUE_INITIAUX, "initiaux" }, { QUE_SUCCES, "succes" }, { QUE_ECHEC, "echec" },
    };
    for (int i = 0; i < g_nQuetes; ++i) {
        g_queteCourante = i;
        for (auto& x : t) {
            g_quandCourant = x.quand;
            LoisParcourirEffets(g_quetes[i].adr + x.off, VisiterEffet, g_quetes[i].nom, g_quetes[i].rep);
        }
    }
    g_queteCourante = -1;
    int nQue = g_nLiens - nEvt;
    // 3) Liens depuis les effets des lois (FrostpunkSocietyConfig base +
    //    0x4070460, +0xB0 lois ; loi +0x18 nom, +0xE0 effets).
    uintptr_t lois = 0; int nl = 0;
    if (LireTableau(Base() + 0x4070460 + 0xB0, lois, nl))
        for (int i = 0; i < nl; ++i) {
            uintptr_t loi = 0;
            if (!SafeRead((void*)(lois + (uintptr_t)i * 8), &loi, 8) || loi <= 0x10000) continue;
            static char nomLoi[128];
            LireTexte(loi + 0x18, nomLoi, sizeof(nomLoi));
            g_loiCourante = nomLoi;
            g_adrLoiCourante = loi;
            LoisParcourirEffets(loi + 0xE0, VisiterEffet, nomLoi, "Lois");
        }
    g_loiCourante = nullptr;
    int nLois = g_nLiens - nEvt - nQue;
    // 4) Liens depuis TOUS les dilemmes, y compris ceux qu'aucun evenement ne
    //    cree (sequences : SequenceActionCreateDilemma). FrostpunkDilemmasConfig
    //    (reflexion FUN_1413e6a10, 0x1E8) : "Dilemmas" +0x130 = repertoires
    //    EntryDirectory<FrostpunkDilemmaEntry> (+0x00 nom, +0x18 dilemmes).
    //    Instance statique presumee base + 0x4069880 (collection DAT_144069970
    //    = config +0xF0, comme QuestsConfig +0x58 / repertoires +0x98).
    //    Dilemme : +0x48 effets initiaux, +0x60 ignores, +0x78 choix ; choix
    //    (FrostpunkDilemmaChoiceEntry) : +0x38 lien de QUETE lancee
    //    directement a la resolution (FUN_1417d40c0 -> FUN_14155e350 ->
    //    FUN_1416e8570), +0x78 effets, +0x90 resultats.
    {
        uintptr_t reps = 0; int nr = 0;
        bool ok = LireTableau(Base() + 0x4069880 + 0x130, reps, nr) && nr > 0;
        printf("[QUETE] dilemmes : config +4069880, %d repertoire(s)%s\n", nr, ok ? "" : " (ILLISIBLE)");
        static char nomRepDil[80], nomDil[128];
        for (int r = 0; ok && r < nr; ++r) {
            uintptr_t rep = 0, ent = 0; int ne = 0;
            if (!SafeRead((void*)(reps + (uintptr_t)r * 8), &rep, 8) || rep <= 0x10000) continue;
            char nomBrut[64];
            LireTexte(rep, nomBrut, sizeof(nomBrut));
            sprintf_s(nomRepDil, "Dilemmes/%s", nomBrut);
            bool memeNom = false;
            for (int k = 0; k < g_nReps; ++k) if (strcmp(g_reps[k].nom, nomBrut) == 0) memeNom = true;
            LireTableau(rep + REP_ENTREES, ent, ne);
            printf("[QUETE]   repertoire de dilemmes \"%s\" : %d dilemme(s), repertoire d'evenements du meme nom : %s\n",
                nomBrut, ne, memeNom ? "oui" : "NON");
            for (int e = 0; e < ne; ++e) {
                uintptr_t d = 0;
                if (!SafeRead((void*)(ent + (uintptr_t)e * 8), &d, 8) || d <= 0x10000) continue;
                LireTexte(d, nomDil, sizeof(nomDil));
                g_dilCourant = nomDil;
                g_repDilCourant = nomRepDil;
                g_adrDilCourant = d;
                LoisParcourirEffets(d + 0x48, VisiterEffet, nomDil, nomRepDil);
                LoisParcourirEffets(d + 0x60, VisiterEffet, nomDil, nomRepDil);
                uintptr_t choix = 0; int nc = 0;
                if (!LireTableau(d + 0x78, choix, nc) || nc > 16) continue;
                for (int c = 0; c < nc; ++c) {
                    uintptr_t ch = 0;
                    if (!SafeRead((void*)(choix + (uintptr_t)c * 8), &ch, 8) || ch <= 0x10000) continue;
                    LoisParcourirEffets(ch + 0x78, VisiterEffet, nomDil, nomRepDil);
                    LoisParcourirEffets(ch + 0x90, VisiterEffet, nomDil, nomRepDil);
                    Guid g{};
                    if (LireGuid(ch + 0x38, g) && (g.a || g.b)) {
                        int iq = IndiceQuete(g);
                        if (iq >= 0) AjouterLien('S', iq, nullptr, d, nomDil, nomRepDil);
                    }
                }
            }
        }
        g_dilCourant = nullptr;
        g_repDilCourant = nullptr;
    }
    printf("[QUETE] %d lien(s) : %d depuis des evenements, %d depuis des quetes, %d depuis des lois, %d depuis des dilemmes\n",
        g_nLiens, nEvt, nQue, nLois, g_nLiens - nEvt - nQue - nLois);

    // 3) Detail de chaque quete (hors succes Steam et Endless).
    for (int i = 0; i < g_nQuetes; ++i) {
        if (strcmp(g_quetes[i].rep, "Achievements") == 0 || strcmp(g_quetes[i].rep, "CustomScenarios") == 0) continue;
        DecrireQuete(i);
    }

    // 4) Quetes atteignables par scenario (hors Endless et debug).
    uintptr_t tab = 0; int ns = 0;
    LireTableau(Base() + OFF_SCENARIOS_CONFIG + CFG_SCENARIOS, tab, ns);
    for (int s = 0; s < ns; ++s) {
        uintptr_t scn = 0;
        if (SafeRead((void*)(tab + (uintptr_t)s * 8), &scn, 8) && scn > 0x10000) ReleverScenario(scn);
    }
}

// --- Trace des demarrages de quete en jeu ----------------------------------------
// FUN_1416e8570(systeme des quetes DAT_14408f0e8, quete, &contexte) -> char :
// demarrer une quete (appelee par la commande console StartQuest,
// FUN_1413abc00). Chaque demarrage est journalise avec l'evenement en cours
// et, pour les 40 premiers, la pile d'appels (adresses Ghidra) : on voit ainsi
// les lancements que les donnees ne montrent pas (sequences, code).
constexpr uintptr_t OFF_DEMARRER_QUETE = 0x16E8570;
typedef uintptr_t (*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
Fonction4_t oDemarrerQuete = nullptr;
volatile LONG g_nDemarrages = 0;

// --- Tableaux d'effets en cours d'execution --------------------------------------
// FUN_14169aee0(executeur, &tableau d'effets, contexte, sortie, ...) : execute
// une liste d'effets. Appelee par l'invocation d'evenement (evt +0x58), par les
// effets de loi, et par la file des effets temporises (FUN_14169d890 :
// "Executing scheduled timed effect...", tableau = TimedEffect + 0x40). Trace
// du 2026-11-01 : 005 LONDONERS est lancee depuis cette file.
constexpr uintptr_t OFF_EXECUTER_EFFETS = 0x169AEE0;
typedef uintptr_t (*Fonction6_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
Fonction6_t oExecuterEffets = nullptr;
thread_local uintptr_t t_tableaux[16];
thread_local int       t_nTableaux = 0;

// Test du 2026-11-01 : le TimedEffect execute par la file n'appartient a
// aucune donnee parcourue (copie, ou structure non atteinte). On note donc,
// a chaque liste executee, ses TimedEffect avec le contexte du moment de leur
// programmation : evenement en cours et liste la plus exterieure.
struct Programmation {
    uintptr_t timed;           // objet FrostpunkTimedEffect
    uintptr_t exterieur;       // liste d'effets la plus exterieure a ce moment
    uintptr_t liste;           // liste qui contient le TimedEffect
    char      evt[96];         // evenement en cours (g_nomEvtEnCours)
};
constexpr int MAX_PROGRAMMATIONS = 512;
Programmation g_progs[MAX_PROGRAMMATIONS];
int  g_nProgs = 0;
SRWLOCK g_verrouProgs = SRWLOCK_INIT;

void NoterTimedEffects(uintptr_t tab)
{
    uintptr_t t = 0; uint32_t n = 0;
    if (!SafeRead((void*)tab, &t, 8) || !SafeRead((void*)(tab + 8), &n, 4) || t <= 0x10000 || n == 0 || n > 64) return;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t e = 0; char cls[64] = "";
        if (!SafeRead((void*)(t + (uintptr_t)i * 8), &e, 8) || e <= 0x10000 || !GetClassNameFP(e, cls, sizeof(cls))) continue;
        if (strcmp(cls, "FrostpunkTimedEffect") != 0) continue;
        AcquireSRWLockExclusive(&g_verrouProgs);
        int k = 0;
        while (k < g_nProgs && g_progs[k].timed != e) ++k;
        if (k == g_nProgs) {
            if (g_nProgs < MAX_PROGRAMMATIONS) ++g_nProgs;
            else k = (int)(e % MAX_PROGRAMMATIONS);    // plein : on recycle
        }
        Programmation& p = g_progs[k];
        p.timed = e;
        p.exterieur = t_nTableaux > 0 ? t_tableaux[0] : tab;
        p.liste = tab;
        strncpy_s(p.evt, g_nomEvtEnCours[0] ? g_nomEvtEnCours : "(aucun)", _TRUNCATE);
        ReleaseSRWLockExclusive(&g_verrouProgs);
    }
}

uintptr_t hkExecuterEffets(uintptr_t ex, uintptr_t tab, uintptr_t ctx, uintptr_t sortie, uintptr_t p5, uintptr_t p6)
{
    if (t_nTableaux < 16) t_tableaux[t_nTableaux] = tab;
    ++t_nTableaux;
    NoterTimedEffects(tab);
    uintptr_t r = oExecuterEffets(ex, tab, ctx, sortie, p5, p6);
    --t_nTableaux;
    return r;
}

// Recherche du proprietaire d'un tableau d'effets : objet "Frostpunk..." dont
// un champ (+0x18..+0x60) est ce tableau, atteint par le parcours des
// evenements, des quetes ou des lois ; ou evenement dont c'est le +0x58.
uintptr_t g_tableauCherche = 0;
int       g_nTrouvailles = 0;
const char* g_ouCherche = "";

// Repli (test du 2026-11-01 : la liste du TimedEffect qui lance 005 LONDONERS
// n'appartient a aucun objet parcouru, mais elle est a 0x380 du TimedEffect
// qui lance 004, dans "[Act 1 Arc 1] U: Step 3 controller") : objet parcouru
// le plus proche en memoire juste en dessous du tableau cherche.
uintptr_t g_plusProcheObj = 0;
char      g_plusProcheDesc[256] = "";

void VisiterProprietaire(uintptr_t obj, const char* cls, uintptr_t evt, const char* nomEvt, const char* nomRep)
{
    if (obj < g_tableauCherche && g_tableauCherche - obj < 0x2000 &&
        (!g_plusProcheObj || obj > g_plusProcheObj)) {
        g_plusProcheObj = obj;
        sprintf_s(g_plusProcheDesc, "%s 0x%llX (-0x%llX), dans %s \"%s\" (%s)", SansPrefixe(cls),
            (unsigned long long)obj, (unsigned long long)(g_tableauCherche - obj), g_ouCherche,
            nomEvt ? nomEvt : "?", nomRep ? nomRep : "?");
    }
    if (g_nTrouvailles >= 6) return;
    for (int off = 0x18; off <= 0x60; off += 8) {
        if (obj + off != g_tableauCherche) continue;
        ++g_nTrouvailles;
        printf("[QUETE]     tableau d'effets = %s +0x%X, dans %s \"%s\" (%s)\n", SansPrefixe(cls), off,
            g_ouCherche, nomEvt ? nomEvt : "?", nomRep ? nomRep : "?");
        return;
    }
    if (evt && evt + 0x58 == g_tableauCherche && g_nTrouvailles < 6) {
        ++g_nTrouvailles;
        printf("[QUETE]     tableau d'effets = effets de l'evenement \"%s\" (%s)\n", nomEvt, nomRep);
        g_tableauCherche = 0;   // un seul signalement par evenement
    }
}

void ChercherProprietaire(uintptr_t tab)
{
    g_tableauCherche = tab;
    g_nTrouvailles = 0;
    g_plusProcheObj = 0;
    g_plusProcheDesc[0] = 0;
    g_ouCherche = "un evenement";
    LoisParcourirEvenements(VisiterProprietaire);
    g_ouCherche = "les effets de la quete";
    for (int i = 0; i < g_nQuetes && g_nTrouvailles < 6; ++i) {
        if (g_quetes[i].adr + QUE_INITIAUX == tab || g_quetes[i].adr + QUE_SUCCES == tab || g_quetes[i].adr + QUE_ECHEC == tab) {
            printf("[QUETE]     tableau d'effets = effets directs de la quete \"%s\"\n", g_quetes[i].nom);
            ++g_nTrouvailles;
        }
        const int offs[3] = { QUE_INITIAUX, QUE_SUCCES, QUE_ECHEC };
        for (int k = 0; k < 3; ++k)
            LoisParcourirEffets(g_quetes[i].adr + offs[k], VisiterProprietaire, g_quetes[i].nom, g_quetes[i].rep);
    }
    g_ouCherche = "les effets de la loi";
    uintptr_t lois = 0; int nl = 0;
    if (LireTableau(Base() + 0x4070460 + 0xB0, lois, nl))
        for (int i = 0; i < nl && g_nTrouvailles < 6; ++i) {
            uintptr_t loi = 0; char nomLoi[128];
            if (!SafeRead((void*)(lois + (uintptr_t)i * 8), &loi, 8) || loi <= 0x10000) continue;
            LireTexte(loi + 0x18, nomLoi, sizeof(nomLoi));
            if (loi + 0xE0 == tab) { printf("[QUETE]     tableau d'effets = effets directs de la loi \"%s\"\n", nomLoi); ++g_nTrouvailles; }
            LoisParcourirEffets(loi + 0xE0, VisiterProprietaire, nomLoi, "Lois");
        }
    if (g_nTrouvailles == 0) {
        printf("[QUETE]     tableau d'effets 0x%llX : proprietaire introuvable dans les donnees\n",
            (unsigned long long)tab);
        if (g_plusProcheObj) printf("[QUETE]     objet parcouru le plus proche en dessous : %s\n", g_plusProcheDesc);
        // Contenu du tableau : classes des effets qu'il porte.
        char classes[512];
        ClassesDuTableau(tab, classes, sizeof(classes));
        printf("[QUETE]     effets du tableau : [%s]\n", classes);
    }
}

// Test du 2026-11-01 : les 40 premiers demarrages d'une partie neuve sont des
// succes Steam (Achievements), et "evenement en cours" est toujours vide (les
// effets de quete ne passent pas dans l'invocation hookee : effets differes).
// La pile est donc tracee pour toute quete hors Achievements / CustomScenarios.
uintptr_t hkDemarrerQuete(uintptr_t sys, uintptr_t quete, uintptr_t ctx, uintptr_t p4)
{
    char nom[128] = "?";
    if (quete > 0x10000) LireTexte(quete, nom, sizeof(nom));
    char evt[128];
    strcpy_s(evt, g_nomEvtEnCours[0] ? g_nomEvtEnCours : "(aucun evenement en cours)");
    if (g_nQuetes == 0) RecenserQuetes();
    const char* rep = "?";
    for (int i = 0; i < g_nQuetes; ++i) if (g_quetes[i].adr == quete) { rep = g_quetes[i].rep; break; }
    uintptr_t res = oDemarrerQuete(sys, quete, ctx, p4);
    LONG n = InterlockedIncrement(&g_nDemarrages);
    bool succesSteam = strcmp(rep, "Achievements") == 0 || strcmp(rep, "CustomScenarios") == 0;
    printf("[QUETE] DEMARRAGE #%ld \"%s\" (repertoire %s) -> %s ; evenement en cours : %s\n", n, nom, rep,
        (res & 0xFF) ? "lancee" : "refusee", evt);
    static LONG nTraces = 0;
    if (!succesSteam && InterlockedIncrement(&nTraces) <= 200) {
        TracerPile("demarrage de quete", quete, n);
        // Origine : proprietaires des tableaux d'effets en cours (du plus
        // exterieur au plus interieur).
        int nt = t_nTableaux < 16 ? t_nTableaux : 16;
        for (int i = 0; i < nt; ++i) {
            uintptr_t tab = t_tableaux[i];
            printf("[QUETE]   execution d'effets niveau %d : tableau 0x%llX\n", i, (unsigned long long)tab);
            ChercherProprietaire(tab);
            // Liste d'un TimedEffect (+0x40) : contexte de sa programmation.
            uintptr_t timed = tab - 0x40;
            char cls[64] = "";
            if (!GetClassNameFP(timed, cls, sizeof(cls)) || strcmp(cls, "FrostpunkTimedEffect") != 0) continue;
            Programmation p{};
            bool connu = false;
            AcquireSRWLockShared(&g_verrouProgs);
            for (int k = 0; k < g_nProgs; ++k) if (g_progs[k].timed == timed) { p = g_progs[k]; connu = true; break; }
            ReleaseSRWLockShared(&g_verrouProgs);
            if (!connu) {
                printf("[QUETE]     TimedEffect 0x%llX : programmation non vue (avant le lancement du jeu, ou "
                    "sauvegarde chargee)\n", (unsigned long long)timed);
                continue;
            }
            printf("[QUETE]     TimedEffect 0x%llX programme pendant l'evenement \"%s\" ; liste 0x%llX, "
                "liste exterieure 0x%llX\n", (unsigned long long)timed, p.evt,
                (unsigned long long)p.liste, (unsigned long long)p.exterieur);
            ChercherProprietaire(p.liste);
            if (p.exterieur != p.liste) ChercherProprietaire(p.exterieur);
        }
    }
    return res;
}

// --- Checks de fin de quete (demande de l'auteur du 2026-11-01) --------------
// FUN_1416d22a0(systeme des quetes, id d'instance, succes) = TERMINER une
// quete (16 appelants : objectifs remplis, FinishQuestEffect FUN_14168e090 /
// FUN_1416d2710, console, delai expire...). Instances : systeme +0x1C0
// (enregistrements de 0x90 ; indice par FUN_1416c5ec0(systeme + 0x1C0, &id)),
// +0x10 = FrostpunkQuestEntry, +0x18 = etat (-1 active, puis !succes).
// Seules les quetes de l'histoire du scenario courant (known_quests.h :
// Histoire, Dilemme, Repertoire) envoient un check, a leur SUCCES ; lois,
// secondaires et communes a tous les scenarios n'en envoient pas.
// Sauvegarde locale mods\ap_quetes_<GameId>.txt (une ligne "Q <GUID>" par
// check envoye, cumulative) : une quete refaite apres rechargement d'une
// sauvegarde plus ancienne n'envoie pas deux fois son check.
constexpr uintptr_t OFF_TERMINER_QUETE  = 0x16D22A0;
constexpr uintptr_t OFF_INDICE_INSTANCE = 0x16C5EC0;
constexpr int SYS_INSTANCES  = 0x1C0;
constexpr int INST_PAS       = 0x90;
constexpr int INST_QUETE     = 0x10;

typedef uintptr_t (*TerminerQuete_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
TerminerQuete_t oTerminerQuete = nullptr;
typedef int (*IndiceInstance_t)(uintptr_t, int*);

int AppelerIndiceInstance(uintptr_t sys, int id)
{
    __try { return ((IndiceInstance_t)(Base() + OFF_INDICE_INSTANCE))(sys + SYS_INSTANCES, &id); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

uint64_t g_partieQuetes[2] = {};
bool     g_partieQuetesLue = false;
Guid     g_quetesEnvoyees[256];
int      g_nQuetesEnvoyees = 0;

bool FichierQuetes(char* out, int taille)
{
    char base[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_quetes_%016llX%016llX.txt", base,
        (unsigned long long)g_partieQuetes[1], (unsigned long long)g_partieQuetes[0]);
    return true;
}

// Relit les checks de quete deja envoyes quand le GameId change.
bool AssurerPartieQuetes()
{
    uint64_t gid[2] = {};
    uintptr_t scn = SystemeParId(IdSysteme::Scenario);
    if (!scn || !SafeRead((void*)(scn + 0x110), gid, sizeof(gid)) || (!gid[0] && !gid[1])) return false;
    if (g_partieQuetesLue && gid[0] == g_partieQuetes[0] && gid[1] == g_partieQuetes[1]) return true;
    g_partieQuetes[0] = gid[0];
    g_partieQuetes[1] = gid[1];
    g_partieQuetesLue = true;
    g_nQuetesEnvoyees = 0;
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (FichierQuetes(chemin, sizeof(chemin)) && fopen_s(&f, chemin, "r") == 0 && f) {
        char ligne[128];
        while (fgets(ligne, sizeof(ligne), f) && g_nQuetesEnvoyees < 256) {
            unsigned long long a = 0, b = 0;
            if (sscanf_s(ligne, "Q %16llX%16llX", &a, &b) == 2) g_quetesEnvoyees[g_nQuetesEnvoyees++] = { a, b };
        }
        fclose(f);
    }
    printf("[QUETEAP] partie %016llX%016llX : %d check(s) de quete deja envoye(s)\n",
        (unsigned long long)gid[1], (unsigned long long)gid[0], g_nQuetesEnvoyees);
    return true;
}

// Quete de l'histoire du scenario courant (known_quests.h), ou nullptr.
const QueteInfo* QueteHistoire(const Guid& g, const char* scenario)
{
    for (int i = 0; i < KNOWN_QUESTS_COUNT; ++i) {
        const QueteInfo& q = KNOWN_QUESTS[i];
        if (q.guidA != g.a || q.guidB != g.b || strcmp(q.scenario, scenario) != 0) continue;
        if (q.categorie == QueteCat::Histoire || q.categorie == QueteCat::Dilemme ||
            q.categorie == QueteCat::Repertoire) return &q;
    }
    return nullptr;
}

void QueteTerminee(uintptr_t quete, bool succes)
{
    char nom[128] = "?", scenario[64] = "?";
    LireTexte(quete, nom, sizeof(nom));
    uintptr_t scn = SystemeParId(IdSysteme::Scenario), objScn = 0;
    if (scn && SafeRead((void*)(scn + 0x68), &objScn, 8) && objScn > 0x10000) LireTexte(objScn, scenario, sizeof(scenario));
    Guid g{};
    LireGuid(quete + 8, g);
    const QueteInfo* info = QueteHistoire(g, scenario);
    static LONG nJournal = 0;
    if (info || InterlockedIncrement(&nJournal) <= 100)
        printf("[QUETEAP] FIN \"%s\" (%s) : %s%s\n", nom, scenario, succes ? "succes" : "echec",
            info ? " -> quete de l'histoire" : "");
    if (!info || !succes) return;
    if (!AssurerPartieQuetes()) {
        printf("[QUETEAP]   partie illisible : check non envoye\n");
        return;
    }
    for (int i = 0; i < g_nQuetesEnvoyees; ++i)
        if (GuidEgal(g_quetesEnvoyees[i], g)) {
            printf("[QUETEAP]   check deja envoye dans cette partie\n");
            return;
        }
    if (g_nQuetesEnvoyees < 256) g_quetesEnvoyees[g_nQuetesEnvoyees++] = g;
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (FichierQuetes(chemin, sizeof(chemin)) && fopen_s(&f, chemin, "a") == 0 && f) {
        fprintf(f, "Q %016llX%016llX\n", (unsigned long long)g.a, (unsigned long long)g.b);
        fclose(f);
    }
    SendQuestCheck(info->nom, info->nomAffiche);
}

uintptr_t hkTerminerQuete(uintptr_t sys, uintptr_t id, uintptr_t succes, uintptr_t p4)
{
    // L'instance est lue AVANT l'original (il retire la quete des actives).
    uintptr_t quete = 0;
    int idx = AppelerIndiceInstance(sys, (int)id);
    uintptr_t tab = 0;
    if (idx >= 0 && SafeRead((void*)(sys + SYS_INSTANCES), &tab, 8) && tab > 0x10000)
        SafeRead((void*)(tab + (uintptr_t)idx * INST_PAS + INST_QUETE), &quete, 8);
    uintptr_t res = oTerminerQuete(sys, id, succes, p4);
    if (quete > 0x10000) QueteTerminee(quete, (succes & 0xFF) != 0);
    return res;
}

} // namespace

void InstallHooksQuetes(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_DEMARRER_QUETE,  (void*)&hkDemarrerQuete,  (void**)&oDemarrerQuete,  "demarrage de quete" },
        { OFF_EXECUTER_EFFETS, (void*)&hkExecuterEffets, (void**)&oExecuterEffets, "execution d'une liste d'effets" },
        { OFF_TERMINER_QUETE,  (void*)&hkTerminerQuete,  (void**)&oTerminerQuete,  "fin de quete (checks)" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[QUETE] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}

void QuetesDemanderReleve()
{
    g_releveDemande = true;
    printf("[QUETE] releve demande : il s'execute sur le thread du jeu (jeu hors pause)\n");
}

void QuetesPompe()
{
    if (!g_releveDemande) return;
    g_releveDemande = false;
    Releve();
    printf("[QUETE] releve termine\n");
    ScootingReleverSites();
    ScootingReleverTablesDeSites();
    SitesContenuReleve();
    RecettesReleve();
}

void TraduireCleJeu(uintptr_t adrCle, char* out, int taille)
{
    Traduire(adrCle, out, taille);
}
