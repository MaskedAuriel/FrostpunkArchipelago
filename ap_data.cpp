// ---- ap_data.cpp ----
#include "ap_data.h"
#include "mod_common.h"
#include "archipelago.h"   // ApNomDuCheckLocation (scouts)

// Table compacte : les identifiants de location peuvent valoir n'importe quoi,
// on ne les utilise pas comme indices.
static const int AP_MAX_CHECKS = 1024;

struct ApSlot {
    ApCheck  c;
    bool     valide;
    wchar_t  descW[160];
    bool     descPrete;
    wchar_t  joueurW[64];
    bool     joueurPret;
};

static ApSlot g_slots[AP_MAX_CHECKS];
static int    g_nSlots = 0;

static void CopieAscii(char* dst, int taille, const char* src)
{
    int i = 0;
    if (src) for (; src[i] && i < taille - 1; ++i) dst[i] = src[i];
    dst[i] = 0;
}

ApSlot* SlotById(int id)
{
    for (int i = 0; i < g_nSlots; ++i)
        if (g_slots[i].valide && g_slots[i].c.id == id) return &g_slots[i];
    return nullptr;
}

// Nom du check d'apres le scout (2026-11-04, demande de l'auteur : le meme
// "<joueur> - <item>" que les technos et les lois). Id local d'un site =
// 100000 + n, location AP = 7747300000 + n. Recopie seulement si le texte a
// change (lu depuis plusieurs threads : le texte reecrit est identique).
static void RafraichirDepuisScout(ApSlot* s)
{
    if (s->c.id < 100000) return;
    char nom[160];
    if (!ApNomDuCheckLocation(7747300000LL + (s->c.id - 100000), nom, sizeof(nom))) return;
    if (strcmp(nom, s->c.description) == 0) return;
    CopieAscii(s->c.description, sizeof(s->c.description), nom);
    s->descPrete = false;
}

void ApSetCheck(int id, const char* joueur, const char* description)
{
    ApSlot* s = SlotById(id);
    if (!s) {
        if (g_nSlots >= AP_MAX_CHECKS) { printf("[AP] table pleine\n"); return; }
        s = &g_slots[g_nSlots++];
    }
    s->valide = true;
    s->descPrete = false;
    s->joueurPret = false;
    s->c.id = id;
    CopieAscii(s->c.joueur, sizeof(s->c.joueur), joueur);
    CopieAscii(s->c.description, sizeof(s->c.description), description);
}

const ApCheck* ApCheckById(int id)
{
    ApSlot* s = SlotById(id);
    if (!s) return nullptr;
    RafraichirDepuisScout(s);
    return &s->c;
}

const wchar_t* ApDescriptionW(int id)
{
    ApSlot* s = SlotById(id);
    if (!s) return nullptr;
    RafraichirDepuisScout(s);
    if (!s->descPrete) {
        // Octet -> caractere : le texte est en Latin-1 (scouts, ApNoterScout).
        int j = 0;
        for (; s->c.description[j] && j < 159; ++j)
            s->descW[j] = (wchar_t)(unsigned char)s->c.description[j];
        s->descW[j] = 0;
        s->descPrete = true;
    }
    return s->descW;
}

// (ApChargerChecksDeTest, 4 checks de test 1001-1004 dont deux simulaient
// la reception d'une progressive, retire le 2026-11-04.)

const wchar_t* ApJoueurW(int id)
{
    ApSlot* s = SlotById(id);
    if (!s) return nullptr;
    if (!s->joueurPret) {
        int j = 0;
        for (; s->c.joueur[j] && j < 63; ++j)
            s->joueurW[j] = (wchar_t)(unsigned char)s->c.joueur[j];
        s->joueurW[j] = 0;
        s->joueurPret = true;
    }
    return s->joueurW;
}
