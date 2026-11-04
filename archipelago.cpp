// archipelago.cpp -- surface de communication Archipelago <-> jeu
#include "archipelago.h"
#include "mod_common.h"
#include "tech_brain.h"
#include "resources.h"
#include "ap_data.h"
#include "MessageManager.h"
#include "lois_ap.h"
#include "temperature.h"
#include "scenarios_ap.h"
#include "scenario.h"
#include "dilemmes_ap.h"
#include "groupes.h"
#include "known_ap_ids.h"   // inclus ICI SEULEMENT (tables statiques)

// ============================================================================
//  Ids Archipelago (known_ap_ids.h, genere par apworld\outils\generer.py)
// ============================================================================

// 'cle' est-il l'un des noms de 'liste' (separes par '|') ? Sert aux quetes
// paralleles, dont la cle porte plusieurs noms internes.
static bool CleDansListe(const char* liste, const char* cle)
{
    size_t n = strlen(cle);
    for (const char* p = liste; p; ) {
        const char* fin = strchr(p, '|');
        size_t len = fin ? (size_t)(fin - p) : strlen(p);
        if (len == n && _strnicmp(p, cle, n) == 0) return true;
        p = fin ? fin + 1 : nullptr;
    }
    return false;
}

// Location AP du scenario en cours, par genre et cle. nullptr si absente.
static const ApLocationInfo* LocationAP(ApLocGenre genre, const char* cle)
{
    const char* scn = ScenarioVictoryId();
    if (!cle || !scn[0]) return nullptr;
    for (const ApLocationInfo& l : AP_LOCATIONS) {
        if (l.genre != genre || _stricmp(l.scenario, scn) != 0) continue;
        if (genre == ApLocGenre::Quete ? CleDansListe(l.cle, cle) : _stricmp(l.cle, cle) == 0)
            return &l;
    }
    return nullptr;
}

// Envoi d'une location au serveur. TODO reseau : transmettre l'id.
static void EnvoyerLocation(const ApLocationInfo* l)
{
    printf("[AP CHECK] location %lld \"%s\"\n", (long long)l->id, l->nom);
}

// Journal d'un check sans location AP dans ce scenario : rien n'est envoye.
static void LocationAbsente(const char* genre, const char* cle)
{
    printf("[AP CHECK] %s \"%s\" : aucune location AP dans le scenario \"%s\", rien envoye\n",
        genre, cle ? cle : "?", ScenarioVictoryId());
}

int64_t ApItemIdCourant(const char* genre, const char* cle)
{
    ApItemGenre g;
    if (_stricmp(genre, "tree") == 0) g = ApItemGenre::Arbre;
    else if (_stricmp(genre, "law") == 0) g = ApItemGenre::Loi;
    else if (_stricmp(genre, "temperature") == 0) g = ApItemGenre::Temperature;
    else return 0;
    const char* scn = ScenarioVictoryId();
    for (const ApItemInfo& it : AP_ITEMS) {
        if (it.genre != g || !it.scenario || _stricmp(it.scenario, scn) != 0) continue;
        if (!cle || _stricmp(it.cle, cle) == 0) return it.id;
    }
    return 0;
}

// Duree du trajet des groupes de livraison, en heures de jeu (option
// delivery_hours du slot ; 24 h par defaut, demande de l'auteur).
static int g_heuresLivraison = 24;

void ApReglerHeuresLivraison(int heures)
{
    g_heuresLivraison = heures > 0 ? heures : 24;
    printf("[AP ITEMS] duree de livraison des groupes : %d h\n", g_heuresLivraison);
}

// Contenu d'un site ou filler ("wood 144;workers 27") : UN groupe AP qui
// part d'un site et marche vers la ville, survivants par classe et
// ressources livres a son arrivee (groupes.cpp). Thread du jeu.
static bool LivrerContenu(const ApItemInfo& it)
{
    static const char* SURVIVANTS[] = { "workers", "engineers", "children", "automaton" };
    GroupeAP g = {};
    g.cheminTemplate = "GameEntities/WorldMap/SurvivorGroups/SurvivorsFromExpeditionSite";
    g.nom = it.nom;
    g.survivants = -1;
    // Arrivees echelonnees (demande de l'auteur, 2026-11-04) : le groupe
    // arrive g_heuresLivraison heures apres le dernier groupe AP en route
    // (24 h de delai, un groupe a 8 h -> 32 h, le suivant 56 h...).
    float attente = GroupesHeuresAvantDerniereArrivee();
    g.heures = attente + (float)g_heuresLivraison;
    g.sansMorts = true;

    char tmp[256], noms[MAX_CARGAISON_AP + MAX_CLASSES_AP][32], liste[512] = "";
    int nCargaison = 0, nClasses = 0, nNoms = 0;
    strcpy_s(tmp, it.cle);
    char* ctx = nullptr;
    for (char* tok = strtok_s(tmp, ";", &ctx); tok; tok = strtok_s(nullptr, ";", &ctx)) {
        char nom[32] = {};
        int quantite = 0;
        if (sscanf_s(tok, "%31s %d", nom, (unsigned)sizeof(nom), &quantite) != 2 || quantite <= 0) {
            printf("[AP ITEMS] item %lld : element \"%s\" illisible\n", (long long)it.id, tok);
            continue;
        }
        if (_stricmp(nom, "technology") == 0) {
            printf("[AP ITEMS] item %lld : %d technology ignoree\n", (long long)it.id, quantite);
            continue;
        }
        bool survivant = false;
        for (const char* s : SURVIVANTS) survivant |= _stricmp(s, nom) == 0;
        if (survivant ? nClasses >= MAX_CLASSES_AP : nCargaison >= MAX_CARGAISON_AP) {
            printf("[AP ITEMS] item %lld : %d %s en trop, ignore\n", (long long)it.id, quantite, nom);
            continue;
        }
        strcpy_s(noms[nNoms], nom);
        if (survivant) g.classes[nClasses++] = { noms[nNoms], quantite };
        else           g.cargaison[nCargaison++] = { noms[nNoms], quantite };
        ++nNoms;
        char morceau[48];
        sprintf_s(morceau, "%s%d %s", liste[0] ? ", " : "", quantite, nom);
        strcat_s(liste, morceau);
    }

    char description[600], texte[600];
    sprintf_s(description, "Heading to the city: %s.", liste);
    sprintf_s(texte, "An Archipelago convoy has reached the city: %s.", liste);
    g.description = description;
    g.dilemmeTitre = it.nom;
    g.dilemmeTexte = texte;
    g.dilemmeChoix = "Welcome the convoy";

    int slot = GroupeCreer(g);
    printf("[AP ITEMS] \"%s\" (%s) : groupe %s (slot %d, arrivee dans %.1f h = %.1f h d'attente + %d h)\n",
        it.nom, liste, slot >= 0 ? "parti" : "NON cree", slot, g.heures, attente, g_heuresLivraison);
    return slot >= 0;
}

// ============================================================================
//  Items par scenario (2026-11-03)
// ============================================================================
// Un item d'arbre, de loi ou de montee de temperature appartient a UN
// scenario. Il est note a la reception dans mods\ap_items_recus.txt (une
// ligne "<id>" par reception, tous scenarios confondus), quel que soit le
// scenario en cours, puis applique par ApItemsPompe dans toute partie de ce
// scenario : chaque module compte deja ses items par partie (sauvegardes
// locales par GameId) ; la pompe applique la difference. Une nouvelle partie
// du scenario recoit donc tous les items deja recus.
//
// Items livres (contenu de site, filler, piege ; point 3) : notes de meme,
// puis livres par la pompe (un groupe AP par contenu, une bulle par piege).
// - Contenu d'un site : lie au scenario du site (decision de l'auteur,
//   2026-11-03, option B ; AP_ITEMS.scenario renseigne par l'apworld),
//   livre dans chaque partie de ce scenario, comme les technos ; livraisons
//   comptees par partie dans mods\ap_items_livres_<GameId>.txt.
// - Filler, piege (scenario nullptr) : livres UNE fois, dans la partie en
//   cours quel que soit son scenario ; mods\ap_items_livres.txt.
// Une ligne "<id>" par livraison. Un item recu hors partie attend.
// TODO reseau : un fichier par graine / slot, dedoublonne par index d'item.

static const int AP_ITEM_COUNT = (int)(sizeof(AP_ITEMS) / sizeof(AP_ITEMS[0]));
static int     g_itemsRecus[AP_ITEM_COUNT];   // receptions par entree de AP_ITEMS
static int     g_itemsLivres[AP_ITEM_COUNT];  // items sans scenario : livraisons faites
static int     g_livresPartie[AP_ITEM_COUNT]; // items d'un scenario : livraisons dans la partie
static char    g_fichierPartie[64] = "";      // "ap_items_livres_<GameId>.txt", vide hors partie
static bool    g_itemsLus = false;
static SRWLOCK g_verrouItems = SRWLOCK_INIT;

static bool EstItemDeScenario(const ApItemInfo& it)
{
    return it.scenario && (it.genre == ApItemGenre::Arbre || it.genre == ApItemGenre::Loi ||
        it.genre == ApItemGenre::Temperature);
}

static bool EstItemCommun(const ApItemInfo& it)
{
    return it.genre == ApItemGenre::Site || it.genre == ApItemGenre::Filler || it.genre == ApItemGenre::Piege;
}

static bool CheminItems(char* out, size_t taille, const char* fichier)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\%s", base, fichier);
    return true;
}

// Une ligne "<id>" par evenement : compte par entree de AP_ITEMS.
static int LireComptes(const char* fichier, int* comptes)
{
    memset(comptes, 0, sizeof(int) * AP_ITEM_COUNT);
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminItems(chemin, sizeof(chemin), fichier) || fopen_s(&f, chemin, "r") != 0 || !f) return 0;
    char ligne[64];
    int n = 0;
    while (fgets(ligne, sizeof(ligne), f)) {
        long long id = 0;
        if (sscanf_s(ligne, "%lld", &id) != 1) continue;
        for (int i = 0; i < AP_ITEM_COUNT; ++i)
            if (AP_ITEMS[i].id == id) { ++comptes[i]; ++n; break; }
    }
    fclose(f);
    return n;
}

static void AjouterLigne(const char* fichier, int index)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (CheminItems(chemin, sizeof(chemin), fichier) && fopen_s(&f, chemin, "a") == 0 && f) {
        fprintf(f, "%lld\n", (long long)AP_ITEMS[index].id);
        fclose(f);
    } else printf("[AP ITEMS] ecriture de mods\\%s impossible\n", fichier);
}

// Sous le verrou exclusif.
static void ChargerItemsSiBesoin()
{
    if (g_itemsLus) return;
    g_itemsLus = true;
    int n = LireComptes("ap_items_recus.txt", g_itemsRecus);
    int l = LireComptes("ap_items_livres.txt", g_itemsLivres);
    printf("[AP ITEMS] %d item(s) deja recu(s), %d item(s) commun(s) deja livre(s)\n", n, l);
}

// Note une reception. Renvoie le nombre de receptions de cet item.
static int NoterItemRecu(int index)
{
    AcquireSRWLockExclusive(&g_verrouItems);
    ChargerItemsSiBesoin();
    int n = ++g_itemsRecus[index];
    AjouterLigne("ap_items_recus.txt", index);
    ReleaseSRWLockExclusive(&g_verrouItems);
    return n;
}

static void NoterItemLivre(int index)
{
    AcquireSRWLockExclusive(&g_verrouItems);
    if (AP_ITEMS[index].scenario) {
        ++g_livresPartie[index];
        AjouterLigne(g_fichierPartie, index);
    } else {
        ++g_itemsLivres[index];
        AjouterLigne("ap_items_livres.txt", index);
    }
    ReleaseSRWLockExclusive(&g_verrouItems);
}

// Nouvelle partie : livraisons deja faites dans cette partie (items lies a
// un scenario). Thread du jeu.
static void ChargerLivresPartie(const uint64_t gid[2])
{
    AcquireSRWLockExclusive(&g_verrouItems);
    sprintf_s(g_fichierPartie, "ap_items_livres_%016llX%016llX.txt",
        (unsigned long long)gid[1], (unsigned long long)gid[0]);
    int n = LireComptes(g_fichierPartie, g_livresPartie);
    ReleaseSRWLockExclusive(&g_verrouItems);
    printf("[AP ITEMS] partie %016llX%016llX : %d contenu(s) de site deja livre(s)\n",
        (unsigned long long)gid[1], (unsigned long long)gid[0], n);
}

// Livre un item commun dans la partie en cours. Thread du jeu.
static bool LivrerItemCommun(const ApItemInfo& it)
{
    if (it.genre == ApItemGenre::Piege) {
        bool ok = DilemmeAPDemander(it.cle);
        printf("[AP ITEMS] piege \"%s\" (%s) : %s\n", it.nom, it.cle, ok ? "bulle demandee" : "REFUSE");
        return ok;
    }
    return LivrerContenu(it);
}

// Items de la partie 'gid' deja appliques (compte du module), -1 si le
// module n'a pas encore relu la sauvegarde locale de cette partie.
static int ItemsAppliques(const ApItemInfo& it, const uint64_t gid[2])
{
    switch (it.genre) {
    case ApItemGenre::Arbre:       return TechProgressivesRecues(atoi(it.cle), gid);
    case ApItemGenre::Loi:         return LoisAPItemsRecus(it.cle, gid);
    case ApItemGenre::Temperature: return TemperatureItemsRecus(gid);
    default:                       return -1;
    }
}

// Applique un item. 'silencieux' : effet seul (rattrapage au chargement,
// une popup de synthese), sinon le Receive* et sa popup.
static bool AppliquerItem(const ApItemInfo& it, bool silencieux)
{
    switch (it.genre) {
    case ApItemGenre::Arbre:
        return silencieux ? OnProgressiveReceived(atoi(it.cle)) : ReceiveProgressive(atoi(it.cle), "Archipelago");
    case ApItemGenre::Loi:
        return silencieux ? LoisAPRecevoir(it.cle, nullptr) : ReceiveLawProgressive(it.cle, "Archipelago");
    case ApItemGenre::Temperature:
        if (!silencieux) return ReceiveTemperatureRise(1, "Archipelago");
        TemperatureRecevoirMontee(1);
        return true;
    default:
        return false;
    }
}

void ApItemsPompe()
{
    static uint64_t partieVue[2] = {};
    static bool silencieux = true;
    static bool refuse[AP_ITEM_COUNT];   // refuse dans cette partie : plus retente

    if (!PartiePrete()) return;
    uint64_t gid[2] = {};
    uintptr_t scn = SystemeParId(IdSysteme::Scenario);
    if (!scn || !SafeRead((void*)(scn + 0x110), gid, sizeof(gid)) || (!gid[0] && !gid[1])) return;
    // g_run (thread du mod) doit decrire cette partie : sinon son scenario
    // peut etre celui de la precedente.
    if (!g_run.valid || g_run.gameId[0] != gid[0] || g_run.gameId[1] != gid[1]) return;
    if (gid[0] != partieVue[0] || gid[1] != partieVue[1]) {
        partieVue[0] = gid[0];
        partieVue[1] = gid[1];
        silencieux = true;   // rattrapage de la nouvelle partie : une seule popup
        memset(refuse, 0, sizeof(refuse));
        ChargerLivresPartie(gid);
    }

    const char* scenario = g_run.scenario;
    int appliques = 0;
    bool pasPret = false;
    AcquireSRWLockExclusive(&g_verrouItems);
    ChargerItemsSiBesoin();
    int recus[AP_ITEM_COUNT], livres[AP_ITEM_COUNT];
    memcpy(recus, g_itemsRecus, sizeof(recus));
    for (int i = 0; i < AP_ITEM_COUNT; ++i)
        livres[i] = AP_ITEMS[i].scenario ? g_livresPartie[i] : g_itemsLivres[i];
    ReleaseSRWLockExclusive(&g_verrouItems);

    // Items livres : un seul par passage (un groupe a la fois) ; apres un
    // echec (plus de slot de groupe...), nouvel essai 5 s plus tard. Contenu
    // d'un site : seulement dans une partie de son scenario.
    static DWORD prochainEssai = 0;
    if (GetTickCount() >= prochainEssai) {
        for (int i = 0; i < AP_ITEM_COUNT; ++i) {
            if (!EstItemCommun(AP_ITEMS[i]) || livres[i] >= recus[i]) continue;
            if (AP_ITEMS[i].scenario && _stricmp(AP_ITEMS[i].scenario, scenario) != 0) continue;
            if (LivrerItemCommun(AP_ITEMS[i])) NoterItemLivre(i);
            else prochainEssai = GetTickCount() + 5000;
            break;
        }
    }

    for (int i = 0; i < AP_ITEM_COUNT; ++i) {
        const ApItemInfo& it = AP_ITEMS[i];
        if (!recus[i] || refuse[i] || !EstItemDeScenario(it) || _stricmp(it.scenario, scenario) != 0) continue;
        int faits = ItemsAppliques(it, gid);
        if (faits < 0) { pasPret = true; continue; }
        for (; faits < recus[i]; ++faits) {
            if (!AppliquerItem(it, silencieux)) {
                printf("[AP ITEMS] \"%s\" : %d/%d applique(s), refuse (arbre epuise ?)\n", it.nom, faits, recus[i]);
                refuse[i] = true;
                break;
            }
            ++appliques;
        }
    }

    if (appliques && silencieux) {
        printf("[AP ITEMS] partie %s : %d item(s) deja recu(s) appliques\n", scenario, appliques);
        char texte[96];
        sprintf_s(texte, "%d item(s) received earlier applied to this game", appliques);
        AfficherNotificationAP("Archipelago items", texte);
    }
    if (!pasPret) silencieux = false;   // rattrapage termine : popups normales
}

int64_t ApItemIdAutreScenarioDeTest()
{
    const char* scn = ScenarioVictoryId();
    int candidats[AP_ITEM_COUNT], n = 0;
    for (int i = 0; i < AP_ITEM_COUNT; ++i)
        if (AP_ITEMS[i].genre == ApItemGenre::Arbre && _stricmp(AP_ITEMS[i].scenario, scn) != 0)
            candidats[n++] = i;
    return n ? AP_ITEMS[candidats[rand() % n]].id : 0;
}

// ============================================================================
//  Serveur -> jeu
// ============================================================================

bool ReceiveItem(int64_t itemId, const char* fromPlayer)
{
    int index = -1;
    for (int i = 0; i < AP_ITEM_COUNT; ++i)
        if (AP_ITEMS[i].id == itemId) { index = i; break; }
    if (index < 0) {
        printf("[AP RECU] item %lld de %s : id inconnu, ignore\n",
            (long long)itemId, fromPlayer ? fromPlayer : "?");
        return false;
    }
    const ApItemInfo* it = &AP_ITEMS[index];
    printf("[AP RECU] item %lld \"%s\" (de %s)\n", (long long)itemId, it->nom, fromPlayer ? fromPlayer : "?");

    // Item d'un scenario : note, puis applique par ApItemsPompe dans toute
    // partie de ce scenario (tout de suite si c'est la partie en cours).
    if (EstItemDeScenario(*it)) {
        int n = NoterItemRecu(index);
        printf("[AP RECU] item %lld : %d recu(s) pour \"%s\"%s\n", (long long)itemId, n, it->scenario,
            _stricmp(it->scenario, ScenarioVictoryId()) == 0 ? "" : " (autre scenario : applique dans ses parties)");
        return true;
    }

    // Contenu de site (lie a son scenario), filler, piege : note, puis livre
    // par ApItemsPompe.
    if (EstItemCommun(*it)) {
        int n = NoterItemRecu(index);
        if (it->scenario)
            printf("[AP RECU] item %lld : %d recu(s) pour \"%s\", livre dans chaque partie de ce scenario%s\n",
                (long long)itemId, n, it->scenario,
                _stricmp(it->scenario, ScenarioVictoryId()) == 0 ? "" : " (autre scenario)");
        else
            printf("[AP RECU] item %lld : %d recu(s), livre dans la partie en cours%s\n", (long long)itemId, n,
                PartiePrete() ? "" : " (des qu'une partie est prete)");
        return true;
    }

    if (it->genre == ApItemGenre::Scenario) return ReceiveScenarioUnlock(it->cle, fromPlayer);
    return false;
}

int64_t ApItemCommunDeTest()
{
    // A tour de role : un contenu de site du scenario en cours avec des
    // survivants, un autre avec des ressources, le filler "Wood Crate", le
    // piege "Wood Trap". Tant que known_ap_ids.h ne lie pas les sites a leur
    // scenario (apworld a regenerer), contenus de test fixes (A New Home).
    static int k = 0;
    const char* scn = ScenarioVictoryId();
    int64_t id = 0;
    if (k < 2) {
        int candidats[AP_ITEM_COUNT], n = 0;
        for (int i = 0; i < AP_ITEM_COUNT; ++i) {
            const ApItemInfo& it = AP_ITEMS[i];
            if (it.genre != ApItemGenre::Site || !it.scenario || _stricmp(it.scenario, scn) != 0) continue;
            bool survivants = strstr(it.cle, "workers") || strstr(it.cle, "engineers") || strstr(it.cle, "children");
            if (survivants == (k == 0)) candidats[n++] = i;
        }
        id = n ? AP_ITEMS[candidats[rand() % n]].id : (k == 0 ? 7748500104LL : 7748500001LL);
    }
    else id = k == 2 ? 7748600001LL : 7748700001LL;
    k = (k + 1) % 4;
    return id;
}

bool ReceiveProgressive(int tree, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] progressive arbre %d de %s : partie pas prete, ignore\n",
            tree, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : la techno suivante de l'arbre passe a "recue".
    const char* tech = nullptr;
    if (!OnProgressiveReceived(tree, &tech)) {
        printf("[AP RECU] progressive arbre %d de %s : arbre epuise ou hors bornes\n",
            tree, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%s (from %s)", tech ? tech : "?", fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Technology received", texte);

    printf("[AP RECU] progressive arbre %d -> %s (de %s)\n",
        tree, tech ? tech : "?", fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveResource(const char* resource, int amount, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] %d %s de %s : partie pas prete, ignore\n",
            amount, resource, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : credit mis en attente, applique par le hook du setter.
    if (!Res_Add(resource, amount)) {
        printf("[AP RECU] %d %s de %s : credit refuse\n",
            amount, resource, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%+d %s (from %s)", amount, resource, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Resource received", texte);

    printf("[AP RECU] %+d %s (de %s)\n", amount, resource, fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveLawProgressive(const char* tree, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] loi, arbre %s de %s : partie pas prete, ignore\n",
            tree ? tree : "?", fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : l'arbre compte un item de plus ; la loi signee a ce rang
    //    recoit son effet (applique par LoisAPPompe, thread du jeu).
    int rang = 0;
    if (!LoisAPRecevoir(tree, &rang)) {
        printf("[AP RECU] loi, arbre %s de %s : arbre inconnu\n",
            tree ? tree : "?", fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[200];
    sprintf_s(texte, "%s #%d (from %s)", tree, rang, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Law received", texte);

    printf("[AP RECU] loi : arbre %s, item %d (de %s)\n", tree, rang, fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveTemperatureRise(int levels, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] montee de temperature (%+d) de %s : partie pas prete, ignore\n",
            levels, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : decalage Archipelago augmente ; recalcul par TemperaturePompe.
    TemperatureRecevoirMontee(levels);

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%+d degrees (from %s)", levels * 10, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Temperature rise", texte);

    printf("[AP RECU] montee de temperature : %+d niveau(x) (de %s)\n", levels, fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveScenarioUnlock(const char* scenario, const char* fromPlayer)
{
    // 1. Effet : le menu des scenarios lit ScenarioDebloque a chaque ouverture.
    int r = ScenarioDebloquer(scenario);
    if (r < 0) {
        printf("[AP RECU] deblocage du scenario \"%s\" de %s : scenario inconnu, ignore\n",
            scenario ? scenario : "?", fromPlayer ? fromPlayer : "?");
        return false;
    }
    printf("[AP RECU] scenario \"%s\" debloque (de %s)%s\n", scenario, fromPlayer ? fromPlayer : "?",
        r == 0 ? " : deja debloque" : "");

    // 2. Popup, seulement en partie (au menu, aucun message ne s'affiche).
    if (r == 1 && PartiePrete()) {
        char texte[160];
        sprintf_s(texte, "%s (from %s)", scenario, fromPlayer ? fromPlayer : "?");
        AfficherNotificationAP("Scenario unlocked", texte);
    }
    return true;
}

// ============================================================================
//  Jeu -> serveur
// ============================================================================

void SendResearchCheck(uint64_t techHash)
{
    // 1. Location AP de cette techno (cle = hash en hexadecimal).
    char cle[20];
    sprintf_s(cle, "%016llX", (unsigned long long)techHash);
    const ApLocationInfo* l = LocationAP(ApLocGenre::Techno, cle);
    if (!l) { LocationAbsente("recherche", cle); return; }

    // 2. Ce que contient cette location (pour l'instant : savoir local).
    char texte[160];
    AP_LocationText(techHash, texte, sizeof(texte));

    // 3. Envoi.
    printf("[AP CHECK] recherche %s -> %s\n", cle, texte);
    EnvoyerLocation(l);

    // 4. Popup.
    AfficherNotificationAP("Check sent", texte);
}

void ApCleDeSite(const char* site, char* out, int taille)
{
    // Paires exclusives de Refugees ([ref]Trail_Bend / [ref]Trail_Bend-dead,
    // Black_Spire, White_Pass) : un seul des deux sites apparait, un seul
    // check, cle = le nom sans "-dead".
    strncpy_s(out, taille, site ? site : "", _TRUNCATE);
    size_t n = strlen(out);
    if (n > 5 && _stricmp(out + n - 5, "-dead") == 0) out[n - 5] = 0;
}

int ApSitesLocations(ApSiteLoc* out, int max)
{
    int n = 0;
    for (const ApLocationInfo& l : AP_LOCATIONS)
        if (l.genre == ApLocGenre::Site && n < max) out[n++] = { l.scenario, l.cle, l.nom, l.id };
    return n;
}

void SendLocationCheck(int locationId, const char* site)
{
    // 1. Location AP du site (cle = nom interne, SITE_PAYLOADS ; paire
    //    exclusive : nom sans "-dead").
    char cle[128];
    ApCleDeSite(site, cle, sizeof(cle));
    const ApLocationInfo* l = LocationAP(ApLocGenre::Site, cle);
    if (!l) { LocationAbsente("site", site); return; }

    const ApCheck* ac = ApCheckById(locationId);
    const char* texte = ac ? ac->description : "(unknown content)";

    // 2. Envoi.
    printf("[AP CHECK] site \"%s\" (id local %d) -> %s\n", site, locationId, texte);
    EnvoyerLocation(l);

    // 3. Popup.
    AfficherNotificationAP("Check sent", texte);

    // TEMPORAIRE : simule la reponse du serveur quand l'item est pour nous.
    if (ac && ac->arbreTest >= 0)
        ReceiveProgressive(ac->arbreTest, ac->joueur);
}

void SendLawCheck(const char* tree, int rank, const char* lawName, bool effectPending)
{
    // 1. Location AP du rang (cle = "<arbre>|<rang>").
    char cle[128];
    sprintf_s(cle, "%s|%d", tree, rank);
    const ApLocationInfo* l = LocationAP(ApLocGenre::Loi, cle);
    if (!l) { LocationAbsente("loi", cle); return; }

    // 2. Envoi.
    printf("[AP CHECK] loi \"%s\" -> arbre %s, rang %d%s\n", lawName, tree, rank,
        effectPending ? " (effet en attente)" : "");
    EnvoyerLocation(l);

    // 3. Popup.
    char texte[200];
    sprintf_s(texte, "Law: %s (%s #%d)%s", lawName, tree, rank,
        effectPending ? " - effect pending" : "");
    AfficherNotificationAP("Check sent", texte);
}

void SendQuestCheck(const char* questName, const char* displayName)
{
    // 1. Location AP de la quete (quetes paralleles : une seule location).
    const ApLocationInfo* l = LocationAP(ApLocGenre::Quete, questName);
    if (!l) { LocationAbsente("quete", questName); return; }

    // 2. Envoi.
    printf("[AP CHECK] quete \"%s\" (\"%s\") accomplie\n", questName, displayName);
    EnvoyerLocation(l);

    // 3. Popup.
    char texte[200];
    sprintf_s(texte, "Quest: %s", displayName && displayName[0] && displayName[0] != '(' ? displayName : questName);
    AfficherNotificationAP("Check sent", texte);
}

int64_t ApQueteLocation(const char* questName)
{
    const ApLocationInfo* l = LocationAP(ApLocGenre::Quete, questName);
    return l ? l->id : 0;
}

void SendTemperatureCheck(int rank)
{
    // 1. Location AP de la montee (cle = rang).
    char cle[16];
    sprintf_s(cle, "%d", rank);
    const ApLocationInfo* l = LocationAP(ApLocGenre::Temperature, cle);
    if (!l) { LocationAbsente("montee de temperature", cle); return; }

    // 2. Envoi.
    printf("[AP CHECK] montee de temperature %d retiree\n", rank);
    EnvoyerLocation(l);

    // 3. Popup.
    char texte[160];
    sprintf_s(texte, "Temperature level #%d held back", rank);
    AfficherNotificationAP("Check sent", texte);
}

// --- Goal : victoire dans tous les scenarios du slot -------------------------
// Victoires comptees (nom interne, une ligne par scenario) dans
// mods\ap_victoires.txt, cumulatif, tous scenarios. Scenarios du goal :
// option "scenarios" du slot (ApReglerScenariosGoal, slot_data) ; en
// attendant, A New Home seul (defaut du YAML) -- TEMPORAIRE.
// TODO reseau : un fichier par graine / slot.
static char g_scenariosGoal[6][16] = { "DEFAULT" };
static int  g_nScenariosGoal = 1;
static bool g_goalEnvoye = false;

void ApReglerScenariosGoal(const char* const* scenarios, int n)
{
    g_nScenariosGoal = 0;
    for (int i = 0; i < n && g_nScenariosGoal < 6; ++i)
        strcpy_s(g_scenariosGoal[g_nScenariosGoal++], scenarios[i]);
    printf("[AP GOAL] %d scenario(s) a gagner\n", g_nScenariosGoal);
}

static bool CheminVictoires(char* out, size_t taille)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_victoires.txt", base);
    return true;
}

// Scenario deja gagne (fichier relu a chaque appel : rare).
static bool ScenarioGagne(const char* scenario)
{
    char chemin[MAX_PATH], ligne[64];
    FILE* f = nullptr;
    if (!CheminVictoires(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) return false;
    bool trouve = false;
    while (!trouve && fgets(ligne, sizeof(ligne), f)) {
        ligne[strcspn(ligne, "\r\n")] = 0;
        trouve = _stricmp(ligne, scenario) == 0;
    }
    fclose(f);
    return trouve;
}

void SendScenarioVictory(const char* scenario, const char* displayName)
{
    // 1. Evenement du goal : la victoire du scenario est notee (une fois).
    bool deja = ScenarioGagne(scenario);
    if (!deja) {
        char chemin[MAX_PATH];
        FILE* f = nullptr;
        if (CheminVictoires(chemin, sizeof(chemin)) && fopen_s(&f, chemin, "a") == 0 && f) {
            fprintf(f, "%s\n", scenario);
            fclose(f);
        }
    }
    int gagnes = 0;
    for (int i = 0; i < g_nScenariosGoal; ++i) gagnes += ScenarioGagne(g_scenariosGoal[i]) ? 1 : 0;
    printf("[AP GOAL] victoire \"%s\"%s : %d/%d scenario(s) du goal gagne(s)\n", scenario,
        deja ? " (deja comptee)" : "", gagnes, g_nScenariosGoal);

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%s (%d/%d scenarios)", displayName, gagnes, g_nScenariosGoal);
    AfficherNotificationAP("Scenario completed", texte);

    // 3. Goal.  TODO reseau : StatusUpdate CLIENT_GOAL.
    if (gagnes >= g_nScenariosGoal && !g_goalEnvoye) {
        g_goalEnvoye = true;
        printf("[AP GOAL] GOAL ATTEINT : tous les scenarios du slot sont gagnes\n");
        AfficherNotificationAP("Goal reached", "All scenarios of this slot are completed.");
    }
}

// ============================================================================
//  slot_data
// ============================================================================

static bool g_exclurePurposePolitics = false;

bool ApExclurePurposePolitics() { return g_exclurePurposePolitics; }

void ApAppliquerSlotData(const ApSlotData& d)
{
    static const char* const DIFFICULTES[] = { "easy", "medium", "hard", "very_hard" };
    static const char* const CONNUS[] = { "DEFAULT", "TechHaven", "REFUGEES", "WINTERHOME", "BUILDERS", "Reconquest" };

    // 1. Scenarios du goal (noms internes connus seulement).
    const char* retenus[6];
    int n = 0;
    for (int i = 0; i < d.nScenarios && i < 6; ++i) {
        bool connu = false;
        for (const char* c : CONNUS) connu |= d.scenarios[i] && _stricmp(c, d.scenarios[i]) == 0;
        if (connu) retenus[n++] = d.scenarios[i];
        else printf("[AP SLOT] scenario inconnu \"%s\", ignore\n", d.scenarios[i] ? d.scenarios[i] : "?");
    }
    if (n) ApReglerScenariosGoal(retenus, n);
    else printf("[AP SLOT] aucun scenario valide : liste du goal inchangee\n");

    // 2. Difficulte imposee.
    int niveau = -1;
    for (int i = 0; i < 4; ++i)
        if (d.goalDifficulty && _stricmp(d.goalDifficulty, DIFFICULTES[i]) == 0) niveau = i;
    if (niveau >= 0) DifficulteReglerCible(niveau);
    else printf("[AP SLOT] goal_difficulty \"%s\" inconnue : cible inchangee\n", d.goalDifficulty ? d.goalDifficulty : "?");

    // 3. Duree de livraison des groupes.
    ApReglerHeuresLivraison(d.deliveryHours);

    // 4. Notes seulement.
    g_exclurePurposePolitics = d.excludePurposePolitics;
    printf("[AP SLOT] scenario de depart \"%s\" (son deblocage arrive comme item), "
        "exclude_purpose_politics=%d\n", d.startingScenario ? d.startingScenario : "?", d.excludePurposePolitics ? 1 : 0);
}

void ApChargerSlotDataDeTest()
{
    char base[MAX_PATH] = {}, chemin[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(chemin, sizeof(chemin), _TRUNCATE, "%smods\\ap_slot_data.txt", base);
    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "r") != 0 || !f) {
        printf("[AP SLOT] pas de mods\\ap_slot_data.txt : valeurs par defaut\n");
        return;
    }
    // Chaines gardees en statique : ApSlotData ne porte que des pointeurs.
    static char listeScn[128], depart[32], difficulte[16];
    ApSlotData d = {};
    d.deliveryHours = 24;
    char ligne[256];
    while (fgets(ligne, sizeof(ligne), f)) {
        ligne[strcspn(ligne, "\r\n")] = 0;
        char* egal = strchr(ligne, '=');
        if (!egal) continue;
        *egal = 0;
        const char* cle = ligne;
        const char* val = egal + 1;
        if (_stricmp(cle, "scenarios") == 0) strcpy_s(listeScn, val);
        else if (_stricmp(cle, "starting_scenario") == 0) strcpy_s(depart, val);
        else if (_stricmp(cle, "goal_difficulty") == 0) strcpy_s(difficulte, val);
        else if (_stricmp(cle, "exclude_purpose_politics") == 0) d.excludePurposePolitics = atoi(val) != 0;
        else if (_stricmp(cle, "delivery_hours") == 0) d.deliveryHours = atoi(val);
    }
    fclose(f);
    char* ctx = nullptr;
    for (char* s = strtok_s(listeScn, ",", &ctx); s && d.nScenarios < 6; s = strtok_s(nullptr, ",", &ctx))
        d.scenarios[d.nScenarios++] = s;
    d.startingScenario = depart;
    d.goalDifficulty = difficulte;
    printf("[AP SLOT] mods\\ap_slot_data.txt applique (test)\n");
    ApAppliquerSlotData(d);
}

// Copie ASCII pour la console du jeu (UTF-8 non ASCII -> '?').
static void AsciiPourJournal(const char* s, char* out, int maxlen)
{
    int i = 0;
    for (; s && s[i] && i < maxlen - 1; ++i)
        out[i] = (s[i] >= 0x20 && s[i] < 0x7F) ? s[i] : '?';
    out[i] = 0;
}

void RequestConnection(const char* server, const char* slot, const char* password)
{
    // 1. Envoi.  TODO reseau : la classe reseau ouvrira la connexion ici.
    char s[256], j[256];
    AsciiPourJournal(server, s, sizeof(s));
    AsciiPourJournal(slot, j, sizeof(j));
    printf("[AP CONNEXION] demande : serveur \"%s\", slot \"%s\", mot de passe %s (%d octets)\n",
        s, j, (password && password[0]) ? "fourni" : "vide", password ? (int)strlen(password) : 0);
}
