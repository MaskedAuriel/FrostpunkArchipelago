// sauvegardes_ap.cpp -- sauvegardes de la session Archipelago seulement
// (voir sauvegardes_ap.h ; decompiles du 2026-11-04).
//
// FUN_141284eb0 (liste des sauvegardes) :
//   FUN_141cdee60(profil, &liste, L"saves", L"*.save");
//   liste = { +0x00 tableau de chaines du moteur (wchar_t*), +0x08 nombre
//   (int), +0x0C capacite (int), +0x10 allocateur } ; chaque element est un
//   nom de fichier ("Ma ville.save"), libere par FUN_140e1d180(&element).
// FUN_1412892f0(profil, nom, p3, p4) : nom = wchar_t* sans extension,
//   fichier ecrit = "<nom>.save" dans "saves" (FUN_141ce0e70).
#include "sauvegardes_ap.h"
#include "mod_common.h"
#include "archipelago.h"
#include "MinHook.h"
#include <cstdio>
#include <cwchar>

namespace {

static const uintptr_t OFF_ECRIRE_SAUVEGARDE = Adr(0x12892F0);   // FUN_1412892f0
static const uintptr_t OFF_LISTER_FICHIERS   = Adr(0x1CDEE60);   // FUN_141cdee60
static const uintptr_t OFF_LIBERER_CHAINE    = Adr(0xE1D180);    // FUN_140e1d180(&chaine)
// Reconstruction du cache (2026-11-10), comme la fin de FUN_1412878d0 :
//   FUN_141284eb0(profil, &tmp) remplit tmp = { tableau d'elements de 0x10
//   {nom, infos 0xD8}, nombre +0x08, capacite +0x0C, allocateur +0x10 } ;
//   si allocateur(profil +0x1B0) == tmp.allocateur : FUN_141283f90(profil
//   +0x1B0, 1) (elements et tableau liberes) puis tmp deplace dans le profil.
static const uintptr_t OFF_CONSTRUIRE_LISTE  = Adr(0x1284EB0);   // FUN_141284eb0
static const uintptr_t OFF_DETRUIRE_LISTE    = Adr(0x1283F90);   // FUN_141283f90
constexpr int       PROFIL_LISTE          = 0x1B0;       // cache des sauvegardes du profil
constexpr int       MAX_NOM               = 260;
constexpr int       MAX_LISTE             = 4096;        // garde-fou du tableau
const char*         FICHIER               = "ap_sauvegardes.txt";

typedef uintptr_t (*Fonction4_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
typedef void (*LibererChaine_t)(uintptr_t*);
Fonction4_t oEcrireSauvegarde = nullptr;
Fonction4_t oListerFichiers = nullptr;

// Chaine large lue dans la memoire du jeu, au plus MAX_NOM - 1 caracteres.
bool LireLarge(uintptr_t p, wchar_t* out)
{
    out[0] = 0;
    if (p < 0x10000) return false;
    for (int i = 0; i < MAX_NOM - 1; ++i) {
        wchar_t c = 0;
        if (!SafeRead((void*)(p + i * 2), &c, 2)) { out[i] = 0; return i > 0; }
        out[i] = c;
        if (!c) return true;
    }
    out[MAX_NOM - 1] = 0;
    return true;
}

// Pour le journal : ASCII, '?' au-dela (regle des printf).
void PourJournal(const wchar_t* w, char* out, size_t taille)
{
    size_t i = 0;
    for (; w[i] && i + 1 < taille; ++i) out[i] = (w[i] >= 0x20 && w[i] < 0x7F) ? (char)w[i] : '?';
    out[i] = 0;
}

// Nom sans l'extension ".save" (comparaison).
void SansExtension(wchar_t* nom)
{
    size_t n = wcslen(nom);
    if (n > 5 && _wcsicmp(nom + n - 5, L".save") == 0) nom[n - 5] = 0;
}

// Noms notes dans la session courante (UTF-8, une ligne par sauvegarde).
int LireNoms(wchar_t (*noms)[MAX_NOM], int max)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!ApCheminSession(chemin, sizeof(chemin), FICHIER) || fopen_s(&f, chemin, "r") != 0 || !f) return 0;
    char ligne[MAX_NOM * 4];
    int n = 0;
    while (n < max && fgets(ligne, sizeof(ligne), f)) {
        size_t l = strlen(ligne);
        while (l && (ligne[l - 1] == '\n' || ligne[l - 1] == '\r')) ligne[--l] = 0;
        if (!l) continue;
        if (MultiByteToWideChar(CP_UTF8, 0, ligne, -1, noms[n], MAX_NOM) > 0) ++n;
    }
    fclose(f);
    return n;
}

bool NomConnu(const wchar_t* nom, wchar_t (*noms)[MAX_NOM], int n)
{
    for (int i = 0; i < n; ++i) if (_wcsicmp(noms[i], nom) == 0) return true;
    return false;
}

wchar_t g_noms[1024][MAX_NOM];   // thread du jeu seulement (ecriture et liste)
uintptr_t g_profil = 0;              // profil vu par la derniere liste (cache +0x1B0)
char      g_sessionListe[MAX_PATH] = "";   // fichier de session de la derniere liste

// Ecriture d'une sauvegarde : nom note dans la session (sans doublon).
uintptr_t hkEcrireSauvegarde(uintptr_t profil, uintptr_t nom, uintptr_t p3, uintptr_t p4)
{
    wchar_t w[MAX_NOM];
    if (LireLarge(nom, w) && w[0]) {
        SansExtension(w);
        char journal[MAX_NOM];
        PourJournal(w, journal, sizeof(journal));
        int n = LireNoms(g_noms, 1024);
        if (NomConnu(w, g_noms, n)) {
            printf("[SAV] sauvegarde \"%s\" : deja notee dans la session\n", journal);
        } else {
            char chemin[MAX_PATH], utf8[MAX_NOM * 4];
            FILE* f = nullptr;
            if (WideCharToMultiByte(CP_UTF8, 0, w, -1, utf8, sizeof(utf8), nullptr, nullptr) > 0 &&
                ApCheminSession(chemin, sizeof(chemin), FICHIER) && fopen_s(&f, chemin, "a") == 0 && f) {
                fprintf(f, "%s\n", utf8);
                fclose(f);
                printf("[SAV] sauvegarde \"%s\" notee dans la session\n", journal);
            } else printf("[SAV] sauvegarde \"%s\" : ecriture de %s impossible\n", journal, FICHIER);
        }
    } else printf("[SAV] ecriture d'une sauvegarde : nom illisible (0x%llX)\n", (unsigned long long)nom);
    // Instantane des livraisons d'items AP a cette date de jeu (2026-11-10).
    ApItemsAvantSauvegarde();
    return oEcrireSauvegarde(profil, nom, p3, p4);
}

// Enumeration des fichiers : pour "saves" / "*.save", on retire du tableau
// rendu les sauvegardes absentes de la session.
uintptr_t hkListerFichiers(uintptr_t profil, uintptr_t liste, uintptr_t dossier, uintptr_t motif)
{
    uintptr_t r = oListerFichiers(profil, liste, dossier, motif);
    wchar_t d[MAX_NOM], m[MAX_NOM];
    if (!liste || !LireLarge(dossier, d) || !LireLarge(motif, m) ||
        _wcsicmp(d, L"saves") != 0 || _wcsicmp(m, L"*.save") != 0) return r;

    uintptr_t tab = 0;
    int32_t nombre = 0;
    // Profil et session de cette liste (reconstruction si la session change).
    g_profil = profil;
    if (!ApCheminSession(g_sessionListe, sizeof(g_sessionListe), FICHIER)) g_sessionListe[0] = 0;
    if (!SafeRead((void*)liste, &tab, 8) || !SafeRead((void*)(liste + 8), &nombre, 4) ||
        nombre <= 0 || nombre > MAX_LISTE || tab < 0x10000) return r;

    int nNoms = LireNoms(g_noms, 1024);
    LibererChaine_t liberer = (LibererChaine_t)((uintptr_t)GetModuleHandleW(nullptr) + OFF_LIBERER_CHAINE);
    int gardees = 0;
    for (int i = 0; i < nombre; ++i) {
        uintptr_t el = 0;
        if (!SafeRead((void*)(tab + i * 8), &el, 8)) continue;
        wchar_t w[MAX_NOM];
        bool lu = LireLarge(el, w);
        if (lu) SansExtension(w);
        char journal[MAX_NOM];
        PourJournal(lu ? w : L"?", journal, sizeof(journal));
        if (lu && NomConnu(w, g_noms, nNoms)) {
            SafeWrite((void*)(tab + gardees * 8), &el, 8);
            ++gardees;
            continue;
        }
        printf("[SAV] liste : \"%s\" masquee (hors session Archipelago)\n", journal);
        __try { liberer(&el); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            printf("[SAV] EXCEPTION 0x%08lX a la liberation de \"%s\"\n", GetExceptionCode(), journal);
        }
    }
    uintptr_t zero = 0;
    for (int i = gardees; i < nombre; ++i) SafeWrite((void*)(tab + i * 8), &zero, 8);
    SafeWrite((void*)(liste + 8), &gardees, 4);
    printf("[SAV] liste des sauvegardes : %d fichier(s), %d de la session gardee(s) (%d nom(s) notes)\n",
        nombre, gardees, nNoms);
    return r;
}

typedef uintptr_t (*ConstruireListe_t)(uintptr_t profil, uintptr_t* liste);
typedef void (*DetruireListe_t)(uintptr_t liste, uintptr_t liberer);

} // namespace

// Reconstruit le cache des sauvegardes du profil (profil +0x1B0) quand la
// session a change depuis la derniere liste. La liste passe par
// hkListerFichiers : filtree avec la session courante.
bool SauvegardesSuivreSession()
{
    if (!g_profil || !oListerFichiers) return false;
    char session[MAX_PATH];
    if (!ApCheminSession(session, sizeof(session), FICHIER)) return false;
    if (_stricmp(session, g_sessionListe) == 0) return false;

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    ConstruireListe_t construire = (ConstruireListe_t)(base + OFF_CONSTRUIRE_LISTE);
    DetruireListe_t detruire = (DetruireListe_t)(base + OFF_DETRUIRE_LISTE);
    uintptr_t cache = g_profil + PROFIL_LISTE;
    uintptr_t tmp[3] = { 0, 0, 0 };   // { tableau, nombre | capacite, allocateur }
    int32_t avant = 0;
    SafeRead((void*)(cache + 8), &avant, 4);
    printf("[SAV] session changee depuis la liste des sauvegardes : reconstruction (%d en cache)\n", avant);

    bool ok = false;
    __try {
        construire(g_profil, tmp);
        uintptr_t allocCache = 0;
        SafeRead((void*)(cache + 0x10), &allocCache, 8);
        if (allocCache == tmp[2]) {
            detruire(cache, 1);
            SafeWrite((void*)cache, &tmp[0], 8);
            SafeWrite((void*)(cache + 8), &tmp[1], 8);   // nombre et capacite
            tmp[0] = tmp[1] = 0;
            ok = true;
        } else {
            printf("[SAV] allocateurs differents (cache 0x%llX, liste 0x%llX) : cache garde\n",
                (unsigned long long)allocCache, (unsigned long long)tmp[2]);
            detruire((uintptr_t)tmp, 1);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[SAV] EXCEPTION 0x%08lX a la reconstruction de la liste des sauvegardes\n", GetExceptionCode());
        g_profil = 0;   // plus d'essai
        return false;
    }
    int32_t apres = 0;
    SafeRead((void*)(cache + 8), &apres, 4);
    printf("[SAV] liste des sauvegardes reconstruite : %d sauvegarde(s) de la session\n", apres);
    return ok;
}

void InstallHooksSauvegardes(uintptr_t modBase)
{
    struct { uintptr_t off; void* hk; void** orig; const char* nom; } h[] = {
        { OFF_ECRIRE_SAUVEGARDE, (void*)&hkEcrireSauvegarde, (void**)&oEcrireSauvegarde, "ecriture d'une sauvegarde" },
        { OFF_LISTER_FICHIERS,   (void*)&hkListerFichiers,   (void**)&oListerFichiers,   "liste des fichiers de sauvegarde" },
    };
    for (auto& x : h) {
        void* p = (void*)(modBase + x.off);
        bool ok = MH_CreateHook(p, x.hk, x.orig) == MH_OK && MH_EnableHook(p) == MH_OK;
        printf("[SAV] hook %s (+%llX) %s\n", x.nom, (unsigned long long)x.off, ok ? "OK" : "KO");
    }
}
