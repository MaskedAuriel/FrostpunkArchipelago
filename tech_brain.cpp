// tech_brain.cpp
#include "tech_brain.h"
#include "tech_state.h"
#include "known_techs.h"
#include "scenario.h"
#include "archipelago.h"
#include <cstdlib>

bool g_showApInfo = true;
static const int BONUS_TREE = 36;   // arbre debloque par palier
static const int BONUS_EVERY = 5;    // une progressive de BONUS_TREE toutes les 5
// Desactive (2026-11-03) : l'apworld met les items "Progressive Tree Tier"
// dans le pool ; un bonus en plus fausserait le compte des items recus.
static const bool BONUS_ACTIF = false;
static int g_sinceBonus = 0;



static void TechSauver();   // sauvegarde locale (plus bas)

static const char* NameOfHash(uint64_t hash)
{
    const TechInfo* t = TechByHash(hash);
    return t ? t->name : "?";
}

void SyncGameState(TechRuntime* r)
{
    if (!r) return;
    int32_t st = r->received ? 2 : 0;
    WriteTechStateByHash(r->hash, st);
}

bool TechEstUnCheck(uint64_t hash)
{
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        const TechGroup& grp = KNOWN_TECH_GROUPS[g];
        if (!ScenarioUsesGroup(grp.label)) continue;
        for (int i = 0; i < grp.count; ++i)
            if (grp.items[i].hash == hash) return true;
    }
    return false;
}

void AP_SendCheck(uint64_t hash)
{
    // Point de sortie unique vers Archipelago : archipelago.cpp.
    SendResearchCheck(hash);
}

// Test de l'auteur (2026-11-01) : recherche terminee, puis chargement d'une
// sauvegarde faite PENDANT cette recherche, puis recherche terminee a
// nouveau -> le check partait une 2e fois. La sauvegarde locale etant
// cumulative, la techno est deja "researched" a ce moment : plus d'envoi.
void OnResearchComplete(uint64_t hash)
{
    // Techno sans check dans ce scenario (depart, hors table) : vanilla.
    if (!TechEstUnCheck(hash)) {
        printf("[AP] recherche terminee : %s, pas un check dans ce scenario\n", NameOfHash(hash));
        return;
    }
    TechRuntime* r = RT_Get(hash);
    if (!r) return;
    bool dejaEnvoye = r->researched;
    r->researched = true;
    SyncGameState(r);          // received ? 2 : 0 -> une techno recue RESTE a 2
    printf("[AP] recherche terminee : %s (recu=%d) -> etat %d%s\n",
           NameOfHash(hash), r->received, r->received ? 2 : 0,
           dejaEnvoye ? " ; check deja envoye dans cette partie, pas de renvoi" : "");
    if (dejaEnvoye) return;
    TechSauver();
    AP_SendCheck(hash);
}

void OnItemReceived(uint64_t hash)
{
    TechRuntime* r = RT_Get(hash);
    if (!r) return;
    r->received = true;
    SyncGameState(r);          // -> etat 2, effet applique
}

void ReceiveAll()
{
    int n = ForEachTech(&OnItemReceived);
    printf("[TEST] item recu applique a %d technos\n", n);
}

        // declarer  extern bool g_showApInfo;  dans le .h

// Nom du check porte par cette techno (ce que le joueur enverra).
// TODO reseau : l'objet reel fourni par le serveur pour cette location ;
// en attendant, le nom interne de la techno.
void AP_NomDuCheck(uint64_t hash, char* out, int maxlen)
{
    const TechInfo* t = TechByHash(hash);
    _snprintf_s(out, maxlen, _TRUNCATE, "%s", t ? t->name : "unknown location");
}

// Texte Archipelago associe a cette techno (= a ce location check).
// Tant que le reseau n'existe pas, on affiche ce qu'on sait localement.
void AP_LocationText(uint64_t hash, char* out, int maxlen)
{
    TechRuntime* r = RT_Get(hash);
    char nom[128];
    AP_NomDuCheck(hash, nom, sizeof(nom));
    _snprintf_s(out, maxlen, _TRUNCATE,
        "[AP] Sending: %s%s", nom, (r && r->researched) ? " (already sent)" : "");
}
static const int MAX_TREES = 96;          // arbres 0..81 (outils\technos_arbres.tsv)
static int g_progressive[MAX_TREES];      // nb de progressives recues par arbre

// Techno occupant une position donnee dans un arbre. nullptr si absente.
static const TechInfo* TechAtTreePosition(int tree, int pos)
{
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        const TechGroup& grp = KNOWN_TECH_GROUPS[g];
        if (!ScenarioUsesGroup(grp.label)) continue;
        for (int i = 0; i < grp.count; ++i)
            if (grp.items[i].progressiveTree == tree &&
                grp.items[i].progressiveTreePosition == pos)
                return &grp.items[i];
    }
    return nullptr;
}

// Le serveur envoie "Progressive Tree <tree>". Renvoie false si l'arbre est
// epuise ou hors bornes.

// Taille de l'arbre dans le scenario courant (un groupe par scenario depuis
// le 2026-11-03 : sans ce filtre, les technos communes compteraient 6 fois).
static int TreeSize(int tree)
{
    int n = 0;
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        if (!ScenarioUsesGroup(KNOWN_TECH_GROUPS[g].label)) continue;
        for (int i = 0; i < KNOWN_TECH_GROUPS[g].count; ++i)
            if (KNOWN_TECH_GROUPS[g].items[i].progressiveTree == tree) n++;
    }
    return n;
}

bool OnProgressiveReceived(int tree, const char** nomTech)
{
    if (nomTech) *nomTech = nullptr;
    if (tree < 0 || tree >= MAX_TREES) {
        printf("[AP] arbre %d hors bornes (0..%d)\n", tree, MAX_TREES - 1);
        return false;
    }
    int pos = g_progressive[tree];                 // positions a partir de 0
    const TechInfo* t = TechAtTreePosition(tree, pos);
    if (!t) return false;                          // arbre epuise

    g_progressive[tree] = pos + 1;
    OnItemReceived(t->hash);
    if (nomTech) *nomTech = t->name;

    printf("[AP] arbre %2d position %d (%d/%d) -> %s\n",
        tree, pos, pos + 1, TreeSize(tree), t->name);
    if (BONUS_ACTIF && tree != BONUS_TREE && ++g_sinceBonus >= BONUS_EVERY) {
        g_sinceBonus = 0;
        OnProgressiveReceived(BONUS_TREE);   // silencieux si l'arbre est epuise
    }
    TechSauver();
    return true;
}

// --- Sauvegarde locale des technologies (2026-10-29) --------------------------
// Demande de l'auteur : une sauvegarde LOCALE, prioritaire ; le serveur
// Archipelago viendra la completer (jamais l'effacer). Un fichier par partie
// (GameId du systeme de scenario, comme ap_pending_<GameId> des scouts) :
// mods\ap_technos_<GameId>.txt, reecrit en entier a chaque changement :
//   T <hash 16 hex> <researched 0|1> <received 0|1>
//   A <arbre> <progressives recues>
//   B <progressives depuis le dernier bonus>
// Cumulatif, comme Archipelago : charger une sauvegarde plus ancienne de la
// meme partie garde les checks envoyes et les items recus (le serveur ne les
// reprend pas non plus). Au changement de partie (TechPersistanceTick) :
// remise a zero, relecture, puis etat du jeu reapplique a chaque techno.
static SRWLOCK g_verrouTechnos = SRWLOCK_INIT;
static bool    g_technosPretes = false;   // fichier de la partie courante relu
static uint64_t g_partieTechnos[2] = {};  // GameId de la partie relue

static bool CheminTechnos(char* out, size_t taille)
{
    if (!g_run.valid || (!g_run.gameId[0] && !g_run.gameId[1])) return false;
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_technos_%016llX%016llX.txt", base,
        (unsigned long long)g_run.gameId[1], (unsigned long long)g_run.gameId[0]);
    return true;
}

static void TechSauver()
{
    // Pas avant d'avoir relu la sauvegarde de la partie : on ecraserait le
    // fichier d'une partie rechargee par l'etat vide du debut de chargement.
    if (!g_technosPretes) return;
    char chemin[MAX_PATH], temp[MAX_PATH + 8];
    if (!CheminTechnos(chemin, sizeof(chemin))) return;
    _snprintf_s(temp, sizeof(temp), _TRUNCATE, "%s.tmp", chemin);
    AcquireSRWLockExclusive(&g_verrouTechnos);
    FILE* f = nullptr;
    if (fopen_s(&f, temp, "w") != 0 || !f) {
        ReleaseSRWLockExclusive(&g_verrouTechnos);
        printf("[TECH] ecriture de la sauvegarde locale impossible\n");
        return;
    }
    int n = 0;
    for (int i = 0; i < RT_Count(); ++i) {
        TechRuntime* r = RT_At(i);
        if (!r || (!r->researched && !r->received)) continue;
        fprintf(f, "T %016llX %d %d\n", (unsigned long long)r->hash, r->researched ? 1 : 0, r->received ? 1 : 0);
        ++n;
    }
    for (int t = 0; t < MAX_TREES; ++t) if (g_progressive[t]) fprintf(f, "A %d %d\n", t, g_progressive[t]);
    fprintf(f, "B %d\n", g_sinceBonus);
    fclose(f);
    MoveFileExA(temp, chemin, MOVEFILE_REPLACE_EXISTING);   // remplacement d'un bloc
    ReleaseSRWLockExclusive(&g_verrouTechnos);
}

static int TechCharger()
{
    char chemin[MAX_PATH];
    if (!CheminTechnos(chemin, sizeof(chemin))) return -1;
    FILE* f = nullptr;
    if (fopen_s(&f, chemin, "r") != 0 || !f) return 0;
    char ligne[128];
    int n = 0;
    while (fgets(ligne, sizeof(ligne), f)) {
        unsigned long long h = 0;
        int a = 0, b = 0;
        if (ligne[0] == 'T' && sscanf_s(ligne + 2, "%llx %d %d", &h, &a, &b) == 3) {
            TechRuntime* r = RT_Get(h);
            if (r) { r->researched = a != 0; r->received = b != 0; ++n; }
        }
        else if (ligne[0] == 'A' && sscanf_s(ligne + 2, "%d %d", &a, &b) == 2 && a >= 0 && a < MAX_TREES)
            g_progressive[a] = b;
        else if (ligne[0] == 'B' && sscanf_s(ligne + 2, "%d", &a) == 1)
            g_sinceBonus = a;
    }
    fclose(f);
    return n;
}

// Boucle du mod (tick d'une seconde, apres CaptureRunState) : a chaque
// nouvelle partie (chargement, autre scenario), l'etat des technos de
// l'ancienne partie est oublie, celui de la nouvelle relu puis reapplique.
void TechPersistanceTick()
{
    static unsigned generationVue = 0xFFFFFFFF;
    static uint64_t partieVue[2] = {};
    unsigned gen = PartieGeneration();
    if (!PartiePrete() || !g_run.valid || !EnsureTechSelf()) return;
    if (gen == generationVue && g_run.gameId[0] == partieVue[0] && g_run.gameId[1] == partieVue[1]) return;
    generationVue = gen;
    partieVue[0] = g_run.gameId[0];
    partieVue[1] = g_run.gameId[1];

    g_technosPretes = false;
    RT_Reset();
    memset(g_progressive, 0, sizeof(g_progressive));
    g_sinceBonus = 0;
    int n = TechCharger();
    int appliquees = 0;
    for (int i = 0; i < RT_Count(); ++i) {
        TechRuntime* r = RT_At(i);
        if (r && (r->researched || r->received)) { SyncGameState(r); ++appliquees; }
    }
    g_partieTechnos[0] = g_run.gameId[0];
    g_partieTechnos[1] = g_run.gameId[1];
    g_technosPretes = true;
    printf("[TECH] partie %016llX%016llX (generation %u) : %s, %d techno(s) reappliquee(s)\n",
        (unsigned long long)g_run.gameId[1], (unsigned long long)g_run.gameId[0], gen,
        n < 0 ? "identifiant de partie absent" : n == 0 ? "aucune sauvegarde locale (nouvelle partie)" : "sauvegarde locale relue",
        appliquees);
}

int TechProgressivesRecues(int tree, const uint64_t gid[2])
{
    if (tree < 0 || tree >= MAX_TREES || !g_technosPretes) return -1;
    if (gid[0] != g_partieTechnos[0] || gid[1] != g_partieTechnos[1]) return -1;
    return g_progressive[tree];
}

// Un arbre progressif non epuise du scenario courant, au hasard (-1 si aucun).
int ArbreProgressifAuHasard()
{
    static bool seeded = false;
    if (!seeded) { srand(GetTickCount()); seeded = true; }

    int dispo[MAX_TREES], n = 0;
    for (int t = 0; t < MAX_TREES; ++t) {
        if (t == BONUS_TREE) continue;
        if (TechAtTreePosition(t, g_progressive[t])) dispo[n++] = t;
    }
    return n ? dispo[rand() % n] : -1;
}

void ReceiveRandomProgressive()
{
    int t = ArbreProgressifAuHasard();
    if (t < 0) { printf("[AP] tous les arbres sont epuises\n"); return; }
    OnProgressiveReceived(t);
}

// Etat de la progression, pour verifier apres plusieurs tirages.
void DumpProgression()
{
    int total = 0;
    printf("\n[PROG] arbres entames :\n");
    for (int t = 0; t < MAX_TREES; ++t) {
        if (!g_progressive[t]) continue;
        // Positions a partir de 0 : la prochaine a recevoir est g_progressive[t].
        bool reste = (TechAtTreePosition(t, g_progressive[t]) != nullptr);
        printf("   arbre %2d : %d recue(s)%s\n",
            t, g_progressive[t], reste ? "" : "  [epuise]");
        printf("   arbre %2d : %d/%d\n", t, g_progressive[t], TreeSize(t));
        total += g_progressive[t];
    }
    printf("[PROG] total : %d progressives\n", total);
}


