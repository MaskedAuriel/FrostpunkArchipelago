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
static int g_sinceBonus = 0;



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

void AP_SendCheck(uint64_t hash)
{
    // Point de sortie unique vers Archipelago : archipelago.cpp.
    SendResearchCheck(hash);
}

void OnResearchComplete(uint64_t hash)
{
    TechRuntime* r = RT_Get(hash);
    if (!r) return;
    r->researched = true;
    SyncGameState(r);          // received ? 2 : 0 -> une techno recue RESTE a 2
    printf("[AP] recherche terminee : %s (recu=%d) -> etat %d\n",
           NameOfHash(hash), r->received, r->received ? 2 : 0);
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

// Texte Archipelago associe a cette techno (= a ce location check).
// Tant que le reseau n'existe pas, on affiche ce qu'on sait localement.
void AP_LocationText(uint64_t hash, char* out, int maxlen)
{
    TechRuntime* r = RT_Get(hash);
    const TechInfo* t = TechByHash(hash);

    // TODO reseau : remplacer par l'objet reel et le joueur destinataire,
    // fournis par le serveur pour cette location.
    _snprintf_s(out, maxlen, _TRUNCATE,
        "[AP] Sending: %s%s",
        t ? t->name : "unknown location",
        (r && r->researched) ? " (already sent)" : "");
}
static const int MAX_TREES = 79;
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

static int TreeSize(int tree)
{
    int n = 0;
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g)
        for (int i = 0; i < KNOWN_TECH_GROUPS[g].count; ++i)
            if (KNOWN_TECH_GROUPS[g].items[i].progressiveTree == tree) n++;
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
    if (tree != BONUS_TREE && ++g_sinceBonus >= BONUS_EVERY) {
        g_sinceBonus = 0;
        OnProgressiveReceived(BONUS_TREE);   // silencieux si l'arbre est epuise
    }
    return true;
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
        bool reste = (TechAtTreePosition(t, g_progressive[t] + 1) != nullptr);
        printf("   arbre %2d : %d recue(s)%s\n",
            t, g_progressive[t], reste ? "" : "  [epuise]");
        printf("   arbre %2d : %d/%d\n", t, g_progressive[t], TreeSize(t));
        total += g_progressive[t];
    }
    printf("[PROG] total : %d progressives\n", total);
}


