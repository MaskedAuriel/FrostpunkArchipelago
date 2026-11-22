#include "IconSubManager.h"
#include "tech_runtime.h"
#include "lois_ap.h"        // IconeCheckDeTest
#include "tech_brain.h"     // AP_NomDuCheck
#include "archipelago.h"    // ApIconeDuCheck (scouts)
#include "historique_ap.h"  // HistoriqueNoeudTechnoConstruit (legende de l'arbre)

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

        // Une seule sauvegarde par gabarit : une 2e application (ouverture de
        // l'arbre puis recherche terminee) relirait notre propre case, et la
        // restauration la rendrait au menu de construction pour de bon.
        bool dejaSauve = false;
        for (int s = 0; s < g_nIconesSauvees; ++s)
            if (g_iconesSauvees[s].tpl == g_gabaritsBat[i].tpl) { dejaSauve = true; break; }

        if (!dejaSauve && g_nIconesSauvees < 64) {
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
// Release (AP_RELEASE, 2026-11-07) : pas d'icones de test, toutes les technos
// connues sont inscrites avec le logo Archipelago ("icon_vanilla") comme
// repli quand le serveur n'a pas (encore) donne de scout.
void ApChargerTechnosDeTest()
{
#ifdef AP_RELEASE
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        const TechGroup& grp = KNOWN_TECH_GROUPS[g];
        for (int i = 0; i < grp.count; ++i) {
            bool connue = false;
            for (int k = 0; k < g_nTechnosAP; ++k)
                if (g_technosAP[k].hash == grp.items[i].hash) { connue = true; break; }
            if (!connue) ApAjouterTechno(grp.items[i].hash, "icon_vanilla");
        }
    }
    printf("[ICON] %d techno(s) Archipelago chargee(s) (repli : icon_vanilla)\n", g_nTechnosAP);
    return;
#endif
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
    int explicites = g_nTechnosAP;

    // Toutes les autres technos connues : type de check TEMPORAIRE tire par un
    // hachage stable de leur nom (IconeCheckDeTest, comme les lois et les
    // sites), en attendant le serveur. Une techno partagee par plusieurs
    // scenarios n'est ajoutee qu'une fois (meme hash).
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        const TechGroup& grp = KNOWN_TECH_GROUPS[g];
        for (int i = 0; i < grp.count; ++i) {
            bool connue = false;
            for (int k = 0; k < g_nTechnosAP; ++k)
                if (g_technosAP[k].hash == grp.items[i].hash) { connue = true; break; }
            if (!connue) ApAjouterTechno(grp.items[i].hash, IconeCheckDeTest(grp.items[i].name, 0));
        }
    }
    printf("[ICON] %d techno(s) Archipelago chargee(s) (%d de test, %d par hachage du nom)\n",
        g_nTechnosAP, explicites, g_nTechnosAP - explicites);
}




// --- Capture des gabarits de batiment a leur construction -------------------
// +0x1916F90 : constructeur de FrostpunkBuildingComponentTemplate (seule
// fonction qui charge sa vtable +0x20E6710 ; valide le 2026-10-15 : 124/124).
// Remplace le balayage memoire : ScootingInit lit ces captures.
typedef uintptr_t(*CtorGabarit_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static CtorGabarit_t oCtorGabarit = nullptr;
static uintptr_t     g_gabObjets[1024];
static ListeCaptures g_capGab = { SRWLOCK_INIT, g_gabObjets, 1024, 0,
                                  "FrostpunkBuildingComponentTemplate", 0 };

static uintptr_t hkCtorGabarit(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    uintptr_t r = oCtorGabarit(a, b, c, d);
    CaptureAjouter(g_capGab, a);
    return r;
}

void InstallCaptureGabarits(uintptr_t modBase)
{
    void* p = (void*)(modBase + Adr(0x1916F90));
    if (MH_CreateHook(p, (void*)&hkCtorGabarit, (void**)&oCtorGabarit) == MH_OK &&
        MH_EnableHook(p) == MH_OK) printf("[CAPGAB] hook +1916F90 OK\n");
    else printf("[CAPGAB] hook +1916F90 KO\n");
}

int GabaritsBatCaptures(uintptr_t* out, int maxOut)
{
    return CaptureLire(g_capGab, out, maxOut);
}

void RapportCapturesGabarits()
{
    static uintptr_t vivants[1024];
    int n = CaptureLire(g_capGab, vivants, 1024);
    int enCache = 0;
    for (int i = 0; i < n; ++i) if (GabaritBatEnCache(vivants[i])) enCache++;
    printf("[CAPGAB] %ld appel(s) du constructeur, %d en liste, %d vivant(s), %d/%d dans le cache\n",
        g_capGab.appels, g_capGab.n, n, enCache, g_nGabaritsBat);
}

uintptr_t PremierGabaritBat() { return g_nGabaritsBat > 0 ? g_gabaritsBat[0].tpl : 0; }
int NombreGabaritsBat() { return g_nGabaritsBat; }
bool GabaritBatEnCache(uintptr_t tpl)
{
    for (int i = 0; i < g_nGabaritsBat; ++i)
        if (g_gabaritsBat[i].tpl == tpl) return true;
    return false;
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
    // Type du check d'apres le serveur (scout) ; a defaut, icone de test.
    char cle[20];
    sprintf_s(cle, "%016llX", (unsigned long long)t.hash);
    const char* nom = ApIconeDuCheck("techno", cle);
    if (nom) {
        const TechIcons* ic = IconeParNom(nom);
        if (ic) return ic;
    }
    return t.icone;
}

// Applique l'icone courante d'une techno. Appelable a tout moment :
// a l'ouverture de l'arbre, ou juste apres l'envoi d'un check.
// N'est plus appelee par l'arbre (voir InstallHooksIconesTechnos) : ecriture
// permanente, a reserver aux diagnostics.
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

// --- Icones AP posees pendant la construction de l'interface (2026-11-01) ----
// L'ancienne pose (ecriture permanente de techno +0x80 et du gabarit de
// batiment +0x178 a l'ouverture de l'arbre) dependait d'une detection
// d'ouverture qui n'a jamais fonctionne (TexteArbreTechno n'etait appelee
// nulle part), et une ecriture dans le gabarit de batiment s'affiche aussi au
// menu de construction. Decompiles du 2026-11-01 : l'image d'une techno est
// posee par
//   - FUN_141bc5810(panneau, techno, sortie, p4) : noeud de l'arbre (appele
//     par FUN_141bc3eb0 pour chaque techno a la construction de l'arbre) ;
//   - FUN_141be67f0(panneau) : recherche en cours, techno =
//     *(*(base + 0x4062BA8) + 0x2260) ;
//   - FUN_141bb2b60(panneau, techno) : autre panneau.
// Chacune pose soit BUILDING_IMAGE (techno qui debloque un batiment : case
// du gabarit +0x178, atlas des miniatures #3037), soit TECH_IMAGE (case de
// techno +0x80, atlas #1179), par FUN_141bb2d30(image, case x|y<<32, nb).
// On entoure ces trois fonctions : techno +0x80 = notre case de l'atlas des
// technos le temps de l'appel (puis rendue), et dans FUN_141bb2d30 toute
// autre case (= celle du batiment) devient notre case de l'atlas des
// miniatures. Rien ne reste ecrit hors de ces appels.
typedef uintptr_t(*Fonction4Ui_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void(*PoserCaseUi_t)(uintptr_t, uint64_t, uint64_t);
static Fonction4Ui_t oNoeudTechno = nullptr, oRechercheEnCours = nullptr, oPanneauTechno = nullptr;
static PoserCaseUi_t oPoserCaseUi = nullptr;
static Fonction4Ui_t oBatimentsDebloques = nullptr;
static thread_local const TechIcons* t_iconeUi = nullptr;
static thread_local uintptr_t t_techUi = 0;
static const uintptr_t OFF_NOEUD_TECHNO = Adr(0x1BC5810), OFF_RECHERCHE_EN_COURS = Adr(0x1BE67F0),
                    OFF_PANNEAU_TECHNO = Adr(0x1BB2B60), OFF_POSER_CASE_UI = Adr(0x1BB2D30),
                    OFF_BATIMENTS_DEBLOQUES = Adr(0x16A83B0), OFF_SYSTEME_TECHNOS_BIS = Adr(0x4062BA8);

static uint64_t CasePackee(int32_t col, int32_t lig) { return (uint32_t)col | ((uint64_t)(uint32_t)lig << 32); }

static const TechIcons* IconeDeLaTechno(uintptr_t tech)
{
    uint64_t h = 0;
    if (tech <= 0x10000 || !SafeRead((void*)(tech + 0x20), &h, 8) || !h) return nullptr;
    if (!TechEstUnCheck(h)) return nullptr;      // techno de depart : icone du jeu
    for (int i = 0; i < g_nTechnosAP; ++i)
        if (g_technosAP[i].hash == h) return IconeEffective(g_technosAP[i]);
    return nullptr;
}

// Contexte d'une construction : techno +0x80 remplacee, icone notee pour le
// hook de FUN_141bb2d30 ; rendu a la sortie (appels imbriques compris).
struct ContexteIconeUi {
    uintptr_t tech = 0;
    int32_t avant[2] = {};
    bool ecrit = false;
    const TechIcons* precedente = nullptr;
    uintptr_t techPrecedente = 0;

    explicit ContexteIconeUi(uintptr_t t)
    {
        precedente = t_iconeUi;
        techPrecedente = t_techUi;
        const TechIcons* ic = IconeDeLaTechno(t);
        t_iconeUi = ic;
        t_techUi = ic ? t : 0;
        if (!ic) return;
        tech = t;
        if (SafeRead((void*)(tech + 0x80), avant, 8)) {
            int32_t cr[2] = { ic->techCol, ic->techRow };
            ecrit = SafeWrite((void*)(tech + 0x80), cr, 8);
        }
    }
    ~ContexteIconeUi()
    {
        if (ecrit) SafeWrite((void*)(tech + 0x80), avant, 8);
        t_iconeUi = precedente;
        t_techUi = techPrecedente;
    }
};

// 1er test (2026-11-01, captures de l'auteur) : nos cases de l'atlas des
// miniatures (#3037) sont DECENTREES dans BUILDING_IMAGE (Coal Thumper dans
// l'arbre, Coal Mining dans l'atelier), alors que celles de l'atlas de l'arbre
// (#1179, TECH_IMAGE) sont bien placees. Les trois fonctions choisissent
// BUILDING_IMAGE si FUN_1416a83b0(DAT_14408ea70, techno) (recherche pure dans
// la liste des batiments debloques) renvoie une liste non vide : pour NOTRE
// techno, pendant la construction, on repond « aucun batiment » -> le jeu
// prend TECH_IMAGE, avec notre case de techno +0x80.
static uintptr_t hkBatimentsDebloques(uintptr_t sys, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    if (t_techUi && tech == t_techUi) return 0;
    return oBatimentsDebloques(sys, tech, p3, p4);
}

static void hkPoserCaseUi(uintptr_t image, uint64_t cas, uint64_t nb)
{
    const TechIcons* ic = t_iconeUi;
    if (ic && cas != CasePackee(ic->techCol, ic->techRow))
        cas = CasePackee(ic->batCol, ic->batRow);    // image du batiment
    oPoserCaseUi(image, cas, nb);
}

static uintptr_t g_panneauTechnos = 0;   // panneau de l'arbre (vu au 1er noeud construit)

uintptr_t IconesTechnoPanneau() { return g_panneauTechnos; }

// Nom des noeuds : le noeud garde le nom de la techno (decision de l'auteur,
// 2026-11-04 : sinon le joueur ne sait plus quelle techno il a recue). Le
// nom du check n'apparait que dans la description (hkTranslate, ApDescBuffer).
// Historique des essais (texte brut, cle "@AP/TechNom", traduction a la
// validation FUN_14111a680) : docs/chantiers/reseau.md.
static uintptr_t hkNoeudTechno(uintptr_t panneau, uintptr_t tech, uintptr_t sortie, uintptr_t p4)
{
    g_panneauTechnos = panneau;
    uintptr_t r;
    {
        ContexteIconeUi c(tech);
        r = oNoeudTechno(panneau, tech, sortie, p4);
    }
    HistoriqueNoeudTechnoConstruit();   // anneau de la legende de l'arbre (historique_ap.cpp)
    return r;
}

// Test du 2026-11-01 : une techno recherchee arbre deja construit gardait
// l'icone de son check et son nom sans [Researched] : les noeuds ne sont
// reconstruits qu'a la selection d'un onglet (FUN_141bc8440). On met donc a
// jour le noeud nous-memes, comme FUN_141bc5810 le fait a la construction.
// Noeuds du panneau (FUN_141bc3eb0 / FUN_141bbf450) : panneau +0x1A8, compte
// +0x1B0, enregistrements de 0x80 : +0x00 cle 16 octets (= techno +0x20),
// +0x10 poignee du noeud, +0x18 poignee du texte TECH_NAME, +0x20 poignee de
// l'image affichee. Image : FUN_141107140(image, base + 0x4074548, 0, 0) puis
// FUN_141bb2d30(image, case, *(base + 0x4074550)) ; nom :
// FUN_14111bd70(texte, techno + 0x18, base + 0x2BFD628) (retraduit, donc
// [Researched] par hkTranslate). Thread du jeu (fin de recherche).
typedef void(*PoserTextureUi_t)(uintptr_t, uintptr_t, uintptr_t, uint32_t);
typedef void(*PoserTexteCle_t)(uintptr_t, uintptr_t, uintptr_t);
static const uintptr_t OFF_POSER_TEXTURE_UI = Adr(0x1107140), OFF_TEXTURE_ARBRE = Adr(0x4074548),
                    OFF_CASES_ARBRE = Adr(0x4074550), OFF_POSER_TEXTE_CLE = Adr(0x111BD70),
                    OFF_FORMAT_TEXTE = Adr(0x2BFD628);

void IconesTechnoRafraichirNoeud(uintptr_t tech)
{
    uintptr_t panneau = g_panneauTechnos, liste = 0;
    int32_t n = 0;
    if (!panneau || tech <= 0x10000 || !SafeRead((void*)(panneau + 0x1A8), &liste, 8) ||
        !SafeRead((void*)(panneau + 0x1B0), &n, 4) || liste <= 0x10000 || n <= 0 || n > 1024) return;
    uint64_t cle[2] = {};
    if (!SafeRead((void*)(tech + 0x20), cle, 16)) return;
    const TechIcons* ic = IconeDeLaTechno(tech);
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    __try {
        for (int i = 0; i < n; ++i) {
            uintptr_t rec = liste + (uintptr_t)i * 0x80;
            uint64_t k[2] = {};
            if (!SafeRead((void*)rec, k, 16) || k[0] != cle[0] || k[1] != cle[1]) continue;
            uintptr_t pImage = 0, image = 0, pTexte = 0, texte = 0;
            SafeRead((void*)(rec + 0x20), &pImage, 8);
            SafeRead((void*)(rec + 0x18), &pTexte, 8);
            if (pImage > 0x10000) SafeRead((void*)pImage, &image, 8);
            if (pTexte > 0x10000) SafeRead((void*)pTexte, &texte, 8);
            if (image > 0x10000 && ic) {
                uint64_t nb = 0;
                SafeRead((void*)(base + OFF_CASES_ARBRE), &nb, 8);
                ((PoserTextureUi_t)(base + OFF_POSER_TEXTURE_UI))(image, base + OFF_TEXTURE_ARBRE, 0, 0);
                oPoserCaseUi(image, CasePackee(ic->techCol, ic->techRow), nb);
            }
            if (texte > 0x10000)
                ((PoserTexteCle_t)(base + OFF_POSER_TEXTE_CLE))(texte, tech + 0x18, base + OFF_FORMAT_TEXTE);
            printf("[ICON] noeud de l'arbre mis a jour apres recherche (icone \"%s\", image 0x%llX, nom 0x%llX)\n",
                ic ? ic->nom : "-", (unsigned long long)image, (unsigned long long)texte);
            return;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[ICON] EXCEPTION 0x%08lX a la mise a jour d'un noeud de l'arbre\n", GetExceptionCode());
    }
}

// Bug du 2026-11-10 (The Arks, "Improved Heaters" = Boiler Upgrade grise
// alors que Boilers, recherchee puis RECUE, est a l'etat 2 ; debloque au
// rechargement) : la reception d'une techno deja recherchee pose l'etat 2
// directement (SyncGameState), sans evenement du jeu, donc les noeuds deja
// construits gardent "NOT_AVAILABLE". On rafraichit chaque noeud du panneau
// par la fonction du jeu FUN_141bca5a0(panneau, &GUID) (decompile du
// 2026-11-10 : etat du noeud par FUN_141bc3cb0, liens, couts) ; elle passe
// par hkRafraichirNoeud (elevation des technos recherchees). Elle dereference
// sans controle l'enregistrement trouve par GUID : seulement les GUID de la
// liste du panneau (+0x1A8, enregistrements de 0x80, cle a +0x00). Panneau
// revalide par sa classe (il peut avoir ete detruit). Thread du jeu.
typedef void(*RafraichirNoeudJeu_t)(uintptr_t panneau, uintptr_t guid);
static const uintptr_t OFF_RAFRAICHIR_NOEUD_JEU = Adr(0x1BCA5A0);

void IconesTechnoRafraichirArbre()
{
    uintptr_t panneau = g_panneauTechnos, liste = 0;
    int32_t n = 0;
    if (!panneau) return;
    char cls[128] = "";
    if (!GetClassNameFP(panneau, cls, sizeof(cls)) || !StrContains(cls, "Technology")) {
        printf("[ICON] arbre non rafraichi : panneau 0x%llX de classe \"%s\"\n", (unsigned long long)panneau, cls);
        g_panneauTechnos = 0;
        return;
    }
    if (!SafeRead((void*)(panneau + 0x1A8), &liste, 8) || !SafeRead((void*)(panneau + 0x1B0), &n, 4) ||
        liste <= 0x10000 || n <= 0 || n > 1024) return;
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    int faits = 0;
    __try {
        for (int i = 0; i < n; ++i) {
            uintptr_t rec = liste + (uintptr_t)i * 0x80;
            uint64_t k[2] = {};
            if (!SafeRead((void*)rec, k, 16) || (!k[0] && !k[1])) continue;
            ((RafraichirNoeudJeu_t)(base + OFF_RAFRAICHIR_NOEUD_JEU))(panneau, rec);
            ++faits;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[ICON] EXCEPTION 0x%08lX au rafraichissement de l'arbre (%d noeud(s) faits)\n",
            GetExceptionCode(), faits);
        g_panneauTechnos = 0;
        return;
    }
    printf("[ICON] arbre rafraichi apres reception : %d noeud(s) (panneau %s)\n", faits, cls);
}

static uintptr_t hkPanneauTechno(uintptr_t panneau, uintptr_t tech, uintptr_t p3, uintptr_t p4)
{
    ContexteIconeUi c(tech);
    return oPanneauTechno(panneau, tech, p3, p4);
}

static uintptr_t hkRechercheEnCours(uintptr_t panneau, uintptr_t p2, uintptr_t p3, uintptr_t p4)
{
    uintptr_t sys = 0, tech = 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    if (SafeRead((void*)(base + OFF_SYSTEME_TECHNOS_BIS), &sys, 8) && sys > 0x10000)
        SafeRead((void*)(sys + TECH_ACTIVE_OFF), &tech, 8);
    ContexteIconeUi c(tech);
    return oRechercheEnCours(panneau, p2, p3, p4);
}

void InstallHooksIconesTechnos(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_POSER_CASE_UI, (void*)&hkPoserCaseUi, (void**)&oPoserCaseUi, "pose d'une case d'interface" },
        { OFF_BATIMENTS_DEBLOQUES, (void*)&hkBatimentsDebloques, (void**)&oBatimentsDebloques, "batiments debloques par une techno" },
        { OFF_NOEUD_TECHNO, (void*)&hkNoeudTechno, (void**)&oNoeudTechno, "noeud de l'arbre des technos" },
        { OFF_RECHERCHE_EN_COURS, (void*)&hkRechercheEnCours, (void**)&oRechercheEnCours, "recherche en cours" },
        { OFF_PANNEAU_TECHNO, (void*)&hkPanneauTechno, (void**)&oPanneauTechno, "panneau d'une techno" },    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[ICON] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}

