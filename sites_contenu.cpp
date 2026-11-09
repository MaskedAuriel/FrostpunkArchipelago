// ---- sites_contenu.cpp ----
// Contenu reel des sites d'exploration (2026-11-03, demande de la session
// apworld : site avec survivants ou noyaux de vapeur = item useful). Le champ
// "contenu" de known_sites.h ne voyait que les ressources posees (+0x48) :
// 105 sites "(rien)". Decompiles du 2026-11-03 (adresses Ghidra) :
//
//   FrostpunkExpeditionSiteComponent (0x170, reflexion FUN_141a157b0) :
//     +0x34 State, +0x38 Variant (optionnel), +0x48 Resources et +0x60
//     BonusResources ({definition, quantite} au pas de 0x10), +0x78
//     Technologies (Set), +0x90 Artifacts (Set), +0xA8 OutsiderSurvivors
//     (Dynarray<FrostpunkSurvivorsEntry>), +0x110 ExplorationDilemma (int :
//     indice du dilemme retenu), +0x160 ExplorationDate.
//   FrostpunkExpeditionSiteComponentTemplate (FUN_141a161d0) : +0x118
//     Treasure (FrostpunkExpeditionSiteTreasureEntry embarque, 0xF0,
//     FUN_141a17aa0) ; +0x3D0 Exploration dilemmas (ExplorationDilemma, 0x28 :
//     +0x00 lien du dilemme, +0x10 Conditions) ; +0x3E8 Exploration instant
//     effects (ExplorationInstantEffect, 0x78 : +0x00 liens de
//     FrostpunkWorldMapEffectEntry, +0x18 Conditions, +0x60/+0x68/+0x70 textes).
//   Treasure : +0x00 Resources, +0x18 Bonus resources (FrostpunkResourceAmount,
//     0x14 : +0x00 lien de ressource, +0x10 Amount), +0x30 Technologies, +0x48
//     Artifacts (liens), +0x60 Custom survivor group template, +0x70 Survivors
//     (FrostpunkSurvivorsEntry, 0x24 : +0x00 Count, +0x04 lien de classe
//     sociale, +0x14 statistiques initiales), +0x88 Survivors city reached
//     dilemma, +0xA8 Randomized resources et +0xC0 Randomized bonus resources
//     (0x20 : +0x00 ressource, +0x10 poids), +0xD8 Randomized survivors (0x30 :
//     +0x00 classe sociale, +0x10 statistiques, +0x20 poids).
//   Effets a ressources : FrostpunkChangeExpeditionResourcesEffect (+0x18
//     "Resources to change") et FrostpunkGiveResourcesToStationingExpedition
//     Effect (+0x18 "Resources") : Dynarray<FrostpunkResourceAmount> ;
//     FrostpunkChangeResourcesEffect (0x40) : +0x18 lien de ressource, +0x28
//     Addition (IntegerExpression), +0x30 Multiplier, +0x38 Change type.
//   Collections (EntryCollection<T>, recherche par lien = vtable +0x88) :
//     classes sociales base + 0x4072078 (FrostpunkPopulationConfig base +
//     0x4072020, +0x58), effets de carte base + 0x40806F8 (FrostpunkWorldMap
//     Config base + 0x40805A0, +0x158 ; entree : +0x00 nom, +0x18 Effects).
//   FrostpunkResourceEntry (0xD8) : +0x00 nom, +0x08 GUID, +0x28 Display name.
//
// Lignes (une valeur par colonne, separateur tabulation) :
//   [SCONT] S <scenario> <site> <etat> <variante> <dilemme retenu> <nb dilemmes> <nb effets instantanes>
//   [SCONT] R <scenario> <site> <source> <ressource> <quantite>
//   [SCONT] P <scenario> <site> <source> <classe sociale> <nombre>
//   [SCONT] X <scenario> <site> <source> <technologies|artefacts> <nombre>
//   [SCONT] D <scenario> <site> <i> <dilemme> <nb choix> <titre traduit> <conditions>
//   [SCONT] C <scenario> <site> <dilemme> <n> <texte du choix traduit>
//   [SCONT] E <scenario> <site> <source> <profondeur> <classe> <details>
// source : "composant", "composant bonus", "gabarit", "gabarit bonus",
// "gabarit alea", "gabarit alea bonus", "dilemme <nom> initiaux|ignore|choix N",
// "instantane <i>".
#include "sites_contenu.h"
#include "mod_common.h"
#include "scenario.h"      // g_run
#include "scooting.h"      // ScootingNomDuGabarit
#include "quetes.h"        // TraduireCleJeu
#include "lois.h"          // LoisDecrireExigences
#include "evenements.h"    // DilemmeParGuid
#include <cstring>

namespace {

static const uintptr_t OFF_ECONOMIE        = Adr(0x40916D0);
constexpr int       ECO_RESSOURCES      = 0x2240;   // entrees de 0x70, compte +8
constexpr int       ECO_PAS             = 0x70;
static const uintptr_t OFF_CLASSES_SOCIALES= Adr(0x4072078);
static const uintptr_t OFF_STATS_INITIALES = Adr(0x4072140);   // config de population + 0x120 (FUN_141440690)
static const uintptr_t OFF_EFFETS_CARTE    = Adr(0x40806F8);
constexpr int       VT_PAR_LIEN         = 0x88;

constexpr int SITE_VARIANTE   = 0x38;
constexpr int SITE_RES        = 0x48;
constexpr int SITE_BONUS      = 0x60;
constexpr int SITE_TECHS      = 0x78;
constexpr int SITE_ARTEFACTS  = 0x90;
constexpr int SITE_SURVIVANTS = 0xA8;
constexpr int SITE_DIL_RETENU = 0x110;

constexpr int GAB_TRESOR      = 0x118;
constexpr int GAB_DILEMMES    = 0x3D0;
constexpr int GAB_INSTANTANES = 0x3E8;

constexpr int TR_RES          = 0x00;
constexpr int TR_BONUS        = 0x18;
constexpr int TR_TECHS        = 0x30;
constexpr int TR_ARTEFACTS    = 0x48;
constexpr int TR_SURVIVANTS   = 0x70;
constexpr int TR_ALEA_RES     = 0xA8;
constexpr int TR_ALEA_BONUS   = 0xC0;
constexpr int TR_ALEA_SURV    = 0xD8;

const char* g_scn = "?";
const char* g_site = "?";

bool LireTab(uintptr_t adr, uintptr_t& tab, uint32_t& n, uint32_t max = 256)
{
    tab = 0; n = 0;
    if (!SafeRead((void*)adr, &tab, 8) || !SafeRead((void*)(adr + 8), &n, 4)) { n = 0; return false; }
    if (n == 0) return true;
    if (tab <= 0x10000 || n > max) { tab = 0; n = 0; return false; }
    return true;
}

bool LireNom(uintptr_t adrPtr, char* out, int taille)
{
    out[0] = 0;
    uintptr_t p = 0;
    if (!SafeRead((void*)adrPtr, &p, 8) || p <= 0x10000) return false;
    int i = 0;
    for (; i < taille - 1; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1) || !c) break;
        if (c < 0x20 || c >= 0x7F) { out[0] = 0; return false; }
        out[i] = c;
    }
    out[i] = 0;
    return i >= 2;
}

bool GuidNul(const uint8_t* g)
{
    for (int i = 0; i < 16; ++i) if (g[i]) return false;
    return true;
}

void GuidHex(const uint8_t* g, char* out, int taille)
{
    int o = 0;
    for (int i = 0; i < 16 && o + 2 < taille; ++i) o += sprintf_s(out + o, taille - o, "%02X", g[i]);
}

typedef uintptr_t (*ParLien_t)(uintptr_t, const uint8_t*);

// Entree d'une EntryCollection par son lien (GUID), appel protege.
uintptr_t EntreeParLien(uintptr_t coll, const uint8_t* guid)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), vt = 0, fn = 0;
    if (GuidNul(guid)) return 0;
    if (!SafeRead((void*)coll, &vt, 8) || vt < base || vt > base + 0x5000000) return 0;
    if (!SafeRead((void*)(vt + VT_PAR_LIEN), &fn, 8) || fn < base || fn > base + 0x2000000) return 0;
    __try { return ((ParLien_t)fn)(coll, guid); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// Nom interne d'une ressource par son GUID : conteneur de l'economie
// (definition +0x00 nom, +0x08 GUID).
void NomRessource(const uint8_t* guid, char* out, int taille)
{
    uintptr_t eco = 0, arr = 0;
    uint32_t n = 0;
    if (SafeRead((void*)((uintptr_t)GetModuleHandleW(NULL) + OFF_ECONOMIE), &eco, 8) && eco > 0x10000 &&
        LireTab(eco + ECO_RESSOURCES, arr, n, 64)) {
        for (uint32_t i = 0; i < n; ++i) {
            uintptr_t def = 0;
            uint8_t g[16] = {};
            if (!SafeRead((void*)(arr + (uintptr_t)i * ECO_PAS), &def, 8) || def <= 0x10000) continue;
            if (SafeRead((void*)(def + 8), g, 16) && memcmp(g, guid, 16) == 0 && LireNom(def, out, taille)) return;
        }
    }
    char h[40];
    GuidHex(guid, h, sizeof(h));
    sprintf_s(out, taille, "?%s", h);
}

void NomClasseSociale(const uint8_t* guid, char* out, int taille)
{
    uintptr_t e = EntreeParLien((uintptr_t)GetModuleHandleW(NULL) + OFF_CLASSES_SOCIALES, guid);
    if (e > 0x10000 && LireNom(e, out, taille)) return;
    char h[40];
    GuidHex(guid, h, sizeof(h));
    sprintf_s(out, taille, GuidNul(guid) ? "(aucune)" : "?%s", h);
}

// Dynarray<FrostpunkResourceAmount> (0x14 : lien +0x00, quantite +0x10).
void LignesMontants(uintptr_t adr, const char* source)
{
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(adr, tab, n, 64)) return;
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t g[16] = {};
        int32_t q = 0;
        char nom[64];
        SafeRead((void*)(tab + (uintptr_t)i * 0x14), g, 16);
        SafeRead((void*)(tab + (uintptr_t)i * 0x14 + 0x10), &q, 4);
        NomRessource(g, nom, sizeof(nom));
        printf("[SCONT]\tR\t%s\t%s\t%s\t%s\t%d\n", g_scn, g_site, source, nom, q);
    }
}

// Contenu courant du composant : {definition, quantite} au pas de 0x10.
void LignesContenuComposant(uintptr_t adr, const char* source)
{
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(adr, tab, n, 64)) return;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t def = 0;
        int64_t q = 0;
        char nom[64] = "?";
        SafeRead((void*)(tab + (uintptr_t)i * 0x10), &def, 8);
        SafeRead((void*)(tab + (uintptr_t)i * 0x10 + 8), &q, 8);
        if (def > 0x10000 && !LireNom(def, nom, sizeof(nom)) && !LireNom(def + 8, nom, sizeof(nom))) strcpy_s(nom, "?");
        printf("[SCONT]\tR\t%s\t%s\t%s\t%s\t%lld\n", g_scn, g_site, source, nom, (long long)q);
    }
}

// Dynarray<FrostpunkSurvivorsEntry> (0x24 : nombre +0x00, classe +0x04).
void LignesSurvivants(uintptr_t adr, const char* source)
{
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(adr, tab, n, 64)) return;
    for (uint32_t i = 0; i < n; ++i) {
        int32_t nb = 0;
        uint8_t g[16] = {};
        char cls[64];
        SafeRead((void*)(tab + (uintptr_t)i * 0x24), &nb, 4);
        SafeRead((void*)(tab + (uintptr_t)i * 0x24 + 4), g, 16);
        NomClasseSociale(g, cls, sizeof(cls));
        printf("[SCONT]\tP\t%s\t%s\t%s\t%s\t%d\n", g_scn, g_site, source, cls, nb);
    }
}

void LigneCompte(uintptr_t adr, const char* source, const char* quoi)
{
    uintptr_t tab = 0; uint32_t n = 0;
    LireTab(adr, tab, n, 1024);
    if (n) printf("[SCONT]\tX\t%s\t%s\t%s\t%s\t%u\n", g_scn, g_site, source, quoi, n);
}

// --- Parcours des effets ------------------------------------------------------
constexpr int MAX_VUS = 2048;
uintptr_t g_vus[MAX_VUS];
int       g_nVus = 0;

bool DejaVu(uintptr_t p)
{
    for (int i = 0; i < g_nVus; ++i) if (g_vus[i] == p) return true;
    if (g_nVus < MAX_VUS) g_vus[g_nVus++] = p;
    return g_nVus >= MAX_VUS;
}

// Taille d'un objet de reflexion : vtable [3] = fiche, fiche +0x88 = taille.
typedef uintptr_t (*Fiche_t)(uintptr_t);
int TailleObjet(uintptr_t obj)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), vt = 0, fn = 0, fiche = 0;
    uint64_t taille = 0;
    if (!SafeRead((void*)obj, &vt, 8) || vt < base || vt > base + 0x5000000) return 0;
    if (!SafeRead((void*)(vt + 0x18), &fn, 8) || fn < base || fn > base + 0x2000000) return 0;
    __try { fiche = ((Fiche_t)fn)(obj); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    if (fiche <= 0x10000 || !SafeRead((void*)(fiche + 0x88), &taille, 8)) return 0;
    if (taille < 0x10 || taille > 0x1000) return 0;
    return (int)taille;
}

bool ClasseFrostpunk(uintptr_t p, char* cls, int taille)
{
    cls[0] = 0;
    return p > 0x10000 && GetClassNameFP(p, cls, taille) && strncmp(cls, "Frostpunk", 9) == 0;
}

// Classes dont les champs bruts sont journalises (a decoder au besoin).
bool ClasseInteressante(const char* c)
{
    const char* mots[] = { "Resource", "Survivor", "Agent", "Citizen", "Expedition", "Technolog",
                           "Artifact", "Variant", "Population", "Settlement", "Expression", "Treasure" };
    for (const char* m : mots) if (strstr(c, m)) return true;
    return false;
}

// Champs non nuls (dwords) de +0x18 a la fin de l'objet, textes en clair.
void ChampsBruts(uintptr_t obj, char* out, int taille)
{
    out[0] = 0;
    int t = TailleObjet(obj);
    if (t <= 0) t = 0x40;
    for (int off = 0x08; off + 4 <= t && off < 0x100; off += 4) {
        char item[128], texte[96];
        if ((off & 7) == 0 && off + 8 <= t && LireNom(obj + off, texte, sizeof(texte))) {
            sprintf_s(item, "+%X\"%s\" ", off, texte);
            off += 4;
        } else {
            int32_t v = 0;
            SafeRead((void*)(obj + off), &v, 4);
            if (v == 0) continue;
            float f = 0;
            memcpy(&f, &v, 4);
            if (f > 0.001f && f < 100000.0f && (v & 0xFFFF) != 0 && v > 0x30000000 && v < 0x4B000000)
                sprintf_s(item, "+%X=%d(%.2f) ", off, v, f);
            else sprintf_s(item, "+%X=%d ", off, v);
        }
        strncat_s(out, taille, item, _TRUNCATE);
    }
}

void VisiterTableau(uintptr_t adr, int prof, const char* source);
void VisiterDilemme(uintptr_t dil, int prof, const char* source);

// Pointeur sur une expression (Addition de ChangeResources...) : classe + champs.
void DecrireExpression(uintptr_t adrPtr, char* out, int taille)
{
    uintptr_t e = 0;
    char cls[96], ch[400];
    out[0] = 0;
    if (!SafeRead((void*)adrPtr, &e, 8) || e <= 0x10000 || !GetClassNameFP(e, cls, sizeof(cls))) return;
    ChampsBruts(e, ch, sizeof(ch));
    sprintf_s(out, taille, "%s{%s}", cls, ch);
}

void VisiterObjet(uintptr_t obj, int prof, const char* source)
{
    if (prof > 8 || DejaVu(obj)) return;
    char cls[96];
    if (!ClasseFrostpunk(obj, cls, sizeof(cls))) return;
    const char* c = cls + 9;
    char det[1024] = "";

    if (!strcmp(cls, "FrostpunkChangeExpeditionResourcesEffect") ||
        !strcmp(cls, "FrostpunkGiveResourcesToStationingExpeditionEffect")) {
        printf("[SCONT]\tE\t%s\t%s\t%s\t%d\t%s\t(voir R)\n", g_scn, g_site, source, prof, c);
        char src[200];
        sprintf_s(src, "%s / %s", source, c);
        LignesMontants(obj + 0x18, src);
    } else if (!strcmp(cls, "FrostpunkChangeResourcesEffect")) {
        uint8_t g[16] = {};
        char nom[64], add[400], mul[400];
        int32_t type = 0;
        SafeRead((void*)(obj + 0x18), g, 16);
        SafeRead((void*)(obj + 0x38), &type, 4);
        NomRessource(g, nom, sizeof(nom));
        DecrireExpression(obj + 0x28, add, sizeof(add));
        DecrireExpression(obj + 0x30, mul, sizeof(mul));
        sprintf_s(det, "ressource=%s addition=%s multiplicateur=%s type=%d", nom, add, mul, type);
        printf("[SCONT]\tE\t%s\t%s\t%s\t%d\t%s\t%s\n", g_scn, g_site, source, prof, c, det);
    } else {
        if (ClasseInteressante(cls)) ChampsBruts(obj, det, sizeof(det));
        printf("[SCONT]\tE\t%s\t%s\t%s\t%d\t%s\t%s\n", g_scn, g_site, source, prof, c, det);
    }

    // Dilemme enchaine (CreateDilemmaEffect...) : +0x18 lien.
    if (strstr(cls, "DilemmaEffect")) {
        uint8_t g[16] = {};
        SafeRead((void*)(obj + 0x18), g, 16);
        uintptr_t dil = GuidNul(g) ? 0 : DilemmeParGuid(g);
        if (dil) VisiterDilemme(dil, prof + 1, source);
    }

    // Generique : tout tableau d'objets "Frostpunk..." dans l'objet, et les
    // structures embarquees sans vtable (Switch -> CaseEntry...), comme lois.cpp.
    int taille = TailleObjet(obj);
    for (int off = 0x08; off + 0x10 <= taille; off += 8) {
        uintptr_t tab = 0; uint32_t n = 0;
        if (!SafeRead((void*)(obj + off), &tab, 8) || !SafeRead((void*)(obj + off + 8), &n, 4)) continue;
        if (tab <= 0x10000 || n == 0 || n > 256) continue;
        uintptr_t premier = 0;
        char c2[96];
        if (!SafeRead((void*)tab, &premier, 8) || premier <= 0x10000) continue;
        if (GetClassNameFP(premier, c2, sizeof(c2))) {
            if (!strncmp(c2, "Frostpunk", 9)) VisiterTableau(obj + off, prof + 1, source);
            continue;
        }
        for (uint32_t i = 0; i < n && i < 64; ++i) {
            uintptr_t elt = 0;
            if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &elt, 8) || elt <= 0x10000 || DejaVu(elt)) continue;
            for (int o2 = 0; o2 + 0x10 <= 0x80; o2 += 8) {
                uintptr_t t2 = 0, p2 = 0; uint32_t n2 = 0;
                char c3[96];
                if (!SafeRead((void*)(elt + o2), &t2, 8) || !SafeRead((void*)(elt + o2 + 8), &n2, 4)) continue;
                if (t2 <= 0x10000 || n2 == 0 || n2 > 256 || !SafeRead((void*)t2, &p2, 8) || p2 <= 0x10000) continue;
                if (GetClassNameFP(p2, c3, sizeof(c3)) && !strncmp(c3, "Frostpunk", 9))
                    VisiterTableau(elt + o2, prof + 1, source);
            }
        }
    }
}

void VisiterTableau(uintptr_t adr, int prof, const char* source)
{
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(adr, tab, n, 256)) return;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t o = 0;
        if (SafeRead((void*)(tab + (uintptr_t)i * 8), &o, 8) && o > 0x10000) VisiterObjet(o, prof, source);
    }
}

// FrostpunkDilemmaEntry : +0x00 nom, +0x48 effets initiaux, +0x60 effets si
// ignore, +0x78 choix (FrostpunkDilemmaChoiceEntry : +0x78 effets, +0x90
// resultats, +0xA8 texte).
void VisiterDilemme(uintptr_t dil, int prof, const char* sourceParent)
{
    if (prof > 8 || DejaVu(dil)) return;
    char nom[128] = "?", src[256];
    LireNom(dil, nom, sizeof(nom));
    sprintf_s(src, "%s%sdilemme %s initiaux", sourceParent, sourceParent[0] ? " > " : "", nom);
    VisiterTableau(dil + 0x48, prof, src);
    sprintf_s(src, "%s%sdilemme %s ignore", sourceParent, sourceParent[0] ? " > " : "", nom);
    VisiterTableau(dil + 0x60, prof, src);
    uintptr_t tab = 0; uint32_t n = 0;
    if (!LireTab(dil + 0x78, tab, n, 16)) return;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t ch = 0;
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &ch, 8) || ch <= 0x10000) continue;
        char texte[512];
        TraduireCleJeu(ch + 0xA8, texte, sizeof(texte));
        printf("[SCONT]\tC\t%s\t%s\t%s\t%u\t%s\n", g_scn, g_site, nom, i + 1, texte);
        sprintf_s(src, "%s%sdilemme %s choix %u", sourceParent, sourceParent[0] ? " > " : "", nom, i + 1);
        VisiterTableau(ch + 0x78, prof, src);
        // Resultats : objets polymorphes ou structures embarquees.
        uintptr_t rt = 0; uint32_t rn = 0;
        if (!LireTab(ch + 0x90, rt, rn, 16)) continue;
        for (uint32_t k = 0; k < rn; ++k) {
            uintptr_t r = 0;
            char c[96], srcR[300];
            if (!SafeRead((void*)(rt + (uintptr_t)k * 8), &r, 8) || r <= 0x10000) continue;
            sprintf_s(srcR, "%s resultat %u", src, k + 1);
            if (ClasseFrostpunk(r, c, sizeof(c))) { VisiterObjet(r, prof + 1, srcR); continue; }
            for (int o2 = 0; o2 + 0x10 <= 0x80; o2 += 8) {
                uintptr_t t2 = 0, p2 = 0; uint32_t n2 = 0;
                char c3[96];
                if (!SafeRead((void*)(r + o2), &t2, 8) || !SafeRead((void*)(r + o2 + 8), &n2, 4)) continue;
                if (t2 <= 0x10000 || n2 == 0 || n2 > 256 || !SafeRead((void*)t2, &p2, 8) || p2 <= 0x10000) continue;
                if (GetClassNameFP(p2, c3, sizeof(c3)) && !strncmp(c3, "Frostpunk", 9))
                    VisiterTableau(r + o2, prof + 1, srcR);
            }
        }
    }
}

void ReleverSite(uintptr_t comp)
{
    uintptr_t tpl = 0;
    int32_t etat = -1, dilRetenu = -1;
    int32_t variante[2] = { 0, -1 };
    SafeRead((void*)(comp + 0x20), &tpl, 8);
    SafeRead((void*)(comp + 0x34), &etat, 4);
    SafeRead((void*)(comp + SITE_VARIANTE), variante, 8);
    SafeRead((void*)(comp + SITE_DIL_RETENU), &dilRetenu, 4);
    static char nom[128];
    strcpy_s(nom, "?");
    ScootingNomDuGabarit(tpl, nom, sizeof(nom));
    g_site = nom;
    g_nVus = 0;

    uintptr_t dils = 0, inst = 0; uint32_t nDil = 0, nInst = 0;
    LireTab(tpl + GAB_DILEMMES, dils, nDil, 64);
    LireTab(tpl + GAB_INSTANTANES, inst, nInst, 64);
    printf("[SCONT]\tS\t%s\t%s\t%d\t%d\t%d\t%u\t%u\n", g_scn, g_site, etat,
        variante[0] ? variante[1] : -1, dilRetenu, nDil, nInst);

    // Contenu courant (partie neuve : tirage deja fait, carte figee).
    LignesContenuComposant(comp + SITE_RES, "composant");
    LignesContenuComposant(comp + SITE_BONUS, "composant bonus");
    LignesSurvivants(comp + SITE_SURVIVANTS, "composant");
    LigneCompte(comp + SITE_TECHS, "composant", "technologies");
    LigneCompte(comp + SITE_ARTEFACTS, "composant", "artefacts");

    // Tresor du gabarit (declaration, avant tirage).
    if (tpl > 0x10000) {
        uintptr_t tr = tpl + GAB_TRESOR;
        LignesMontants(tr + TR_RES, "gabarit");
        LignesMontants(tr + TR_BONUS, "gabarit bonus");
        LignesSurvivants(tr + TR_SURVIVANTS, "gabarit");
        LigneCompte(tr + TR_TECHS, "gabarit", "technologies");
        LigneCompte(tr + TR_ARTEFACTS, "gabarit", "artefacts");
        const int aleaOff[2] = { TR_ALEA_RES, TR_ALEA_BONUS };
        const char* aleaSrc[2] = { "gabarit alea", "gabarit alea bonus" };
        for (int a = 0; a < 2; ++a) {
            uintptr_t t = 0; uint32_t n = 0;
            if (!LireTab(tr + aleaOff[a], t, n, 64)) continue;
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t g[16] = {};
                char r[64];
                SafeRead((void*)(t + (uintptr_t)i * 0x20), g, 16);
                NomRessource(g, r, sizeof(r));
                printf("[SCONT]\tR\t%s\t%s\t%s\t%s\t?\n", g_scn, g_site, aleaSrc[a], r);
            }
        }
        uintptr_t t = 0; uint32_t n = 0;
        if (LireTab(tr + TR_ALEA_SURV, t, n, 64))
            for (uint32_t i = 0; i < n; ++i) {
                uint8_t g[16] = {};
                char cls[64];
                SafeRead((void*)(t + (uintptr_t)i * 0x30), g, 16);
                NomClasseSociale(g, cls, sizeof(cls));
                printf("[SCONT]\tP\t%s\t%s\tgabarit alea\t%s\t?\n", g_scn, g_site, cls);
            }
    }

    // Dilemmes d'exploration (ExplorationDilemma, 0x28).
    for (uint32_t i = 0; i < nDil; ++i) {
        uintptr_t e = dils + (uintptr_t)i * 0x28;
        uint8_t g[16] = {};
        SafeRead((void*)e, g, 16);
        uintptr_t dil = GuidNul(g) ? 0 : DilemmeParGuid(g);
        char dnom[128] = "?", titre[512] = "-", cond[1024] = "";
        uint32_t nChoix = 0;
        uintptr_t tc = 0;
        LoisDecrireExigences(e + 0x10, cond, sizeof(cond));
        if (dil) {
            LireNom(dil, dnom, sizeof(dnom));
            TraduireCleJeu(dil + 0xD8, titre, sizeof(titre));
            LireTab(dil + 0x78, tc, nChoix, 16);
        } else {
            GuidHex(g, dnom + 1, sizeof(dnom) - 1);
        }
        printf("[SCONT]\tD\t%s\t%s\t%u\t%s\t%u\t%s\t%s\n", g_scn, g_site, i, dnom, nChoix, titre, cond[0] ? cond : "-");
        if (dil) VisiterDilemme(dil, 0, "");
    }

    // Effets instantanes d'exploration (0x78 : +0x00 liens d'effets de carte).
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    for (uint32_t i = 0; i < nInst; ++i) {
        uintptr_t e = inst + (uintptr_t)i * 0x78, liens = 0;
        uint32_t nl = 0;
        char cond[1024] = "", src[64];
        LoisDecrireExigences(e + 0x18, cond, sizeof(cond));
        sprintf_s(src, "instantane %u", i);
        printf("[SCONT]\tE\t%s\t%s\t%s\t0\t(conditions)\t%s\n", g_scn, g_site, src, cond[0] ? cond : "-");
        if (!LireTab(e, liens, nl, 32)) continue;
        for (uint32_t k = 0; k < nl; ++k) {
            uint8_t g[16] = {};
            SafeRead((void*)(liens + (uintptr_t)k * 0x10), g, 16);
            uintptr_t w = EntreeParLien(base + OFF_EFFETS_CARTE, g);
            char wnom[128] = "?", s2[256];
            if (w) LireNom(w, wnom, sizeof(wnom));
            else GuidHex(g, wnom + 1, sizeof(wnom) - 1);
            sprintf_s(s2, "%s %s", src, wnom);
            printf("[SCONT]\tE\t%s\t%s\t%s\t0\t(effet de carte)\t%s\n", g_scn, g_site, s2, w ? "" : "non resolu");
            if (w) VisiterTableau(w + 0x18, 1, s2);
        }
    }
}

// Ressources de l'economie : nom interne, GUID, nom affiche (verification
// des noms mod de Last Autumn).
void ReleverRessources()
{
    uintptr_t eco = 0, arr = 0;
    uint32_t n = 0;
    if (!SafeRead((void*)((uintptr_t)GetModuleHandleW(NULL) + OFF_ECONOMIE), &eco, 8) || eco <= 0x10000 ||
        !LireTab(eco + ECO_RESSOURCES, arr, n, 64)) {
        printf("[SRES] economie illisible\n");
        return;
    }
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t def = 0;
        uint8_t g[16] = {};
        char nom[64] = "?", hex[40], aff[256];
        if (!SafeRead((void*)(arr + (uintptr_t)i * ECO_PAS), &def, 8) || def <= 0x10000) continue;
        LireNom(def, nom, sizeof(nom));
        SafeRead((void*)(def + 8), g, 16);
        GuidHex(g, hex, sizeof(hex));
        TraduireCleJeu(def + 0x28, aff, sizeof(aff));
        printf("[SRES]\t%s\t%s\t%s\t%s\n", g_scn, nom, hex, aff);
    }
}

} // namespace

void SitesContenuReleve()
{
    static uintptr_t objs[512];
    g_scn = g_run.valid ? g_run.scenario : "?";
    ReleverRessources();
    int n = ComposantsDuSysteme(IdSysteme::ExpeditionSite, "FrostpunkExpeditionSiteComponent", objs, 512);
    int ns = 0;
    for (int i = 0; i < n; ++i) {
        if (!ClasseExacte(objs[i], "FrostpunkExpeditionSiteComponent")) continue;
        ReleverSite(objs[i]);
        ++ns;
    }
    printf("[SCONT] %d site(s) releve(s) (scenario %s)\n", ns, g_scn);
}

// Entree d'une EntryCollection par nom (FrostpunkPopulationConfig, ctor
// FUN_141440690 : collection = vtable + 3 champs, puis tableau +0x20, compte
// +0x28 -- SUPPOSE). Entree : +0x00 nom, lien (GUID) suppose a +0x08 ou
// +0x10, retenu seulement si la recherche par lien du jeu rend la meme
// entree. 'nom' : egal, ou prefixe de 5 lettres si 'prefixe'.
static bool EntreeCollectionParNom(uintptr_t coll, const char* quoi, const char* nom, bool prefixe,
                                   uint8_t guid[16])
{
    uintptr_t tab = 0;
    uint32_t n = 0;
    if (!LireTab(coll + 0x20, tab, n, 256) || !n) {
        uint8_t brut[0x40] = {};
        SafeRead((void*)coll, brut, sizeof(brut));
        printf("[SCONT] collection %s : tableau illisible, octets", quoi);
        for (int i = 0; i < 0x40; ++i) printf("%s%02X", i % 8 ? "" : " ", brut[i]);
        printf("\n");
        return false;
    }
    char liste[512] = "";
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t e = 0;
        char nomE[64];
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000 || !LireNom(e, nomE, sizeof(nomE))) continue;
        if (strlen(liste) + strlen(nomE) + 3 < sizeof(liste)) { strcat_s(liste, liste[0] ? ", " : ""); strcat_s(liste, nomE); }
        bool ok = prefixe ? _strnicmp(nomE, nom, 5) == 0 : _stricmp(nomE, nom) == 0;
        if (!ok) continue;
        static const int DECALAGES[] = { 0x08, 0x10 };
        for (int off : DECALAGES) {
            uint8_t g[16] = {};
            if (SafeRead((void*)(e + off), g, 16) && EntreeParLien(coll, g) == e) {
                memcpy(guid, g, 16);
                printf("[SCONT] collection %s : \"%s\" -> \"%s\" (lien a +0x%X)\n", quoi, nom, nomE, off);
                return true;
            }
        }
        printf("[SCONT] collection %s : \"%s\" trouve mais lien non verifie\n", quoi, nomE);
    }
    printf("[SCONT] collection %s : \"%s\" absent (%u entree(s) : %s)\n", quoi, nom, n, liste);
    return false;
}

// Statistiques initiales "malade" (pas "gravement malade") pour les groupes AP
// (demande de l'auteur, 2026-11-04 : les groupes arrivaient gravement
// malades, statistiques copiees d'un site). FrostpunkCitizenInitialStatistics
// Entry (0xA8, reflexion FUN_141446850) : +0x00 nom, +0x7C "Health level"
// (lien vers FrostpunkCitizenHealthLevel). Les noms des entrees ne sont pas
// connus a l'avance : toutes sont journalisees la 1re fois ([SCONT] stats ...).
// Releve du 2026-11-04 (17 entrees) : "SurvivorsIll" (sante 7D9B74C7...) =
// malade ; "SurvivorsGravelyIll" et "[Reconquest] Ex-Convicts sick" (sante
// A942714D..., la meme) = gravement malade (1er essai : la recherche de
// "sick" avait retenu Ex-Convicts sick, a tort). On retient donc
// "SurvivorsIll" par son nom.
// 2026-11-06 (l'auteur) : TOUT le groupe arrivait malade ; seuls les
// "gravement malades" doivent devenir "malades", les autres gardent leur
// etat. StatsSansGravite remplace donc les statistiques d'une entree
// seulement si leur niveau de sante (+0x7C) est celui de SurvivorsGravelyIll.

// Lien (GUID) d'une entree de la collection des statistiques ; 0 si absent.
static uintptr_t StatsParNom(const char* nom, uint8_t guid[16])
{
    uintptr_t coll = (uintptr_t)GetModuleHandleW(NULL) + OFF_STATS_INITIALES, tab = 0;
    uint32_t n = 0;
    if (!LireTab(coll + 0x20, tab, n, 256) || !n) return 0;
    for (uint32_t i = 0; i < n; ++i) {
        uintptr_t e = 0;
        char nomE[64];
        if (!SafeRead((void*)(tab + (uintptr_t)i * 8), &e, 8) || e <= 0x10000 || !LireNom(e, nomE, sizeof(nomE))) continue;
        if (_stricmp(nomE, nom) != 0) continue;
        static const int DECALAGES[] = { 0x08, 0x10 };
        for (int off : DECALAGES) {
            uint8_t g[16] = {};
            if (SafeRead((void*)(e + off), g, 16) && EntreeParLien(coll, g) == e) { memcpy(guid, g, 16); return e; }
        }
    }
    return 0;
}

static void StatsSansGravite(const char* classe, uint8_t stats[16])
{
    uintptr_t coll = (uintptr_t)GetModuleHandleW(NULL) + OFF_STATS_INITIALES;
    uint8_t gGrave[16], gMalade[16], santeGrave[16] = {}, santeEntree[16] = {};
    uintptr_t grave = StatsParNom("SurvivorsGravelyIll", gGrave), malade = StatsParNom("SurvivorsIll", gMalade);
    uintptr_t e = EntreeParLien(coll, stats);
    if (!grave || !malade || !e) {
        printf("[SCONT] classe \"%s\" : sante non verifiee (grave 0x%llX, malade 0x%llX, entree 0x%llX)\n",
            classe, (unsigned long long)grave, (unsigned long long)malade, (unsigned long long)e);
        return;
    }
    SafeRead((void*)(grave + 0x7C), santeGrave, 16);
    SafeRead((void*)(e + 0x7C), santeEntree, 16);
    char nom[64] = "?";
    LireNom(e, nom, sizeof(nom));
    if (memcmp(santeGrave, santeEntree, 16) == 0) {
        memcpy(stats, gMalade, 16);
        printf("[SCONT] classe \"%s\" : \"%s\" (gravement malade) -> \"SurvivorsIll\"\n", classe, nom);
    }
    else printf("[SCONT] classe \"%s\" : statistiques \"%s\" gardees\n", classe, nom);
}

// Entree de survivants d'une classe sociale, prise dans les sites de la carte
// (composant +0xA8, tresor du gabarit +0x70, survivants aleatoires +0xD8) ;
// classe et statistiques retenues par nom de classe pour toute la session
// (liens globaux de la configuration).
bool SitesEntreeSurvivants(const char* classeAP, uint8_t entree[0x24], const char* site)
{
    // Noms de l'apworld -> noms internes des classes sociales (inverse de
    // SURVIVANTS d'outils\sites_contenu.py ; collection relevee le
    // 2026-11-04 : Workers, [Builders] Convicts, Automaton, Specialists,
    // [Reconquest] Administrators, Children).
    static const struct { const char* ap; const char* jeu; } NOMS[] = {
        { "workers", "Workers" }, { "engineers", "Specialists" },
        { "children", "Children" }, { "automaton", "Automaton" },
    };
    const char* classe = classeAP;
    for (const auto& n : NOMS) if (_stricmp(n.ap, classeAP) == 0) classe = n.jeu;

    struct Connue { char nom[32]; uint8_t e[0x24]; };
    static Connue connues[8];
    static int nConnues = 0;

    // Une entree candidate (classe +0x00, statistiques +0x10) : retenue si
    // sa classe s'appelle 'classe'. 'garder' : mise en cache (recherche
    // generale ; pas pour le site d'origine, propre a chaque item).
    bool garder = true;
    auto essayer = [&](const uint8_t* cls, const uint8_t* stats) -> bool {
        char nom[64];
        NomClasseSociale(cls, nom, sizeof(nom));
        if (_stricmp(nom, classe) != 0) return false;
        memset(entree, 0, 0x24);
        memcpy(entree + 0x04, cls, 16);
        memcpy(entree + 0x14, stats, 16);
        if (garder && nConnues < 8) {
            strcpy_s(connues[nConnues].nom, classe);
            memcpy(connues[nConnues].e, entree, 0x24);
            ++nConnues;
        }
        StatsSansGravite(classe, entree + 0x14);   // gravement malade -> malade
        printf("[SCONT] classe sociale \"%s\" trouvee dans les sites de la carte\n", classe);
        return true;
    };

    static uintptr_t objs[512];
    int n = ComposantsDuSysteme(IdSysteme::ExpeditionSite, "FrostpunkExpeditionSiteComponent", objs, 512);

    // 1. Site d'origine du contenu (2026-11-06) : SES survivants (composant
    //    +0xA8 jusqu'a sa capacite, son compte etant remis a 0 par scooting ;
    //    tresor du gabarit +0x70 ; survivants aleatoires +0xD8).
    if (site && *site) {
        garder = false;
        for (int i = 0; i < n; ++i) {
            uintptr_t gab = 0, tab = 0;
            char nomSite[128] = "";
            if (!ClasseExacte(objs[i], "FrostpunkExpeditionSiteComponent") ||
                !SafeRead((void*)(objs[i] + 0x20), &gab, 8) || gab <= 0x10000 ||
                !ScootingNomDuGabarit(gab, nomSite, sizeof(nomSite)) || _stricmp(nomSite, site) != 0) continue;
            uint8_t e[0x30];
            uint32_t k = 0, cap = 0;
            if (SafeRead((void*)(objs[i] + SITE_SURVIVANTS), &tab, 8) &&
                SafeRead((void*)(objs[i] + SITE_SURVIVANTS + 12), &cap, 4) && tab > 0x10000 && cap <= 64)
                for (uint32_t j = 0; j < cap; ++j)
                    if (SafeRead((void*)(tab + (uintptr_t)j * 0x24), e, 0x24) && essayer(e + 0x04, e + 0x14)) return true;
            if (LireTab(gab + GAB_TRESOR + TR_SURVIVANTS, tab, k, 64))
                for (uint32_t j = 0; j < k; ++j)
                    if (SafeRead((void*)(tab + (uintptr_t)j * 0x24), e, 0x24) && essayer(e + 0x04, e + 0x14)) return true;
            if (LireTab(gab + GAB_TRESOR + TR_ALEA_SURV, tab, k, 64))
                for (uint32_t j = 0; j < k; ++j)
                    if (SafeRead((void*)(tab + (uintptr_t)j * 0x30), e, 0x30) && essayer(e, e + 0x10)) return true;
            break;
        }
        printf("[SCONT] classe \"%s\" absente du site \"%s\" : recherche generale\n", classe, site);
        garder = true;
    }

    for (int c = 0; c < nConnues; ++c)
        if (_stricmp(connues[c].nom, classe) == 0) {
            memcpy(entree, connues[c].e, 0x24);
            StatsSansGravite(classe, entree + 0x14);
            return true;
        }

    for (int i = 0; i < n; ++i) {
        if (!ClasseExacte(objs[i], "FrostpunkExpeditionSiteComponent")) continue;
        uintptr_t gab = 0, tab = 0;
        uint32_t k = 0;
        uint8_t e[0x30];
        // Survivants du composant, puis du tresor (0x24 : classe +0x04, stats +0x14).
        uintptr_t listes[2] = { objs[i] + SITE_SURVIVANTS, 0 };
        if (SafeRead((void*)(objs[i] + 0x20), &gab, 8) && gab > 0x10000)
            listes[1] = gab + GAB_TRESOR + TR_SURVIVANTS;
        for (uintptr_t adr : listes) {
            if (!adr || !LireTab(adr, tab, k, 64)) continue;
            for (uint32_t j = 0; j < k; ++j)
                if (SafeRead((void*)(tab + (uintptr_t)j * 0x24), e, 0x24) && essayer(e + 0x04, e + 0x14)) return true;
        }
        // Survivants aleatoires du tresor (0x30 : classe +0x00, stats +0x10).
        if (gab > 0x10000 && LireTab(gab + GAB_TRESOR + TR_ALEA_SURV, tab, k, 64))
            for (uint32_t j = 0; j < k; ++j)
                if (SafeRead((void*)(tab + (uintptr_t)j * 0x30), e, 0x30) && essayer(e, e + 0x10)) return true;
    }
    printf("[SCONT] classe sociale \"%s\" absente des sites de la carte (%d site(s))\n", classe, n);

    // Repli : collections globales de la configuration de population
    // (classes base + 0x4072078, statistiques initiales base + 0x4072140).
    // Statistiques : entree dont le nom commence comme la classe, sinon
    // celles d'une classe deja trouvee (jamais de lien nul).
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uint8_t cls[16] = {}, stats[16] = {};
    if (!EntreeCollectionParNom(base + OFF_CLASSES_SOCIALES, "classes", classe, false, cls)) return false;
    if (!EntreeCollectionParNom(base + OFF_STATS_INITIALES, "statistiques", classe, true, stats)) {
        if (!nConnues) {
            printf("[SCONT] classe \"%s\" : aucune statistique initiale utilisable\n", classe);
            return false;
        }
        memcpy(stats, connues[0].e + 0x14, 16);
        printf("[SCONT] classe \"%s\" : statistiques de \"%s\" reprises\n", classe, connues[0].nom);
    }
    return essayer(cls, stats);
}
