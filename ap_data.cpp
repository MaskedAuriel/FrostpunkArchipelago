// ---- ap_data.cpp ----
#include "ap_data.h"
#include "mod_common.h"

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

void ApSetCheck(int id, const char* joueur, const char* description, int arbreTest)
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
    s->c.arbreTest = arbreTest;
    CopieAscii(s->c.joueur, sizeof(s->c.joueur), joueur);
    CopieAscii(s->c.description, sizeof(s->c.description), description);
}

const ApCheck* ApCheckById(int id)
{
    ApSlot* s = SlotById(id);
    return s ? &s->c : nullptr;
}

const wchar_t* ApDescriptionW(int id)
{
    ApSlot* s = SlotById(id);
    if (!s) return nullptr;
    if (!s->descPrete) {
        // Conversion octet a octet : suffit pour du latin simple. A remplacer
        // par MultiByteToWideChar quand le serveur enverra de l'UTF-8.
        int j = 0;
        for (; s->c.description[j] && j < 159; ++j)
            s->descW[j] = (wchar_t)(unsigned char)s->c.description[j];
        s->descW[j] = 0;
        s->descPrete = true;
    }
    return s->descW;
}

void ApDumpChecks()
{
    for (int i = 0; i < g_nSlots; ++i) {
        if (!g_slots[i].valide) continue;
        printf("  [%5d] %-16s arbre=%-3d  %s\n",
            g_slots[i].c.id, g_slots[i].c.joueur,
            g_slots[i].c.arbreTest, g_slots[i].c.description);
    }
    printf("[AP] %d check(s) connu(s)\n", g_nSlots);
}

void ApChargerChecksDeTest()
{
    // Les identifiants doivent correspondre a ceux de SITE_PAYLOADS.
    ApSetCheck(1001, "[Masked Frostpunk Item]", "[AP] Envoi : Coal Mine Progressive", 28);
    ApSetCheck(1003, "[Masked Frostpunk Item]", "[AP] Envoi : Generator Power Progressive", 12);
    ApSetCheck(1002, "Alice", "[AP] Envoi : Bombes fumigenes", -1);

    printf("[AP] checks de test charges :\n");
    ApDumpChecks();
}

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