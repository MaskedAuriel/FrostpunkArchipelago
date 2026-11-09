// resources.cpp
#include "resources.h"
#include "MinHook.h"
#include "ap_ressources.h"
#include "MessageManager.h"
#include "groupes.h"
#include "evenements.h"
#include "dilemmes_ap.h"
#include "lois.h"         // LoisPompe (debug : pas de delai entre deux lois)
#include "lois_ap.h"      // LoisAPPompe (effets des lois en attente d'item)
#include "temperature.h"  // TemperaturePompe (montees de temperature AP)
#include "quetes.h"       // QuetesPompe (releve F9)
#include "victoire.h"     // VictoirePompe (fin de partie de test, Maj / Ctrl + F9)
#include "archipelago.h"  // ApItemsPompe (items recus, par scenario)

volatile bool g_traceRes = false;

typedef int32_t(*ModifyResource_t)(uintptr_t self, uintptr_t guidPtr,
    int32_t amount, uintptr_t p4,
    int32_t p5, bool p6);

typedef int     (*ResIdToIndex_t)(uintptr_t container, uintptr_t* pResId);

static ModifyResource_t oModifyResource = nullptr;
static ResIdToIndex_t   ResIdToIndex    = nullptr;

// --- Table des GUID connus (16 octets a resId+0x08, en ordre memoire) --------
struct KnownResource { const char* name; uint8_t guid[16]; };
static const KnownResource KNOWN[] = {
    { "ration",             { 0xFE,0xBC,0x57,0xFB, 0x94,0x6B,0xF4,0x90, 0x7B,0x48,0xB9,0x8B, 0x42,0x1B,0x7E,0x06 } },
    { "coal",               { 0x0B,0xCF,0x0B,0x94, 0xB9,0x43,0x37,0x85, 0x9C,0x44,0xBC,0xDF, 0x67,0xF9,0xBE,0xE1 } },
    { "wood",               { 0x66,0x05,0xA1,0xA7, 0x75,0x94,0xC3,0xB8, 0x0A,0x43,0xE6,0x27, 0x70,0xEB,0x23,0x42 } },
    { "steamcore",          { 0xD4,0xD2,0x23,0x36, 0x7A,0x20,0x42,0xB4, 0x4D,0x46,0x51,0x41, 0xB7,0xC4,0x47,0x2B } },
    { "steel",              { 0xD3,0xF7,0x9D,0x03, 0xA4,0xED,0x94,0x91, 0x8C,0x4B,0xFA,0xE9, 0x06,0x90,0xBA,0x67 } },
    { "rawfood",            { 0x75,0xB6,0x78,0xB1, 0x89,0xC7,0x33,0x98, 0x01,0x46,0x92,0x0C, 0x17,0x98,0x52,0x16 } },
    { "prosthesis",         { 0x35,0x8B,0xA1,0xC0, 0x33,0xF8,0xFA,0x91, 0x2E,0x4D,0x27,0xE0, 0xEF,0x42,0xD1,0xD3 } },
    { "structuralprofiles", { 0x07,0xB7,0x2F,0xF5, 0x5B,0xA8,0x02,0x84, 0x7C,0x44,0x06,0x25, 0xA8,0x33,0x13,0xB9 } },
    { "steelcomposite",     { 0x70,0xD2,0x93,0xB0, 0x13,0xAB,0xFA,0x91, 0x77,0x40,0xE9,0x28, 0x8F,0xAC,0x3F,0x55 } },
    { "steamexchanger",     { 0x8A,0x6C,0x63,0x63, 0x16,0xBF,0x21,0x86, 0xD4,0x46,0x71,0xD9, 0xF1,0xAE,0x48,0x4C } },
    { "corpse",             { 0x8D,0x2C,0x2A,0x33, 0xF6,0xBF,0x1D,0x8B, 0x30,0x4F,0xB0,0x97, 0x47,0x15,0xF7,0xDF } },
};
static const int KNOWN_COUNT = sizeof(KNOWN) / sizeof(KNOWN[0]);

// --- Contexte par ressource ---------------------------------------------------
// self et resId (2e argument = adresse du GUID = definition + 8) sont ANCRES a
// l'init de la partie depuis le conteneur de l'economie (Res_InitDepuisEconomie)
// : toutes les ressources sont pretes sans attendre que le jeu les touche.
// Res_Add credite avec p4 = 0, p5 = 0, p6 = false (voir le hook). arg4..arg6
// captures sur les vrais appels ne servent plus qu'au diagnostic.
struct ResCtx {
    uint8_t   guid[16];
    uintptr_t self;
    uintptr_t resId;
    uintptr_t arg4;
    int32_t   arg5;
    bool      arg6;
    bool      argsPropres;   // arg4..arg6 vus sur cette ressource
    bool      valid;         // self et resId connus
    int32_t   enAttente;     // credit demande par Res_Add, pas encore applique
};
static ResCtx g_res[32];
static int    g_resCount = 0;

// Arguments 4..6 du dernier appel du jeu, toutes ressources confondues.
static struct { uintptr_t arg4; int32_t arg5; bool arg6; bool valid; char source[24]; } g_argsRef = {};

// Releve des combinaisons d'arguments 4..6 vues (diagnostic : comprendre leur
// role pour choisir des valeurs sures). Une ligne par combinaison nouvelle.
struct ComboArgs { char classe4[48]; int32_t arg5; bool arg6; bool arg4Nul; };
static ComboArgs g_combos[64];
static int       g_nCombos = 0;

// Le detour tourne sur le thread du jeu, Res_* sur le notre : ils partagent g_res.
static CRITICAL_SECTION g_lock;
static bool g_lockReady = false;

static ResCtx* FindByGuid(const uint8_t* g)
{
    for (int i = 0; i < g_resCount; ++i)
        if (memcmp(g_res[i].guid, g, 16) == 0) return &g_res[i];
    return nullptr;
}

static const KnownResource* FindKnown(const char* name)
{
    for (int i = 0; i < KNOWN_COUNT; ++i)
        if (strcmp(KNOWN[i].name, name) == 0) return &KNOWN[i];
    return nullptr;
}

static const char* NomConnu(const uint8_t* g)
{
    for (int i = 0; i < KNOWN_COUNT; ++i)
        if (memcmp(KNOWN[i].guid, g, 16) == 0) return KNOWN[i].name;
    return "?";
}

static ResCtx* ContexteDe(const KnownResource* k)
{
    ResCtx* c = FindByGuid(k->guid);
    if (c || g_resCount >= 32) return c;
    c = &g_res[g_resCount++];
    memset(c, 0, sizeof(*c));
    memcpy(c->guid, k->guid, 16);
    return c;
}

// Convention de la cle (2e argument du setter) par rapport a la definition de
// la ressource : 0 = la cle EST la definition, 8 = la cle est son GUID
// (definition + 8). Deduite du premier appel du jeu (DeduireConventionCle) ;
// -1 tant qu'inconnue. Le 2026-10-27, la supposition "definition + 8" a fait
// lire n'importe quoi aux statistiques du setter (AV a Frostpunk.exe+174391D).
static int g_decalageCle = -1;

// Appelee par le hook, verrou pris, tant que la convention est inconnue.
static void DeduireConventionCle(uintptr_t self, uintptr_t cle)
{
    uintptr_t arr = 0; uint32_t cnt = 0;
    if (!SafeRead((void*)(self + RES_CONT_OFF), &arr, 8)) return;
    if (!SafeRead((void*)(self + RES_CONT_OFF + 8), &cnt, 4)) return;
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) return;
    for (uint32_t e = 0; e < cnt; ++e) {
        uintptr_t def = 0;
        if (!SafeRead((void*)(arr + (uintptr_t)e * RES_ENTRY_SIZE), &def, 8) || def <= 0x10000) continue;
        if (cle == def || cle == def + 8) {
            g_decalageCle = (int)(cle - def);
            JournalImmediat("[RES] convention de cle du jeu : definition + %d (cle 0x%llX, entree %u)\n",
                g_decalageCle, (unsigned long long)cle, e);
            return;
        }
    }
    static int signale = 0;
    if (signale++ < 5)
        JournalImmediat("[RES] cle du jeu 0x%llX : ni une definition ni definition + 8 du conteneur (self 0x%llX)\n",
            (unsigned long long)cle, (unsigned long long)self);
}

// Appelee par le hook, verrou pris. Reconnait la ressource par son GUID (a
// cle + 8 ou cle selon la convention), memorise les arguments 4..6 de l'appel
// et releve les combinaisons nouvelles.
static void CapturerContexte(uintptr_t self, uintptr_t guidPtr, int32_t amount,
    uintptr_t p4, int32_t p5, bool p6)
{
    if (g_decalageCle < 0) return;
    uint8_t g[16] = {};
    if (!SafeRead((void*)(guidPtr - (uintptr_t)g_decalageCle + 8), g, 16)) return;
    const KnownResource* k = nullptr;
    for (int i = 0; i < KNOWN_COUNT; ++i)
        if (memcmp(KNOWN[i].guid, g, 16) == 0) { k = &KNOWN[i]; break; }
    if (!k) return;

    // --- releve des combinaisons d'arguments ---
    char cls[48] = { 0 };
    if (p4 <= 0x10000 || !GetClassNameFP(p4, cls, sizeof(cls))) strcpy_s(cls, p4 ? "?" : "-");
    bool vu = false;
    for (int i = 0; i < g_nCombos && !vu; ++i)
        vu = StrEq(g_combos[i].classe4, cls) && g_combos[i].arg5 == p5 &&
             g_combos[i].arg6 == p6 && g_combos[i].arg4Nul == (p4 == 0);
    if (!vu && g_nCombos < 64) {
        ComboArgs& n = g_combos[g_nCombos++];
        strcpy_s(n.classe4, cls); n.arg5 = p5; n.arg6 = p6; n.arg4Nul = (p4 == 0);
        JournalImmediat("[RES] combinaison #%d : %s %+d -> self=0x%llX cle=0x%llX arg4=0x%llX (%s) arg5=%d arg6=%d\n",
            g_nCombos, k->name, amount, (unsigned long long)self, (unsigned long long)guidPtr,
            (unsigned long long)p4, cls, p5, p6 ? 1 : 0);
    }

    ResCtx* c = ContexteDe(k);
    if (!c) return;
    if (c->valid && (c->self != self || c->resId != guidPtr)) {
        JournalImmediat("[RES] %s : appel du jeu different de l'ancrage (self 0x%llX / 0x%llX, arg2 0x%llX / 0x%llX)\n",
            k->name, (unsigned long long)self, (unsigned long long)c->self,
            (unsigned long long)guidPtr, (unsigned long long)c->resId);
    }
    c->self = self;
    c->resId = guidPtr;
    c->arg4 = p4;
    c->arg5 = p5;
    c->arg6 = p6;
    c->argsPropres = true;
    c->valid = true;

    g_argsRef.arg4 = p4; g_argsRef.arg5 = p5; g_argsRef.arg6 = p6; g_argsRef.valid = true;
    strcpy_s(g_argsRef.source, k->name);
}

// --- Application d'un credit, protegee -------------------------------------
// Diagnostic du crash du 2026-10-27 (credit applique a la reprise apres pause,
// plantage sans trace : le relais du journal perd les dernieres lignes).
// Chaque etape est ecrite par JournalImmediat ; l'appel du setter est protege
// par SEH pour donner l'adresse fautive au lieu de planter.

static int FiltreCredit(EXCEPTION_POINTERS* ep, DWORD* code, uintptr_t* adresse)
{
    *code = ep->ExceptionRecord->ExceptionCode;
    *adresse = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}

static bool AppelSetterProtege(uintptr_t self, uintptr_t cle, int32_t q,
    int32_t* retour, DWORD* code, uintptr_t* adresse)
{
    __try {
        // Arguments fixes (decompiles du 2026-10-27) :
        //  p4 = 0     : aucune provenance (seuls les ecouteurs le lisent) ;
        //  p5 = 0     : categorie generique -> revenu si q > 0 ;
        //  p6 = false : comportement normal, notifications de seuil comprises.
        *retour = oModifyResource(self, cle, q, 0, 0, false);
        return true;
    }
    __except (FiltreCredit(GetExceptionInformation(), code, adresse)) {
        return false;
    }
}

static void AppliquerCredit(ResCtx& c, int32_t q)
{
    const char* nom = NomConnu(c.guid);
    uintptr_t modBase = (uintptr_t)GetModuleHandleW(NULL);

    // 0. Convention de cle pas encore vue sur un appel du jeu : on attend.
    if (g_decalageCle < 0) {
        c.enAttente += q;
        static int signale = 0;
        if (signale++ < 3)
            JournalImmediat("[RES] credit %s %+d differe : convention de cle du jeu pas encore connue\n", nom, q);
        return;
    }

    // 1. Entree du conteneur de cette ressource, trouvee par son GUID
    //    (definition + 8), sans appeler ResIdToIndex. La cle est ensuite
    //    construite selon la convention observee sur les appels du jeu.
    uintptr_t arr = 0; uint32_t cnt = 0;
    SafeRead((void*)(c.self + RES_CONT_OFF), &arr, 8);
    SafeRead((void*)(c.self + RES_CONT_OFF + 8), &cnt, 4);
    int index = -1;
    uintptr_t defTrouvee = 0;
    for (uint32_t e = 0; arr > 0x10000 && cnt <= 64 && e < cnt && index < 0; ++e) {
        uintptr_t def = 0; uint8_t g[16] = {};
        if (!SafeRead((void*)(arr + (uintptr_t)e * RES_ENTRY_SIZE), &def, 8) || def <= 0x10000) continue;
        if (SafeRead((void*)(def + 8), g, 16) && memcmp(g, c.guid, 16) == 0) {
            index = (int)e;
            defTrouvee = def;
        }
    }
    if (index < 0) {
        JournalImmediat("[RES] credit %s %+d ANNULE : GUID absent du conteneur (%u entree(s))\n",
            nom, q, cnt);
        return;
    }
    uintptr_t cle = defTrouvee + (uintptr_t)g_decalageCle;
    int32_t avant = 0;
    SafeRead((void*)(arr + (uintptr_t)index * RES_ENTRY_SIZE + RES_STOCK_OFF), &avant, 4);

    // 2. Ecouteurs de l'evenement "ressource modifiee" (eco+0x2108, nombre
    //    eco+0x2110, entrees {objet, fonction} de 0x10). BISSECTION du crash
    //    du 2026-10-27 : on les liste, puis on les coupe le temps du credit
    //    (nombre mis a 0, restaure apres). Si le crash disparait, le coupable
    //    est l'un d'eux.
    uintptr_t ecouteurs = 0; int32_t nEcouteurs = 0;
    SafeRead((void*)(c.self + 0x2108), &ecouteurs, 8);
    SafeRead((void*)(c.self + 0x2110), &nEcouteurs, 4);
    if (nEcouteurs < 0 || nEcouteurs > 64) nEcouteurs = 0;
    static bool listes = false;
    if (!listes) {
        listes = true;
        JournalImmediat("[RES] %d ecouteur(s) de l'evenement ressource :\n", nEcouteurs);
        for (int32_t e = 0; e < nEcouteurs; ++e) {
            uintptr_t ent[2] = {};
            SafeRead((void*)(ecouteurs + (uintptr_t)e * 0x10), ent, 16);
            char cls[96] = { 0 };
            if (ent[0] <= 0x10000 || !GetClassNameFP(ent[0], cls, sizeof(cls))) strcpy_s(cls, "?");
            JournalImmediat("   [%d] objet 0x%llX (%s)  fonction Frostpunk.exe+%llX\n", e,
                (unsigned long long)ent[0], cls,
                (unsigned long long)(ent[1] >= modBase ? ent[1] - modBase : ent[1]));
        }
    }
    // Bissection du 2026-10-28 : ecouteurs coupes -> credits OK ; le crash
    // restant venait d'une popup emise depuis le thread du mod (corrige :
    // file + PompeNotifications). On reactive les ecouteurs pour verifier
    // qu'ils n'etaient pas en cause. Remettre a true si le crash revient.
    const bool SANS_ECOUTEURS = false;
    int32_t zeroEcouteur = 0;
    if (SANS_ECOUTEURS) SafeWrite((void*)(c.self + 0x2110), &zeroEcouteur, 4);

    // 3. Appel du setter, protege.
    JournalImmediat("[RES] credit %s %+d : appel du setter (self 0x%llX, cle 0x%llX = definition + %d, index %d, stock %d, ecouteurs %s)\n",
        nom, q, (unsigned long long)c.self, (unsigned long long)cle, g_decalageCle, index, avant,
        SANS_ECOUTEURS ? "COUPES" : "actifs");
    int32_t r = 0; DWORD code = 0; uintptr_t adresse = 0;
    bool ok = AppelSetterProtege(c.self, cle, q, &r, &code, &adresse);
    if (SANS_ECOUTEURS) SafeWrite((void*)(c.self + 0x2110), &nEcouteurs, 4);
    if (!ok) {
        // L'evenement a ete interrompu en cours de diffusion : son compteur
        // d'invocation (+0x2120) est reste incremente. On le remet a 0.
        int16_t zero = 0;
        SafeWrite((void*)(c.self + 0x2120), &zero, 2);
        JournalImmediat("[RES] EXCEPTION 0x%08lX a 0x%llX (Frostpunk.exe+%llX) pendant le credit %s %+d"
            " -- compteur d'evenement remis a 0\n",
            code, (unsigned long long)adresse,
            (unsigned long long)(adresse >= modBase ? adresse - modBase : 0), nom, q);
        return;
    }
    int32_t apres = 0;
    SafeRead((void*)(arr + (uintptr_t)index * RES_ENTRY_SIZE + RES_STOCK_OFF), &apres, 4);
    JournalImmediat("[RES] credit AP applique : %s %+d (retour %d, stock %d -> %d)\n",
        nom, q, r, avant, apres);
}

// Adresse de la valeur : base = *(self+0x2240) ; addr = base + index*0x70 + 8
static bool ResolveAddr(ResCtx* c, uintptr_t& outAddr)
{
    if (!c || !c->valid || !ResIdToIndex) return false;
    uintptr_t cont = c->self + RES_CONT_OFF;
    uintptr_t base = 0;
    if (!SafeRead((void*)cont, &base, sizeof(base)) || !base) return false;

    uintptr_t resIdCopy = c->resId;      // ResIdToIndex attend un POINTEUR
    int index = ResIdToIndex(cont, &resIdCopy);
    if (index < 0) return false;

    outAddr = base + (uintptr_t)index * RES_ENTRY_SIZE + RES_STOCK_OFF;
    return true;
}



static int32_t hkModifyResource(uintptr_t self, uintptr_t guidPtr,
    int32_t amount, uintptr_t p4,
    int32_t p5, bool p6)
{
    // --- Garde-fou : notre ressource fictive ne doit JAMAIS atteindre le
    //     stock de la ville. Le jeu irait la chercher dans son conteneur,
    //     ou elle ne figure pas.
    if (ApPorteurEstNotreGuid(guidPtr)) {
        printf("[APRES] credit de %d intercepte et annule\n", amount);
        return 0;                    // zero unite appliquee : retour legitime
    }

    // --- Capture du contexte, puis credits en attente (Res_Add) -----------
    //     Tout se passe ici, sur le thread du jeu : on ne touche jamais
    //     l'economie depuis le thread du mod.
    if (g_lockReady && guidPtr > 0x10000) {
        EnterCriticalSection(&g_lock);
        if (g_decalageCle < 0) DeduireConventionCle(self, guidPtr);
        CapturerContexte(self, guidPtr, amount, p4, p5, p6);

        // Le setter diffuse un evenement aux ecouteurs (self+0x2108, compteur
        // d'invocation a self+0x2120, assert "Invoking an event again while
        // it is being handled!"). Si cet appel vient d'un ecouteur, on est
        // DANS l'evenement : on ne credite pas, on attend un appel de premier
        // niveau.
        int16_t invocations = 0;
        SafeRead((void*)(self + 0x2120), &invocations, 2);

        for (int i = 0; i < g_resCount && invocations == 0; ++i) {
            ResCtx& c = g_res[i];
            if (!c.valid || c.enAttente == 0) continue;
            int32_t q = c.enAttente;
            c.enAttente = 0;
            AppliquerCredit(c, q);
        }
        LeaveCriticalSection(&g_lock);

        // Popups AP en attente : emises ici, sur le thread du jeu et hors
        // evenement (jamais depuis le thread du mod).
        NoterThread("ModifyResource");
        if (invocations == 0) {
            PompeNotifications();
            CarteAssurerAcces();     // avant : un groupe ne part qu'avec la carte deverrouillee
            GroupesPompe();
            DilemmesPompe();
            LoisPompe();
            LoisAPPompe();
            TemperaturePompe();
            ApItemsPompe();          // apres LoisAPPompe / TemperaturePompe (parties relues)
            QuetesPompe();
            VictoirePompe();
        }
    }

    // --- Tracage optionnel des variations de stock ------------------------
    if (g_traceRes && guidPtr > 0x10000) {
        char nom[64] = { 0 };
        uintptr_t np = 0;
        // La definition commence 8 octets avant son GUID.
        if (SafeRead((void*)(guidPtr - 8), &np, 8) && np > 0x10000)
            SafeRead((void*)np, nom, sizeof(nom) - 1);
        if (nom[0] < 0x20 || nom[0] > 0x7E) nom[0] = 0;

        printf("[RES] %-20s %+6d\n", nom[0] ? nom : "?", amount);
    }

    return oModifyResource(self, guidPtr, amount, p4, p5, p6);
}

int Res_Get(const char* name)
{
    const KnownResource* k = FindKnown(name);
    if (!k || !g_lockReady) return -1;

    int result = -1;
    EnterCriticalSection(&g_lock);
    ResCtx* c = FindByGuid(k->guid);
    uintptr_t addr = 0;
    if (ResolveAddr(c, addr)) {
        int v = -1;
        if (SafeRead((void*)addr, &v, sizeof(v))) result = v;
    }
    LeaveCriticalSection(&g_lock);
    return result;
}

// Ajoute 'delta' a une ressource. Le credit est mis en attente et applique par
// le hook, sur le thread du jeu, au prochain appel de ModifyResource (6
// arguments : l'ancienne version n'en passait que 4, regle n.5). self et le
// GUID viennent de l'ancrage fait a l'init : toute ressource du conteneur est
// creditable, meme si le jeu ne l'a jamais touchee.
bool Res_Add(const char* name, int delta)
{
    const KnownResource* k = FindKnown(name);
    if (!k) { printf("[RES] ressource \"%s\" inconnue\n", name); return false; }
    if (!g_lockReady || !oModifyResource) return false;

    bool ok = false;
    EnterCriticalSection(&g_lock);
    ResCtx* c = FindByGuid(k->guid);
    if (c && c->valid) {
        c->enAttente += delta;
        ok = true;
    }
    LeaveCriticalSection(&g_lock);

    if (ok) printf("[RES] credit AP en attente : %s %+d\n", name, delta);
    else    printf("[RES] %s : absente du conteneur de l'economie de cette partie\n", name);
    return ok;
}

// Definition d'une ressource : resId (ancre a l'init) = definition + 8.
uintptr_t Res_Definition(const char* name)
{
    const KnownResource* k = name ? FindKnown(name) : nullptr;
    if (!g_lockReady || !k) return 0;
    uintptr_t def = 0;
    EnterCriticalSection(&g_lock);
    ResCtx* c = FindByGuid(k->guid);
    if (c && c->valid && c->resId > 0x10008) def = c->resId - 8;
    LeaveCriticalSection(&g_lock);
    return def;
}

bool Res_Guid(const char* name, uint8_t* out16)
{
    const KnownResource* k = name ? FindKnown(name) : nullptr;
    if (!k) return false;
    memcpy(out16, k->guid, 16);
    return true;
}

// Oublie les contextes : apres un chargement, self et les definitions changent.
void Res_OublierContextes()
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < g_resCount; ++i) {
        g_res[i].valid = false; g_res[i].argsPropres = false; g_res[i].enAttente = 0;
    }
    g_argsRef.valid = false;
    // g_decalageCle est une propriete du moteur, pas de la partie : conservee.
    LeaveCriticalSection(&g_lock);
}

// Ancrage a l'init de la partie : chaque entree du conteneur de l'economie
// (+0x2240, compte +0x2248, pas 0x70) porte en +0x00 la definition de la
// ressource ; son GUID est a definition + 8, c'est le 2e argument du setter.
// Renvoie le nombre de ressources connues ancrees.
int Res_InitDepuisEconomie()
{
    if (!g_lockReady) return 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL), eco = 0;
    if (!SafeRead((void*)(base + Adr(0x40916D0)), &eco, 8) || eco <= 0x10000) {
        printf("[RES] init : economie absente\n");
        return 0;
    }
    uintptr_t arr = 0; uint32_t cnt = 0;
    SafeRead((void*)(eco + RES_CONT_OFF), &arr, 8);
    SafeRead((void*)(eco + RES_CONT_OFF + 8), &cnt, 4);
    if (arr <= 0x10000 || cnt == 0 || cnt > 64) {
        printf("[RES] init : conteneur illisible (0x%llX, %u entree(s))\n", (unsigned long long)arr, cnt);
        return 0;
    }

    int ancrees = 0;
    char manquantes[256] = { 0 };
    EnterCriticalSection(&g_lock);
    for (int i = 0; i < KNOWN_COUNT; ++i) {
        bool trouvee = false;
        for (uint32_t e = 0; e < cnt && !trouvee; ++e) {
            uintptr_t def = 0; uint8_t g[16] = {};
            if (!SafeRead((void*)(arr + (uintptr_t)e * RES_ENTRY_SIZE), &def, 8) || def <= 0x10000) continue;
            if (!SafeRead((void*)(def + 8), g, 16) || memcmp(g, KNOWN[i].guid, 16) != 0) continue;
            ResCtx* c = ContexteDe(&KNOWN[i]);
            if (!c) break;
            c->self = eco;
            c->resId = def + 8;
            c->valid = true;
            trouvee = true;
        }
        if (trouvee) ancrees++;
        else { strcat_s(manquantes, " "); strcat_s(manquantes, KNOWN[i].name); }
    }
    LeaveCriticalSection(&g_lock);

    printf("[RES] init : %d/%d ressource(s) ancree(s) depuis l'economie 0x%llX (%u entree(s))%s%s\n",
        ancrees, KNOWN_COUNT, (unsigned long long)eco, cnt,
        manquantes[0] ? " -- absentes :" : "", manquantes);
    return ancrees;
}

void InstallResourceHooks(uintptr_t modBase)
{
    if (!g_lockReady) { InitializeCriticalSection(&g_lock); g_lockReady = true; }

    ResIdToIndex = reinterpret_cast<ResIdToIndex_t>(modBase + GameOffsets::ResIdToIndex);

    void* target = (void*)(modBase + GameOffsets::ModifyResource);
    if (MH_CreateHook(target, (void*)&hkModifyResource, (void**)&oModifyResource) != MH_OK
        || MH_EnableHook(target) != MH_OK)
        printf("[!] hook ModifyResource KO\n");
}
