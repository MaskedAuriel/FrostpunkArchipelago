#include "scooting.h"
#include "mod_common.h"
#include "scenario.h"
#include "tech_brain.h"     // OnProgressiveReceived
#include "ap_data.h"
#include "ap_ressources.h"
#include "archipelago.h"
#include <intrin.h>

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



static const SitePayload SITE_PAYLOADS[] = {
    { "REFUGEES", "[ref]Builder_Camp", 1001 },
    { "DEFAULT",  "Site_052_A3",       1002 },
    { "REFUGEES",  "[t-plain][landmark]site11",       1003 },
};

static const int SITE_PAYLOAD_COUNT =
(int)(sizeof(SITE_PAYLOADS) / sizeof(SITE_PAYLOADS[0]));

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

static const SitePayload* PayloadDuSite(const char* site)
{
    if (!site || !site[0] || !g_run.valid) return nullptr;
    for (int i = 0; i < SITE_PAYLOAD_COUNT; ++i) {
        if (!StrEq(SITE_PAYLOADS[i].scenario, g_run.scenario)) continue;
        if (!StrEq(SITE_PAYLOADS[i].site, site)) continue;
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
                SendLocationCheck(g_exps[i].enAttente[k]->locationId);
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
// Soit la notre, soit l'une des 11 du conteneur de la ville.
static bool DefRessourceValide(uintptr_t def)
{
    if (def <= 0x10000) return false;
    if (def == ApPorteurDef()) return true;

    if (!EnsureResSelf()) return false;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000)
        return false;

    for (int i = 0; i < 12; ++i) {
        uintptr_t d = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &d, 8)) continue;
        if (d == def) return true;
    }
    return false;
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

// Recalcule la liste. Appelee une fois par tick, jamais depuis hkTranslate.
static void MettreAJourListePortee()
{
    // Le retour a la ligne n'est pas rendu par les infobulles : on separe
    // par une barre verticale.
    static const wchar_t* SEPARATEUR = L"\n";

    g_listePortee[0] = 0;
    g_nListePortee = 0;

    int cible = ExpeditionVisee();
    if (cible < 0 || !g_exps[cible].valide || g_exps[cible].nEnAttente <= 0) return;

    int pos = 0;
    for (int k = 0; k < g_exps[cible].nEnAttente; ++k) {
        const ApCheck* ac = ApCheckById(g_exps[cible].enAttente[k]->locationId);
        const char* s = ac ? ac->description : "(inconnu)";

        if (g_nListePortee > 0)
            for (int j = 0; SEPARATEUR[j] && pos < 510; ++j)
                g_listePortee[pos++] = SEPARATEUR[j];

        for (int j = 0; s[j] && pos < 511; ++j)
            g_listePortee[pos++] = (wchar_t)(unsigned char)s[j];

        g_nListePortee++;
    }
    g_listePortee[pos] = 0;

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





