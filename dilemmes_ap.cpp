// dilemmes_ap.cpp
// ---------------------------------------------------------------------------
// Catalogue des dilemmes Archipelago et file des demandes. Voir dilemmes_ap.h
// pour la marche a suivre ; la mecanique est dans evenements.cpp
// (DilemmeAPCreer).
//
// Rappels pour ecrire une entree :
// - textes en anglais, ASCII uniquement (affiches en jeu) ;
// - au plus 4 choix ; la liste s'arrete au premier choix sans texte ;
// - quantite signee : -50 = perte, +50 = gain ; la ressource doit exister
//   dans l'economie de la partie (sinon le choix n'a pas d'effet, log [RES]) ;
// - icone : un nom de ARCHIPELAGO_ICONS_TECH (IconSubManager.h), dont la
//   case bulleCol/bulleRow est dans l'atlas des bulles (entree 2350) ;
// - choixSiIgnore : indice du choix applique si la bulle expire.
// ---------------------------------------------------------------------------
#include "dilemmes_ap.h"
#include "evenements.h"
#include "mod_common.h"
#include <cstdio>
#include <cstring>

static const DilemmeAP DILEMMES_AP[] = {
    {
        "trap_coal_or_steel",
        "Archipelago Trap",
        "Archipelago",
        "Another world has sent you a trap. Choose what the city will sacrifice.",
        "icon_trap",
        "Archipelago Trap",
        0,
        {
            { "Lose 50 coal",  "coal",  -50, nullptr },
            { "Lose 50 steel", "steel", -50, nullptr },
        }
    },
    // Modele a copier :
    // {
    //     "mon_id",
    //     "Title", "Archipelago", "Description.",
    //     "icon_vanilla", "Archipelago Event",
    //     0,
    //     {
    //         { "First choice",  "wood",  +30, nullptr },
    //         { "Second choice", nullptr,   0, &MaFonction },
    //     }
    // },
};

const DilemmeAP* DilemmeAPParId(const char* id)
{
    if (!id) return nullptr;
    for (const DilemmeAP& d : DILEMMES_AP) if (!strcmp(d.id, id)) return &d;
    return nullptr;
}

// --- File des demandes (thread du mod -> thread du jeu) ----------------------
constexpr int TAILLE_FILE = 16;
static const DilemmeAP* g_file[TAILLE_FILE] = {};
static int              g_nFile = 0;
static SRWLOCK          g_verrouFile = SRWLOCK_INIT;

bool DilemmeAPDemander(const char* id)
{
    const DilemmeAP* d = DilemmeAPParId(id);
    if (!d) { printf("[DIL] dilemme \"%s\" inconnu du catalogue\n", id ? id : "?"); return false; }
    AcquireSRWLockExclusive(&g_verrouFile);
    bool ok = g_nFile < TAILLE_FILE;
    if (ok) g_file[g_nFile++] = d;
    ReleaseSRWLockExclusive(&g_verrouFile);
    if (ok) printf("[DIL] dilemme \"%s\" demande : cree sur le thread du jeu au prochain mouvement "
                   "de ressource (jeu hors pause)\n", id);
    else    printf("[DIL] file des dilemmes pleine : \"%s\" ignore\n", id);
    return ok;
}

void DilemmesPompe()
{
    if (g_nFile <= 0 || !PartiePrete()) return;
    const DilemmeAP* aCreer[TAILLE_FILE];
    AcquireSRWLockExclusive(&g_verrouFile);
    int n = g_nFile;
    memcpy(aCreer, g_file, n * sizeof(aCreer[0]));
    g_nFile = 0;
    ReleaseSRWLockExclusive(&g_verrouFile);
    for (int i = 0; i < n; ++i) DilemmeAPCreer(*aCreer[i]);
}
