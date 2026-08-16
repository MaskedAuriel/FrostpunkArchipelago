#include "scooting.h"
#include "mod_common.h"
#include "scenario.h"
#include "tech_brain.h"     // OnProgressiveReceived
#include "ap_data.h"
#include "ap_ressources.h"

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
static SiteCache g_sites[64];
static int       g_nSites = 0;
static uintptr_t g_sending = 0;      // FrostpunkExpeditionSendingComponent
static uintptr_t g_blocVille = 0;
static bool      g_pret = false;

volatile bool    g_traceScout = false;   // trace verbeuse, a basculer sur une touche

static const char* PORTEUR_AP = "Steam Cores";
volatile bool g_siteApAffiche = true;

struct GabaritCache { uintptr_t tpl; char nom[128]; };
static GabaritCache g_gabarits[64];
static int          g_nGabarits = 0;


static uint64_t g_modeleTresor[4];
static bool     g_modeleTresorPret = false;
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

// Systeme de selection, resolu une fois a l'init.
static uintptr_t g_selSys = 0;
volatile bool g_apDonneesActif = true;
// ============================================================================
//  Utilitaires
// ============================================================================

static bool StrEq(const char* a, const char* b)
{
    if (!a || !b) return false;
    for (int i = 0; ; ++i) { if (a[i] != b[i]) return false; if (a[i] == 0) return true; }
}

// Filtre exact : CollectObjectsByClass utilise StrContains, qui attraperait
// aussi les ...ComponentTemplate.
static bool ClasseExacte(uintptr_t obj, const char* ref)
{
    char cls[128] = { 0 };
    if (!GetClassNameFP(obj, cls, sizeof(cls))) return false;
    for (int j = 0; ; ++j) {
        if (cls[j] != ref[j]) return false;
        if (ref[j] == 0) return true;
    }
}

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
extern bool g_defEnregistree = false;

static bool CachesValides()
{
    uintptr_t def = ApPorteurDef();
    //printf("Valeur de def : %lu\n", (unsigned long)def);
    if (!def)
    {
        return false;
        printf("Raison 1\n");
    }
   

    if (!EnsureResSelf())
    {
        printf("Raison 2\n");
        return false;
    }
    
    uintptr_t arr = 0; uint32_t cnt = 0;
    if (!SafeRead((void*)(g_selfRessource + 0x2240), &arr, 8))
    {
        printf("Raison 3\n");
        return false;
    }
    if (!SafeRead((void*)(g_selfRessource + 0x2248), &cnt, 4))
    {
        printf("Raison 4\n");
        return false;
    }
    if (arr <= 0x10000 || cnt == 0 || cnt > 64)
    {
        printf("Raison 5\n");
        return false;
    }

    //printf("Valeur de cnt : %lu\n", (unsigned long)cnt);

    bool enregistree = false;
    for (uint32_t i = 0; i < cnt; ++i) {
        uintptr_t d = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)i * 0x70), &d, 8))                      
            continue;
        //printf("    Valeur de d : %lu\n", (unsigned long)d);
        if (d == def) { enregistree = true; break; }
    }
    //printf("%s\n", g_defEnregistree ? "true" : "false");
    if (!enregistree && g_defEnregistree)
    {
        printf("Raison 6\n");
        return false;
    }

    if (!g_sending || !ClasseExacte(g_sending, "FrostpunkExpeditionSendingComponent"))
    {
        printf("Raison 7\n");
        return false;
    }
        
    if (g_nSites > 0 && !EstSiteComponent(g_sites[0].comp))
    {
        printf("Raison 8\n");
        return false;
    }
        

    return true;
}

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
    const int offs[3] = { 0x180, 0x168, 0x198 };   // technos, ressources, artefacts
    for (int i = 0; i < 3; ++i) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(comp + offs[i]), &arr, 8)) continue;
        if (!SafeRead((void*)(comp + offs[i] + 8), &cnt, 4)) continue;
        if (arr <= 0x10000 || cnt == 0 || cnt > 64) continue;
        uint32_t zero = 0;
        SafeWrite((void*)(comp + offs[i] + 8), &zero, 4);
    }
}

static void SurveillerSites()
{
    if (!g_apDonneesActif) return;
    for (int i = 0; i < g_nSites; ++i) {
        int32_t etat = -1;
        if (!SafeRead((void*)(g_sites[i].comp + 0x34), &etat, 4)) continue;

        int32_t avant = g_sites[i].dernierEtat;
        g_sites[i].dernierEtat = etat;
        if (avant != 0 || etat == 0) continue;

        const SitePayload* p = PayloadDuSite(g_sites[i].nom);
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

            uintptr_t def = ApPorteurDef();
            if (def && InjecterPorteur(g_exps[e].comp, def, g_exps[e].nEnAttente))
                g_exps[e].derniereCargo = (uint32_t)g_exps[e].nEnAttente;

            ScootingSauverEnAttente();
            break;
        }
    }
}
static void RecenserSites()
{
    g_nSites = 0;
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n && g_nSites < 64; ++i) {
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
        ScootingSubstituerContenuSites();
        for (int i = 0; i < g_nSites; ++i)
            if (g_sites[i].bloc == bloc) return g_sites[i].nom;
    }
    return "(avant-poste ou autre)";
}


static uintptr_t TrouverBlocVille()
{
    static uintptr_t objs[64];
    int n = CollectObjectsByClass("FrostpunkCityComponent", objs, 64);

    for (int i = 0; i < n; ++i) {
        if (!ClasseExacte(objs[i], "FrostpunkCityComponent")) continue;

        uintptr_t ent = 0, bloc = 0;
        if (!SafeRead((void*)(objs[i] + 0x18), &ent, 8) || ent <= 0x10000) continue;
        if (!SafeRead((void*)(ent + 0x08), &bloc, 8) || bloc <= 0x10000) continue;

        printf("[SCOUT] ville : composant 0x%llX -> bloc 0x%llX\n",
            (unsigned long long)objs[i], (unsigned long long)bloc);
        return bloc;
    }
    printf("[SCOUT] FrostpunkCityComponent introuvable\n");
    return 0;
}

static uintptr_t DefRessourceParNom(const char* nom)
{
    if (!EnsureResSelf()) return 0;
    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000)
        return 0;

    for (int i = 0; i < 12; ++i) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;
        uintptr_t np = 0; char n[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000) SafeRead((void*)np, n, sizeof(n) - 1);
        if (StrEq(n, nom)) return def;
    }
    return 0;
}

// Remplace toute la cargaison par une seule ressource porteuse.
// Element de CarriedResources : {pointeur def (8 o), quantite (8 o)}.
bool InjecterPorteur(uintptr_t comp, uintptr_t def, int64_t quantite)
{
    if (def <= 0x10000) { printf("[INJ] definition nulle\n"); return false; }

    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
    if (!SafeRead((void*)(comp + 0x168), &arr, 8)) { printf("[INJ] pointeur illisible\n"); return false; }
    if (!SafeRead((void*)(comp + 0x168 + 8), &cnt, 4)) { printf("[INJ] compte illisible\n");   return false; }
    if (!SafeRead((void*)(comp + 0x168 + 12), &cap, 4)) { printf("[INJ] capacite illisible\n"); return false; }

    printf("[INJ] CarriedResources : arr=0x%llX  cnt=%u  cap=%u\n",(unsigned long long)arr, cnt, cap);

    if (arr <= 0x10000) { printf("[INJ] tableau non alloue -- rien a ecrire\n"); return false; }
    if (cap < 1) { printf("[INJ] capacite nulle\n");                      return false; }

    if (!SafeWrite((void*)(arr + 0), &def, 8)) { printf("[INJ] ecriture def KO\n");  return false; }
    if (!SafeWrite((void*)(arr + 8), &quantite, 8)) { printf("[INJ] ecriture qte KO\n");  return false; }

    uint32_t un = 1;
    if (!SafeWrite((void*)(comp + 0x168 + 8), &un, 4)) { printf("[INJ] ecriture compte KO\n"); return false; }

    // technos et artefacts vides
    uint32_t zero = 0;
    SafeWrite((void*)(comp + 0x180 + 8), &zero, 4);
    SafeWrite((void*)(comp + 0x198 + 8), &zero, 4);

    printf("[INJ] porteur injecte (quantite %lld)\n", (long long)quantite);
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
    int n = CollectObjectsByClass("FrostpunkExpeditionComponent", objs, 128);

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

// ============================================================================
//  Initialisation
// ============================================================================
void ScootingInit()
{
    g_nSites = 0; g_sending = 0; g_pret = false; g_blocVille = 0;

    // La definition porteuse est clonee depuis une definition existante :
    // sans conteneur de ressources resolu, rien n'est possible.
    if (!ApPorteurInit()) {
        printf("[SCOUT] porteur non fabrique -- init reportee\n");
        return;
    }
    //ApPorteurEnregistrer();

    RecenserSites();
    ResoudreSelectionSystem();
    g_blocVille = TrouverBlocVille();

    static uintptr_t objs[256];
    int m = CollectObjectsByClass("FrostpunkExpeditionSendingComponent", objs, 256);
    for (int i = 0; i < m; ++i) {
        if (!ClasseExacte(objs[i], "FrostpunkExpeditionSendingComponent")) continue;
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(objs[i] + 0x60), &arr, 8)) continue;
        if (!SafeRead((void*)(objs[i] + 0x68), &cnt, 4)) continue;
        if (arr > 0x10000 && cnt <= 32) { g_sending = objs[i]; break; }
    }

    g_pret = (g_nSites > 0 && g_sending != 0);
    printf("[SCOUT] init : %d site(s), ville=0x%llX, sending=0x%llX -> %s\n",
        g_nSites, (unsigned long long)g_blocVille,
        (unsigned long long)g_sending, g_pret ? "pret" : "incomplet");

    ScootingAppliquerTresors();
    ScootingSubstituerContenuSites();
    if (g_pret) {
        RescanExpeditions();
        ScootingReparerCargaisons();     // vide les pointeurs invalides
        ScootingChargerEnAttente();      // restaure et reinjecte
        ScootingEtatApresInit();
    }
}

void ScootingTick()
{
    // Enregistrement le plus tot possible : sans lui, le jeu ne peut pas
    // resoudre notre GUID en deserialisant la cargaison d'un scout.   
    // --- changement de partie ---
    static uint64_t dernierGame[2] = { 0, 0 };
    if (g_run.valid &&
        (g_run.gameId[0] != dernierGame[0] || g_run.gameId[1] != dernierGame[1])) {
        dernierGame[0] = g_run.gameId[0];
        dernierGame[1] = g_run.gameId[1];
        g_pret = false; g_blocVille = 0; g_nExps = 0;
        printf("[SCOUT] nouvelle partie detectee\n");
    }

    if (PartieRechargee()) {
        g_pret = false;
        g_blocVille = 0;
        g_nExps = 0;
        g_nSites = 0;
        g_nGabarits = 0;              // les gabarits sont recharges avec le scenario
        g_modeleTresorPret = false;
    }
        
    // --- initialisation differee ---
    if (!g_pret) {
        if (!g_run.valid) return;
        static DWORD stableDepuis = 0;
        if (stableDepuis == 0) {
            stableDepuis = GetTickCount();
            printf("[SCOUT] attente de stabilisation\n");
            return;
        }
        if (GetTickCount() - stableDepuis < 5000) return;
        stableDepuis = 0;
        printf("[SCOUT] init...\n");
        ScootingInit();
        printf("[SCOUT] init terminee (pret=%d)\n", (int)g_pret);
        if (!g_pret) { stableDepuis = GetTickCount(); return; }
    }

    // --- expeditions declarees ---
    uint32_t declarees = 0;
    if (!SafeRead((void*)(g_sending + 0x68), &declarees, 4)) return;
    if (declarees > 16) return;

    int valides = 0;
    for (int i = 0; i < g_nExps; ++i) {
        g_exps[i].valide = ExpValide(g_exps[i].comp);
        if (g_exps[i].valide) valides++;
    }
    if (valides != (int)declarees) {
        RescanExpeditions();
        ScootingReparerCargaisons();
        ScootingChargerEnAttente();
    }

    // --- detection par changement d'etat des sites -------------------------
    // Doit venir APRES la validation des expeditions (SurveillerSites les
    // parcourt) et AVANT la boucle, pour que la cargaison injectee y soit
    // deja prise en compte.
    SurveillerSites();

    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide) continue;
        uintptr_t c = g_exps[i].comp;

        if (g_traceScout)
            printf("[SCOUT] exp %d @0x%llX\n", g_exps[i].id, (unsigned long long)c);

        uintptr_t loc = 0, tgt = 0;
        if (!SafeRead((void*)(c + 0x120), &loc, 8)) continue;
        if (!SafeRead((void*)(c + 0x128), &tgt, 8)) continue;

        uint32_t cargo = CargaisonTotale(c);

        const char* nl = NomDuBloc(loc);
        const char* nt = NomDuBloc(tgt);

        // Le lieu de reference pour un chargement : le lieu courant s'il existe,
        // sinon le dernier visite — la cargaison arrive parfois apres le depart.
        if (loc && loc != g_blocVille) g_exps[i].dernierSite = loc;
        uintptr_t site = loc ? loc : g_exps[i].dernierSite;
        const char* ns = (site == loc) ? nl : NomDuBloc(site);

        if (tgt && tgt != g_exps[i].dernierTarget)
            printf("[SCOUT] exp %d part vers %s (porte %u)\n",
                g_exps[i].id, nt ? nt : "?", cargo);

        if (loc && loc != g_exps[i].dernierLieu)
            printf("[SCOUT] exp %d arrive a %s (porte %u)\n",
                g_exps[i].id, nl ? nl : "?", cargo);

        // --- chargement sur un site configure (sites qui donnent vraiment
        //     quelque chose ; les autres sont traites par SurveillerSites) ---
        bool chargementTraite = false;
        if (cargo > g_exps[i].derniereCargo && site) {
            const SitePayload* p = PayloadDuSite(ns);
            printf("[SCOUT] chargement +%u sur %s -> %s\n",
                cargo - g_exps[i].derniereCargo, ns ? ns : "?",
                p ? "site configure" : "site non configure");

            if (p) {
                chargementTraite = true;

                bool empile = false;
                if (DejaEnAttente(g_exps[i], p)) {
                    printf("[SCOUT] exp %d : location %d deja en attente\n",
                        g_exps[i].id, p->locationId);
                }
                else if (g_exps[i].nEnAttente < MAX_EN_ATTENTE) {
                    g_exps[i].enAttente[g_exps[i].nEnAttente++] = p;
                    empile = true;
                    const ApCheck* ac = ApCheckById(p->locationId);
                    printf("[SCOUT] exp %d ramasse la location %d (%s)  total=%d\n",
                        g_exps[i].id, p->locationId,
                        ac ? ac->description : "contenu inconnu du serveur",
                        g_exps[i].nEnAttente);
                }
                else {
                    printf("[SCOUT] exp %d : file pleine, location perdue\n", g_exps[i].id);
                }

                uintptr_t def = ApPorteurDef();
                int q = g_exps[i].nEnAttente > 0 ? g_exps[i].nEnAttente : 1;
                if (def && InjecterPorteur(c, def, q)) {
                    cargo = (uint32_t)q;
                }
                else {
                    ViderCargaison(c);
                    cargo = 0;
                    printf("[SCOUT] porteur indisponible, cargaison videe\n");
                }

                if (empile) ScootingSauverEnAttente();
            }
        }

        // --- retour en ville : envoi des checks ---
        bool enVille = (loc && g_blocVille && loc == g_blocVille);
        if (enVille && g_exps[i].nEnAttente > 0) {
            for (int k = 0; k < g_exps[i].nEnAttente; ++k) {
                const SitePayload* p = g_exps[i].enAttente[k];
                const ApCheck* ac = ApCheckById(p->locationId);
                printf("[AP CHECK] location %d -> %s\n", p->locationId,
                    ac ? ac->description : "(inconnue)");
                // TODO reseau : AP_SendCheck(p->locationId);

                if (ac && ac->arbreTest >= 0) {
                    printf("[SCOUT] livraison locale : progressive arbre %d\n", ac->arbreTest);
                    OnProgressiveReceived(ac->arbreTest);
                }
            }
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

void ScootingDumpTemplate(const char* nomCherche)
{
    // 1) la liste des champs nommes, par la reflexion
    DumpRecordProperties(FindClassRecord("FrostpunkExpeditionSiteComponentTemplate"));

    // 2) le gabarit du site demande
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (nomCherche && nomCherche[0] && !StrEq(nom, nomCherche)) continue;

        printf("\n[TPL] %s -> gabarit 0x%llX\n", nom, (unsigned long long)tpl);

        // Tous les tableaux dynamiques du gabarit : pointeur + compte plausibles
        for (int off = 0; off + 16 <= 0x438; off += 8) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            if (!SafeRead((void*)(tpl + off), &arr, 8)) continue;
            if (!SafeRead((void*)(tpl + off + 8), &cnt, 4)) continue;
            if (!SafeRead((void*)(tpl + off + 12), &cap, 4)) continue;
            if (arr <= 0x10000 || cnt == 0 || cnt > 32 || cap < cnt || cap > 64) continue;

            printf("   +0x%03X : %u element(s) (cap %u) @0x%llX\n",
                off, cnt, cap, (unsigned long long)arr);

            // Contenu suppose : {pointeur, quantite} de 0x10 octets
            for (uint32_t k = 0; k < cnt && k < 6; ++k) {
                uintptr_t def = 0; int64_t q = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8);
                char rn[96] = { 0 };
                if (def > 0x10000) {
                    uintptr_t np = 0;
                    if (SafeRead((void*)def, &np, 8) && np > 0x10000)
                        SafeRead((void*)np, rn, sizeof(rn) - 1);
                    if (rn[0] < 0x20 || rn[0] > 0x7E) rn[0] = 0;
                }
                printf("      [%u] 0x%llX %-24s q=%lld\n",
                    k, (unsigned long long)def, rn, (long long)q);
            }
        }
        if (nomCherche && nomCherche[0]) return;      // un seul si cible
    }
}
// Etat des tableaux de contenu des sites : alloues ou non ?
// Un site dont le tableau existe deja peut recevoir notre item sans allocation.
// Etat des tableaux de contenu des sites : alloues ou non ?
void ScootingDumpSiteCargo()
{
    struct Champ { int off; const char* nom; };
    const Champ champs[4] = {
        { 0x048, "Resources"        },
        { 0x060, "BonusResources"   },
        { 0x078, "Technologies"     },
        { 0x090, "Artifacts"        },
    };

    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    printf("\n[SITES] tableaux de contenu\n");
    int vus = 0, alloues = 0;
    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        int32_t id = -1, st = -1;
        SafeRead((void*)(objs[i] + 0x30), &id, 4);
        SafeRead((void*)(objs[i] + 0x34), &st, 4);

        uintptr_t tpl = 0; char nom[128] = { 0 };
        SafeRead((void*)(objs[i] + 0x20), &tpl, 8);
        NomDuGabarit(tpl, nom, sizeof(nom));

        // Ligne compacte : un seul resume par site
        char resume[256] = { 0 }; int pos = 0; int nAlloc = 0;
        for (int c = 0; c < 4; ++c) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            SafeRead((void*)(objs[i] + champs[c].off), &arr, 8);
            SafeRead((void*)(objs[i] + champs[c].off + 8), &cnt, 4);
            SafeRead((void*)(objs[i] + champs[c].off + 12), &cap, 4);
            if (cnt > 64) cnt = 0;
            if (cap > 64) cap = 0;
            if (arr > 0x10000) nAlloc++;
            pos += _snprintf_s(resume + pos, sizeof(resume) - pos, _TRUNCATE,
                "%s n=%u/cap=%u%s  ", champs[c].nom, cnt, cap,
                (arr > 0x10000) ? "" : "(nul)");
        }
        if (nAlloc) alloues++;

        printf("  id=%-3d etat=%d %-34s %s\n",
            id, st, nom[0] ? nom : "(sans nom)", resume);

        // Detail du contenu quand il y en a
        for (int c = 0; c < 2; ++c) {
            uintptr_t arr = 0; uint32_t cnt = 0;
            SafeRead((void*)(objs[i] + champs[c].off), &arr, 8);
            SafeRead((void*)(objs[i] + champs[c].off + 8), &cnt, 4);
            if (arr <= 0x10000 || cnt == 0 || cnt > 8) continue;
            for (uint32_t k = 0; k < cnt; ++k) {
                uintptr_t def = 0; int64_t q = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8);
                char rn[64] = { 0 }; uintptr_t np = 0;
                if (def > 0x10000 && SafeRead((void*)def, &np, 8) && np > 0x10000)
                    SafeRead((void*)np, rn, sizeof(rn) - 1);
                printf("        %s[%u] %-20s x%lld\n", champs[c].nom, k, rn, (long long)q);
            }
        }
        vus++;
    }
    printf("[SITES] %d site(s), dont %d avec au moins un tableau alloue\n", vus, alloues);
}
// ============================================================================
//  Diagnostic
// ============================================================================

// Dump du contenu des cargaisons, pour identifier le format des elements.
void ScootingDumpCargo()
{
    static uintptr_t objs[64];
    int n = CollectObjectsByClass("FrostpunkExpeditionComponent", objs, 64);

    for (int i = 0; i < n; ++i) {
        if (!ExpValide(objs[i])) continue;

        int32_t id = -1;
        SafeRead((void*)(objs[i] + 0x30), &id, 4);

        struct Champ { int off; const char* nom; };
        const Champ champs[2] = {
            { 0x168, "CarriedResources" },
            { 0x180, "CarriedTechnologies" }
        };

        for (int ci = 0; ci < 2; ++ci) {
            const Champ& ch = champs[ci];
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            SafeRead((void*)(objs[i] + ch.off), &arr, 8);
            SafeRead((void*)(objs[i] + ch.off + 8), &cnt, 4);
            SafeRead((void*)(objs[i] + ch.off + 12), &cap, 4);
            if (!cnt || cnt > 16 || arr <= 0x10000) continue;

            printf("\n[CARGO] exp %d  %s : %u element(s) (capacite %u) @0x%llX\n",
                id, ch.nom, cnt, cap, (unsigned long long)arr);

            for (int off = 0; off < (int)cnt * 0x20 && off < 0x80; off += 8) {
                uint64_t v = 0;
                if (!SafeRead((void*)(arr + off), &v, 8)) break;

                char info[160] = { 0 };
                if (v > 0x10000 && v < 0x7FFFFFFFFFFF) {
                    uintptr_t p = 0; char s[64] = { 0 };
                    if (SafeRead((void*)v, &p, 8) && p > 0x10000 &&
                        SafeRead((void*)p, s, sizeof(s) - 1) &&
                        s[0] >= 0x20 && s[0] < 0x7F)
                        _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> def \"%s\"", s);
                    if (!info[0]) {
                        char cls[96] = { 0 };
                        if (GetClassNameFP((uintptr_t)v, cls, sizeof(cls)))
                            _snprintf_s(info, sizeof(info), _TRUNCATE, "  [%s]", cls);
                    }
                }
                printf("   +0x%02X = %016llX  (i32=%d)%s\n",
                    off, (unsigned long long)v, (int)(uint32_t)v, info);
            }
        }
    }
}

void ScootingDumpTreasure(const char* nomCherche)
{
    // 1) quelle classe porte un champ nomme "Treasure" ?
    FindPropertyByName("Treasure");
    ScanClassesDistinct("Treasure", 0);

    // 2) l'objet embarque du gabarit vise
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (nomCherche && nomCherche[0] && !StrEq(nom, nomCherche)) continue;

        uintptr_t tr = tpl + 0x118;      // EmbeddedObject : la structure est inline
        printf("\n[TREASURE] %s -> 0x%llX\n", nom, (unsigned long long)tr);

        char cls[128] = { 0 };
        if (GetClassNameFP(tr, cls, sizeof(cls))) printf("   classe = %s\n", cls);

        DumpObject(tr, 0x108);           // jusqu'au champ suivant connu (+0x220)

        // Tableaux dynamiques a l'interieur du butin
        printf("   tableaux :\n");
        for (int off = 0; off + 16 <= 0x108; off += 8) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            if (!SafeRead((void*)(tr + off), &arr, 8)) continue;
            if (!SafeRead((void*)(tr + off + 8), &cnt, 4)) continue;
            if (!SafeRead((void*)(tr + off + 12), &cap, 4)) continue;
            if (arr <= 0x10000 || cnt == 0 || cnt > 32 || cap < cnt || cap > 64) continue;

            printf("     +0x%03X : %u element(s) (cap %u) @0x%llX\n",
                off, cnt, cap, (unsigned long long)arr);
            for (uint32_t k = 0; k < cnt && k < 6; ++k) {
                uintptr_t def = 0; int64_t q = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8);
                char rn[96] = { 0 };
                if (def > 0x10000) {
                    uintptr_t np = 0;
                    if (SafeRead((void*)def, &np, 8) && np > 0x10000)
                        SafeRead((void*)np, rn, sizeof(rn) - 1);
                    if (rn[0] < 0x20 || rn[0] > 0x7E) rn[0] = 0;
                }
                printf("        [%u] 0x%llX %-24s q=%lld\n",
                    k, (unsigned long long)def, rn, (long long)q);
            }
        }
        if (nomCherche && nomCherche[0]) return;
    }
}



// Sonde legere du champ Treasure d'un gabarit de site. Aucun balayage memoire.
void ScootingProbeTreasure(const char* nomCherche)
{
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (nomCherche && nomCherche[0] && !StrEq(nom, nomCherche)) continue;

        printf("\n[TREASURE] %s  gabarit=0x%llX\n", nom, (unsigned long long)tpl);

        // Octets bruts de +0x110 a +0x220 : couvre Treasure et ses voisins
        for (int off = 0x110; off < 0x220; off += 8) {
            uint64_t v = 0;
            if (!SafeRead((void*)(tpl + off), &v, 8)) break;
            if (!v) continue;

            char info[160] = { 0 };
            if (v > 0x10000 && v < 0x7FFFFFFFFFFF) {
                char cls[96] = { 0 };
                if (GetClassNameFP((uintptr_t)v, cls, sizeof(cls)))
                    _snprintf_s(info, sizeof(info), _TRUNCATE, "  [%s]", cls);
                if (!info[0]) {
                    uintptr_t np = 0; char s[64] = { 0 };
                    if (SafeRead((void*)v, &np, 8) && np > 0x10000 &&
                        SafeRead((void*)np, s, sizeof(s) - 1) &&
                        s[0] >= 0x20 && s[0] < 0x7F)
                        _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> def \"%s\"", s);
                }
            }
            printf("   +0x%03X = %016llX  (i32=%d)%s\n",
                off, (unsigned long long)v, (int)(uint32_t)v, info);
        }
        if (nomCherche && nomCherche[0]) return;
    }
    printf("[TREASURE] site \"%s\" introuvable\n", nomCherche ? nomCherche : "?");
}

void ScootingDumpTreasureArrays(const char* nomCherche)
{
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (nomCherche && nomCherche[0] && !StrEq(nom, nomCherche)) continue;

        printf("\n[TREASURE] %s  gabarit=0x%llX\n", nom, (unsigned long long)tpl);

        // Le Treasure occupe +0x118 jusqu'au champ suivant (+0x220)
        for (int off = 0x118; off + 16 <= 0x220; off += 8) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            if (!SafeRead((void*)(tpl + off), &arr, 8)) continue;
            if (!SafeRead((void*)(tpl + off + 8), &cnt, 4)) continue;
            if (!SafeRead((void*)(tpl + off + 12), &cap, 4)) continue;
            if (arr <= 0x10000 || cnt == 0 || cnt > 32 || cap < cnt || cap > 64) continue;

            printf("   tableau a +0x%03X : %u element(s), capacite %u  @0x%llX\n",
                off, cnt, cap, (unsigned long long)arr);

            // On essaie les deux pas plausibles : 0x10 puis 0x08
            for (int pas = 0x10; pas >= 0x08; pas -= 0x08) {
                printf("      -- pas 0x%02X --\n", pas);
                for (uint32_t k = 0; k < cnt && k < 6; ++k) {
                    uintptr_t v0 = 0; int64_t v1 = 0;
                    SafeRead((void*)(arr + (uintptr_t)k * pas), &v0, 8);
                    if (pas == 0x10)
                        SafeRead((void*)(arr + (uintptr_t)k * pas + 8), &v1, 8);

                    char rn[96] = { 0 }; char cls[96] = { 0 };
                    if (v0 > 0x10000) {
                        uintptr_t np = 0;
                        if (SafeRead((void*)v0, &np, 8) && np > 0x10000)
                            SafeRead((void*)np, rn, sizeof(rn) - 1);
                        if (rn[0] < 0x20 || rn[0] > 0x7E) rn[0] = 0;
                        GetClassNameFP(v0, cls, sizeof(cls));
                    }
                    printf("         [%u] 0x%016llX %-22s %s q=%lld\n",
                        k, (unsigned long long)v0, rn,
                        cls[0] ? cls : "", (long long)v1);
                }
                if (pas == 0x10) break;      // si 0x10 donne des noms, inutile d'essayer 8
            }
        }
        if (nomCherche && nomCherche[0]) return;
    }
    printf("[TREASURE] site introuvable\n");
}

// Contenu du Treasure, avec resolution des GUID contre les definitions
// de ressources connues.
void ScootingResoudreTreasure(const char* nomCherche)
{
    // 1) table GUID -> nom, depuis le conteneur de ressources
    struct Res { uint64_t g[2]; char nom[64]; uintptr_t def; };
    Res res[16]; int nres = 0;

    if (EnsureResSelf()) {
        uintptr_t cont = 0;
        if (SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) && cont > 0x10000) {
            for (int i = 0; i < 12 && nres < 16; ++i) {
                uintptr_t def = 0;
                if (!SafeRead((void*)(cont + (uintptr_t)i * 0x70), &def, 8) || def <= 0x10000) continue;
                Res& r = res[nres];
                r.def = def; r.nom[0] = 0;
                SafeRead((void*)(def + 0x08), &r.g[0], 8);
                SafeRead((void*)(def + 0x10), &r.g[1], 8);
                uintptr_t np = 0;
                if (SafeRead((void*)def, &np, 8) && np > 0x10000)
                    SafeRead((void*)np, r.nom, sizeof(r.nom) - 1);
                if (r.nom[0]) nres++;
            }
        }
    }
    printf("[TREASURE] %d definition(s) de ressource connues\n", nres);
    for (int i = 0; i < nres; ++i)
        printf("   %-22s guid=%016llX%016llX\n", res[i].nom,
            (unsigned long long)res[i].g[1], (unsigned long long)res[i].g[0]);

    // 2) le Treasure du gabarit vise
    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (nomCherche && nomCherche[0] && !StrEq(nom, nomCherche)) continue;

        printf("\n[TREASURE] %s  gabarit=0x%llX\n", nom, (unsigned long long)tpl);

        for (int off = 0x118; off + 16 <= 0x220; off += 8) {
            uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
            if (!SafeRead((void*)(tpl + off), &arr, 8)) continue;
            if (!SafeRead((void*)(tpl + off + 8), &cnt, 4)) continue;
            if (!SafeRead((void*)(tpl + off + 12), &cap, 4)) continue;
            if (arr <= 0x10000 || cnt == 0 || cnt > 32 || cap < cnt || cap > 64) continue;

            printf("   tableau a +0x%03X : %u element(s), capacite %u  @0x%llX\n",
                off, cnt, cap, (unsigned long long)arr);

            for (uint32_t k = 0; k < cnt && k < 8; ++k) {
                uint64_t g[2] = {};
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &g[0], 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &g[1], 8);

                const char* trouve = "(non resolu)";
                for (int r = 0; r < nres; ++r)
                    if (res[r].g[0] == g[0] && res[r].g[1] == g[1]) { trouve = res[r].nom; break; }

                printf("      [%u] %016llX%016llX  -> %s\n",
                    k, (unsigned long long)g[1], (unsigned long long)g[0], trouve);
            }
        }
        if (nomCherche && nomCherche[0]) return;
    }
    printf("[TREASURE] site introuvable\n");
}

void ScootingTrouverParGuid(uint64_t g0, uint64_t g1)
{
    printf("\n[GUID] recherche de %016llX%016llX\n",
        (unsigned long long)g1, (unsigned long long)g0);

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi;
    int trouves = 0;

    while (trouves < 8 && VirtualQuery((void*)addr, &mbi, sizeof(mbi))) {
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= addr) break;

        bool lisible = (mbi.State == MEM_COMMIT) &&
            !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
        if (lisible && mbi.RegionSize <= 0x4000000) {
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; p + 16 <= next; p += 8) {
                uint64_t a = 0, b = 0;
                if (!SafeRead((void*)p, &a, 8) || a != g0) continue;
                if (!SafeRead((void*)(p + 8), &b, 8) || b != g1) continue;

                // Hypothese : GUID a +0x08 -> l'objet commence 8 octets avant
                uintptr_t obj = p - 8;
                printf("\n   occurrence a 0x%llX  (objet suppose 0x%llX)\n",
                    (unsigned long long)p, (unsigned long long)obj);

                char cls[128] = { 0 };
                if (GetClassNameFP(obj, cls, sizeof(cls)))
                    printf("      classe = %s\n", cls);

                for (int off = -8; off < 0x50; off += 8) {
                    uint64_t v = 0;
                    if (!SafeRead((void*)(obj + off), &v, 8)) break;
                    if (!v) continue;

                    char info[160] = { 0 };
                    if (v > 0x10000 && v < 0x7FFFFFFFFFFF) {
                        char s[96] = { 0 };
                        if (SafeRead((void*)v, s, sizeof(s) - 1) &&
                            s[0] >= 0x20 && s[0] < 0x7F && s[1])
                            _snprintf_s(info, sizeof(info), _TRUNCATE, "  -> \"%s\"", s);
                        if (!info[0]) {
                            char c2[96] = { 0 };
                            if (GetClassNameFP((uintptr_t)v, c2, sizeof(c2)))
                                _snprintf_s(info, sizeof(info), _TRUNCATE, "  [%s]", c2);
                        }
                    }
                    printf("      %+04d = %016llX  (i32=%d)%s\n",
                        off, (unsigned long long)v, (int)(uint32_t)v, info);
                }
                trouves++;
                if (trouves >= 8) break;
            }
        }
        addr = next;
    }
    printf("[GUID] %d occurrence(s)\n", trouves);
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
    if (!g_apDonneesActif) return;
    if (!ApPorteurDef()) { printf("[TRES] porteur non fabrique\n"); return; }

    uint64_t gr[2] = {};
    SafeRead((void*)(ApPorteurDef() + 0x08), &gr[0], 8);
    SafeRead((void*)(ApPorteurDef() + 0x10), &gr[1], 8);

    // GUID de la categorie de quantite "Tiny", relevee sur un site vanilla.
    static const uint64_t TINY0 = 0xBBD644503EBBE4F3ULL;
    static const uint64_t TINY1 = 0xE89FF91A05FD4629ULL;

    // Un element de tresor = 32 octets : {GUID ressource, GUID quantite}.
    // Un tampon par site configure, jamais libere : le jeu pourrait ecrire
    // dedans en consommant le butin, donc pas de partage.
    static uint64_t tampons[16][4];
    static int nTampons = 0;

    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);
    int faits = 0, fournis = 0;

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (!PayloadDuSite(nom)) continue;

        uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
        SafeRead((void*)(tpl + 0x1C0), &arr, 8);
        SafeRead((void*)(tpl + 0x1C0 + 8), &cnt, 4);
        SafeRead((void*)(tpl + 0x1C0 + 12), &cap, 4);

        if (arr > 0x10000 && cnt > 0 && cnt <= 32) {
            // Tresor existant : on ecrase le premier element et on reduit a 1.
            SafeWrite((void*)arr, &gr[0], 8);
            SafeWrite((void*)(arr + 8), &gr[1], 8);
            uint32_t un = 1;
            SafeWrite((void*)(tpl + 0x1C0 + 8), &un, 4);
            printf("[TRES] %s : tresor remplace\n", nom);
            faits++;
            continue;
        }

        // Tresor absent : on fournit le notre.
        if (nTampons >= 16) { printf("[TRES] %s : plus de tampon\n", nom); continue; }
        uint64_t* t = tampons[nTampons];
        t[0] = gr[0]; t[1] = gr[1];      // GUID de notre item
        t[2] = TINY0; t[3] = TINY1;      // quantite "Tiny"
        nTampons++;

        uintptr_t p = (uintptr_t)t;
        uint32_t un = 1;
        if (SafeWrite((void*)(tpl + 0x1C0), &p, 8) &&
            SafeWrite((void*)(tpl + 0x1C0 + 8), &un, 4) &&
            SafeWrite((void*)(tpl + 0x1C0 + 12), &un, 4)) {
            printf("[TRES] %s : tresor FOURNI a 0x%llX\n", nom, (unsigned long long)p);
            fournis++;
        }
        else {
            printf("[TRES] %s : ecriture du tresor refusee\n", nom);
            nTampons--;
        }
    }
    printf("[TRES] %d remplace(s), %d fourni(s)\n", faits, fournis);
}
void ScootingVerifierTresors()
{
    uintptr_t defAP = DefRessourceParNom(PORTEUR_AP);
    uint64_t gap[2] = {};
    if (defAP) {
        SafeRead((void*)(defAP + 0x08), &gap[0], 8);
        SafeRead((void*)(defAP + 0x10), &gap[1], 8);
    }

    static uintptr_t objs[256];
    int n = CollectObjectsByClass("FrostpunkExpeditionSiteComponent", objs, 256);

    // gabarits deja vus, pour reperer les doublons
    uintptr_t vus[64]; int nvus = 0;

    for (int i = 0; i < n; ++i) {
        if (!EstSiteComponent(objs[i])) continue;

        uintptr_t tpl = 0; char nom[128] = { 0 };
        if (!SafeRead((void*)(objs[i] + 0x20), &tpl, 8) || tpl <= 0x10000) continue;
        NomDuGabarit(tpl, nom, sizeof(nom));
        if (!PayloadDuSite(nom)) continue;

        bool deja = false;
        for (int k = 0; k < nvus; ++k) if (vus[k] == tpl) deja = true;
        if (!deja && nvus < 64) vus[nvus++] = tpl;

        int32_t id = -1, st = -1;
        SafeRead((void*)(objs[i] + 0x30), &id, 4);
        SafeRead((void*)(objs[i] + 0x34), &st, 4);

        uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
        SafeRead((void*)(tpl + 0x1C0), &arr, 8);
        SafeRead((void*)(tpl + 0x1C0 + 8), &cnt, 4);
        SafeRead((void*)(tpl + 0x1C0 + 12), &cap, 4);

        printf("\n[VERIF] %s  site id=%d etat=%d  gabarit=0x%llX%s\n",
            nom, id, st, (unsigned long long)tpl, deja ? "  (deja vu)" : "");
        printf("        tresor : n=%u cap=%u @0x%llX\n",
            cnt, cap, (unsigned long long)arr);
        if (arr <= 0x10000 || cnt == 0 || cnt > 32) continue;

        for (uint32_t k = 0; k < cnt && k < 4; ++k) {
            uint64_t g[2] = {};
            SafeRead((void*)(arr + (uintptr_t)k * 0x20), &g[0], 8);
            SafeRead((void*)(arr + (uintptr_t)k * 0x20 + 8), &g[1], 8);
            bool estAp = (defAP && g[0] == gap[0] && g[1] == gap[1]);
            printf("        [%u] %016llX%016llX %s\n", k,
                (unsigned long long)g[1], (unsigned long long)g[0],
                estAp ? "<-- PORTEUR" : "");
        }
    }
    printf("\n[VERIF] %d gabarit(s) distinct(s) pour les sites configures\n", nvus);
}

void TracerCargaison(uintptr_t comp, const char* prefixe)
{
    struct Champ { int off; const char* nom; };
    const Champ champs[2] = {
        { 0x168, "ressources" },
        { 0x180, "technos"    }
    };

    for (int ci = 0; ci < 2; ++ci) {
        uintptr_t arr = 0; uint32_t cnt = 0;
        SafeRead((void*)(comp + champs[ci].off), &arr, 8);
        SafeRead((void*)(comp + champs[ci].off + 8), &cnt, 4);
        if (arr <= 0x10000 || cnt == 0 || cnt > 16) continue;

        for (uint32_t k = 0; k < cnt; ++k) {
            if (ci == 0) {                       // {def, quantite} de 0x10
                uintptr_t def = 0; int64_t q = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8);
                char rn[64] = { 0 }; uintptr_t np = 0;
                if (def > 0x10000 && SafeRead((void*)def, &np, 8) && np > 0x10000)
                    SafeRead((void*)np, rn, sizeof(rn) - 1);
                printf("%s   %s[%u] %-20s x%lld\n", prefixe, champs[ci].nom, k,
                    rn[0] ? rn : "?", (long long)q);
            }
            else {                             // pointeurs de 8
                uintptr_t te = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 8), &te, 8);
                if (te <= 0x10000) continue;
                uintptr_t dsc = 0; char tn[64] = { 0 };
                SafeRead((void*)(te + 0x18), &dsc, 8);
                if (dsc > 0x10000) SafeRead((void*)dsc, tn, sizeof(tn) - 1);
                printf("%s   %s[%u] %s\n", prefixe, champs[ci].nom, k,
                    tn[0] ? tn : "?");
            }
        }
    }
}

void ScootingDumpRessources()
{
    if (!EnsureResSelf()) { printf("[RES] self introuvable\n"); return; }

    uintptr_t cont = 0;
    if (!SafeRead((void*)(g_selfRessource + RES_CONT_OFF), &cont, 8) || cont <= 0x10000) {
        printf("[RES] conteneur illisible\n"); return;
    }
    printf("\n[RES] conteneur 0x%llX\n", (unsigned long long)cont);

    for (int i = 0; i < 20; ++i) {
        uintptr_t e = cont + (uintptr_t)i * 0x70;
        uintptr_t def = 0;
        if (!SafeRead((void*)e, &def, 8)) break;

        if (def <= 0x10000) { printf("   [%2d] libre\n", i); continue; }

        uintptr_t np = 0; char nom[64] = { 0 };
        if (SafeRead((void*)def, &np, 8) && np > 0x10000)
            SafeRead((void*)np, nom, sizeof(nom) - 1);
        if (nom[0] < 0x20 || nom[0] > 0x7E) { printf("   [%2d] illisible\n", i); continue; }

        int32_t stock = 0;
        SafeRead((void*)(e + 0x08), &stock, 4);

        // Icone : chemin + coordonnees
        uintptr_t chemin = 0; char cp[96] = { 0 };
        int32_t col = -1, row = -1, nc = -1, nr = -1;
        SafeRead((void*)(def + 0x98), &chemin, 8);
        SafeRead((void*)(def + 0x98 + 8), &col, 4);
        SafeRead((void*)(def + 0x98 + 12), &row, 4);
        SafeRead((void*)(def + 0x98 + 16), &nc, 4);
        SafeRead((void*)(def + 0x98 + 20), &nr, 4);
        if (chemin > 0x10000) SafeRead((void*)chemin, cp, sizeof(cp) - 1);

        printf("   [%2d] %-22s stock=%-6d def=0x%llX  icone (%d,%d)/%dx%d  %s\n",
            i, nom, stock, (unsigned long long)def, col, row, nc, nr, cp);
    }
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

        //Ce bloc charge dans l'expédition les location dans l'inventaire du scout.
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

// Compte et capacite du conteneur de ressources.
// "Resources" est declaree a +0x2240 de FrostpunkEconomy et s'etend sur
// 24 octets : pointeur, compte, capacite.
void ScootingProbeConteneur()
{
    if (!EnsureResSelf()) { printf("[CONT] self introuvable\n"); return; }

    uintptr_t arr = 0; uint32_t cnt = 0, cap = 0; uint64_t reste = 0;
    SafeRead((void*)(g_selfRessource + 0x2240), &arr, 8);
    SafeRead((void*)(g_selfRessource + 0x2248), &cnt, 4);
    SafeRead((void*)(g_selfRessource + 0x224C), &cap, 4);
    SafeRead((void*)(g_selfRessource + 0x2250), &reste, 8);

    printf("\n[CONT] self=0x%llX\n", (unsigned long long)g_selfRessource);
    printf("[CONT] ptr=0x%llX  count=%u  capacite=%u  (+0x2250 = %016llX)\n",
        (unsigned long long)arr, cnt, cap, (unsigned long long)reste);

    if (arr <= 0x10000 || cnt == 0 || cnt > 64) { printf("[CONT] valeurs incoherentes\n"); return; }

    printf("[CONT] place libre : %d entree(s)\n", (int)cap - (int)cnt);

    // Contenu de l'emplacement suivant, s'il existe
    if (cap > cnt) {
        uintptr_t e = arr + (uintptr_t)cnt * 0x70;
        printf("[CONT] emplacement %u a 0x%llX :\n", cnt, (unsigned long long)e);
        for (int off = 0; off < 0x20; off += 8) {
            uint64_t v = 0;
            if (!SafeRead((void*)(e + off), &v, 8)) break;
            printf("        +0x%02X = %016llX\n", off, (unsigned long long)v);
        }
    }
}



static void ResoudreSelectionSystem()
{
    g_selSys = 0;
    static uintptr_t objs[16];
    int n = CollectObjectsByClass("FrostpunkSelectionSystem", objs, 16);
    for (int i = 0; i < n; ++i) {
        if (!ClasseExacte(objs[i], "FrostpunkSelectionSystem")) continue;
        uintptr_t arr = 0; uint32_t cnt = 0;
        if (!SafeRead((void*)(objs[i] + 0x2250), &arr, 8)) continue;
        if (!SafeRead((void*)(objs[i] + 0x2250 + 8), &cnt, 4)) continue;
        if (cnt <= 64) { g_selSys = objs[i]; break; }
    }
    printf("[SCOUT] selection system = 0x%llX\n", (unsigned long long)g_selSys);
}

// Expedition selectionnee, ou -1. Lecture seule : aucun scan.


// Recalcule la liste. Appelee une fois par tick, jamais depuis hkTranslate.
static void MettreAJourListePortee()
{
    // Le retour a la ligne n'est pas rendu par les infobulles : on separe
    // par une barre verticale.
    static const wchar_t* SEPARATEUR = L"\n";

    g_listePortee[0] = 0;
    g_nListePortee = 0;

    int cible = ExpeditionSelectionnee();
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

void ScootingEtatApresInit()
{
    printf("\n[ETAT] apres init : %d expedition(s)\n", g_nExps);
    for (int i = 0; i < g_nExps; ++i) {
        if (!g_exps[i].valide) continue;
        uintptr_t c = g_exps[i].comp;

        uintptr_t arr = 0; uint32_t cnt = 0, cap = 0;
        SafeRead((void*)(c + 0x168), &arr, 8);
        SafeRead((void*)(c + 0x168 + 8), &cnt, 4);
        SafeRead((void*)(c + 0x168 + 12), &cap, 4);

        printf("  exp %d  enAttente=%d  CarriedResources arr=0x%llX cnt=%u cap=%u\n",
            g_exps[i].id, g_exps[i].nEnAttente,
            (unsigned long long)arr, cnt, cap);

        if (arr > 0x10000 && cnt > 0 && cnt <= 16) {
            for (uint32_t k = 0; k < cnt; ++k) {
                uintptr_t def = 0; int64_t q = 0;
                SafeRead((void*)(arr + (uintptr_t)k * 0x10), &def, 8);
                SafeRead((void*)(arr + (uintptr_t)k * 0x10 + 8), &q, 8);
                char rn[64] = { 0 }; uintptr_t np = 0;
                if (def > 0x10000 && SafeRead((void*)def, &np, 8) && np > 0x10000)
                    SafeRead((void*)np, rn, sizeof(rn) - 1);
                printf("     [%u] def=0x%llX %-20s x%lld  %s\n",
                    k, (unsigned long long)def, rn[0] ? rn : "?", (long long)q,
                    (def == ApPorteurDef()) ? "<-- le notre" : "");
            }
        }
    }
}

static HANDLE TasDuJeu(uintptr_t pointeurConnu)
{
    static HANDLE cache = nullptr;
    if (cache) return cache;
    if (pointeurConnu <= 0x10000) return nullptr;

    HANDLE tas[64];
    DWORD n = GetProcessHeaps(64, tas);
    printf("[TAS] %lu tas dans le processus\n", n);

    for (DWORD i = 0; i < n; ++i) {
        if (HeapValidate(tas[i], 0, (void*)pointeurConnu)) {
            cache = tas[i];
            printf("[TAS] pointeur 0x%llX appartient au tas %p\n",
                (unsigned long long)pointeurConnu, tas[i]);
            return cache;
        }
    }
    printf("[TAS] tas introuvable pour 0x%llX\n", (unsigned long long)pointeurConnu);
    return nullptr;
}

