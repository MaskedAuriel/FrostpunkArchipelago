#include "IconSubManager.h"
#include "tech_runtime.h"

struct GabaritBat { uintptr_t tpl; char nom[64]; };
static GabaritBat g_gabaritsBat[256];
static int        g_nGabaritsBat = 0;


struct IconeSauvee { uintptr_t tpl; int32_t col, row; };
static IconeSauvee g_iconesSauvees[64];
static int         g_nIconesSauvees = 0;
// Ecrit l'icone sur TOUTES les entrees portant ce nom, et relit pour verifier.
bool SetTechIconTile(uint64_t hash, const TechIcons* icone)
{
    if (!icone || !EnsureTechSelf()) return false;

    uintptr_t base = 0; uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);
    if (base <= 0x10000 || cnt == 0 || cnt > 4096) return false;

    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t te = 0;
        if (!SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8)) continue;
        if (te <= 0x10000) continue;

        uint64_t h = 0;
        if (!SafeRead((void*)(te + 0x20), &h, 8) || h != hash) continue;

        int32_t avant[2] = {};
        SafeRead((void*)(te + 0x80), avant, 8);

        // Miniature de batiment : l'atlas des technos n'est pas consulte.
        if (avant[0] == 0 && avant[1] == 2) {
            printf("[ICON] hash %016llX : miniature de batiment, "
                "case (%d,%d) a appliquer sur le gabarit\n",
                (unsigned long long)hash, icone->batCol, icone->batRow);
            return false;
        }

        int32_t cr[2] = { icone->techCol, icone->techRow };
        if (!SafeWrite((void*)(te + 0x80), cr, 8)) return false;

        printf("[ICON] hash %016llX : (%d,%d) -> (%d,%d)  [%s]\n",
            (unsigned long long)hash, avant[0], avant[1],
            cr[0], cr[1], icone->nom);
        return true;
    }

    printf("[ICON] hash %016llX introuvable\n", (unsigned long long)hash);
    return false;
}


// Nom de batiment porte par un gabarit, extrait de son Display name.
// La casse varie : "CoalThumper/name" comme "Sawmill/Name".

static bool NomDuBatiment(uintptr_t tpl, char* out, int maxlen)
{
    uintptr_t pk = 0; char cle[192] = { 0 };
    if (!SafeRead((void*)(tpl + 0x138), &pk, 8) || pk <= 0x10000) return false;
    if (!SafeRead((void*)pk, cle, sizeof(cle) - 1)) return false;
    return NomBatimentDepuisCle(cle, out, maxlen);
}

static bool StrEqCasse(const char* a, const char* b)
{
    for (int i = 0; ; ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return false;
        if (x == 0) return true;
    }
}

// Applique une case d'atlas a TOUS les gabarits portant ce nom : certains
// batiments existent en plusieurs variantes de scenario.
bool AppliquerIconeTechno(uint64_t hash, const TechIcons* icone)
{
    if (!icone || !EnsureTechSelf()) return false;

    uintptr_t base = 0; uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);
    if (base <= 0x10000 || cnt == 0 || cnt > 4096) return false;

    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t te = 0;
        if (!SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8)) continue;
        if (te <= 0x10000) continue;

        uint64_t h = 0;
        if (!SafeRead((void*)(te + 0x20), &h, 8) || h != hash) continue;

        int32_t avant[2] = {};
        SafeRead((void*)(te + 0x80), avant, 8);

        // Tuile d'atlas : changement direct et permanent.
        if (avant[0] != 0 || avant[1] != 2) {
            int32_t cr[2] = { icone->techCol, icone->techRow };
            SafeWrite((void*)(te + 0x80), cr, 8);
            printf("[ICON] %016llX tuile (%d,%d) -> (%d,%d) [%s]\n",
                (unsigned long long)hash, avant[0], avant[1],
                cr[0], cr[1], icone->nom);
            return true;
        }

        // Miniature : on passe par le batiment, donc temporairement.
        char bat[64] = { 0 };
        uintptr_t pk = 0; char cle[192] = { 0 };
        SafeRead((void*)(te + 0x78), &pk, 8);
        if (pk > 0x10000) SafeRead((void*)pk, cle, sizeof(cle) - 1);

        const char* m = "Buildings/";
        for (int j = 0; cle[j]; ++j) {
            int q = 0;
            while (m[q] && cle[j + q] == m[q]) q++;
            if (m[q]) continue;
            const char* p = cle + j + q;
            int z = 0;
            for (; p[z] && p[z] != '/' && z < 63; ++z) bat[z] = p[z];
            bat[z] = 0;
            break;
        }

        if (!bat[0]) {
            printf("[ICON] %016llX : miniature sans batiment identifiable (%s)\n",
                (unsigned long long)hash, cle);
            return false;
        }

        AppliquerIconeBatiment(bat, icone->batCol, icone->batRow);
        return false;
    }

    printf("[ICON] hash %016llX introuvable\n", (unsigned long long)hash);
    return false;
}

// Restaure toutes les icones touchees.

void AppliquerIconeBatiment(const char* nomBat, int32_t col, int32_t row)
{
    for (int i = 0; i < g_nGabaritsBat; ++i) {
        if (!StrEqCasse(g_gabaritsBat[i].nom, nomBat)) continue;

        int32_t avant[2] = {};
        if (!SafeRead((void*)(g_gabaritsBat[i].tpl + 0x178), avant, 8)) continue;

        if (g_nIconesSauvees < 64) {
            g_iconesSauvees[g_nIconesSauvees].tpl = g_gabaritsBat[i].tpl;
            g_iconesSauvees[g_nIconesSauvees].col = avant[0];
            g_iconesSauvees[g_nIconesSauvees].row = avant[1];
            g_nIconesSauvees++;
        }

        int32_t cr[2] = { col, row };
        SafeWrite((void*)(g_gabaritsBat[i].tpl + 0x178), cr, 8);
    }
}


void RestaurerIconesBatiments()
{
    for (int i = 0; i < g_nIconesSauvees; ++i) {
        int32_t cr[2] = { g_iconesSauvees[i].col, g_iconesSauvees[i].row };
        SafeWrite((void*)(g_iconesSauvees[i].tpl + 0x178), cr, 8);
    }
    if (g_nIconesSauvees)
        printf("[BATICON] %d icone(s) restauree(s)\n", g_nIconesSauvees);
    g_nIconesSauvees = 0;
}



TechnoAP g_technosAP[256];
int      g_nTechnosAP = 0;

// Retrouve une icone par son nom dans la table.
static const TechIcons* IconeParNom(const char* nom)
{
    const int n = (int)(sizeof(ARCHIPELAGO_ICONS_TECH) / sizeof(ARCHIPELAGO_ICONS_TECH[0]));
    for (int i = 0; i < n; ++i)
        if (StrEq(ARCHIPELAGO_ICONS_TECH[i].nom, nom)) return &ARCHIPELAGO_ICONS_TECH[i];
    return nullptr;
}

void ApAjouterTechno(uint64_t hash, const char* nomIcone, const char* batiment)
{
    if (g_nTechnosAP >= 256) return;

    const TechIcons* ic = IconeParNom(nomIcone);
    if (!ic) { printf("[ICON] icone \"%s\" inconnue\n", nomIcone); return; }

    TechnoAP& t = g_technosAP[g_nTechnosAP++];
    t.hash = hash;
    t.icone = ic;
    t.entree = 0;
    t.miniature = false;
    t.batiment[0] = 0;

    if (batiment) {
        int j = 0;
        for (; batiment[j] && j < 63; ++j) t.batiment[j] = batiment[j];
        t.batiment[j] = 0;
    }
}

// TEMPORAIRE : quelques technos en dur pour valider la chaine.
void ApChargerTechnosDeTest()
{
    ApAjouterTechno(0x8BC47F368138DA92ULL, "icon_trap");         // Gathering Upgrade
    ApAjouterTechno(0xB69B51DB31F4B001ULL, "icon_vanilla");      // Gathering Insulation
    ApAjouterTechno(0x851032DABCC703AEULL, "icon_progressive");  // Gathering Insulation 2
    ApAjouterTechno(0xB300360E7D7A3295ULL, "icon_vanilla");      // ResourceDepotUpgrade
    ApAjouterTechno(0x96031EA6A430BF85ULL, "icon_progressive", "ResourceDepotBig");  // Resource Depot Big
    ApAjouterTechno(0xBF4935A464F03A66ULL, "icon_vanilla");      // Gathering upgrade 2
    ApAjouterTechno(0x86A71762D9705649ULL, "icon_vanilla");      // CoalThumper
    ApAjouterTechno(0xB5B909AE4C1E54F6ULL, "icon_progressive");  // SteamCoalThumper
    ApAjouterTechno(0x86CCBA956FEBC7DEULL, "icon_vanilla");      // Coal Thumper Upgrade
    ApAjouterTechno(0xA3796F28B9AC2A23ULL, "icon_filler");       // Coal Mine
    ApAjouterTechno(0xB82524B0D19D88D5ULL, "icon_trap");         // Adv Coal Mine
    ApAjouterTechno(0xAB63782F194E8021ULL, "icon_filler");       // Steam Coal Mine
    ApAjouterTechno(0xA5A0DE87E8514928ULL, "icon_vanilla");      // Coal mine upgrade 1
    ApAjouterTechno(0x960C27264EA17CF9ULL, "icon_filler");       // Coal Mine upgrade 2
    ApAjouterTechno(0xBEC33A80C6853BA3ULL, "icon_trap");         // CharcoalKiln
    ApAjouterTechno(0xBBD8663CFA3F3D7DULL, "icon_trap");         // Kiln upgrade
    ApAjouterTechno(0x87221793ACD9ED8AULL, "icon_vanilla");      // Kiln upgrade 2
    ApAjouterTechno(0xA9F39DF2AEEB21B1ULL, "icon_trap");         // Sawmill
    ApAjouterTechno(0xAC019A23402AB883ULL, "icon_progressive");  // Steam Sawmill
    ApAjouterTechno(0x9D7F762601622CA6ULL, "icon_filler");       // Sawmill Upgrade
    ApAjouterTechno(0xBC770E739218DAB4ULL, "icon_important");    // Sawmill Upgrade - range
    ApAjouterTechno(0xB3C7DF784DC97A83ULL, "icon_filler");       // WallDrill
    ApAjouterTechno(0x9DA3ABAE1FA72621ULL, "icon_trap");         // Adv Wall Drill
    ApAjouterTechno(0x88698E693179DD2DULL, "icon_filler");       // Steam WallDrill
    ApAjouterTechno(0x87856A059CE65971ULL, "icon_trap");         // Steel Mine
    ApAjouterTechno(0x884E0D684DAAB02DULL, "icon_filler");       // Adv Steel Mine
    ApAjouterTechno(0xA9AC3601AE5EA23CULL, "icon_important");    // Steam Steel Mine
    printf("[ICON] %d techno(s) Archipelago chargee(s)\n", g_nTechnosAP);
}




void RemplirCacheGabarits(uintptr_t* objs, int n)
{
    g_nGabaritsBat = 0;
    for (int i = 0; i < n && g_nGabaritsBat < 256; ++i) {
        char nom[64] = { 0 };
        if (!NomDuBatiment(objs[i], nom, sizeof(nom))) continue;
        GabaritBat& g = g_gabaritsBat[g_nGabaritsBat++];
        g.tpl = objs[i];
        int j = 0; for (; nom[j] && j < 63; ++j) g.nom[j] = nom[j];
        g.nom[j] = 0;
    }
    printf("[BATICON] %d gabarit(s) en cache\n", g_nGabaritsBat);
}
// Plus aucun balayage : on parcourt le cache.

void ResoudreTechnosAP()
{
    if (!EnsureTechSelf()) return;

    uintptr_t base = 0; uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);
    if (base <= 0x10000 || cnt == 0 || cnt > 4096) return;

    for (int i = 0; i < g_nTechnosAP; ++i) g_technosAP[i].entree = 0;

    int resolues = 0, minis = 0;
    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t te = 0;
        if (!SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8)) continue;
        if (te <= 0x10000) continue;

        uint64_t h = 0;
        if (!SafeRead((void*)(te + 0x20), &h, 8)) continue;

        for (int i = 0; i < g_nTechnosAP; ++i) {
            if (g_technosAP[i].hash != h) continue;

            g_technosAP[i].entree = te;

            // Un batiment declare a la main l'emporte : certaines technos
            // ont une cle generique d'ou rien n'est deductible.
            if (!g_technosAP[i].batiment[0])
                EstMiniature(te, g_technosAP[i].batiment, 64);

            g_technosAP[i].miniature = (g_technosAP[i].batiment[0] != 0);
            if (g_technosAP[i].miniature) minis++;
            resolues++;
            break;
        }
    }

    printf("[ICON] %d/%d techno(s) resolue(s), dont %d miniature(s)\n",
        resolues, g_nTechnosAP, minis);
}
static bool EstMiniature(uintptr_t te, char* batOut, int maxlen)
{
    uintptr_t pk = 0; char cle[192] = { 0 };
    if (!SafeRead((void*)(te + 0x78), &pk, 8) || pk <= 0x10000) return false;
    if (!SafeRead((void*)pk, cle, sizeof(cle) - 1)) return false;
    return NomBatimentDepuisCle(cle, batOut, maxlen);
}

static bool NomBatimentDepuisCle(const char* cle, char* out, int maxlen)
{
    out[0] = 0;

    const char* pref = "@Buildings/";
    bool courte = true;
    for (int i = 0; pref[i]; ++i) if (cle[i] != pref[i]) { courte = false; break; }
    if (courte) {
        const char* p = cle + 11;
        int k = 0;
        for (; p[k] && p[k] != '/' && k < maxlen - 1; ++k) out[k] = p[k];
        out[k] = 0;
        return k > 0;
    }

    // Forme longue : il faut que le chemin passe par "/Buildings/"
    const char* bat = "/Buildings/";
    bool estBatiment = false;
    for (int i = 0; cle[i]; ++i) {
        int q = 0;
        while (bat[q] && cle[i + q] == bat[q]) q++;
        if (!bat[q]) { estBatiment = true; break; }
    }
    if (!estBatiment) return false;

    const char* m = "/Components/";
    int pos = -1;
    for (int i = 0; cle[i]; ++i) {
        int q = 0;
        while (m[q] && cle[i + q] == m[q]) q++;
        if (!m[q]) { pos = i; break; }
    }
    if (pos <= 0) return false;

    int debut = pos;
    while (debut > 0 && cle[debut - 1] != '/') debut--;
    int k = 0;
    for (int i = debut; i < pos && k < maxlen - 1; ++i) out[k++] = cle[i];
    out[k] = 0;
    return k > 0;
}

void DiagnostiquerTechno(const char* nomCherche)
{
    if (!EnsureTechSelf()) return;

    uintptr_t base = 0; uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);

    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t te = 0;
        if (!SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8)) continue;
        if (te <= 0x10000) continue;

        uintptr_t dsc = 0; char nom[64] = { 0 };
        SafeRead((void*)(te + 0x18), &dsc, 8);
        if (dsc > 0x10000) SafeRead((void*)dsc, nom, sizeof(nom) - 1);
        if (!StrContains(nom, nomCherche)) continue;

        uintptr_t pk = 0; char cle[192] = { 0 };
        SafeRead((void*)(te + 0x78), &pk, 8);
        if (pk > 0x10000) SafeRead((void*)pk, cle, sizeof(cle) - 1);

        char bat[64] = { 0 };
        bool mini = NomBatimentDepuisCle(cle, bat, sizeof(bat));

        int32_t t[2] = {};
        SafeRead((void*)(te + 0x80), t, 8);

        printf("\n[DIAG] %s\n", nom);
        printf("   cle courte : \"%s\"\n", cle);
        printf("   miniature  : %d   batiment extrait : \"%s\"\n", (int)mini, bat);
        printf("   tuile      : (%d,%d)\n", t[0], t[1]);

        if (mini) {
            int n = 0;
            for (int g = 0; g < g_nGabaritsBat; ++g)
                if (StrEqCasse(g_gabaritsBat[g].nom, bat)) n++;
            printf("   gabarits correspondants dans le cache : %d\n", n);
        }
    }
}

static const TechIcons* IconeEffective(const TechnoAP& t)
{
    TechRuntime* r = RT_Get(t.hash);
    if (r && r->researched) {
        const TechIcons* done = IconeParNom("icon_done");
        if (done) return done;
    }
    return t.icone;
}

// Applique l'icone courante d'une techno. Appelable a tout moment :
// a l'ouverture de l'arbre, ou juste apres l'envoi d'un check.
void RafraichirIconeTechno(TechnoAP& t)
{
    if (!t.entree) return;
    const TechIcons* ic = IconeEffective(t);
    if (!ic) return;

    if (t.miniature) {
        if (t.batiment[0])
            AppliquerIconeBatiment(t.batiment, ic->batCol, ic->batRow);
    }
    else {
        int32_t cr[2] = { ic->techCol, ic->techRow };
        SafeWrite((void*)(t.entree + 0x80), cr, 8);
    }
}

void RafraichirToutesLesIcones()
{
    for (int i = 0; i < g_nTechnosAP; ++i) RafraichirIconeTechno(g_technosAP[i]);
}