#include "scooting.h"
#include "mod_common.h"
#include "scenario.h"
#include "tech_brain.h"     // OnProgressiveReceived
#include "ap_data.h"
#include "ap_ressources.h"
#include "archipelago.h"
#include "quetes.h"         // TraduireCleJeu
#include "lois_ap.h"        // icones AP des marqueurs de site
#include "lois.h"           // LoisDecrireExigences (conditions de tirage)
#include <intrin.h>
#include <algorithm>
#include <cmath>

// ============================================================================
//  Etat interne
// ============================================================================

static const int MAX_EN_ATTENTE = 8;

struct ExpCache {
    uintptr_t comp;
    int32_t   id;
    uintptr_t dernierLieu;
    uintptr_t dernierTarget;
    uintptr_t dernierSite;       // dernier lieu non nul, pour le chargement differe
    uint32_t  derniereCargo;
    bool      valide;
    const SitePayload* enAttente[MAX_EN_ATTENTE];
    int                nEnAttente;
};

struct SiteCache {
    uintptr_t comp;
    uintptr_t bloc;
    char      nom[128];
    int32_t   dernierEtat;
    int8_t    carteDecouvert;    // WorldMapEntityComponent +0x30 au dernier tick (-1 = inconnu)
    int8_t    carteVisible;      // +0x48
};

static ExpCache  g_exps[16];
static int       g_nExps = 0;
// Le nombre de sites depend du scenario (42 dans celui de test, davantage
// ailleurs) : marge large.
static const int MAX_SITES = 256;
static SiteCache g_sites[MAX_SITES];
static int       g_nSites = 0;
// Compte de la liste du systeme Expedition (+0x2080) lors de la derniere
// relecture : s'il change, une expedition est apparue ou a disparu.
static uint32_t g_compteListeExp = 0;
static uintptr_t g_blocVille = 0;
volatile bool      g_pret = false;


struct GabaritCache { uintptr_t tpl; char nom[128]; };

static bool g_persistChargee = false;
// ============================================================================
//  Table des contenus de sites
// ============================================================================



// Tous les sites qui sont des locations AP (2026-11-04, demande de l'auteur ;
// remplace les 4 sites de test 1001-1004) : construite au demarrage par
// ScootingPayloadsInit depuis known_ap_ids.h, jamais reconstruite (les
// enAttente des expeditions pointent dedans). locationId = id local stable
// = 100000 + numero de la location AP (7747300000 + n), garde dans
// ap_pending_<GameId>.txt.
static const int   MAX_PAYLOADS = 512;
static SitePayload SITE_PAYLOADS[MAX_PAYLOADS];
static int         SITE_PAYLOAD_COUNT = 0;

void ScootingPayloadsInit()
{
    static ApSiteLoc locs[MAX_PAYLOADS];
    int n = ApSitesLocations(locs, MAX_PAYLOADS);
    SITE_PAYLOAD_COUNT = 0;
    for (int i = 0; i < n; ++i) {
        int id = 100000 + (int)(locs[i].id - 7747300000LL);
        SITE_PAYLOADS[SITE_PAYLOAD_COUNT++] = { locs[i].scenario, locs[i].cle, id };
        // Texte de repli = nom de la location ; des que le scout est connu,
        // ApCheckById affiche "<joueur> - <item>" (ap_data.cpp).
        ApSetCheck(id, "", locs[i].nom);
    }
    printf("[SCOUT] %d site(s) AP (locations de known_ap_ids.h)\n", SITE_PAYLOAD_COUNT);
}

static wchar_t g_listePortee[512];
static int     g_nListePortee = 0;

static uint64_t  g_tamponsInj[16][2 * 4];
static uintptr_t g_proprioInj[16] = {};

// Systeme de selection, resolu une fois a l'init.
static uintptr_t g_selSys = 0;
volatile bool g_apDonneesActif = true;
// ============================================================================
//  Utilitaires
// ============================================================================


// Filtre exact : CollectObjectsByClass utilise StrContains, qui attraperait
// aussi les ...ComponentTemplate.

static bool EstSiteComponent(uintptr_t obj)
{
    return ClasseExacte(obj, "FrostpunkExpeditionSiteComponent");
}

static bool EstExpeditionComponent(uintptr_t obj)
{
    return ClasseExacte(obj, "FrostpunkExpeditionComponent");
}

// "@Generated/.../ExpeditionSites/Story/Site_013_Observatory_Ruins/..."
//  -> "Site_013_Observatory_Ruins"
static bool NomDeSite(const char* chemin, char* out, int maxlen)
{
    out[0] = 0;
    const char* marqueur = "ExpeditionSites/";
    const char* p = nullptr;
    for (int i = 0; chemin[i]; ++i) {
        int j = 0;
        while (marqueur[j] && chemin[i + j] == marqueur[j]) j++;
        if (!marqueur[j]) { p = chemin + i + j; break; }
    }
    if (!p) return false;
    while (*p && *p != '/') p++;          // saute le dossier (Story, _SHARED...)
    if (*p == '/') p++;
    int k = 0;
    for (; p[k] && p[k] != '/' && k < maxlen - 1; ++k) out[k] = p[k];
    out[k] = 0;
    return k > 0;
}

typedef uintptr_t(*Infobulle_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static Infobulle_t oInfobulle = nullptr;

// La cle "Generated/Templates/.../ExpeditionSites/Refugees/[ref]Builder_Camp/..."
// designe-t-elle un site configure ? Renvoie sa location, ou -1.

// Liste des items portes par l'expedition SELECTIONNEE, un par ligne.
// Sans expedition selectionnee on ne renvoie rien : c'est ce qui permet a
// l'appelant de basculer sur l'affichage du site.
// Lecture seule, sans le moindre acces memoire au jeu.
int ScootingListePortee(wchar_t* out, int maxChars)
{
    if (!out || maxChars < 2) return 0;
    int j = 0;
    for (; g_listePortee[j] && j < maxChars - 1; ++j) out[j] = g_listePortee[j];
    out[j] = 0;
    return g_nListePortee;
}


static bool DejaEnAttente(const ExpCache& e, const SitePayload* p)
{
    for (int k = 0; k < e.nEnAttente; ++k)
        if (e.enAttente[k] == p) return true;
    return false;
}

// Paire exclusive ([ref]X / [ref]X-dead) : les deux sites renvoient le meme
// contenu, donc un seul check (ApCleDeSite).
static const SitePayload* PayloadDuSite(const char* site)
{
    if (!site || !site[0] || !g_run.valid) return nullptr;
    char cle[128];
    ApCleDeSite(site, cle, sizeof(cle));
    for (int i = 0; i < SITE_PAYLOAD_COUNT; ++i) {
        if (!StrEq(SITE_PAYLOADS[i].scenario, g_run.scenario)) continue;
        if (!StrEq(SITE_PAYLOADS[i].site, cle)) continue;
        return &SITE_PAYLOADS[i];
    }
    return nullptr;
}





// Un gabarit porte un nom par etat ; on prend le premier lisible.
static bool NomDuGabarit(uintptr_t tpl, char* out, int maxlen)
{
    out[0] = 0;
    if (tpl <= 0x10000) return false;
    const int offs[4] = { 0x270, 0x268, 0x278, 0x280 };
    char brut[256];
    for (int i = 0; i < 4; ++i) {
        uintptr_t p = 0;
        if (!SafeRead((void*)(tpl + offs[i]), &p, 8) || p <= 0x10000) continue;
        brut[0] = 0;
        if (!SafeRead((void*)p, brut, sizeof(brut) - 1)) continue;
        if (brut[0] < 0x20 || brut[0] > 0x7E) continue;
        if (NomDeSite(brut, out, maxlen)) return true;
    }
    return false;
}

bool ScootingNomDuGabarit(uintptr_t tpl, char* out, int maxlen)
{
    return NomDuGabarit(tpl, out, maxlen);
}

// Le site nomme porte-t-il un contenu configure pour le scenario en cours ?


static void ViderCargaison(uintptr_t comp)
{
    const int offs[3] = { 0x180, 0x168, 0x198 };
    for (int i = 0; i < 3; ++i) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(comp + offs[i]), &arr, 8)) continue;
        if (!SafeRead((void*)(comp + offs[i] + 8), &cnt, 4)) continue;
        uint32_t zero = 0;
        if (arr > 0x10000 && cnt > 0 && cnt <= 64)
            SafeWrite((void*)(comp + offs[i] + 8), &zero, 4);

        // Si le tableau vient de nous, on le retire completement : le jeu
        // ne doit jamais detenir notre pointeur quand il libere l'expedition.
        if (offs[i] == 0x168 &&
            arr >= (uintptr_t)&g_tamponsInj[0][0] &&
            arr <  (uintptr_t)&g_tamponsInj[16][0]) {
            uintptr_t nul = 0;
            SafeWrite((void*)(comp + 0x168),      &nul,  8);
            SafeWrite((void*)(comp + 0x168 + 12), &zero, 4);
        }
    }
}

// ---------------------------------------------------------------------------
// Visibilite des sites sur la carte du monde (2026-11-03, pour rattacher
// chaque site a la region logique de l'apworld ou il devient visible).
// Decompiles du 2026-11-03 :
//   FrostpunkWorldMapEntityComponent (reflexion FUN_14197cbd0, 0x78) :
//   +0x30 Discovered (bool), +0x38 DiscoveryTime, +0x48 Visible (bool),
//   +0x4C SquaredDistanceToCity (float), +0x50 DisabledResons.
//   Type du composant : FUN_14132fd90() ; composant = *(entite + 0x2A0 +
//   type x 8).
//   FrostpunkWorldMapEntityComponentTemplate (FUN_14197d2f0, 0x30) :
//   +0x28 "Initially revealed", +0x29 "Revealed in sight range".
//   FrostpunkExpeditionSiteComponent (FUN_141a157b0) : +0xE0
//   AdditionalConnections, +0xF8 "Reveals" (SafePtrDynarray<Entity> : sites
//   reveles par l'exploration de celui-ci), +0xD8 SiteInspected.
//   Effets : FrostpunkRevealWorldMapEntityEffect (0x18, sans champ : revele
//   l'entite du contexte), FrostpunkAddSiteRevealsEffect (+0x18 "Sites to
//   reveal templates", GUID de gabarits au pas de 0x10).
// ---------------------------------------------------------------------------
constexpr uintptr_t OFF_TYPE_ENTITE_CARTE = 0x132FD90;
constexpr int WM_DECOUVERT = 0x30, WM_VISIBLE = 0x48, WM_DIST2 = 0x4C;
constexpr int WMT_REVELE_INIT = 0x28, WMT_REVELE_VUE = 0x29;
constexpr int SITE_REVELE = 0xF8;

static uintptr_t ComposantCarteDuSite(uintptr_t compSite)
{
    static int type = -1;
    if (type == -1) {
        uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
        __try { type = ((int(*)())(base + OFF_TYPE_ENTITE_CARTE))(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { type = -2; }
        printf("[SREV] type du composant de carte : %d\n", type);
    }
    if (type < 0 || type >= 512) return 0;
    uintptr_t ent = 0, wm = 0;
    if (!SafeRead((void*)(compSite + 0x18), &ent, 8) || ent <= 0x10000) return 0;
    if (!SafeRead((void*)(ent + 0x2A0 + (uintptr_t)type * 8), &wm, 8) || wm <= 0x10000) return 0;
    return ClasseExacte(wm, "FrostpunkWorldMapEntityComponent") ? wm : 0;
}

// Gabarit du composant de carte : cherche dans les premiers qwords (+0x20
// presume, comme le composant de site).
static uintptr_t GabaritCarte(uintptr_t wm)
{
    for (int off = 0x08; off <= 0x40; off += 8) {
        uintptr_t t = 0;
        if (SafeRead((void*)(wm + off), &t, 8) && t > 0x10000 &&
            ClasseExacte(t, "FrostpunkWorldMapEntityComponentTemplate")) return t;
    }
    return 0;
}

// Trace en jeu : chaque site qui devient decouvert ou visible (et
// l'inverse). Lecture seule, thread du mod.
static void SurveillerVisibilite()
{
    for (int i = 0; i < g_nSites; ++i) {
        uintptr_t wm = ComposantCarteDuSite(g_sites[i].comp);
        if (!wm) continue;
        uint8_t dec = 0, vis = 0;
        if (!SafeRead((void*)(wm + WM_DECOUVERT), &dec, 1) || !SafeRead((void*)(wm + WM_VISIBLE), &vis, 1)) continue;
        int8_t d = dec ? 1 : 0, v = vis ? 1 : 0;
        if (g_sites[i].carteDecouvert >= 0 && (d != g_sites[i].carteDecouvert || v != g_sites[i].carteVisible))
            printf("[SREV] CHANGE \"%s\" (%s) : decouvert %d -> %d, visible %d -> %d\n", g_sites[i].nom,
                g_run.valid ? g_run.scenario : "?", g_sites[i].carteDecouvert, d, g_sites[i].carteVisible, v);
        g_sites[i].carteDecouvert = d;
        g_sites[i].carteVisible = v;
    }
}

static void SurveillerSites()
{
    if (!g_apDonneesActif) return;

    uintptr_t defAP = ApPorteurDef();

    for (int i = 0; i < g_nSites; ++i) {
        const SitePayload* p = PayloadDuSite(g_sites[i].nom);

        // --- contenu : le jeu cree le tableau Resources a la generation du
        //     site, donc apres notre init. On le corrige des qu'il apparait,
        //     et le test sur la definition fait que l'ecriture n'a lieu
        //     qu'une seule fois par site.
        if (p && defAP) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            if (SafeRead((void*)(g_sites[i].comp + 0x48), &arr, 8) &&
                SafeRead((void*)(g_sites[i].comp + 0x48 + 8), &cnt, 4) &&
                SafeRead((void*)(g_sites[i].comp + 0x48 + 12), &cap, 4)) {

                if (arr > 0x10000 && cnt > 0) {
                    uintptr_t def0 = 0;
                    SafeRead((void*)arr, &def0, 8);
                    if (def0 != defAP) {
                        int64_t un64 = 1; uint32_t un = 1;
                        SafeWrite((void*)arr, &defAP, 8);
                        SafeWrite((void*)(arr + 8), &un64, 8);
                        SafeWrite((void*)(g_sites[i].comp + 0x48 + 8), &un, 4);
                        printf("[SITE] %s : contenu substitue\n", g_sites[i].nom);
                    }
                }
                else if (arr <= 0x10000 && MoteurAlloc) {
                    // Site sans tableau : on en fournit un, alloue par le
                    // moteur pour qu'il puisse le liberer lui-meme.
                    uintptr_t bloc = MoteurAlloc(4 * 0x10);
                    if (bloc) {
                        uint64_t vide = 0;
                        for (int z = 0; z < 8; ++z)
                            SafeWrite((void*)(bloc + z * 8), &vide, 8);
                        int64_t un64 = 1;
                        SafeWrite((void*)bloc, &defAP, 8);
                        SafeWrite((void*)(bloc + 8), &un64, 8);

                        uint32_t un = 1, quatre = 4;
                        SafeWrite((void*)(g_sites[i].comp + 0x48), &bloc, 8);
                        SafeWrite((void*)(g_sites[i].comp + 0x48 + 8), &un, 4);
                        SafeWrite((void*)(g_sites[i].comp + 0x48 + 12), &quatre, 4);
                        printf("[SITE] %s : tableau fourni a 0x%llX\n",
                            g_sites[i].nom, (unsigned long long)bloc);
                    }
                }
            }
        }

        // --- exploration : passage de l'etat 0 a un etat visite ---
        int32_t etat = -1;
        if (!SafeRead((void*)(g_sites[i].comp + 0x34), &etat, 4)) continue;

        int32_t avant = g_sites[i].dernierEtat;
        g_sites[i].dernierEtat = etat;
        if (avant != 0 || etat == 0) continue;
        if (!p) continue;

        printf("[SCOUT] site %s explore (etat %d -> %d)\n",
            g_sites[i].nom, avant, etat);

        for (int e = 0; e < g_nExps; ++e) {
            if (!g_exps[e].valide) continue;
            uintptr_t loc = 0;
            if (!SafeRead((void*)(g_exps[e].comp + 0x120), &loc, 8)) continue;
            if (loc != g_sites[i].bloc) continue;

            if (DejaEnAttente(g_exps[e], p)) {
                printf("[SCOUT] exp %d : location %d deja en attente\n",
                    g_exps[e].id, p->locationId);
                break;
            }
            if (g_exps[e].nEnAttente >= MAX_EN_ATTENTE) {
                printf("[SCOUT] exp %d : file pleine, location perdue\n", g_exps[e].id);
                break;
            }

            g_exps[e].enAttente[g_exps[e].nEnAttente++] = p;
            const ApCheck* ac = ApCheckById(p->locationId);
            printf("[SCOUT] exp %d ramasse la location %d (%s)  total=%d\n",
                g_exps[e].id, p->locationId,
                ac ? ac->description : "contenu inconnu du serveur",
                g_exps[e].nEnAttente);

            if (defAP && InjecterPorteur(g_exps[e].comp, defAP, g_exps[e].nEnAttente))
                g_exps[e].derniereCargo = (uint32_t)g_exps[e].nEnAttente;

            ScootingSauverEnAttente();
            break;
        }
    }
}
static void RecenserSites()
{
    g_nSites = 0;
    static uintptr_t objs[512];
    int n = ComposantsDuSysteme(IdSysteme::ExpeditionSite,
        "FrostpunkExpeditionSiteComponent", objs, 512);

    for (int i = 0; i < n && g_nSites < MAX_SITES; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t ent = 0, bloc = 0;
        if (!SafeRead((void*)(objs[i] + 0x18), &ent, 8) || ent <= 0x10000) continue;
        if (!SafeRead((void*)(ent + 0x08), &bloc, 8) || bloc <= 0x10000) continue;

        SiteCache& s = g_sites[g_nSites];
        s.comp = objs[i]; s.bloc = bloc; s.nom[0] = 0;
        s.dernierEtat = -1;
        s.carteDecouvert = s.carteVisible = -1;
        SafeRead((void*)(objs[i] + 0x34), &s.dernierEtat, 4);   // etat courant : pas de faux positif
        uintptr_t tpl = 0;
        SafeRead((void*)(objs[i] + 0x20), &tpl, 8);
        NomDuGabarit(tpl, s.nom, sizeof(s.nom));
        g_nSites++;
    }
    printf("[SCOUT] recensement : %d site(s)\n", g_nSites);
}

static const char* NomDuBloc(uintptr_t bloc)
{
    if (bloc <= 0x10000) return nullptr;

    if (g_blocVille && bloc == g_blocVille) return "VILLE";

    for (int i = 0; i < g_nSites; ++i)
        if (g_sites[i].bloc == bloc) return g_sites[i].nom;

    static DWORD prochain = 0;
    if (GetTickCount() > prochain) {
        prochain = GetTickCount() + 10000;
        printf("[SCOUT] lieu inconnu 0x%llX -- recensement\n", (unsigned long long)bloc);
        RecenserSites();
        ScootingAppliquerTresors();
        for (int i = 0; i < g_nSites; ++i)
            if (g_sites[i].bloc == bloc) return g_sites[i].nom;
    }
    return "(avant-poste ou autre)";
}




// Remplace toute la cargaison par une seule ressource porteuse.
// Element de CarriedResources : {pointeur def (8 o), quantite (8 o)}.
bool InjecterPorteur(uintptr_t comp, uintptr_t def, int64_t quantite)
{
    if (def <= 0x10000) return false;

    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
    if (!SafeRead((void*)(comp + 0x168), &arr, 8)) return false;
    if (!SafeRead((void*)(comp + 0x168 + 8), &cnt, 4)) return false;
    if (!SafeRead((void*)(comp + 0x168 + 12), &cap, 4)) return false;

    // Tableau absent : on fournit le notre. Un par expedition, jamais libere.
    // Il est retire des que la cargaison est videe, pour que le jeu ne le
    // detienne jamais au moment ou il pourrait le liberer.
    
    if (arr <= 0x10000 || cap < 1) {
        int slot = -1;
        for (int i = 0; i < 16; ++i) {
            if (g_proprioInj[i] == comp) { slot = i; break; }
            if (g_proprioInj[i] == 0 && slot < 0) slot = i;
        }
        if (slot < 0) return false;
        g_proprioInj[slot] = comp;
        for (int z = 0; z < 8; ++z) g_tamponsInj[slot][z] = 0;

        arr = (uintptr_t)g_tamponsInj[slot];
        uint32_t quatre = 4;
        if (!SafeWrite((void*)(comp + 0x168), &arr, 8)) return false;
        if (!SafeWrite((void*)(comp + 0x168 + 12), &quatre, 4)) return false;
        printf("[INJ] tampon fourni a 0x%llX\n", (unsigned long long)arr);
    }

    if (!SafeWrite((void*)(arr + 0), &def, 8))      return false;
    if (!SafeWrite((void*)(arr + 8), &quantite, 8)) return false;

    uint32_t un = 1;
    if (!SafeWrite((void*)(comp + 0x168 + 8), &un, 4)) return false;

    uint32_t zero = 0;
    SafeWrite((void*)(comp + 0x180 + 8), &zero, 4);
    SafeWrite((void*)(comp + 0x198 + 8), &zero, 4);
    return true;
}

static bool ExpValide(uintptr_t comp)
{
    if (comp <= 0x10000) return false;
    if (!EstExpeditionComponent(comp)) return false;

    int32_t id = -1;
    if (!SafeRead((void*)(comp + 0x30), &id, 4)) return false;
    if (id < 0 || id > 4096) return false;

    uintptr_t ent = 0;
    if (!SafeRead((void*)(comp + 0x18), &ent, 8)) return false;
    if (ent != 0 && ent <= 0x10000) return false;

    const int lieux[3] = { 0x120, 0x128, 0x130 };
    for (int i = 0; i < 3; ++i) {
        uintptr_t v = 0;
        if (!SafeRead((void*)(comp + lieux[i]), &v, 8)) return false;
        if (v != 0 && v <= 0x10000) return false;
    }

    const int cargos[3] = { 0x168, 0x180, 0x198 };
    for (int i = 0; i < 3; ++i) {
        uint32_t n = 0;
        if (!SafeRead((void*)(comp + cargos[i] + 8), &n, 4)) return false;
        if (n > 64) return false;
    }
    return true;
}

static void RescanExpeditions()
{
    static uintptr_t objs[128];
    int n = ComposantsDuSysteme(IdSysteme::Expedition,
        "FrostpunkExpeditionComponent", objs, 128);

    ExpCache anciens[16];
    int nAnciens = g_nExps;
    for (int i = 0; i < nAnciens; ++i) anciens[i] = g_exps[i];

    g_nExps = 0;
    int rejetes = 0;
    for (int i = 0; i < n && g_nExps < 16; ++i) {
        if (!ExpValide(objs[i])) { rejetes++; continue; }

        ExpCache& e = g_exps[g_nExps];
        e.comp = objs[i];
        e.id = -1;
        SafeRead((void*)(objs[i] + 0x30), &e.id, 4);
        e.dernierLieu = 0; e.dernierTarget = 0; e.dernierSite = 0; e.derniereCargo = 0;
        e.nEnAttente = 0; e.valide = true;

        for (int j = 0; j < nAnciens; ++j) {
            if (anciens[j].comp != e.comp) continue;
            e.dernierLieu = anciens[j].dernierLieu;
            e.dernierTarget = anciens[j].dernierTarget;
            e.dernierSite = anciens[j].dernierSite;
            e.derniereCargo = anciens[j].derniereCargo;
            e.nEnAttente = anciens[j].nEnAttente;
            for (int k = 0; k < e.nEnAttente && k < MAX_EN_ATTENTE; ++k)
                e.enAttente[k] = anciens[j].enAttente[k];
            break;
        }
        g_nExps++;
    }
    printf("[SCOUT] rescan : %d expedition(s) retenue(s), %d rejetee(s)\n",
        g_nExps, rejetes);
}



// Nombre d'entrees de la liste {entite, composant} d'un systeme (+0x2080).
// Pour Expedition : toutes les expeditions, quel que soit le batiment qui
// les a envoyees. 0 si le systeme est absent.
static uint32_t CompteDuSysteme(int idSysteme)
{
    uintptr_t sys = SystemeParId(idSysteme);
    uint32_t cnt = 0;
    if (!sys || !SafeRead((void*)(sys + 0x2080), &cnt, 4) || cnt > 4096) return 0;
    return cnt;
}

void ScootingInit()
{
    g_nSites = 0; g_pret = false; g_blocVille = 0; g_selSys = 0;

    if (!ApPorteurInit()) {
        printf("[SCOUT] porteur non fabrique -- init reportee\n");
        return;
    }

    // Les references globales (systemes, gabarits, entrees de message, technos)
    // sont resolues par PartieTick (mod_common). Ici, seulement les objets du
    // scouting, lus dans les listes {entite, composant} des systemes (+0x2078).
    DWORD t0 = GetTickCount();
    const int MAX_COMP = 512;
    static uintptr_t bufSites[MAX_COMP], bufCity[MAX_COMP], bufExp[MAX_COMP];

    int nSites = ComposantsDuSysteme(IdSysteme::ExpeditionSite,
        "FrostpunkExpeditionSiteComponent", bufSites, MAX_COMP);
    int nCity = ComposantsDuSysteme(IdSysteme::City,
        "FrostpunkCityComponent", bufCity, MAX_COMP);
    int nExp = ComposantsDuSysteme(IdSysteme::Expedition,
        "FrostpunkExpeditionComponent", bufExp, MAX_COMP);
    g_selSys = PartieSelection();

    printf("[SCOUT] ancres : sites=%d city=%d exp=%d sel=%d (%lu ms)\n",
        nSites, nCity, nExp, g_selSys ? 1 : 0, GetTickCount() - t0);

    // --- sites ---
    for (int i = 0; i < nSites && g_nSites < MAX_SITES; ++i) {
        uintptr_t ent = 0, bloc = 0;
        if (!SafeRead((void*)(bufSites[i] + 0x18), &ent, 8) || ent <= 0x10000) continue;
        if (!SafeRead((void*)(ent + 0x08), &bloc, 8) || bloc <= 0x10000) continue;
        SiteCache& s = g_sites[g_nSites];
        s.comp = bufSites[i]; s.bloc = bloc; s.nom[0] = 0;
        s.dernierEtat = -1;
        s.carteDecouvert = s.carteVisible = -1;
        SafeRead((void*)(bufSites[i] + 0x34), &s.dernierEtat, 4);
        uintptr_t tpl = 0;
        SafeRead((void*)(bufSites[i] + 0x20), &tpl, 8);
        NomDuGabarit(tpl, s.nom, sizeof(s.nom));
        g_nSites++;
    }
    if (nSites > MAX_SITES)
        printf("[SCOUT] ATTENTION : %d sites, seuls %d sont suivis (MAX_SITES)\n", nSites, MAX_SITES);

    // --- ville ---
    for (int i = 0; i < nCity; ++i) {
        uintptr_t ent = 0, bloc = 0;
        if (!SafeRead((void*)(bufCity[i] + 0x18), &ent, 8) || ent <= 0x10000) continue;
        if (!SafeRead((void*)(ent + 0x08), &bloc, 8) || bloc <= 0x10000) continue;
        g_blocVille = bloc;
        break;
    }

    // --- expeditions ---
    g_nExps = 0;
    for (int i = 0; i < nExp && g_nExps < 16; ++i) {
        if (!ExpValide(bufExp[i])) continue;
        ExpCache& e = g_exps[g_nExps];
        e.comp = bufExp[i];
        e.id = -1;
        SafeRead((void*)(bufExp[i] + 0x30), &e.id, 4);
        e.dernierLieu = 0; e.dernierTarget = 0; e.dernierSite = 0;
        e.derniereCargo = 0; e.nEnAttente = 0; e.valide = true;
        g_nExps++;
    }
    g_compteListeExp = CompteDuSysteme(IdSysteme::Expedition);

    // Seuls les sites sont indispensables. Les expeditions viennent de la
    // liste du systeme Expedition, vide tant qu'aucune n'existe (vu le
    // 2026-10-24 sur un autre scenario : exp=0) ; le tick suit son compte.
    g_pret = (g_nSites > 0);

    if (g_pret) {
        printf("[SCOUT] init : %d site(s), ville=0x%llX, %d expedition(s) -> pret\n",
            g_nSites, (unsigned long long)g_blocVille, g_nExps);
        CHRONO(ScootingReparerCargaisons());
        g_persistChargee = false;
        return;
    }

    // Diagnostic : sans ces objets le mod ne peut rien faire, autant dire
    // precisement ce qui manque plutot que de repeter "incomplet".
    static int tentatives = 0;
    tentatives++;
    printf("\n[SCOUT] ===== INIT INCOMPLETE (tentative %d) =====\n", tentatives);
    uintptr_t sysSite = SystemeParId(IdSysteme::ExpeditionSite);
    if (g_nSites == 0) {
        if (!sysSite)
            printf("  AUCUN site : global de FrostpunkExpeditionSiteSystem (id 0x%02X) vide\n",
                IdSysteme::ExpeditionSite);
        else if (nSites == 0)
            printf("  AUCUN site : liste +0x2078 de FrostpunkExpeditionSiteSystem vide ou d'une autre classe\n");
        else
            printf("  AUCUN site : %d composant(s) lu(s) mais aucun avec entite (+0x18) et bloc (+0x08) valides\n",
                nSites);
    }
    if (g_nExps == 0)
        printf("  aucune expedition active (normal en debut de partie) -- %d composant(s) dans la liste\n", nExp);
    if (g_blocVille == 0)
        printf("  ville introuvable : %d FrostpunkCityComponent dans la liste de FrostpunkCitySystem\n", nCity);
    printf("[SCOUT] ============================================\n\n");
}
static void OublierPartie()
{
    g_pret = false;
    g_blocVille = 0;
    g_nExps = 0;
    g_nSites = 0;
    g_compteListeExp = 0;
    g_persistChargee = false;
}

void ScootingTick()
{
    // --- cycle de vie : PartieTick (mod_common) detecte les chargements et
    //     resout les references globales. Tant que la partie n'est pas prete,
    //     nos pointeurs peuvent viser des objets detruits : on n'y touche pas.
    //     Une nouvelle generation de partie = refaire notre init. ---
    static unsigned generationVue = 0;
    if (!PartiePrete()) {
        if (g_pret) OublierPartie();
        return;
    }
    if (generationVue != PartieGeneration()) {
        OublierPartie();
        generationVue = PartieGeneration();
    }
    if (!g_pret) {
        static DWORD prochainEssai = 0;
        if (GetTickCount() < prochainEssai) return;
        ScootingInit();
        if (!g_pret) { prochainEssai = GetTickCount() + 5000; return; }
    }

    // Au-dela, on a besoin du scenario courant : il arrive au tick suivant,
    // une fois CaptureRunState alimente par les caches poses a l'init.
    if (!g_run.valid) return;

    // La persistance est indexee sur le GameId : elle ne peut etre lue
    // qu'une fois g_run renseigne, donc apres l'init et non pendant.
    if (!g_persistChargee) {
        g_persistChargee = true;
        CHRONO(ScootingReparerCargaisons());
        CHRONO(ScootingChargerEnAttente());
    }

    // --- expeditions : la liste du systeme Expedition les contient toutes,
    //     quel que soit le batiment d'envoi. On relit nos expeditions quand
    //     son compte change, ou quand l'une des notres n'est plus valide.
    //     (Comparer au compte, pas au nombre de valides : une expedition
    //     refusee par ExpValide relancerait sinon la relecture chaque tick.) ---
    uint32_t compte = CompteDuSysteme(IdSysteme::Expedition);
    bool perdue = false;
    for (int i = 0; i < g_nExps; ++i) {
        g_exps[i].valide = ExpValide(g_exps[i].comp);
        if (!g_exps[i].valide) perdue = true;
    }
    if (compte != g_compteListeExp || perdue) {
        printf("[SCOUT] expeditions : %u dans la liste du jeu (avant %u)%s -- relecture\n",
            compte, g_compteListeExp, perdue ? ", une des notres invalide" : "");
        g_compteListeExp = compte;
        RescanExpeditions();
        ScootingReparerCargaisons();
        ScootingChargerEnAttente();
    }

    SurveillerSites();
    SurveillerVisibilite();

    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide) continue;
        uintptr_t c = g_exps[i].comp;

        uintptr_t loc = 0, tgt = 0;
        if (!SafeRead((void*)(c + 0x120), &loc, 8)) continue;
        if (!SafeRead((void*)(c + 0x128), &tgt, 8)) continue;

        uint32_t cargo = CargaisonTotale(c);

        if (g_exps[i].nEnAttente > 0 && cargo == 0) {
            uintptr_t def = ApPorteurDef();
            if (def && InjecterPorteur(c, def, g_exps[i].nEnAttente)) {
                cargo = (uint32_t)g_exps[i].nEnAttente;
                printf("[SCOUT] exp %d : item restaure (%d en attente)\n",
                    g_exps[i].id, g_exps[i].nEnAttente);
            }
        }

        const char* nl = NomDuBloc(loc);
        const char* nt = NomDuBloc(tgt);

        if (loc && loc != g_blocVille) g_exps[i].dernierSite = loc;
        uintptr_t site = loc ? loc : g_exps[i].dernierSite;
        const char* ns = (site == loc) ? nl : NomDuBloc(site);

        if (tgt && tgt != g_exps[i].dernierTarget)
            printf("[SCOUT] exp %d part vers %s (porte %u)\n",
                g_exps[i].id, nt ? nt : "?", cargo);

        if (loc && loc != g_exps[i].dernierLieu)
            printf("[SCOUT] exp %d arrive a %s (porte %u)\n",
                g_exps[i].id, nl ? nl : "?", cargo);

        bool chargementTraite = false;
        if (cargo > g_exps[i].derniereCargo && site) {
            const SitePayload* p = PayloadDuSite(ns);
            if (p) {
                chargementTraite = true;
                bool empile = false;
                if (!DejaEnAttente(g_exps[i], p) && g_exps[i].nEnAttente < MAX_EN_ATTENTE) {
                    g_exps[i].enAttente[g_exps[i].nEnAttente++] = p;
                    empile = true;
                    const ApCheck* ac = ApCheckById(p->locationId);
                    printf("[SCOUT] exp %d ramasse la location %d (%s)  total=%d\n",
                        g_exps[i].id, p->locationId,
                        ac ? ac->description : "contenu inconnu du serveur",
                        g_exps[i].nEnAttente);
                }

                uintptr_t def = ApPorteurDef();
                int q = g_exps[i].nEnAttente > 0 ? g_exps[i].nEnAttente : 1;
                if (def && InjecterPorteur(c, def, q)) cargo = (uint32_t)q;
                else { ViderCargaison(c); cargo = 0; }

                if (empile) ScootingSauverEnAttente();
            }
        }

        bool enVille = (loc && g_blocVille && loc == g_blocVille);
        if (enVille && g_exps[i].nEnAttente > 0) {
            // Point de sortie unique vers Archipelago : archipelago.cpp.
            for (int k = 0; k < g_exps[i].nEnAttente; ++k)
                SendLocationCheck(g_exps[i].enAttente[k]->locationId, g_exps[i].enAttente[k]->site);
            g_exps[i].nEnAttente = 0;
            g_exps[i].dernierSite = 0;
            ViderCargaison(c);
            cargo = 0;
            ScootingSauverEnAttente();
        }

        if (!chargementTraite && cargo != g_exps[i].derniereCargo)
            printf("[SCOUT] exp %d cargaison %u -> %u  (lieu %s)\n",
                g_exps[i].id, g_exps[i].derniereCargo, cargo,
                loc ? (nl ? nl : "?") : "en route");

        g_exps[i].dernierLieu = loc;
        g_exps[i].dernierTarget = tgt;
        g_exps[i].derniereCargo = cargo;
    }

    MettreAJourListePortee();
}


int ScootingLocationPourCleSite(const char* cle)
{
    if (!cle || !g_run.valid) return -1;

    char nom[128] = { 0 };
    if (!NomDeSite(cle, nom, sizeof(nom))) return -1;

    const SitePayload* p = PayloadDuSite(nom);
    return p ? p->locationId : -1;
}

// Ecrit le GUID de la ressource porteuse dans le tresor de chaque site
// configure. A rejouer a chaque session : c'est une ecriture en memoire vive.
void ScootingAppliquerTresors()
{
    if (!ApPorteurDef()) return;

    uint64_t gr[2] = {};
    SafeRead((void*)(ApPorteurDef() + 0x08), &gr[0], 8);
    SafeRead((void*)(ApPorteurDef() + 0x10), &gr[1], 8);

    int faits = 0;
    // On travaille sur le cache : plus aucun balayage ici.
    for (int i = 0; i < g_nSites; ++i) {
        if (!PayloadDuSite(g_sites[i].nom)) continue;

        uintptr_t tpl = 0;
        if (!SafeRead((void*)(g_sites[i].comp + 0x20), &tpl, 8) || tpl <= 0x10000) continue;

        uintptr_t arr = 0; uint32_t cnt = 0;
        SafeRead((void*)(tpl + 0x1C0), &arr, 8);
        SafeRead((void*)(tpl + 0x1C0 + 8), &cnt, 4);
        if (arr <= 0x10000 || cnt == 0 || cnt > 32) continue;

        SafeWrite((void*)arr, &gr[0], 8);
        SafeWrite((void*)(arr + 8), &gr[1], 8);
        uint32_t un = 1;
        SafeWrite((void*)(tpl + 0x1C0 + 8), &un, 4);
        faits++;
    }
    if (faits) printf("[TRES] %d tresor(s) remplace(s)\n", faits);
}

// Une adresse est-elle une definition de ressource legitime ?
// Soit la notre, soit l'une de celles du conteneur de l'economie.
// Correction du 2026-10-29 : la comparaison se faisait sur l'economie
// memorisee (g_selfRessource), qui peut etre celle de la partie PRECEDENTE
// juste apres un chargement : la vraie definition du charbon d'une cargaison
// du jeu etait alors jugee invalide et la cargaison videe (journal :
// "[REPAIR] exp 1 : definition invalide 0x183AD5940D0"). On lit maintenant
// l'economie actuelle sur son global (+0x40916D0), avec son vrai compte.
static bool DefRessourceValide(uintptr_t def)
{
    if (def <= 0x10000) return false;
    if (def == ApPorteurDef()) return true;

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr), eco = 0, cont = 0;
    uint32_t cnt = 0;
    if (!SafeRead((void*)(base + 0x40916D0), &eco, 8) || eco <= 0x10000 ||
        !SafeRead((void*)(eco + RES_CONT_OFF), &cont, 8) || cont <= 0x10000 ||
        !SafeRead((void*)(eco + RES_CONT_OFF + 8), &cnt, 4) || cnt == 0 || cnt > 64)
        return false;

    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t d = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &d, 8)) continue;
        if (d == def) return true;
    }
    return false;
}

// Vide les cargaisons de ressources dont une definition est invalide (porteur
// AP d'une session precedente). Renvoie le nombre de cargaisons videes.
static int ReparerCargaisonsDe(const uintptr_t* comps, int n, const char* quand)
{
    int repares = 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t c = comps[i], arr = 0;
        uint32_t cnt = 0;
        int32_t id = -1;
        SafeRead((void*)(c + 0x30), &id, 4);
        if (!SafeRead((void*)(c + 0x168), &arr, 8) || !SafeRead((void*)(c + 0x168 + 8), &cnt, 4)) continue;
        if (arr <= 0x10000 || cnt == 0 || cnt > 64) continue;
        for (uint32_t k = 0; k < cnt; ++k) {
            uintptr_t def = 0;
            if (SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8) && DefRessourceValide(def)) continue;
            printf("[REPAIR] (%s) exp %d : definition invalide 0x%llX en position %u -- cargaison videe\n",
                quand, id, (unsigned long long)def, k);
            uint32_t zero = 0;
            SafeWrite((void*)(c + 0x168 + 8), &zero, 4);
            ++repares;
            break;
        }
    }
    return repares;
}

// Sortie de FrostpunkGame::LoadGame (hook de mod_common, thread du jeu) :
// AVANT le premier tick de la partie chargee, plus aucune cargaison ne pointe
// vers une definition morte (chantier "sauvegarde empoisonnee"). Si le
// porteur AP existe deja (une partie a ete jouee dans cette session), les
// items en attente de mods\ap_pending_<GameId>.txt sont reinjectes tout de
// suite ; sinon le tick s'en charge apres l'init (ScootingChargerEnAttente).
void ScootingApresChargement()
{
    uintptr_t comps[16];
    int n = ComposantsDuSysteme(IdSysteme::Expedition, "FrostpunkExpeditionComponent", comps, 16);
    int repares = ReparerCargaisonsDe(comps, n, "sortie de LoadGame");

    uintptr_t def = ApPorteurDef(), scn = SystemeParId(IdSysteme::Scenario);
    uint64_t gid[2] = {};
    int reinjectes = 0;
    if (def && scn && SafeRead((void*)(scn + 0x110), gid, sizeof(gid)) && (gid[0] || gid[1])) {
        char base[MAX_PATH] = { 0 }, chemin[MAX_PATH];
        GetModuleFileNameA(NULL, base, MAX_PATH);
        for (int i = (int)strlen(base) - 1; i >= 0; --i) if (base[i] == '\\') { base[i + 1] = 0; break; }
        _snprintf_s(chemin, sizeof(chemin), _TRUNCATE, "%smods\\ap_pending_%016llX%016llX.txt", base,
            (unsigned long long)gid[1], (unsigned long long)gid[0]);
        int parExp[16] = {};
        FILE* f = nullptr;
        if (fopen_s(&f, chemin, "r") == 0 && f) {
            int idExp = 0, idLoc = 0;
            while (fscanf_s(f, "%d %d", &idExp, &idLoc) == 2)
                for (int i = 0; i < n; ++i) {
                    int32_t id = -1;
                    if (SafeRead((void*)(comps[i] + 0x30), &id, 4) && id == idExp) { parExp[i]++; break; }
                }
            fclose(f);
        }
        for (int i = 0; i < n; ++i)
            if (parExp[i] > 0 && InjecterPorteur(comps[i], def, parExp[i])) ++reinjectes;
    }
    printf("[PERSIST] (sortie de LoadGame) %d expedition(s), %d cargaison(s) videe(s), %d scout(s) recharge(s)%s\n",
        n, repares, reinjectes, def ? "" : " -- porteur AP pas encore cree : reinjection au tick");
}

// Apres un rechargement, la cargaison peut contenir des pointeurs invalides :
// notre definition fabriquee n'existe plus au meme endroit, et le jeu ne
// sait pas la reconstruire. On vide alors la cargaison pour eviter le plantage.

void ScootingReparerCargaisons()
{
    int repares = 0;

    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide) continue;
        uintptr_t c = g_exps[i].comp;

        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(c + 0x168), &arr, 8)) continue;
        if (!SafeRead((void*)(c + 0x168 + 8), &cnt, 4)) continue;
        if (arr <= 0x10000 || cnt == 0 || cnt > 64) continue;

        bool suspect = false;
        for (uint32_t k = 0; k < cnt; ++k) {
            uintptr_t def = 0;
            if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8)) { suspect = true; break; }
            if (!DefRessourceValide(def)) {
                printf("[REPAIR] exp %d : definition invalide 0x%llX en position %u\n",
                    g_exps[i].id, (unsigned long long)def, k);
                suspect = true;
                break;
            }
        }

        if (suspect) {
            uint32_t zero = 0;
            SafeWrite((void*)(c + 0x168 + 8), &zero, 4);
            g_exps[i].derniereCargo = 0;
            g_exps[i].nEnAttente = 0;
            printf("[REPAIR] exp %d : cargaison videe -- le check en attente est PERDU\n",
                g_exps[i].id);
            repares++;
        }
    }

    if (repares) printf("[REPAIR] %d expedition(s) reparee(s)\n", repares);
}

static int ExpeditionSelectionnee()
{
    if (!g_selSys) return -1;

    uintptr_t arr = 0; uint32_t cnt = 0;
    if (!SafeRead((void*)(g_selSys + 0x2250), &arr, 8)) return -1;
    if (!SafeRead((void*)(g_selSys + 0x2250 + 8), &cnt, 4)) return -1;
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) return -1;

    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t ent = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10), &ent, 8)) continue;
        if (ent <= 0x10000) continue;

        for (int i = 0; i < g_nExps; ++i) {
            if (!g_exps[i].valide) continue;
            uintptr_t e = 0;
            if (!SafeRead((void*)(g_exps[i].comp + 0x18), &e, 8)) continue;
            if (e == ent) return i;
        }
    }
    return -1;
}

// Fichier de persistance, un par partie : les locations ramassees mais pas
// encore livrees en ville. Sans ca, sauvegarder avec un scout charge les perd.
static bool CheminPersistance(char* out, int maxlen)
{
    if (!g_run.valid) return false;

    char base[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, base, MAX_PATH);
    for (int i = (int)strlen(base) - 1; i >= 0; --i) {
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    }

    _snprintf_s(out, maxlen, _TRUNCATE, "%smods\\ap_pending_%016llX%016llX.txt",
        base,
        (unsigned long long)g_run.gameId[1],
        (unsigned long long)g_run.gameId[0]);
    return true;
}

// Ecrit l'etat courant : une ligne "idExpedition idLocation" par item porte.
void ScootingSauverEnAttente()
{
    char chemin[MAX_PATH];
    if (!CheminPersistance(chemin, MAX_PATH)) return;

    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "w") != 0 || !f) {
        printf("[PERSIST] ecriture impossible : %s\n", chemin);
        return;
    }

    int n = 0;
    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide) continue;
        for (int k = 0; k < g_exps[i].nEnAttente; ++k) {
            fprintf(f, "%d %d\n", g_exps[i].id, g_exps[i].enAttente[k]->locationId);
            n++;
        }
    }
    fclose(f);
    printf("[PERSIST] %d item(s) en attente sauvegarde(s)\n", n);
}

// Retrouve l'entree de table correspondant a une location.
static const SitePayload* PayloadParLocation(int locationId)
{
    for (int i = 0; i < SITE_PAYLOAD_COUNT; ++i)
        if (SITE_PAYLOADS[i].locationId == locationId) return &SITE_PAYLOADS[i];
    return nullptr;
}

// Relit le fichier et restaure les listes, puis reinjecte l'item porteur
// dans la cargaison des expeditions concernees.
void ScootingChargerEnAttente()
{
    printf("[START PERSIST]");
    char chemin[MAX_PATH];
    if (!CheminPersistance(chemin, MAX_PATH)) return;

    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "r") != 0 || !f) {
        printf("[PERSIST] aucun fichier pour cette partie\n");
        return;
    }

    for (int i = 0; i < g_nExps; ++i) g_exps[i].nEnAttente = 0;

    int lus = 0, idExp = 0, idLoc = 0;
    while (fscanf_s(f, "%d %d", &idExp, &idLoc) == 2) {
        const SitePayload* location = PayloadParLocation(idLoc);
        if (!location) { printf("[PERSIST] location %d inconnue, ignoree\n", idLoc); continue; }
        else
        {            
            printf("[PERSIST] location %d connue\n", idLoc);
            printf("[PERSIST] Restaured this check : %s \n", ApCheckById(idLoc)->description);
        }

        //Ce bloc charge dans l'exp�dition les location dans l'inventaire du scout.
        for (int i = 0; i < g_nExps; ++i) {
            if (!g_exps[i].valide || g_exps[i].id != idExp) continue;

            if (g_exps[i].nEnAttente >= MAX_EN_ATTENTE) break;

            g_exps[i].enAttente[g_exps[i].nEnAttente++] = location;
            lus++;
            break;
        }
    }
    fclose(f);

    //On reinject 
    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide || g_exps[i].nEnAttente <= 0) continue;
        uintptr_t def = ApPorteurDef();
        if (def && InjecterPorteur(g_exps[i].comp, def, g_exps[i].nEnAttente)) {
            g_exps[i].derniereCargo = 1;
            printf("[PERSIST] exp %d : %d item(s) restaure(s)\n",
                g_exps[i].id, g_exps[i].nEnAttente);
        }
        else {
            printf("[PERSIST] exp %d : reinjection impossible\n", g_exps[i].id);
        }
    }
    printf("[END PERSIST] %d item(s) restaure(s) au total\n", lus);
}

uint32_t CargaisonTotale(uintptr_t comp)
{
    uint32_t total = 0;

    uintptr_t arr = 0; uint32_t cnt = 0;
    if (SafeRead((void*)(comp + 0x168), &arr, 8) &&
        SafeRead((void*)(comp + 0x168 + 8), &cnt, 4) &&
        arr > 0x10000 && cnt > 0 && cnt <= 64) {
        for (uint32_t k = 0; k < cnt; ++k) {
            int64_t q = 0;
            if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8)) continue;
            if (q > 0 && q < 100000) total += (uint32_t)q;
        }
    }

    uint32_t nTech = 0;
    if (SafeRead((void*)(comp + 0x180 + 8), &nTech, 4) && nTech <= 64)
        total += nTech;

    return total;
}

// Liste des checks portes par l'expedition 'cible' (indice dans g_exps),
// descriptions separees par un saut de ligne. Renvoie leur nombre.
// Aucun acces memoire au jeu : seulement notre cache.
static int ConstruireListePortee(int cible, wchar_t* out, int maxChars)
{
    static const wchar_t* SEPARATEUR = L"\n";

    if (!out || maxChars < 2) return 0;
    out[0] = 0;
    if (cible < 0 || cible >= g_nExps || !g_exps[cible].valide || g_exps[cible].nEnAttente <= 0) return 0;

    int pos = 0, n = 0;
    for (int k = 0; k < g_exps[cible].nEnAttente; ++k) {
        const ApCheck* ac = ApCheckById(g_exps[cible].enAttente[k]->locationId);
        const char* s = ac ? ac->description : "(inconnu)";

        if (n > 0)
            for (int j = 0; SEPARATEUR[j] && pos < maxChars - 2; ++j)
                out[pos++] = SEPARATEUR[j];

        for (int j = 0; s[j] && pos < maxChars - 1; ++j)
            out[pos++] = (wchar_t)(unsigned char)s[j];

        n++;
    }
    out[pos] = 0;
    return n;
}

// Expedition dont la liste de droite remplit l'inventaire en ce moment
// (hook de FUN_141c2b6e0, groupes.cpp, thread du jeu). La traduction de
// "@Resources/Names/AP_Item" faite pendant ce remplissage donne le texte de
// l'infobulle de la case : elle doit viser CETTE expedition, pas celle
// survolee ou selectionnee sur la carte.
static int g_expAffichee = -1;

void ScootingExpeditionAffichee(uintptr_t comp)
{
    g_expAffichee = -1;
    if (!comp) return;
    for (int i = 0; i < g_nExps; ++i)
        if (g_exps[i].valide && g_exps[i].comp == comp) { g_expAffichee = i; return; }
}

int ScootingListePourAffichage(wchar_t* out, int maxChars, int* exp)
{
    if (exp) *exp = -1;
    if (g_expAffichee >= 0) {
        int n = ConstruireListePortee(g_expAffichee, out, maxChars);
        if (n > 0 && exp) *exp = g_expAffichee;
        return n;
    }
    return ScootingListePortee(out, maxChars);
}

// Recalcule la liste. Appelee une fois par tick, jamais depuis hkTranslate.
static void MettreAJourListePortee()
{
    g_nListePortee = ConstruireListePortee(ExpeditionVisee(), g_listePortee, 512);

    // Trace a chaque changement : les sauts de ligne deviennent des '/'
    // pour rester sur une seule ligne de console.
    static int dernierN = -1;
    if (g_nListePortee != dernierN) {
        dernierN = g_nListePortee;
        char a[512] = {};
        int j = 0;
        for (; g_listePortee[j] && j < 511; ++j) {
            wchar_t c = g_listePortee[j];
            a[j] = (c == L'\n') ? '/' : ((c < 128) ? (char)c : '?');
        }
        a[j] = 0;
        printf("[LISTE] %d item(s) : %s\n", g_nListePortee, a);
    }
}
// Remplace le contenu des sites configures par notre item, UNIQUEMENT quand
// le jeu a deja alloue le tableau. On ne fournit jamais le notre : le jeu
// ecrit dedans en consommant le butin.
void ScootingSubstituerContenuSites()
{
    if (!g_apDonneesActif) return;
    uintptr_t defAP = ApPorteurDef();
    if (!defAP) { printf("[SITE] porteur non fabrique\n"); return; }

    int ecrases = 0, sansTableau = 0;

    for (int i = 0; i < g_nSites; ++i) {
        if (!PayloadDuSite(g_sites[i].nom)) continue;

        uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
        SafeRead((void*)(g_sites[i].comp + 0x48),      &arr, 8);
        SafeRead((void*)(g_sites[i].comp + 0x48 + 8),  &cnt, 4);
        SafeRead((void*)(g_sites[i].comp + 0x48 + 12), &cap, 4);

        if (arr <= 0x10000 || cap < 1) { sansTableau++; continue; }

        // Deja fait ?
        uintptr_t def0 = 0;
        SafeRead((void*)arr, &def0, 8);
        if (def0 == defAP && cnt == 1) continue;

        int64_t un64 = 1; uint32_t un = 1;
        SafeWrite((void*)arr,       &defAP, 8);
        SafeWrite((void*)(arr + 8), &un64,  8);
        SafeWrite((void*)(g_sites[i].comp + 0x48 + 8), &un, 4);
        printf("[SITE] %s : contenu remplace\n", g_sites[i].nom);
        ecrases++;
    }
    printf("[SITE] %d remplace(s), %d sans tableau (affichage indisponible)\n",
           ecrases, sansTableau);
}


bool EnJeu()
{
    uintptr_t eco = 0;
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);
    if (!SafeRead((void*)(modBase + 0x40916D0), &eco, 8)) return false;
    return eco > 0x10000;
}

void ScootingFournirContenuSites()
{
    uintptr_t defAP = ApPorteurDef();
    if (!defAP) { printf("[SITE] porteur non fabrique\n"); return; }
    if (!MoteurAlloc) { printf("[SITE] allocateur non initialise\n"); return; }

    int fournis = 0, deja = 0, nonConfig = 0;
    for (int i = 0; i < g_nSites; ++i) {
        if (!PayloadDuSite(g_sites[i].nom)) { nonConfig++; continue; }

        uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
        SafeRead((void*)(g_sites[i].comp + 0x48), &arr, 8);
        SafeRead((void*)(g_sites[i].comp + 0x48 + 8), &cnt, 4);
        SafeRead((void*)(g_sites[i].comp + 0x48 + 12), &cap, 4);

        printf("[SITE] %-30s arr=0x%llX cnt=%u cap=%u\n",
            g_sites[i].nom, (unsigned long long)arr, cnt, cap);

        if (arr > 0x10000 && cap >= 1) { deja++; continue; }

        uintptr_t bloc = MoteurAlloc(4 * 0x10);
        if (!bloc) { printf("[SITE] allocation refusee\n"); continue; }

        uint64_t vide = 0;
        for (int z = 0; z < 8; ++z) SafeWrite((void*)(bloc + z * 8), &vide, 8);

        int64_t un64 = 1;
        SafeWrite((void*)bloc, &defAP, 8);
        SafeWrite((void*)(bloc + 8), &un64, 8);

        uint32_t un = 1, quatre = 4;
        if (SafeWrite((void*)(g_sites[i].comp + 0x48), &bloc, 8) &&
            SafeWrite((void*)(g_sites[i].comp + 0x48 + 8), &un, 4) &&
            SafeWrite((void*)(g_sites[i].comp + 0x48 + 12), &quatre, 4)) {
            printf("[SITE] %s : tableau alloue a 0x%llX\n",
                g_sites[i].nom, (unsigned long long)bloc);
            fournis++;
        }
    }
    printf("[SITE] %d fourni(s), %d deja pourvu(s), %d non configure(s), %d site(s)\n",
        fournis, deja, nonConfig, g_nSites);
}

static int ExpeditionVisee()
{
    if (!g_selSys) return -1;

    // 1) survol
    uintptr_t hov = 0;
    if (SafeRead((void*)(g_selSys + 0x2298), &hov, 8) && hov > 0x10000) {
        for (int i = 0; i < g_nExps; ++i) {
            if (!g_exps[i].valide) continue;
            uintptr_t e = 0;
            if (!SafeRead((void*)(g_exps[i].comp + 0x18), &e, 8)) continue;
            if (e == hov) return i;
        }
    }

    // 2) repli sur la selection
    uintptr_t arr = 0; uint32_t cnt = 0;
    if (!SafeRead((void*)(g_selSys + 0x2250), &arr, 8)) return -1;
    if (!SafeRead((void*)(g_selSys + 0x2258), &cnt, 4)) return -1;
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) return -1;

    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t ent = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)k * 0x10), &ent, 8)) continue;
        if (ent <= 0x10000) continue;
        for (int i = 0; i < g_nExps; ++i) {
            if (!g_exps[i].valide) continue;
            uintptr_t e = 0;
            if (!SafeRead((void*)(g_exps[i].comp + 0x18), &e, 8)) continue;
            if (e == ent) return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Releve des sites d'exploration de la partie (F9, thread du jeu ; demande de
// l'auteur du 2026-11-01 : liste complete pour leur assigner les checks).
// Une ligne "[SXLS]" par site, lue par outils\quetes_excel.py :
//   scenario, nom interne (cle de SITE_PAYLOADS), dossier du gabarit
//   (Story, Refugees, _SHARED...), noms affiches Display / Unvisited /
//   Explored / Depleted (gabarit +0x270 / +0x268 / +0x278 / +0x280),
//   description (+0x290), etat (+0x34), contenu (+0x48 : {definition,
//   quantite} au pas de 0x10 ; nom = entree de ressource a definition + 8),
//   nombre de dilemmes d'exploration (+0x3D0, compte +0x3D8).
// ---------------------------------------------------------------------------
static void DossierDeSite(uintptr_t tpl, char* out, int maxlen)
{
    strcpy_s(out, maxlen, "?");
    uintptr_t p = 0;
    char brut[256] = "";
    if (tpl <= 0x10000 || !SafeRead((void*)(tpl + 0x270), &p, 8) || p <= 0x10000) return;
    if (!SafeRead((void*)p, brut, sizeof(brut) - 1)) return;
    brut[sizeof(brut) - 1] = 0;
    const char* m = strstr(brut, "ExpeditionSites/");
    if (!m) return;
    m += 16;
    int k = 0;
    for (; m[k] && m[k] != '/' && k < maxlen - 1; ++k) out[k] = m[k];
    out[k] = 0;
}

static void ContenuDuSite(uintptr_t comp, char* out, int maxlen)
{
    out[0] = 0;
    uintptr_t arr = 0, defAP = ApPorteurDef();
    uint32_t cnt = 0;
    if (!SafeRead((void*)(comp + 0x48), &arr, 8) || !SafeRead((void*)(comp + 0x48 + 8), &cnt, 4) ||
        arr <= 0x10000 || cnt == 0 || cnt > 32) {
        strcpy_s(out, maxlen, "(rien)");
        return;
    }
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t def = 0; int64_t q = 0;
        SafeRead((void*)(arr + (uintptr_t)i * 0x10), &def, 8);
        SafeRead((void*)(arr + (uintptr_t)i * 0x10 + 8), &q, 8);
        char nom[64] = "?";
        if (def && def == defAP) strcpy_s(nom, "AP");
        else if (def > 0x10000) {
            // Nom interne de la ressource : qword +0x00 de l'entree. 1er
            // releve du 2026-11-01 : a definition + 8, tout sortait "?" -> la
            // definition du contenu est l'entree elle-meme ; les deux essayes.
            const int essais[2] = { 0, 8 };
            for (int e = 0; e < 2 && !strcmp(nom, "?"); ++e) {
                uintptr_t pn = 0;
                char t[64] = "";
                if (!SafeRead((void*)(def + essais[e]), &pn, 8) || pn <= 0x10000) continue;
                if (!SafeRead((void*)pn, t, sizeof(t) - 1)) continue;
                t[sizeof(t) - 1] = 0;
                int k = 0;
                while (t[k] >= 0x20 && t[k] <= 0x7E) ++k;
                if (k >= 2 && t[k] == 0) strcpy_s(nom, t);
            }
        }
        char item[96];
        sprintf_s(item, "%s%s %lld", i ? ", " : "", nom, (long long)q);
        strncat_s(out, maxlen, item, _TRUNCATE);
    }
}

void ScootingReleverSites()
{
    static uintptr_t objs[512];
    int n = ComposantsDuSysteme(IdSysteme::ExpeditionSite, "FrostpunkExpeditionSiteComponent", objs, 512);
    const char* scenario = g_run.valid ? g_run.scenario : "?";
    printf("[SITES] scenario %s : %d site(s)\n", scenario, n);
    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;
        uintptr_t tpl = 0;
        int32_t etat = -1;
        SafeRead((void*)(objs[i] + 0x20), &tpl, 8);
        SafeRead((void*)(objs[i] + 0x34), &etat, 4);
        char nom[128] = "?", dossier[64], contenu[512];
        NomDuGabarit(tpl, nom, sizeof(nom));
        DossierDeSite(tpl, dossier, sizeof(dossier));
        ContenuDuSite(objs[i], contenu, sizeof(contenu));
        char aff[4][256], desc[1024];
        const int offs[4] = { 0x270, 0x268, 0x278, 0x280 };
        for (int k = 0; k < 4; ++k) TraduireCleJeu(tpl + offs[k], aff[k], sizeof(aff[k]));
        TraduireCleJeu(tpl + 0x290, desc, sizeof(desc));
        uint32_t nDil = 0;
        SafeRead((void*)(tpl + 0x3D8), &nDil, 4);
        if (nDil > 64) nDil = 0;
        printf("[SXLS]\tS\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%u\n", scenario, nom, dossier,
            aff[0], aff[1], aff[2], aff[3], desc, etat, contenu, nDil);
        // Position sur la carte du monde (entite +0x140 : x, y, z), pour
        // verifier que le tirage fixe garde aussi les emplacements (question
        // de l'auteur du 2026-11-01).
        uintptr_t ent = 0;
        float pos[3] = {};
        if (SafeRead((void*)(objs[i] + 0x18), &ent, 8) && ent > 0x10000) SafeRead((void*)(ent + 0x140), pos, sizeof(pos));
        printf("[SPOS]\t%s\t%s\t%.1f\t%.1f\n", scenario, nom, pos[0], pos[2]);
    }

    // Visibilite (2026-11-03) : une ligne "[SREV]\tV" par site : decouvert,
    // visible, "Initially revealed", "Revealed in sight range" (-1 =
    // illisible), distance a la ville ; puis "[SREV]\tR\t<site>\t<cible>" pour
    // chaque site de sa liste "Reveals" (reveles par son exploration).
    static uintptr_t ents[512];
    static char noms[512][128];
    int ne = 0;
    for (int i = 0; i < n && ne < 512; ++i) {
        if (!EstSiteComponent(objs[i])) continue;
        uintptr_t tpl = 0;
        ents[ne] = 0;
        SafeRead((void*)(objs[i] + 0x18), &ents[ne], 8);
        SafeRead((void*)(objs[i] + 0x20), &tpl, 8);
        strcpy_s(noms[ne], "?");
        NomDuGabarit(tpl, noms[ne], sizeof(noms[ne]));
        ++ne;
    }
    auto nomEntite = [&](uintptr_t e) -> const char* {
        if (e <= 0x10000) return nullptr;
        for (int k = 0; k < ne; ++k) if (ents[k] == e) return noms[k];
        return nullptr;
    };
    int nWm = 0, nTpl = 0;
    for (int i = 0, s = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;
        const char* nom = s < ne ? noms[s] : "?";
        ++s;
        uintptr_t wm = ComposantCarteDuSite(objs[i]);
        int dec = -1, vis = -1, rInit = -1, rVue = -1;
        float dist2 = -1.0f;
        if (wm) {
            ++nWm;
            uint8_t b = 0;
            if (SafeRead((void*)(wm + WM_DECOUVERT), &b, 1)) dec = b ? 1 : 0;
            if (SafeRead((void*)(wm + WM_VISIBLE), &b, 1)) vis = b ? 1 : 0;
            SafeRead((void*)(wm + WM_DIST2), &dist2, 4);
            uintptr_t t = GabaritCarte(wm);
            if (t) {
                ++nTpl;
                if (SafeRead((void*)(t + WMT_REVELE_INIT), &b, 1)) rInit = b ? 1 : 0;
                if (SafeRead((void*)(t + WMT_REVELE_VUE), &b, 1)) rVue = b ? 1 : 0;
            }
        }
        printf("[SREV]\tV\t%s\t%s\t%d\t%d\t%d\t%d\t%.0f\n", scenario, nom, dec, vis, rInit, rVue,
            dist2 >= 0 ? sqrtf(dist2) : -1.0f);
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(objs[i] + SITE_REVELE), &arr, 8) || !SafeRead((void*)(objs[i] + SITE_REVELE + 8), &cnt, 4) ||
            arr <= 0x10000 || cnt > 64) cnt = 0;
        for (uint32_t k = 0; k < cnt; ++k) {
            // SafePtr<Entity> : essaye comme entite, puis comme poignee
            // (qword +0 = entite, cf. LirePoignee).
            uintptr_t q = 0, q2 = 0;
            SafeRead((void*)(arr + (uintptr_t)k * 8), &q, 8);
            const char* cible = nomEntite(q);
            if (!cible && q > 0x10000 && SafeRead((void*)q, &q2, 8)) cible = nomEntite(q2);
            char brut[40];
            if (!cible) { sprintf_s(brut, "?0x%llX", (unsigned long long)q); cible = brut; }
            printf("[SREV]\tR\t%s\t%s\t%s\n", scenario, nom, cible);
        }
    }
    printf("[SREV] %d site(s), %d composant(s) de carte, %d gabarit(s) de carte\n", ne, nWm, nTpl);
}

// Sites designes par un nom d'entite (FrostpunkEntitiesByNameSelector,
// 2026-11-03 : les revelations visent "DefaultRandomizationPoint_NNN", nom
// de l'emplacement de tirage). Decompile FUN_1415fd000 (selection) :
// FUN_141017230(gestionnaire d'entites *(base + 0x2B2DE98), &nom, &sortie)
// remplit un Dynarray d'entites {ptr, compte, capacite, allocateur} depuis
// l'index des noms (gestionnaire +0x130 / +0x138). Liberation comme le jeu :
// FUN_140e27c00(ptr) si l'allocateur est nul. Thread du jeu (F9).
// Correspondance : entite du composant de site (+0x18), sinon site a la meme
// position (entite +0x140).
typedef void (*EntitesParNom_t)(uintptr_t, uintptr_t, void*);
struct DynEntites { uintptr_t ptr; int32_t n, cap; uintptr_t alloc; };

static bool AppelerEntitesParNom(uintptr_t fn, uintptr_t mgr, uintptr_t adrNom, DynEntites* d)
{
    __try { ((EntitesParNom_t)fn)(mgr, adrNom, d); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void ScootingSitesDuNom(uintptr_t adrNom, char* out, int taille)
{
    out[0] = 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), mgr = 0;
    if (!SafeRead((void*)(base + 0x2B2DE98), &mgr, 8) || mgr <= 0x10000) { strcpy_s(out, taille, "?gestionnaire"); return; }
    DynEntites d{};
    if (!AppelerEntitesParNom(base + 0x1017230, mgr, adrNom, &d)) { strcpy_s(out, taille, "?appel"); return; }
    static uintptr_t objs[512];
    int n = ComposantsDuSysteme(IdSysteme::ExpeditionSite, "FrostpunkExpeditionSiteComponent", objs, 512);
    for (int i = 0; i < d.n && i < 16 && d.ptr > 0x10000; ++i) {
        uintptr_t ent = 0;
        SafeRead((void*)(d.ptr + (uintptr_t)i * 8), &ent, 8);
        float pos[3] = {};
        bool aPos = ent > 0x10000 && SafeRead((void*)(ent + 0x140), pos, sizeof(pos));
        char nom[128] = "";
        const char* comment = "";
        for (int k = 0; k < n && !nom[0]; ++k) {
            if (!EstSiteComponent(objs[k])) continue;
            uintptr_t e = 0, tpl = 0;
            SafeRead((void*)(objs[k] + 0x18), &e, 8);
            SafeRead((void*)(objs[k] + 0x20), &tpl, 8);
            if (e == ent) { NomDuGabarit(tpl, nom, sizeof(nom)); break; }
        }
        for (int k = 0; k < n && !nom[0] && aPos; ++k) {
            if (!EstSiteComponent(objs[k])) continue;
            uintptr_t e = 0, tpl = 0;
            float p[3] = {};
            SafeRead((void*)(objs[k] + 0x18), &e, 8);
            SafeRead((void*)(objs[k] + 0x20), &tpl, 8);
            if (e > 0x10000 && SafeRead((void*)(e + 0x140), p, sizeof(p)) &&
                fabsf(p[0] - pos[0]) < 1.0f && fabsf(p[2] - pos[2]) < 1.0f) {
                NomDuGabarit(tpl, nom, sizeof(nom));
                comment = " (meme position)";
            }
        }
        char item[200];
        if (nom[0]) sprintf_s(item, "%s%s%s", out[0] ? ", " : "", nom, comment);
        else {
            char cls[96] = "?";
            GetClassNameFP(ent, cls, sizeof(cls));
            sprintf_s(item, "%s?%s (%.0f, %.0f)", out[0] ? ", " : "", cls, pos[0], pos[2]);
        }
        strncat_s(out, taille, item, _TRUNCATE);
    }
    if (d.n == 0) strcpy_s(out, taille, "(aucune entite dans cette partie)");
    if (d.ptr > 0x10000 && d.cap > 0 && !d.alloc) ((void (*)(uintptr_t))(base + 0xE27C00))(d.ptr);
}

// Nom de gabarit d'un GUID (pool des gabarits, recherche pure) : pour les
// effets FrostpunkAddSiteRevealsEffect du releve des quetes.
static uintptr_t StubParGuidSur(uintptr_t pool, const uint8_t* guid);
bool ScootingNomGabaritParGuid(const uint8_t* guid, char* out, int taille)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t stub = StubParGuidSur(base + 0x2B2E710, guid), pnom = 0;
    strcpy_s(out, taille, "?");
    if (stub <= 0x10000 || !SafeRead((void*)(stub + 8), &pnom, 8) || pnom <= 0x10000) return false;
    SafeRead((void*)pnom, out, taille - 1);
    out[taille - 1] = 0;
    return true;
}

// ---------------------------------------------------------------------------
// Tables de tirage des sites (remarque de l'auteur du 2026-11-01 : un F9 ne
// voit que les sites tires dans la partie, ex. The Arks avait resources01-04
// et 06 mais pas 05). Decompiles du 2026-11-01 :
//   FrostpunkScenariosConfig (base + 0x2C34490, reflexion FUN_14129e3e0)
//   +0x368 "World map randomization tables" (pointeurs) ;
//   FrostpunkScenarioWorldMapRandomizationTableEntry (0x90, FUN_1416108b0) :
//   +0x00 nom, +0x08 GUID, +0x18 "Templates" (FrostpunkScenarioRandomWorld
//   MapEntityTemplateEntry, 0x10 = GUID du gabarit d'entite, compte +0x20),
//   +0x30 / +0x48 ressources aleatoires, +0x60 survivants, +0x78 conditions.
//   Resolution (FUN_1418f2a30, systeme de tirage de la carte) :
//   stub = FUN_141035c50(&pool des templates DAT_142b2e710, &guid, 1)
//   (= recherche FUN_141035cf0 + CHARGEMENT ; ici recherche seule) ;
//   stub +0x08 = nom du gabarit (= nom interne du site), FUN_14111c4f0(stub,
//   tampon, taille, 1, 0) = chemin complet. Aucun chargement.
//   Une variation de carte de scenario (FrostpunkScenarioWorldMapScene
//   VariationEntry, 0x58) cite ses tables en +0x18 (liens de 0x10).
// Une ligne "[TXLS]\tT\t<table>" par table, puis "[TXLS]\tE\t<table>\t<site>
// \t<chemin>" par gabarit possible.
// ---------------------------------------------------------------------------
typedef uintptr_t (*StubParGuid_t)(uintptr_t, const void*);
typedef void (*CheminDuStub_t)(uintptr_t, char*, int, uintptr_t, uintptr_t);

// FUN_141035cf0(pool, &guid) = recherche PURE (dichotomie sur les stubs
// tries par GUID : pool +0x30, compte +0x38, stub +0x10 = GUID). 1er essai
// du 2026-11-01 avec FUN_141035c50(pool, &guid, 1) sous le verrou du pool :
// jeu bloque au 1er site (cette fonction CHARGE le gabarit et attend le
// thread de chargement, FUN_14111c700 -> FUN_141004480 ; verrou tenu ->
// interblocage). Ici : ni chargement ni verrou.
static uintptr_t StubParGuidSur(uintptr_t pool, const uint8_t* guid)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    __try { return ((StubParGuid_t)(base + 0x1035CF0))(pool, guid); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static bool CheminDuStubSur(uintptr_t stub, char* out, int taille)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    out[0] = 0;
    __try { ((CheminDuStub_t)(base + 0x111C4F0))(stub, out, taille, 1, 0); }
    __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; return false; }
    return out[0] != 0;
}

void ScootingReleverTablesDeSites()
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t pool = base + 0x2B2E710;
    uintptr_t tables = 0;
    uint32_t nTables = 0;
    if (!SafeRead((void*)(base + 0x2C34490 + 0x368), &tables, 8) ||
        !SafeRead((void*)(base + 0x2C34490 + 0x368 + 8), &nTables, 4) || tables <= 0x10000 || nTables > 512) {
        printf("[SITES] tables de tirage illisibles\n");
        return;
    }
    printf("[SITES] %u table(s) de tirage des sites\n", nTables);
    for (uint32_t t = 0; t < nTables; ++t) {
        uintptr_t table = 0, pn = 0, gab = 0;
        uint32_t nGab = 0;
        char nomTable[128] = "?";
        if (!SafeRead((void*)(tables + (uintptr_t)t * 8), &table, 8) || table <= 0x10000) continue;
        if (SafeRead((void*)table, &pn, 8) && pn > 0x10000) {
            SafeRead((void*)pn, nomTable, sizeof(nomTable) - 1);
            nomTable[sizeof(nomTable) - 1] = 0;
        }
        SafeRead((void*)(table + 0x18), &gab, 8);
        SafeRead((void*)(table + 0x20), &nGab, 4);
        if (gab <= 0x10000 || nGab > 1024) nGab = 0;
        printf("[TXLS]\tT\t%s\t%u\n", nomTable, nGab);
        for (uint32_t k = 0; k < nGab; ++k) {
            uint8_t guid[16] = {};
            SafeRead((void*)(gab + (uintptr_t)k * 0x10), guid, 16);
            uintptr_t stub = StubParGuidSur(pool, guid);
            char nom[128] = "?", chemin[300] = "?";
            uintptr_t pnom = 0;
            if (stub > 0x10000 && SafeRead((void*)(stub + 8), &pnom, 8) && pnom > 0x10000) {
                SafeRead((void*)pnom, nom, sizeof(nom) - 1);
                nom[sizeof(nom) - 1] = 0;
            }
            if (stub > 0x10000) CheminDuStubSur(stub, chemin, sizeof(chemin));
            printf("[TXLS]\tE\t%s\t%s\t%s\n", nomTable, nom, chemin);
        }
    }
}

// ---------------------------------------------------------------------------
// Tirage des sites FIXE (decision de l'auteur du 2026-11-01, option 2 : un
// check par site, la carte d'un scenario doit donc toujours etre la meme).
// FUN_1418f2a30(systeme, &emplacements, table) = tirage d'une table
// (appelants FUN_1418f1440, FUN_14172a770) -> FUN_1418f2290 : melange des
// emplacements et des candidats par groupes de priorite (gabarit +0x430),
// puis appariement (FUN_1418edc80). Seul hasard : le generateur global
// "multiply-with-carry" DAT_142a12d60 / DAT_142a12d64 (multiplicateurs
// 0x9069 et 18000). On y pose une graine fixe le temps de l'appel, puis on
// rend l'etat d'origine (le reste du hasard du jeu n'est pas touche).
// ---------------------------------------------------------------------------
static const bool     SITES_TIRAGE_FIXE = true;
static const uint32_t GRAINE_SITES_A = 0x2B7E1516, GRAINE_SITES_B = 0x28AED2A6;   // non nulles

typedef uintptr_t (*TirerTable_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static TirerTable_t oTirerTable = nullptr;

// Test du 2026-11-01 (The Arks, 2 parties neuves) : memes 31 sites, mais 14
// echangent leurs emplacements -> l'ordre des emplacements fournis au tirage
// varie d'une partie a l'autre. On les trie par position (x puis z) avant
// le tirage. Emplacement = enregistrement de 0x10 ; son 1er qword (cle du
// tirage) est l'entite, ou une poignee (*poignee = entite) : les deux essayes.
static uintptr_t EntiteDeLEmplacement(uintptr_t q)
{
    char cls[96] = "";
    if (q > 0x10000 && GetClassNameFP(q, cls, sizeof(cls)) && strstr(cls, "Entity")) return q;
    uintptr_t e = 0;
    if (q > 0x10000 && SafeRead((void*)q, &e, 8) && e > 0x10000 && GetClassNameFP(e, cls, sizeof(cls)) &&
        strstr(cls, "Entity")) return e;
    return 0;
}

// 'pas' = taille d'un enregistrement (0x10 pour les emplacements du tirage,
// 0x18 pour les sites de la repartition du contenu), 1er qword = entite.
static bool TrierEmplacements(uintptr_t emplacements, uint32_t n, int pas = 0x10)
{
    uintptr_t tab = 0;
    if (!SafeRead((void*)emplacements, &tab, 8) || tab <= 0x10000 || n < 2 || n > 512 || pas > 0x18) return false;
    struct Empl { uint8_t brut[0x18]; float x, z; };
    static Empl e[512];
    for (uint32_t i = 0; i < n; ++i) {
        if (!SafeRead((void*)(tab + (uintptr_t)i * pas), e[i].brut, pas)) return false;
        uintptr_t q = 0;
        memcpy(&q, e[i].brut, 8);
        uintptr_t ent = EntiteDeLEmplacement(q);
        float pos[3] = {};
        if (!ent || !SafeRead((void*)(ent + 0x140), pos, sizeof(pos))) {
            static int nEchecs = 0;
            if (nEchecs++ < 3) {
                char cls[96] = "?";
                GetClassNameFP(q, cls, sizeof(cls));
                printf("[SITES] emplacement %u : entite introuvable (qword 0x%llX, classe %s) -> pas de tri\n",
                    i, (unsigned long long)q, cls);
            }
            return false;
        }
        e[i].x = pos[0];
        e[i].z = pos[2];
    }
    std::sort(e, e + n, [](const Empl& a, const Empl& b) { return a.x != b.x ? a.x < b.x : a.z < b.z; });
    for (uint32_t i = 0; i < n; ++i) SafeWrite((void*)(tab + (uintptr_t)i * pas), e[i].brut, pas);
    return true;
}

// Repartition du contenu des sites FIXE (2026-11-03, demande de l'auteur :
// verifier si les quantites dependent de la difficulte). Decompiles :
// FUN_1418f1440 (systeme de tirage de la carte) tire les sites
// (FUN_1418f2a30, graine fixe ci-dessous) puis appelle FUN_1418ef2f0(&sortie,
// &sites, table) : les TOTAUX de la table du scenario (+0x30 ressources, +0x48
// bonus, +0x60 survivants) sont repartis entre les sites (enregistrements de
// 0x18, 1er qword = entite) apres un melange par le generateur global ; puis
// FUN_141889690 / FUN_141889610 / FUN_141889930 ajoutent tel quel au site
// (+0x48 / +0x60 / +0xA8). AUCUN facteur de difficulte : seul le hasard
// fait varier les quantites -> meme graine et sites tries par position.
typedef uintptr_t (*RepartirContenu_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static RepartirContenu_t oRepartirContenu = nullptr;

static uintptr_t hkRepartirContenu(uintptr_t sortie, uintptr_t sites, uintptr_t table, uintptr_t p4)
{
    if (!SITES_TIRAGE_FIXE) return oRepartirContenu(sortie, sites, table, p4);
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t* etat = (uint32_t*)(base + 0x2A12D60);
    uint32_t avantA = etat[0], avantB = etat[1];
    uint32_t n = 0;
    if (sites > 0x10000) SafeRead((void*)(sites + 8), &n, 4);
    bool trie = TrierEmplacements(sites, n, 0x18);
    etat[0] = GRAINE_SITES_A ^ 0x5A5A5A5A;
    etat[1] = GRAINE_SITES_B ^ 0x5A5A5A5A;
    uintptr_t res = oRepartirContenu(sortie, sites, table, p4);
    etat[0] = avantA;
    etat[1] = avantB;
    printf("[SITES] repartition du contenu fixe : %u site(s)%s\n", n, trie ? ", tries par position" : ", NON tries");
    return res;
}

// Trace des candidats du tirage (2026-11-03 : Last Autumn a tire "53. Goods
// Station" au lieu de "49. EIC Train Station" malgre la graine fixe).
// Decompile FUN_1418f2a30 : chaque gabarit de la table (+0x18, GUID au pas
// de 0x10) -> composant de site (FUN_1411180b0(stub, FUN_14148c130(), 1)) ;
// si ses "conditions de tirage" (+0x400, compte +0x408) existent ET que
// DAT_142a2afa4 == 1, elles sont evaluees (FUN_141696fe0) et le candidat est
// ECARTE si elles sont fausses. Appele apres l'original : les gabarits sont
// alors charges. Ligne "[STIR] <table> <i> <site> conditions=N : ...".
typedef int       (*TypeCompSite_t)();
typedef uintptr_t (*CompDuStub_t)(uintptr_t, int, int);

// 1er essai du 2026-11-03 : conditions=-1 partout, le stub etait passe tel
// quel. FUN_141035c50 = FUN_14111c700(stub) = gabarit d'entite CHARGE, lu en
// stub +0x38 ; c'est lui que FUN_1418f2a30 donne a FUN_1411180b0. Dernier
// argument 0 : pas de creation du composant s'il manque.
static uintptr_t GabaritSiteDuStub(uintptr_t stub)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), gabarit = 0;
    if (!SafeRead((void*)(stub + 0x38), &gabarit, 8) || gabarit <= 0x10000) return 0;
    __try {
        int type = ((TypeCompSite_t)(base + 0x148C130))();
        return ((CompDuStub_t)(base + 0x11180B0))(gabarit, type, 0);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void JournaliserCandidats(uintptr_t table, const char* nomTable)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t gab = 0; uint32_t nGab = 0; int32_t evalue = -1;
    SafeRead((void*)(base + 0x2A2AFA4), &evalue, 4);
    if (!SafeRead((void*)(table + 0x18), &gab, 8) || !SafeRead((void*)(table + 0x20), &nGab, 4) ||
        gab <= 0x10000 || nGab > 1024) return;
    printf("[STIR] table \"%s\" : %u candidat(s), evaluation des conditions = %d\n", nomTable, nGab, evalue);
    for (uint32_t k = 0; k < nGab; ++k) {
        uint8_t guid[16] = {};
        SafeRead((void*)(gab + (uintptr_t)k * 0x10), guid, 16);
        uintptr_t stub = StubParGuidSur(base + 0x2B2E710, guid), pnom = 0;
        char nom[128] = "?";
        if (stub > 0x10000 && SafeRead((void*)(stub + 8), &pnom, 8) && pnom > 0x10000) {
            SafeRead((void*)pnom, nom, sizeof(nom) - 1);
            nom[sizeof(nom) - 1] = 0;
        }
        uintptr_t tpl = stub > 0x10000 ? GabaritSiteDuStub(stub) : 0;
        int32_t nCond = -1, prio = -1;
        static char cond[2048];
        cond[0] = 0;
        if (tpl > 0x10000) {
            SafeRead((void*)(tpl + 0x408), &nCond, 4);
            SafeRead((void*)(tpl + 0x430), &prio, 4);
            if (nCond > 0 && nCond < 64) LoisDecrireExigences(tpl + 0x400, cond, sizeof(cond));
        }
        printf("[STIR]\t%s\t%u\t%s\tconditions=%d\tpriorite=%d\t%s\n", nomTable, k, nom, nCond, prio, cond);
    }
}

static uintptr_t hkTirerTable(uintptr_t sys, uintptr_t emplacements, uintptr_t table, uintptr_t p4)
{
    if (!SITES_TIRAGE_FIXE) return oTirerTable(sys, emplacements, table, p4);
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t* etat = (uint32_t*)(base + 0x2A12D60);
    uint32_t avantA = etat[0], avantB = etat[1];
    // Compte lu AVANT : le tirage vide la liste des emplacements (1er test :
    // "0 emplacement(s)").
    uint32_t nEmpl = 0;
    if (emplacements > 0x10000) SafeRead((void*)(emplacements + 8), &nEmpl, 4);
    bool trie = TrierEmplacements(emplacements, nEmpl);
    etat[0] = GRAINE_SITES_A;
    etat[1] = GRAINE_SITES_B;
    uintptr_t res = oTirerTable(sys, emplacements, table, p4);
    etat[0] = avantA;
    etat[1] = avantB;
    char nom[96] = "?";
    uintptr_t pn = 0;
    if (table > 0x10000 && SafeRead((void*)table, &pn, 8) && pn > 0x10000) {
        SafeRead((void*)pn, nom, sizeof(nom) - 1);
        nom[sizeof(nom) - 1] = 0;
    }
    printf("[SITES] tirage fixe : table \"%s\", %u emplacement(s)%s\n", nom, nEmpl,
        trie ? ", tries par position" : ", NON tries");
    if (table > 0x10000) JournaliserCandidats(table, nom);
    return res;
}

// ---------------------------------------------------------------------------
// Variante de carte FIXE (2026-11-03). Last Autumn : 2 parties neuves, 1
// position de site commune sur 31 -> ce ne sont pas les memes emplacements.
// Decompile FUN_14131de20(parametres, scenario) = application des reglages
// d'un scenario a la creation de la partie : pour chaque liste du scenario
// (disposition de ville +0x1D8, disposition de carte +0x208, et autres
// listes +0xE8, +0x198, +0x150, +0x168, +0x118, +0x138, +0x1B0, +0x100,
// +0x180 ; elements de 0x10), un element tire au hasard par le generateur
// global DAT_142a12d60 / 64 ; parametres +0xA0 = disposition de carte, +0xA8
// = sa variante, tiree par FUN_14160c0f0 (disposition +0x20, elements de
// 0x58, compte +0x28). La variante decide quels emplacements de tirage
// existent. Graine fixe le temps de l'appel pour les 6 scenarios AP (cela
// fige aussi les autres choix de la fonction, dont la disposition de la
// ville), etat d'origine rendu apres. Log "[SITES] reglages ...".
// ---------------------------------------------------------------------------
typedef uintptr_t (*ReglagesScenario_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static ReglagesScenario_t oReglagesScenario = nullptr;
static const uint32_t GRAINE_VARIANTE_A = 0x3243F6A8, GRAINE_VARIANTE_B = 0x5A308D31;   // non nulles

static bool ScenarioAP(const char* nom)
{
    static const char* const AP[] = { "DEFAULT", "TechHaven", "REFUGEES", "WINTERHOME", "BUILDERS", "Reconquest" };
    for (const char* s : AP) if (strcmp(nom, s) == 0) return true;
    return false;
}

static void NomEntree(uintptr_t entree, char* out, int taille)
{
    uintptr_t pn = 0;
    strcpy_s(out, taille, "?");
    if (entree > 0x10000 && SafeRead((void*)entree, &pn, 8) && pn > 0x10000) {
        SafeRead((void*)pn, out, taille - 1);
        out[taille - 1] = 0;
        for (int k = 0; out[k]; ++k) if (out[k] < 0x20 || out[k] > 0x7E) { out[k] = 0; break; }
    }
}

static uintptr_t hkReglagesScenario(uintptr_t params, uintptr_t scenario, uintptr_t p3, uintptr_t p4)
{
    char nom[64];
    NomEntree(scenario, nom, sizeof(nom));
    bool fixe = SITES_TIRAGE_FIXE && scenario > 0x10000 && ScenarioAP(nom);
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    uint32_t* etat = (uint32_t*)(base + 0x2A12D60);
    uint32_t avantA = etat[0], avantB = etat[1];
    if (fixe) { etat[0] = GRAINE_VARIANTE_A; etat[1] = GRAINE_VARIANTE_B; }
    uintptr_t res = oReglagesScenario(params, scenario, p3, p4);
    if (fixe) { etat[0] = avantA; etat[1] = avantB; }
    if (scenario > 0x10000 && params > 0x10000) {
        uintptr_t disp = 0, variante = 0;
        int32_t nDisp = 0, nVar = 0;
        SafeRead((void*)(scenario + 0x210), &nDisp, 4);
        SafeRead((void*)(params + 0xA0), &disp, 8);
        SafeRead((void*)(params + 0xA8), &variante, 8);
        if (disp > 0x10000) SafeRead((void*)(disp + 0x28), &nVar, 4);
        char nDispNom[96], nVarNom[96];
        NomEntree(disp, nDispNom, sizeof(nDispNom));
        NomEntree(variante, nVarNom, sizeof(nVarNom));
        printf("[SITES] reglages \"%s\" : disposition de carte \"%s\" (%d possible(s)), variante \"%s\" (%d possible(s))%s\n",
            nom, nDispNom, nDisp, nVarNom, nVar, fixe ? " - graine fixe" : "");
    }
    return res;
}

// ---------------------------------------------------------------------------
// Icone AP sur les sites de la carte (demande de l'auteur, 2026-11-01).
// Decompiles du 2026-11-01 : le gabarit de site n'a aucune icone ; le marqueur
// est un FrostpunkUIExpeditionSiteSelectorTracker (vtable 0x142217798), mis a
// jour par FUN_141c1a0e0 (vtable [4]) : tracker +0x18 poignee de la racine,
// +0x28 poignee de l'entite suivie, +0x70 / +0x80 / +0x90 / +0x98 poignees
// d'elements ; a la fin, l'etat d'animation de NOM "Tracker preset" du gabarit
// de site (+0x3C8) est joue sur l'element +0x98 (FUN_1410240d0) : l'icone est
// donc une image de la recette d'interface, choisie par cet etat.
// Etape 1 (diagnostic) : relevé, une fois par tracker, du preset et de l'arbre
// des elements, pour trouver l'image de l'icone.
// ---------------------------------------------------------------------------
typedef uintptr_t (*MajTrackerSite_t)(uintptr_t);
static MajTrackerSite_t oMajTrackerSite = nullptr;
constexpr int TRK_RACINE = 0x18, TRK_ENTITE = 0x28, TRK_PRESET_ELEM = 0x98;
constexpr int TPL_SITE_PRESET = 0x3C8;
constexpr int UIE_NOM = 0x1D8, UIE_ENFANT = 0x220, UIE_FRERE = 0x240, UIE_DRAPEAUX = 0x270;

static uintptr_t LirePoignee(uintptr_t p)
{
    uintptr_t h = 0, o = 0;
    if (!SafeRead((void*)p, &h, 8) || h <= 0x10000 || !SafeRead((void*)h, &o, 8)) return 0;
    return o;
}

static void NomUI(uintptr_t el, char* out, int taille)
{
    uintptr_t pn = 0;
    out[0] = 0;
    if (SafeRead((void*)(el + UIE_NOM), &pn, 8) && pn > 0x10000) ReadAscii(pn, out, taille);
}

// Arbre d'un element : nom, classe, visible, translation, taille, et les
// textures lisibles (qwords +0x280..+0x600 vers un texte ".dds" / "Tiles",
// suivis de la case et du nombre de cases).
static void DecrireArbreUI(uintptr_t c, int prof, int* budget, bool freres)
{
    for (; c > 0x10000 && *budget > 0; --*budget) {
        char nom[64], cls[96] = "?";
        NomUI(c, nom, sizeof(nom));
        GetClassNameFP(c, cls, sizeof(cls));
        uint32_t dr = 0;
        float tr[2] = {}, t[2] = {}, coul[4] = {};
        SafeRead((void*)(c + UIE_DRAPEAUX), &dr, 4);
        SafeRead((void*)(c + 0x80), tr, sizeof(tr));
        SafeRead((void*)(c + 0x210), t, sizeof(t));
        SafeRead((void*)(c + 0x120), coul, sizeof(coul));
        // Image : ressource de texture en +0x350 (FUN_141107140), nom en +0x08.
        char tex[96] = "";
        if (strstr(cls, "Picture")) {
            uintptr_t res = 0, pn = 0;
            if (SafeRead((void*)(c + 0x350), &res, 8) && res > 0x10000 && SafeRead((void*)(res + 8), &pn, 8) &&
                pn > 0x10000)
                ReadAscii(pn, tex, sizeof(tex));
        }
        printf("[SITEUI]   %*s\"%s\" %s 0x%llX %s tr (%.1f, %.1f) taille %.1f x %.1f a=%.2f%s%s\n", prof * 2, "",
            nom, cls, (unsigned long long)c, (dr & 1) ? "visible" : "CACHE", tr[0], tr[1], t[0], t[1], coul[3],
            tex[0] ? " texture " : "", tex);
        for (int off = 0x280; off < 0x600; off += 8) {
            uintptr_t p = 0;
            char tx[96] = "";
            if (!SafeRead((void*)(c + off), &p, 8) || p <= 0x10000 || !ReadAscii(p, tx, sizeof(tx))) continue;
            if (!strstr(tx, ".dds") && !strstr(tx, ".texture") && !strstr(tx, "Tiles")) continue;
            int32_t v[4] = {};
            SafeRead((void*)(c + off + 8), v, sizeof(v));
            printf("[SITEUI]   %*s  +0x%X texture \"%s\" case (%d,%d) sur (%d,%d)\n", prof * 2, "", off, tx,
                v[0], v[1], v[2], v[3]);
        }
        uintptr_t enfant = 0, frere = 0;
        SafeRead((void*)(c + UIE_ENFANT), &enfant, 8);
        if (enfant && prof < 12) DecrireArbreUI(enfant, prof + 1, budget, true);
        if (!freres || !SafeRead((void*)(c + UIE_FRERE), &frere, 8)) return;
        c = frere;
    }
}

// Etape 2 (releve du 2026-11-01) : FUN_141c1a0e0 sert a TOUS les selecteurs de
// la carte (ville, groupes...) ; composant de site comme le jeu :
// *(entite + 0x2A0 + FUN_14148c130() x 8). Presets "SIZE_SMALL" / "SIZE_LARGE",
// meme arbre : SITE_SELECTOR -> BUTTON -> "" -> show_anim_clip ->
// highlight_anim_move_container -> not_frozen_container / frozen_container,
// chacun avec une image "ICON" (48 x 48 en (18, 14)) de texture
// "UI/Tiles/Icons.dds" = l'atlas des bulles, ou nos icones sont deja.
// La case de "ICON" est posee par l'animateur d'etat : FUN_1416ae7b0
// (ApplyExpeditionSitePresets) joue NOT_FROZEN / FROZEN, UNVISITED / VISITED /
// EXPLORED / DEPLETED / TARGETED_BY_TASK... sur la racine. Une tuile posee sur
// "ICON" serait donc reecrite au changement d'etat -> comme pour les lois :
// copie de "ICON" sans animateur, ajoutee au meme conteneur, notre case ;
// l'original reduit a une taille nulle (verifie a chaque mise a jour).
// Type du check : TEMPORAIRE, hachage stable du nom du site
// (IconeCheckDeTest) ; "icon_done" une fois le site explore (etat +0x34 >= 2,
// comme EXPLORED / DEPLETED de FUN_1416ae7b0).
constexpr int UIE_PARENT = 0x230, UIE_ANIMATEUR = 0x1E8, UIE_TAILLE = 0x210;
constexpr int SITE_ETAT = 0x34;
static const char* const CONTENEURS_ICONE[2] = { "not_frozen_container", "frozen_container" };

struct MarqueurSite {
    uintptr_t trk, racine, comp;
    uintptr_t conteneur[2], orig[2], copie[2];
    char site[96];
    const char* icone;
};
static MarqueurSite g_marqueurs[256];
static int g_nMarqueurs = 0;

static uintptr_t ChercherUI(uintptr_t el, const char* nom, int prof)
{
    uintptr_t c = 0;
    if (!SafeRead((void*)(el + UIE_ENFANT), &c, 8)) return 0;
    for (int g = 0; c > 0x10000 && g < 64; ++g) {
        char nm[64];
        NomUI(c, nm, sizeof(nm));
        if (!strcmp(nm, nom)) return c;
        if (prof < 10) {
            uintptr_t r = ChercherUI(c, nom, prof + 1);
            if (r) return r;
        }
        if (!SafeRead((void*)(c + UIE_FRERE), &c, 8)) return 0;
    }
    return 0;
}

// Enfants directs "ICON" d'un conteneur : l'original (animateur non nul) et
// notre copie (animateur nul) si elle existe deja.
static void IconesDuConteneur(uintptr_t cont, uintptr_t& orig, uintptr_t& copie)
{
    orig = copie = 0;
    uintptr_t c = 0;
    if (!SafeRead((void*)(cont + UIE_ENFANT), &c, 8)) return;
    for (int g = 0; c > 0x10000 && g < 64; ++g) {
        char nm[64];
        NomUI(c, nm, sizeof(nm));
        if (!strcmp(nm, "ICON")) {
            uintptr_t anim = 0;
            SafeRead((void*)(c + UIE_ANIMATEUR), &anim, 8);
            if (anim) { if (!orig) orig = c; }
            else if (!copie) copie = c;
        }
        if (!SafeRead((void*)(c + UIE_FRERE), &c, 8)) return;
    }
}

// La copie memorisee est-elle toujours la notre (meme conteneur, sans
// animateur) ? Sinon le marqueur a ete reconstruit : on le refait.
static bool CopieValide(uintptr_t copie, uintptr_t cont)
{
    uintptr_t parent = 0, anim = 1;
    return copie && SafeRead((void*)(copie + UIE_PARENT), &parent, 8) && parent == cont &&
        SafeRead((void*)(copie + UIE_ANIMATEUR), &anim, 8) && anim == 0;
}

static void IconeMarqueurSite(uintptr_t trk)
{
    uintptr_t ent = LirePoignee(trk + TRK_ENTITE);
    if (!ent) return;
    uintptr_t comp = 0;
    int type = ((int(*)())((uintptr_t)GetModuleHandleW(NULL) + 0x148C130))();
    if (type < 0 || type >= 512 || !SafeRead((void*)(ent + 0x2A0 + (uintptr_t)type * 8), &comp, 8) ||
        comp <= 0x10000) return;
    uintptr_t racine = LirePoignee(trk + TRK_RACINE);
    if (!racine) return;

    MarqueurSite* m = nullptr;
    for (int i = 0; i < g_nMarqueurs; ++i) if (g_marqueurs[i].trk == trk) { m = &g_marqueurs[i]; break; }
    if (!m) {
        if (g_nMarqueurs >= 256) {
            static bool dit = false;
            if (!dit) { dit = true; printf("[SITEUI] plus de 256 marqueurs : icones AP ignorees au-dela\n"); }
            return;
        }
        m = &g_marqueurs[g_nMarqueurs++];
        memset(m, 0, sizeof(*m));
        m->trk = trk;
    }
    bool aRefaire = m->racine != racine || m->comp != comp;
    for (int k = 0; k < 2 && !aRefaire; ++k)
        if (m->conteneur[k] && !CopieValide(m->copie[k], m->conteneur[k])) aRefaire = true;
    if (aRefaire) {
        memset(m, 0, sizeof(*m));
        m->trk = trk; m->racine = racine; m->comp = comp;
        uintptr_t tpl = 0;
        strcpy_s(m->site, "?");
        if (SafeRead((void*)(comp + 0x20), &tpl, 8) && tpl) NomDuGabarit(tpl, m->site, sizeof(m->site));
    }

    int32_t etat = 0;
    SafeRead((void*)(comp + SITE_ETAT), &etat, 4);
    // Type du check d'apres le serveur (scout) ; a defaut, icone de test.
    const char* icone = "icon_done";
    if (etat < 2) {
        icone = ApIconeDuCheck("site", m->site);
        if (!icone) {
            static int nSansScout = 0;
            if (nSansScout++ < 10) printf("[SITEUI] \"%s\" : pas de scout, icone de test\n", m->site);
            icone = IconeCheckDeTest(m->site, 0);
        }
    }

    static int nJournal = 0;
    for (int k = 0; k < 2; ++k) {
        if (!m->conteneur[k]) {
            uintptr_t cont = ChercherUI(racine, CONTENEURS_ICONE[k], 0);
            if (!cont) continue;
            uintptr_t orig = 0, copie = 0;
            IconesDuConteneur(cont, orig, copie);
            if (!orig) continue;
            bool nouvelle = !copie;
            if (nouvelle) copie = IconeAPSurCopieDe(orig, cont, icone);
            else IconeAPPoser(copie, icone);
            if (!copie) continue;
            m->conteneur[k] = cont; m->orig[k] = orig; m->copie[k] = copie;
            if (nJournal++ < 12) {
                float tr[2] = {}, t[2] = {};
                SafeRead((void*)(copie + 0x80), tr, sizeof(tr));
                SafeRead((void*)(copie + UIE_TAILLE), t, sizeof(t));
                printf("[SITEUI] \"%s\" (%s) : icone \"%s\" %s sur \"%s\" -> copie 0x%llX tr (%.1f, %.1f) taille %.1f x %.1f\n",
                    m->site, etat >= 2 ? "explore" : "non explore", icone, nouvelle ? "creee" : "reprise",
                    CONTENEURS_ICONE[k], (unsigned long long)copie, tr[0], tr[1], t[0], t[1]);
            }
        }
        else if (icone != m->icone) IconeAPPoser(m->copie[k], icone);
        // L'animateur d'etat peut rendre sa taille a l'original : on la
        // remet a zero des qu'elle reapparait.
        float t[2] = {};
        if (m->orig[k] && SafeRead((void*)(m->orig[k] + UIE_TAILLE), t, sizeof(t)) && (t[0] > 0.f || t[1] > 0.f))
            ElementTailleNulle(m->orig[k]);
    }
    if (icone != m->icone && m->icone && nJournal++ < 40)
        printf("[SITEUI] \"%s\" : icone %s -> %s\n", m->site, m->icone, icone);
    m->icone = icone;
}

static uintptr_t hkMajTrackerSite(uintptr_t trk)
{
    uintptr_t res = oMajTrackerSite(trk);
    __try { IconeMarqueurSite(trk); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        static int n = 0;
        if (n++ < 5) printf("[SITEUI] EXCEPTION 0x%08lX a la pose de l'icone d'un site\n", GetExceptionCode());
    }
    return res;
}

void InstallHooksExpeditions(uintptr_t modBase)
{
    void* m = (void*)(modBase + 0x1C1A0E0);
    bool okM = MH_CreateHook(m, (void*)&hkMajTrackerSite, (void**)&oMajTrackerSite) == MH_OK &&
        MH_EnableHook(m) == MH_OK;
    printf("[SITEUI] hook mise a jour du tracker de site (+1C1A0E0) %s\n", okM ? "OK" : "KO");
    // (Hook de l'envoi d'expedition FUN_1418a0a60, debug EXPEDITIONS_RAPIDES,
    // retire le 2026-11-04 a la demande de l'auteur.)
    void* t = (void*)(modBase + 0x18F2A30);
    bool ok = MH_CreateHook(t, (void*)&hkTirerTable, (void**)&oTirerTable) == MH_OK && MH_EnableHook(t) == MH_OK;
    printf("[SITES] hook tirage des sites (+18F2A30) %s%s\n", ok ? "OK" : "KO",
           SITES_TIRAGE_FIXE ? " - tirage fixe" : "");
    void* c = (void*)(modBase + 0x18EF2F0);
    ok = MH_CreateHook(c, (void*)&hkRepartirContenu, (void**)&oRepartirContenu) == MH_OK && MH_EnableHook(c) == MH_OK;
    printf("[SITES] hook repartition du contenu des sites (+18EF2F0) %s%s\n", ok ? "OK" : "KO",
           SITES_TIRAGE_FIXE ? " - repartition fixe" : "");
    void* r = (void*)(modBase + 0x131DE20);
    ok = MH_CreateHook(r, (void*)&hkReglagesScenario, (void**)&oReglagesScenario) == MH_OK && MH_EnableHook(r) == MH_OK;
    printf("[SITES] hook reglages du scenario (+131DE20) %s%s\n", ok ? "OK" : "KO",
           SITES_TIRAGE_FIXE ? " - variante de carte fixe" : "");
}

