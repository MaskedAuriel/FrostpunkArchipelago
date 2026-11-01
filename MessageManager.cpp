#include "MessageManager.h"
#include "evenements.h"   // TuileAtlasBulles
#include "IconSubManager.h"   // ARCHIPELAGO_ICONS_TECH
#include "groupes.h"          // CleMoteur (cles des messages)
#include <cstring>
#include <cmath>      // powf, fabsf
#include <intrin.h>   // _ReturnAddress



static uint8_t g_msgModele[0x200];
static bool      g_msgCapture = false;
static uintptr_t g_msgSysteme = 0;


static const int AP_CLES = 16;
static uint8_t   g_cleBlocs[AP_CLES][0x14 + 64];

typedef void (*PoseTexte_t)(uintptr_t, uintptr_t*);
static PoseTexte_t oPoseTitre = nullptr;      // +0x1AA9D70 -> message+0x98
static PoseTexte_t oPoseTexte = nullptr;      // +0x1AA9C00 -> message+?



typedef uintptr_t(*CreerNotif_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static CreerNotif_t oCreerNotifA = nullptr, oCreerNotifB = nullptr;

typedef void (*CreerMsg_t)(uintptr_t, uintptr_t*, uintptr_t, uintptr_t);
static CreerMsg_t oCreerMessage = nullptr;

static uintptr_t hkCreerNotifA(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    if (c == 0x60) {
        static int n = 0;
        if (++n <= 4)
            printf("[MSG] ctor 0x60 : a=%llX  b=%llX  c=%llX  d=%llX\n",
                (unsigned long long)a, (unsigned long long)b,
                (unsigned long long)c, (unsigned long long)d);
    }
    return oCreerNotifA(a, b, c, d);
}

static uintptr_t g_msgSystemeFab = 0;
static uintptr_t g_msgEntreeRef = 0;
static uintptr_t g_msgParamsRef = 0;

static uintptr_t hkCreerNotifB(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    static int n = 0;
    if (++n <= 3) {
        uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
        IMAGE_DOS_HEADER dos; IMAGE_NT_HEADERS64 nt;
        SafeRead((void*)base, &dos, sizeof(dos));
        SafeRead((void*)(base + dos.e_lfanew), &nt, sizeof(nt));
        uintptr_t fin = base + nt.OptionalHeader.SizeOfImage;
    }
    return oCreerNotifB(a, b, c, d);
}



static void hkCreerMessage(uintptr_t systeme, uintptr_t* sortie,
    uintptr_t entree, uintptr_t params)
{
    oCreerMessage(systeme, sortie, entree, params);
    NoterThread("CreerMessage");

    // Comparaison ponctuelle entre l'appel reel du jeu et nos references.
    static int n = 0;
    if (++n <= 3) {
        uintptr_t pn = 0; char nom[64] = {};
        if (entree > 0x10000 && SafeRead((void*)(entree + 0x18), &pn, 8) && pn > 0x10000)
            SafeRead((void*)pn, nom, sizeof(nom) - 1);
        printf("[CREE] reel : producteur=%llX entree=%llX (\"%s\")\n",
            (unsigned long long)systeme, (unsigned long long)entree, nom);
        printf("[CREE] nous : producteur=%llX entree=%llX\n",
            (unsigned long long)g_msgSystemeFab, (unsigned long long)g_msgEntreeRef);
        fflush(stdout);
    }

    // Filet de securite : ne remplit que si le balayage n'a rien donne.
    if (!g_msgSystemeFab) g_msgSystemeFab = systeme;
    if (!g_msgEntreeRef)  g_msgEntreeRef = entree;
    if (!g_msgParamsRef)  g_msgParamsRef = params;
}

void InstallNotifHooks(uintptr_t modBase)
{
    void* p = (void*)(modBase + 0x1685A50);
    if (MH_CreateHook(p, (void*)&hkCreerNotifA, (void**)&oCreerNotifA) == MH_OK &&
        MH_EnableHook(p) == MH_OK) printf("[MSG] hook +1685A50 OK\n");

    void* q = (void*)(modBase + 0x1AA9DF0);
    if (MH_CreateHook(q, (void*)&hkCreerNotifB, (void**)&oCreerNotifB) == MH_OK &&
        MH_EnableHook(q) == MH_OK) printf("[MSG] hook +1AA9DF0 OK\n");

    void* f = (void*)(modBase + 0x167C580);
    if (MH_CreateHook(f, (void*)&hkCreerMessage, (void**)&oCreerMessage) == MH_OK &&
        MH_EnableHook(f) == MH_OK) printf("[CREE] hook OK\n");
    else printf("[CREE] hook KO\n");


    oPoseTitre = (PoseTexte_t)(modBase + 0x1AA9D70);
    oPoseTexte = (PoseTexte_t)(modBase + 0x1AA9C00);
    printf("[POSE] poseurs resolus\n");
}




typedef void (*EmettreMsg_t)(uintptr_t file, uintptr_t* message);
static EmettreMsg_t EmettreMessage = nullptr;
static uintptr_t    g_fileMessages = 0;

void InitFabriqueMessage(uintptr_t modBase)
{
    EmettreMessage = (EmettreMsg_t)(modBase + 0x16896B0);
    SafeRead((void*)(modBase + 0x408E390), &g_fileMessages, 8);
    printf("[MSG] file de messages = 0x%llX\n", (unsigned long long)g_fileMessages);
}


static bool ModeleCleValide(uintptr_t modele)
{
    if (modele <= 0x10000) return false;
    uintptr_t self = 0; uint32_t refs = 0;
    if (!SafeRead((void*)(modele - 0x0C), &self, 8)) return false;
    if (!SafeRead((void*)(modele - 0x04), &refs, 4)) return false;
    bool ok = (self == modele - 0x14) && refs > 0 && refs < 100000;
    printf("[CLE] modele 0x%llX : self=0x%llX (attendu 0x%llX) refs=%u -> %s\n",
        (unsigned long long)modele, (unsigned long long)self,
        (unsigned long long)(modele - 0x14), refs, ok ? "OK" : "REFUSE");
    return ok;
}

static char* ApCle(const char* cle, uintptr_t modele, int slot)
{
    if (slot < 0 || slot >= AP_CLES) return nullptr;
    uint8_t* b = g_cleBlocs[slot];
    if (!SafeRead((void*)(modele - 0x14), b, 0x14)) return nullptr;

    uintptr_t debut = (uintptr_t)b;
    memcpy(b + 0x08, &debut, 8);
    uint32_t refs = 0x40000000;
    memcpy(b + 0x10, &refs, 4);

    char* dst = (char*)(b + 0x14);
    int i = 0; for (; cle[i] && i < 63; ++i) dst[i] = cle[i];
    dst[i] = 0;
    return dst;
}

// --- Textes des notifications, consultes par hkTranslate ----------------------
// Sauvegarde AP des messages (2026-10-29) : la sauvegarde du jeu garde un
// message AP (ses cles "@AP/Notif/<id>/T|D" et son entree), mais nos textes
// n'existaient qu'en memoire et l'id etait recycle sur 8 places. Desormais
// l'id est UNIQUE et croissant, et chaque message est ecrit dans
// mods\ap_messages.txt ("M <id>\t<icone>\t<titre>\t<texte>", jamais efface,
// un fichier pour toutes les parties). Un id absent du cache est relu dans
// le fichier (absence memorisee : pas d'acces disque a chaque traduction).
struct NotifAP { bool actif; bool absent; int id; char icone[32]; char titre[128]; char texte[256]; };
constexpr int NOTIFS_CACHE = 64;
static NotifAP g_notifs[NOTIFS_CACHE];
static int     g_notifSuivante = -1;   // -1 : pas encore lu dans le fichier

static bool CheminFichierMessages(char* out, size_t taille)
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return false;
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    _snprintf_s(out, taille, _TRUNCATE, "%smods\\ap_messages.txt", base);
    return true;
}

// Champ n (0 = le premier apres l'id) d'une ligne, dans out.
static void ChampMessage(const char* ligne, int n, char* out, size_t taille)
{
    const char* p = strchr(ligne, '\t');
    for (int i = 0; p && i < n; ++i) p = strchr(p + 1, '\t');
    out[0] = 0;
    if (!p) return;
    ++p;
    size_t k = 0;
    while (p[k] && p[k] != '\t' && p[k] != '\n' && p[k] != '\r' && k + 1 < taille) { out[k] = p[k]; ++k; }
    out[k] = 0;
}

// Parcourt le fichier : plus grand id (pour le suivant) et, si 'cherche'
// >= 0, la ligne de cet id dans 'n'. true si trouvee.
static bool ParcourirMessages(int cherche, NotifAP* n, int* idMax)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (idMax) *idMax = -1;
    if (!CheminFichierMessages(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "r") != 0 || !f) return false;
    static char ligne[1024];
    bool trouve = false;
    while (fgets(ligne, sizeof(ligne), f)) {
        if (ligne[0] != 'M' || ligne[1] != ' ') continue;
        int id = atoi(ligne + 2);
        if (idMax && id > *idMax) *idMax = id;
        if (id == cherche && n) {
            ChampMessage(ligne, 0, n->icone, sizeof(n->icone));
            ChampMessage(ligne, 1, n->titre, sizeof(n->titre));
            ChampMessage(ligne, 2, n->texte, sizeof(n->texte));
            trouve = true;
        }
    }
    fclose(f);
    return trouve;
}

static void EcrireChampMessage(FILE* f, const char* t)
{
    fputc('\t', f);
    for (; t && *t; ++t) fputc((*t == '\t' || *t == '\r' || *t == '\n') ? ' ' : *t, f);
}

static void SauverMessage(const NotifAP& n)
{
    char chemin[MAX_PATH];
    FILE* f = nullptr;
    if (!CheminFichierMessages(chemin, sizeof(chemin)) || fopen_s(&f, chemin, "a") != 0 || !f) {
        printf("[MSG] ecriture de mods\\ap_messages.txt impossible\n");
        return;
    }
    fprintf(f, "M %d", n.id);
    EcrireChampMessage(f, n.icone);
    EcrireChampMessage(f, n.titre);
    EcrireChampMessage(f, n.texte);
    fputc('\n', f);
    fclose(f);
}

// Entree du cache pour un id (relue dans le fichier au besoin), ou nullptr.
static const NotifAP* NotifParId(int id)
{
    if (id < 0) return nullptr;
    NotifAP& n = g_notifs[id % NOTIFS_CACHE];
    if (n.id == id && (n.actif || n.absent)) return n.actif ? &n : nullptr;
    NotifAP lu = {};
    lu.id = id;
    if (ParcourirMessages(id, &lu, nullptr)) {
        lu.actif = true;
        printf("[MSG] message AP %d relu dans notre sauvegarde : \"%s\" (icone %s)\n", id, lu.titre, lu.icone);
    }
    else lu.absent = true;
    n = lu;
    return n.actif ? &n : nullptr;
}

// Texte associe a une de nos cles, ou nullptr.
const char* ApNotifTexte(int id, bool titre)
{
    const NotifAP* n = NotifParId(id);
    if (!n) return nullptr;
    return titre ? n->titre : n->texte;
}

// Id d'un message AP d'apres sa cle de titre (message +0x98), -1 sinon.
static int IdDeCleNotif(uintptr_t cle)
{
    char k[64] = "";
    if (cle <= 0x10000 || !ReadAscii(cle, k, sizeof(k))) return -1;
    const char* pre = "@AP/Notif/";
    if (strncmp(k, pre, strlen(pre))) return -1;
    return atoi(k + strlen(pre));
}



void MessageSetReferences(uintptr_t* systemes, int nSys,
    uintptr_t* entrees, int nEnt)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    g_msgParamsRef = base + 0x40B1B30;

    // Plusieurs producteurs et plusieurs entrees de meme nom coexistent :
    // on affiche tout pour pouvoir choisir sur un critere sur.
    printf("[MSG] %d producteur(s) :\n", nSys);
    for (int i = 0; i < nSys && i < 8; ++i) {
        uintptr_t v = 0;
        SafeRead((void*)systemes[i], &v, 8);
        printf("   [%d] %llX  vtable=%llX\n", i,
            (unsigned long long)systemes[i], (unsigned long long)v);
    }

    printf("[MSG] entrees nommees CitizensDied :\n");
    for (int i = 0; i < nEnt; ++i) {
        uintptr_t pn = 0; char nom[64] = {};
        if (!SafeRead((void*)(entrees[i] + 0x18), &pn, 8) || pn <= 0x10000) continue;
        SafeRead((void*)pn, nom, sizeof(nom) - 1);
        if (!StrEq(nom, "CitizensDied")) continue;

        uintptr_t titre = 0;
        SafeRead((void*)(entrees[i] + 0x50), &titre, 8);
        char cle[96] = {};
        if (titre > 0x10000) SafeRead((void*)titre, cle, sizeof(cle) - 1);
        printf("   %llX  titre=\"%s\"\n", (unsigned long long)entrees[i], cle);
    }

    g_msgSystemeFab = (nSys > 0) ? systemes[0] : 0;
    g_msgEntreeRef = 0;
    for (int i = 0; i < nEnt && !g_msgEntreeRef; ++i) {
        uintptr_t pn = 0; char nom[64] = {};
        if (!SafeRead((void*)(entrees[i] + 0x18), &pn, 8) || pn <= 0x10000) continue;
        SafeRead((void*)pn, nom, sizeof(nom) - 1);
        if (StrEq(nom, "CitizensDied")) g_msgEntreeRef = entrees[i];
    }
}

uintptr_t MessageEntreeRef() { return g_msgEntreeRef; }

// --- Capture des entrees de message a leur construction ---------------------
// +0x19E68B0 : constructeur de FrostpunkMessageEntry (seule fonction qui
// charge sa vtable +0x21363B0 ; valide le 2026-10-15 : 510 vivantes sur 582
// construites). Remplace le balayage memoire : ScootingInit lit ces captures.
typedef uintptr_t(*CtorEntree_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static CtorEntree_t  oCtorEntree = nullptr;
static uintptr_t     g_entObjets[2048];
static ListeCaptures g_capEnt = { SRWLOCK_INIT, g_entObjets, 2048, 0,
                                  "FrostpunkMessageEntry", 0 };

static uintptr_t hkCtorEntree(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d)
{
    uintptr_t r = oCtorEntree(a, b, c, d);
    CaptureAjouter(g_capEnt, a);
    return r;
}

void InstallCaptureEntrees(uintptr_t modBase)
{
    void* p = (void*)(modBase + 0x19E68B0);
    if (MH_CreateHook(p, (void*)&hkCtorEntree, (void**)&oCtorEntree) == MH_OK &&
        MH_EnableHook(p) == MH_OK) printf("[CAPENT] hook +19E68B0 OK\n");
    else printf("[CAPENT] hook +19E68B0 KO\n");
}

int EntreesMessageCaptures(uintptr_t* out, int maxOut)
{
    return CaptureLire(g_capEnt, out, maxOut);
}

void RapportCapturesEntrees()
{
    static uintptr_t vivantes[2048];
    int n = CaptureLire(g_capEnt, vivantes, 2048);
    bool refVue = false;
    for (int i = 0; i < n; ++i) if (vivantes[i] == g_msgEntreeRef) { refVue = true; break; }
    printf("[CAPENT] %ld appel(s) du constructeur, %d en liste, %d vivante(s), CitizensDied %s\n",
        g_capEnt.appels, g_capEnt.n, n, refVue ? "presente" : "ABSENTE");
}
// --- File des popups --------------------------------------------------------
// Une popup ne doit JAMAIS etre emise depuis le thread du mod : le 2026-10-28,
// une emission (EmettreMessage) chevauchant un credit de ressource sur le
// thread du jeu (qui peut emettre sa propre notification de seuil) a fait
// planter le jeu. AfficherNotificationAP ne fait donc que mettre en file ;
// PompeNotifications, appelee depuis un hook du thread du jeu, les emet.
struct PopupEnAttente { char titre[128]; char texte[256]; char icone[32]; };
static PopupEnAttente g_filePopups[16];
static int            g_debutPopups = 0, g_nbPopups = 0;
static SRWLOCK        g_verrouPopups = SRWLOCK_INIT;

static void EmettreNotificationMaintenant(const char* titre, const char* texte, const char* icone);

void AfficherNotificationAP(const char* titre, const char* texte, const char* icone)
{
    AcquireSRWLockExclusive(&g_verrouPopups);
    if (g_nbPopups == 16) {                    // pleine : on perd la plus ancienne
        g_debutPopups = (g_debutPopups + 1) % 16;
        g_nbPopups--;
        printf("[MSG] file des popups pleine : la plus ancienne est perdue\n");
    }
    PopupEnAttente& p = g_filePopups[(g_debutPopups + g_nbPopups) % 16];
    strncpy_s(p.titre, titre ? titre : "", _TRUNCATE);
    strncpy_s(p.texte, texte ? texte : "", _TRUNCATE);
    strncpy_s(p.icone, icone ? icone : "icon_vanilla", _TRUNCATE);
    g_nbPopups++;
    ReleaseSRWLockExclusive(&g_verrouPopups);
}

// --- Entree de message propre a nos popups ----------------------------------
// FrostpunkMessageEntry (reflexion FUN_1419e76e0, 0xB8 ; entrees au pas de
// 0xE0) : +0x18 nom, +0x30 Enabled, +0x38 Parameters, +0x50 Title, +0x58
// Description, +0x60 Image (UITextureTile), +0x78 Icon (UITextureTile), +0x90
// Cooldown, +0x98 Category (lien GUID vers FrostpunkMessageCategoryEntry :
// +0x18 type, +0x1C Important, +0x20 Duration, +0x24 World map), +0xA8 Type
// (eMessageType : 0 Notification, 1 Openable notification, 2 Modal...),
// +0xAC Important, +0xB0 Duration, +0xB4 World map.
// Panneau des notifications (FUN_141aab820, frostpunkuinotificationpanel) :
// tuile de l'enfant "ICON" fournie par la notification, et etat d'animation
// "IMPORTANT_NOTIFICATION" (l'orange) joue si l'entree est importante (+0xAC,
// ou +0x1C de sa categorie quand le lien +0x98 se resout).
// Nos popups empruntaient CitizensDied (importante -> orange, icone du jeu).
// On en fait une COPIE par icone : Important a 0, categorie videe, icone =
// case de l'atlas des bulles.
constexpr int ENT_TAILLE       = 0xE0;   // pas des entrees (la reflexion annonce 0xB8)
constexpr int ENT_IMAGE        = 0x60;
constexpr int ENT_ICONE        = 0x78;
constexpr int ENT_CATEGORIE    = 0x98;
constexpr int ENT_TYPE         = 0xA8;
constexpr int ENT_IMPORTANTE   = 0xAC;
constexpr int MAX_ENTREES_AP   = 8;

struct EntreeAP { char icone[32]; uintptr_t entree; };
static EntreeAP g_entreesAP[MAX_ENTREES_AP];
static int      g_nEntreesAP = 0;
static uintptr_t g_entreeRefCopiee = 0;   // l'entree d'origine des copies (change si la reference change)

static void DecrireTuileMsg(const char* libelle, uintptr_t t)
{
    uintptr_t tex = 0;
    int32_t v[4] = {};
    char nom[128] = "";
    SafeRead((void*)t, &tex, 8);
    SafeRead((void*)(t + 8), v, sizeof(v));
    if (tex > 0x10000) ReadAscii(tex, nom, sizeof(nom));
    printf("[MSG]   %s : texture 0x%llX \"%s\" case (%d,%d) sur (%d,%d)\n", libelle,
        (unsigned long long)tex, nom, v[0], v[1], v[2], v[3]);
}

// --- Diagnostic : popups reellement affichees -------------------------------
// Hook de FUN_141aab820(panneau, notification) (ajout d'une popup au panneau
// des notifications). Releve : classe de la notification, message
// (notification +0x18), entree (message +0x80), son nom, Important (+0xAC),
// et la tuile d'icone (FUN_141687ac0 : entree +0x78, ou message +0x28 si
// message +0x20). Test du 2026-10-28 : notre copie d'entree ne changeait
// rien a l'affichage.
typedef void(*AjouterPopup_t)(uintptr_t panneau, uintptr_t notification, uintptr_t p3, uintptr_t p4);
constexpr uintptr_t UICONFIG_ICONE_DEFAUT = 0x40615C0 + 0x258;   // FrostpunkUIConfig "Default notification icon"
static AjouterPopup_t oAjouterPopup = nullptr;

static bool EstNotreEntree(uintptr_t e);
static uintptr_t EntreeDeNotre(const char* icone);

// Pose d'une tuile sur un enfant d'un controle : FUN_1410260c0(controle,
// &nom, tuile, x, y, nx, ny) (le panneau l'appelle avec "ICON" et la tuile
// de FUN_141687ac0). Releve limite a l'ajout de NOS messages : nom de
// l'enfant, texture et case transmises, et controle trouve (retour non nul).
typedef uintptr_t(*PoserTuileEnfant_t)(uintptr_t controle, uintptr_t nom, uintptr_t tuile, uintptr_t x,
                                       uintptr_t y, uintptr_t nx, uintptr_t ny, uintptr_t p8);
static PoserTuileEnfant_t oPoserTuileEnfant = nullptr;
static bool g_dansAjoutAP = false;

static uintptr_t hkPoserTuileEnfant(uintptr_t controle, uintptr_t nom, uintptr_t tuile, uintptr_t x,
                                    uintptr_t y, uintptr_t nx, uintptr_t ny, uintptr_t p8)
{
    uintptr_t r = oPoserTuileEnfant(controle, nom, tuile, x, y, nx, ny, p8);
    if (g_dansAjoutAP) {
        char n[64] = "?", tex[128] = "";
        uintptr_t pn = 0, pt = 0;
        if (nom && SafeRead((void*)nom, &pn, 8) && pn > 0x10000) ReadAscii(pn, n, sizeof(n));
        if (tuile && SafeRead((void*)tuile, &pt, 8) && pt > 0x10000) ReadAscii(pt, tex, sizeof(tex));
        printf("[POPUP]   pose de tuile sur l'enfant \"%s\" : texture \"%s\" case (%d,%d) sur (%d,%d) -> controle 0x%llX\n",
            n, tex, (int)x, (int)y, (int)nx, (int)ny, (unsigned long long)r);
    }
    return r;
}

// Arbre d'un controle d'interface (FUN_141025ff0, recherche d'un enfant par
// nom) : +0x1D8 nom, +0x220 premier enfant, enfant +0x240 frere suivant.
// Test du 2026-10-28 : l'enfant "ICON" n'existe dans AUCUN message (pose de
// tuile toujours refusee) ; l'icone du cercle vient du preset
// (BASIC_NOTIFICATION : bulle de dialogue, WORLD_MAP_NOTIFICATION : pioches).
// On liste l'arbre pour trouver l'element image du preset.
static void DecrireArbreControle(uintptr_t c, int profondeur, int* budget)
{
    for (; c > 0x10000 && *budget > 0; --*budget) {
        char nom[64] = "", cls[96] = "?";
        uintptr_t pn = 0, enfant = 0, frere = 0;
        if (SafeRead((void*)(c + 0x1D8), &pn, 8) && pn > 0x10000) ReadAscii(pn, nom, sizeof(nom));
        GetClassNameFP(c, cls, sizeof(cls));
        printf("[POPUP]     %*s\"%s\" %s 0x%llX\n", profondeur * 2, "", nom, cls, (unsigned long long)c);
        SafeRead((void*)(c + 0x220), &enfant, 8);
        if (enfant && profondeur < 8) DecrireArbreControle(enfant, profondeur + 1, budget);
        if (!SafeRead((void*)(c + 0x240), &frere, 8)) return;
        c = frere;
    }
}

// Controle d'un message ajoute : enregistrements de 0x30 en panneau +0x190
// (compte +0x198) : +0x00 controle, +0x28 notification (FUN_141aad120).
static uintptr_t ControleDuMessage(uintptr_t panneau, uintptr_t notif)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(panneau + 0x190), &tab, 8);
    SafeRead((void*)(panneau + 0x198), &n, 4);
    for (int i = 0; i < n && i < 64 && tab; ++i) {
        uintptr_t ctrl = 0, nt = 0;
        SafeRead((void*)(tab + i * 0x30), &ctrl, 8);
        SafeRead((void*)(tab + i * 0x30 + 0x28), &nt, 8);
        if (nt == notif) return ctrl;
    }
    return 0;
}

// Icone du cercle d'un de NOS messages. Releve de l'arbre (2026-10-28) : le
// cercle est "static_left_notification_circle" / "notification_icon" /
// "icon" (UIPicture, en MINUSCULES) ; le panneau pose l'icone de l'entree sur
// "ICON" (majuscules), introuvable dans tous les messages : l'image du cercle
// vient du preset. On pose donc notre tuile (celle de notre entree) sur
// "icon", APRES l'ajout, avec la fonction du jeu FUN_1410260c0(controle,
// &nom, tuile, x, y, nx, ny). Le nom est construit et detruit par les
// fonctions du moteur (FUN_140e4fa20 / FUN_140e4eab0), comme le panneau.
typedef void(*NomConstruire_t)(uintptr_t* nom, const char* texte);
typedef void(*NomDetruire_t)(uintptr_t* nom);

static void PoserIconeSurControle(uintptr_t ctrl, uintptr_t ent);

static void PoserIconeCercle(uintptr_t panneau, uintptr_t notif, uintptr_t ent)
{
    PoserIconeSurControle(ControleDuMessage(panneau, notif), ent);
}

static void PoserIconeSurControle(uintptr_t ctrl, uintptr_t ent)
{
    uintptr_t tex = 0;
    int32_t v[4] = {};
    if (!ctrl || !SafeRead((void*)(ent + ENT_ICONE), &tex, 8) || !tex ||
        !SafeRead((void*)(ent + ENT_ICONE + 8), v, sizeof(v))) {
        printf("[POPUP]   icone du cercle non posee (controle 0x%llX, texture 0x%llX)\n",
            (unsigned long long)ctrl, (unsigned long long)tex);
        return;
    }
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    NomConstruire_t construire = (NomConstruire_t)(base + 0xE4FA20);
    NomDetruire_t detruire = (NomDetruire_t)(base + 0xE4EAB0);
    uintptr_t nom = 0;
    uintptr_t r = 0;
    __try {
        construire(&nom, "icon");
        r = oPoserTuileEnfant(ctrl, (uintptr_t)&nom, ent + ENT_ICONE, (uint32_t)v[0], (uint32_t)v[1],
                              (uint32_t)v[2], (uint32_t)v[3], 0);
        detruire(&nom);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[POPUP]   EXCEPTION 0x%08lX a la pose de l'icone du cercle\n", GetExceptionCode());
        return;
    }
    printf("[POPUP]   icone du cercle posee sur \"icon\" : case (%d,%d) sur (%d,%d) -> element 0x%llX\n",
        v[0], v[1], v[2], v[3], (unsigned long long)r);
}

// ---------------------------------------------------------------------------
// Couleur du cercle de NOS messages (2026-10-28). Test de l'auteur : notre
// icone s'affiche, mais tout le rond (icone comprise) est teinte en orange.
// Decompiles :
//  - UIElementRecipe (reflexion FUN_14110fc60) : "Color" (HDRColor) et
//    "Color mode" (Static / Multiply alpha / Multiply color) ;
//  - element d'interface (ctor FUN_141023250, 0x280) : couleur LINEAIRE en
//    +0x120 (4 flottants, blanc par defaut) ; posee par la tache 0x18 du
//    thread de rendu (FUN_1410fe640) ;
//  - FUN_1410297f0(element, float[4]) = poser la couleur : entree en GAMMA
//    (puissances 2.2, 2.2, 2.2, 1.0, DAT_14293edf0), mise en file pour le
//    thread de rendu. A appeler depuis le thread du jeu.
// Releve du 2026-10-28 : la teinte n'est PAS une multiplication par le
// parent : le preset du message pose lui-meme, juste APRES l'ajout, l'orange
// sur 3 elements du cercle : "bg" (1, 0.619, 0, 0.698), "decorative_highlight"
// (1, 0.523, 0.058, 1) et "icon" (1, 0.619, 0, ...). Les autres restent
// blancs (ou gris fonce : "small_bg").
// Methode : a l'ajout d'un de nos messages, ces 3 elements recoivent nos
// couleurs par le poseur du moteur (en file APRES les taches du preset) ;
// puis, pendant 2 s, CouleursMessagesTick (boucle du mod, 16 ms) repose nos
// couleurs par ecriture directe en +0x120 des que le preset les a reprises
// (log de l'instant). Avant qu'un message du JEU ne reprenne un controle que
// nous avons colore (controles recycles par le panneau), ces elements
// repassent en blanc (couleur par defaut), en file AVANT les taches de son
// preset : c'est son preset qui decide (l'ancienne "restauration" rendait
// l'orange releve a 1,5 s : contour jaune sur un message de site).
typedef void(*PoserCouleur_t)(uintptr_t element, const float* couleur);
constexpr int ELEM_NOM = 0x1D8, ELEM_ENFANT = 0x220, ELEM_FRERE = 0x240, ELEM_COULEUR = 0x120;
constexpr ULONGLONG COULEUR_DUREE_SURVEILLANCE = 2000;   // ms

// Couleurs LINEAIRES de nos elements (bleu-vert : pas l'orange du jeu). Les
// alphas sont ceux du preset du jeu.
static const struct { const char* nom; float c[4]; } COULEURS_AP[] = {
    { "bg",                   { 0.071f, 0.531f, 0.893f, 0.698f } },
    { "decorative_highlight", { 0.071f, 0.531f, 0.893f, 1.000f } },
    { "icon",                 { 1.000f, 1.000f, 1.000f, 1.000f } },   // couleurs d'origine de notre image
};
constexpr int N_COULEURS_AP = sizeof(COULEURS_AP) / sizeof(COULEURS_AP[0]);

struct ControleColore {
    uintptr_t ctrl = 0;
    uintptr_t elem[N_COULEURS_AP] = {};
    ULONGLONG quand = 0;          // GetTickCount64 de l'ajout
    bool deriveVue = false;       // le preset a deja repris la main (log une fois)
    uintptr_t entree = 0;         // notre copie d'entree (icone du cercle a reposer)
};
static bool g_iconesAReposer = false;   // apres un chargement : 1er passage de PompeNotifications
static ControleColore g_ctrlColores[16];
static SRWLOCK g_verrouCouleurs = SRWLOCK_INIT;

static bool NomElement(uintptr_t c, char* out, size_t taille)
{
    uintptr_t pn = 0;
    out[0] = 0;
    return SafeRead((void*)(c + ELEM_NOM), &pn, 8) && pn > 0x10000 && ReadAscii(pn, out, taille);
}

static uintptr_t ChercherElement(uintptr_t c, const char* nom, int profondeur)
{
    for (int garde = 0; c > 0x10000 && garde < 64; ++garde) {
        char n[64];
        if (NomElement(c, n, sizeof(n)) && !strcmp(n, nom)) return c;
        uintptr_t enfant = 0, frere = 0;
        if (profondeur < 10 && SafeRead((void*)(c + ELEM_ENFANT), &enfant, 8) && enfant) {
            uintptr_t r = ChercherElement(enfant, nom, profondeur + 1);
            if (r) return r;
        }
        if (!SafeRead((void*)(c + ELEM_FRERE), &frere, 8)) return 0;
        c = frere;
    }
    return 0;
}

static bool CouleurProche(const float* a, const float* b)
{
    for (int i = 0; i < 4; ++i) if (fabsf(a[i] - b[i]) > 0.01f) return false;
    return true;
}

// Couleur lineaire -> poseur du moteur (entree en gamma). Thread du jeu.
static bool PoserCouleurMoteur(uintptr_t e, const float* lin)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    PoserCouleur_t poser = (PoserCouleur_t)(base + 0x10297F0);
    float gamma[4] = { powf(lin[0], 1.0f / 2.2f), powf(lin[1], 1.0f / 2.2f), powf(lin[2], 1.0f / 2.2f), lin[3] };
    __try { poser(e, gamma); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[POPUP]   EXCEPTION 0x%08lX a la pose d'une couleur\n", GetExceptionCode());
        return false;
    }
    return true;
}

static ControleColore* ControleColoreDe(uintptr_t ctrl)
{
    for (ControleColore& c : g_ctrlColores) if (ctrl && c.ctrl == ctrl) return &c;
    return nullptr;
}

// AVANT l'ajout d'un message du jeu (thread du jeu) : le panneau va prendre le
// dernier controle libre (+0x1A8 / compte +0x1B0, ou +0x1F8 / +0x200 pour les
// tutoriels, FUN_141aab820). Si c'est un controle que nous avons colore, ses
// elements repassent en blanc avant les taches du preset.
static void CouleurAvantAjoutJeu(uintptr_t panneau)
{
    static const int LISTES[2][2] = { { 0x1A8, 0x1B0 }, { 0x1F8, 0x200 } };
    AcquireSRWLockExclusive(&g_verrouCouleurs);
    for (auto& l : LISTES) {
        uintptr_t tab = 0, ctrl = 0;
        int32_t n = 0;
        if (!SafeRead((void*)(panneau + l[0]), &tab, 8) || !SafeRead((void*)(panneau + l[1]), &n, 4) ||
            !tab || n <= 0 || n > 64 || !SafeRead((void*)(tab + (uintptr_t)(n - 1) * 8), &ctrl, 8)) continue;
        ControleColore* cc = ControleColoreDe(ctrl);
        if (!cc) continue;
        static const float BLANC[4] = { 1, 1, 1, 1 };
        for (uintptr_t e : cc->elem) if (e) PoserCouleurMoteur(e, BLANC);
        printf("[POPUP]   controle 0x%llX (colore par nous) repris par un message du jeu : elements remis en blanc\n",
            (unsigned long long)ctrl);
        *cc = ControleColore();
    }
    ReleaseSRWLockExclusive(&g_verrouCouleurs);
}

// APRES l'ajout d'un de NOS messages (thread du jeu).
static void CouleurApresAjoutAP(uintptr_t panneau, uintptr_t notif, uintptr_t ent)
{
    uintptr_t ctrl = ControleDuMessage(panneau, notif);
    uintptr_t cercle = ctrl ? ChercherElement(ctrl, "static_left_notification_circle", 0) : 0;
    if (!cercle) {
        printf("[POPUP]   couleur : controle 0x%llX / cercle introuvable\n", (unsigned long long)ctrl);
        return;
    }
    AcquireSRWLockExclusive(&g_verrouCouleurs);
    ControleColore* cc = ControleColoreDe(ctrl);
    if (!cc) {
        for (ControleColore& c : g_ctrlColores) if (!c.ctrl) { cc = &c; break; }
        if (!cc) cc = &g_ctrlColores[0];   // plein : ne devrait pas arriver (controles recycles)
        *cc = ControleColore();
        cc->ctrl = ctrl;
    }
    int poses = 0;
    for (int i = 0; i < N_COULEURS_AP; ++i) {
        cc->elem[i] = ChercherElement(cercle, COULEURS_AP[i].nom, 0);
        if (cc->elem[i] && PoserCouleurMoteur(cc->elem[i], COULEURS_AP[i].c)) ++poses;
    }
    cc->quand = GetTickCount64();
    cc->deriveVue = false;
    cc->entree = ent;
    ReleaseSRWLockExclusive(&g_verrouCouleurs);
    printf("[POPUP]   couleur : %d/%d element(s) colore(s) a l'ajout (controle 0x%llX)\n", poses, N_COULEURS_AP,
        (unsigned long long)ctrl);
}

// Sauvegarde AP (2026-10-29) : les messages AP recharges sont ajoutes au
// panneau PENDANT LoadGame (sous l'ecran de chargement) ; l'icone posee sur
// "icon" y est perdue (les couleurs tiennent). On la repose, sur le thread du
// jeu, a la sortie de LoadGame puis au 1er passage de PompeNotifications.
// Un controle repris par un message du jeu a ete oublie (CouleurAvantAjoutJeu).
static void ReposerIconesMessages(const char* quand)
{
    uintptr_t ctrls[16], ents[16];
    int n = 0;
    AcquireSRWLockExclusive(&g_verrouCouleurs);
    for (ControleColore& c : g_ctrlColores)
        if (c.ctrl && c.entree && n < 16) { ctrls[n] = c.ctrl; ents[n] = c.entree; ++n; }
    ReleaseSRWLockExclusive(&g_verrouCouleurs);
    if (n) printf("[POPUP] icones des messages AP reposees (%s) : %d\n", quand, n);
    for (int i = 0; i < n; ++i) PoserIconeSurControle(ctrls[i], ents[i]);
}

void MessagesApresChargement()
{
    ReposerIconesMessages("sortie de LoadGame");
    g_iconesAReposer = true;
}

// Boucle du mod (16 ms) : pendant 2 s apres l'ajout, repose nos couleurs par
// ecriture directe en +0x120 si le preset les a reprises.
void CouleursMessagesTick()
{
    ULONGLONG maintenant = GetTickCount64();
    AcquireSRWLockExclusive(&g_verrouCouleurs);
    for (ControleColore& c : g_ctrlColores) {
        if (!c.ctrl || maintenant - c.quand > COULEUR_DUREE_SURVEILLANCE) continue;
        int reposees = 0;
        for (int i = 0; i < N_COULEURS_AP; ++i) {
            float actuel[4] = {};
            if (!c.elem[i] || !SafeRead((void*)(c.elem[i] + ELEM_COULEUR), actuel, sizeof(actuel))) continue;
            if (CouleurProche(actuel, COULEURS_AP[i].c)) continue;
            if (SafeWrite((void*)(c.elem[i] + ELEM_COULEUR), (void*)COULEURS_AP[i].c, 16)) ++reposees;
        }
        if (reposees && !c.deriveVue) {
            c.deriveVue = true;
            printf("[POPUP]   couleur : le preset a repris la main %llu ms apres l'ajout (%d element(s) reposes)\n",
                (unsigned long long)(maintenant - c.quand), reposees);
        }
    }
    ReleaseSRWLockExclusive(&g_verrouCouleurs);
}

static void DecrireControleDuMessage(uintptr_t panneau, uintptr_t notif)
{
    static int releves = 0;
    if (releves >= 12) return;
    uintptr_t tab = 0;
    int32_t n = 0;
    SafeRead((void*)(panneau + 0x190), &tab, 8);
    SafeRead((void*)(panneau + 0x198), &n, 4);
    for (int i = 0; i < n && i < 64 && tab; ++i) {
        uintptr_t ctrl = 0, nt = 0;
        SafeRead((void*)(tab + i * 0x30), &ctrl, 8);
        SafeRead((void*)(tab + i * 0x30 + 0x28), &nt, 8);
        if (nt != notif) continue;
        ++releves;
        printf("[POPUP]   arbre du controle du message :\n");
        int budget = 60;
        uintptr_t enfant = 0;
        char nom[64] = "";
        uintptr_t pn = 0;
        if (SafeRead((void*)(ctrl + 0x1D8), &pn, 8) && pn > 0x10000) ReadAscii(pn, nom, sizeof(nom));
        printf("[POPUP]     racine \"%s\" 0x%llX\n", nom, (unsigned long long)ctrl);
        SafeRead((void*)(ctrl + 0x220), &enfant, 8);
        DecrireArbreControle(enfant, 1, &budget);
        return;
    }
    printf("[POPUP]   controle du message introuvable (%d enregistrement(s))\n", n);
}

static void hkAjouterPopup(uintptr_t panneau, uintptr_t notif, uintptr_t p3, uintptr_t p4)
{
    char cls[96] = "?", nom[96] = "?";
    uintptr_t msg = 0, ent = 0, pn = 0;
    uint8_t imp = 0, propre = 0;
    GetClassNameFP(notif, cls, sizeof(cls));
    SafeRead((void*)(notif + 0x18), &msg, 8);
    if (msg > 0x10000) {
        SafeRead((void*)(msg + 0x80), &ent, 8);
        SafeRead((void*)(msg + 0x20), &propre, 1);
    }
    // Sauvegarde AP (2026-10-29) : un message AP recharge pointe l'entree
    // d'ORIGINE (CitizensDied : importante, icone du jeu ; notre copie n'est
    // pas sauvegardee). On le reconnait a sa cle de titre (message +0x98,
    // "@AP/Notif/<id>/T") et on lui rend, AVANT l'ajout, la copie de l'icone
    // enregistree pour cet id : icone du cercle, couleurs et ouverture suivent.
    if (msg > 0x10000 && ent > 0x10000 && !EstNotreEntree(ent)) {
        uintptr_t cle = 0;
        SafeRead((void*)(msg + 0x98), &cle, 8);
        int id = IdDeCleNotif(cle);
        if (id >= 0) {
            char nomEnt[64] = "";
            if (SafeRead((void*)(ent + 0x18), &pn, 8) && pn > 0x10000) ReadAscii(pn, nomEnt, sizeof(nomEnt));
            if (!g_msgEntreeRef && !strcmp(nomEnt, "CitizensDied")) g_msgEntreeRef = ent;   // avant PartieTick
            const NotifAP* n = NotifParId(id);
            uintptr_t notre = EntreeDeNotre(n && n->icone[0] ? n->icone : "icon_vanilla");
            if (notre && EstNotreEntree(notre)) {
                SafeWrite((void*)(msg + 0x80), &notre, 8);
                printf("[MSG] message AP %d recharge : entree 0x%llX \"%s\" -> notre copie 0x%llX (icone %s)\n", id,
                    (unsigned long long)ent, nomEnt, (unsigned long long)notre, n ? n->icone : "?");
                ent = notre;
            }
            else printf("[MSG] message AP %d recharge : copie d'entree indisponible (reference 0x%llX)\n", id,
                (unsigned long long)g_msgEntreeRef);
        }
    }
    if (ent > 0x10000) {
        SafeRead((void*)(ent + ENT_IMPORTANTE), &imp, 1);
        if (SafeRead((void*)(ent + 0x18), &pn, 8) && pn > 0x10000) ReadAscii(pn, nom, sizeof(nom));
    }
    printf("[POPUP] %s : message 0x%llX, entree 0x%llX \"%s\"%s, importante %d, icone propre au message %d\n",
        cls, (unsigned long long)msg, (unsigned long long)ent, nom, EstNotreEntree(ent) ? " (NOTRE COPIE)" : "",
        imp, propre);
    if (ent > 0x10000) DecrireTuileMsg("icone lue", propre ? msg + 0x28 : ent + ENT_ICONE);
    // Test du 2026-10-28 : toutes les entrees du jeu ont la meme icone (case
    // (1,9)) alors que les icones affichees varient (site d'exploration...) :
    // l'icone du cercle vient d'une donnee du MESSAGE. On vide le message
    // (0xF0 octets, FUN_141aa9780) pour comparer un message a loupe et un
    // message a icone.
    if (msg > 0x10000) {
        for (int off = 0; off < 0xF0; off += 8) {
            uintptr_t q = 0;
            if (!SafeRead((void*)(msg + off), &q, 8) || !q) continue;
            char t[96] = "", c2[96] = "";
            if (q > 0x10000) {
                if (!GetClassNameFP(q, c2, sizeof(c2))) c2[0] = 0;
                if (!c2[0] && ReadAscii(q, t, sizeof(t)) && (strlen(t) < 3 || (unsigned char)t[0] < 0x20)) t[0] = 0;
            }
            printf("[POPUP]     msg+0x%02X = 0x%016llX %s%s\n", off, (unsigned long long)q, c2, t);
        }
    }

    // Icone par defaut des messages : FrostpunkUIConfig (instance statique
    // DAT_1440615C0, reflexion FUN_14130ef90) +0x258 "Default notification
    // icon". Hypothese du 2026-10-28 : la loupe ((12,4) de Icons.dds) est
    // cette icone par defaut, employee quand le message n'a pas de contexte
    // (les messages du jeu a icone ont un contexte, ex. SiteExplored). Pour
    // NOS messages seulement, sa case est remplacee le temps de l'ajout au
    // panneau, puis restauree.
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t defaut = base + UICONFIG_ICONE_DEFAUT;
    static bool decrite = false;
    if (!decrite) { decrite = true; DecrireTuileMsg("icone par defaut des messages (UIConfig +0x258)", defaut); }
    int32_t caseOrigine[2] = {}, caseNotre[2] = {};
    bool remplacee = false;
    if (EstNotreEntree(ent) && SafeRead((void*)(defaut + 8), caseOrigine, 8) &&
        SafeRead((void*)(ent + ENT_ICONE + 8), caseNotre, 8)) {
        // La case de notre entree est celle de l'atlas des icones (Icons.dds).
        remplacee = SafeWrite((void*)(defaut + 8), caseNotre, 8);
        printf("[POPUP]   icone par defaut (%d,%d) -> (%d,%d) le temps de l'ajout\n",
            caseOrigine[0], caseOrigine[1], caseNotre[0], caseNotre[1]);
    }
    // Test du 2026-10-28 : pour nos messages, l'enfant "ICON" n'existe pas
    // (retour 0). On releve maintenant les poses de tuile pendant l'ajout de
    // TOUS les messages, pour voir quel enfant recoit l'icone variable d'un
    // message du jeu (ex. pioches de SiteExplored).
    g_dansAjoutAP = EstNotreEntree(ent);
    if (!EstNotreEntree(ent)) CouleurAvantAjoutJeu(panneau);
    oAjouterPopup(panneau, notif, p3, p4);
    g_dansAjoutAP = false;
    if (remplacee) SafeWrite((void*)(defaut + 8), caseOrigine, 8);
    if (EstNotreEntree(ent)) {
        PoserIconeCercle(panneau, notif, ent);
        CouleurApresAjoutAP(panneau, notif, ent);
    }
}

// Clic sur une popup : FUN_141aad120(panneau, evenement d'interface) ;
// evenement +0x18 = controle, +0x24 = bouton (0x10000 gauche, 0x10001 droit).
// Il retrouve la notification du controle puis emet l'evenement du panneau
// +0xD0 (gauche) ou +0x108 (droit) avec la notification. Les ecouteurs de
// ces evenements (pas de 0x10 = {objet, fonction}, compte en +8, comme
// l'evenement des ressources) decident de l'ouverture du detail : on les
// liste pour les decompiler.
static AjouterPopup_t oClicPopup = nullptr;

static void ListerEcouteurs(const char* libelle, uintptr_t evt)
{
    uintptr_t tab = 0;
    int32_t n = 0;
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    SafeRead((void*)evt, &tab, 8);
    SafeRead((void*)(evt + 8), &n, 4);
    printf("[POPUP]   %s : %d ecouteur(s)\n", libelle, n);
    for (int i = 0; i < n && i < 16 && tab; ++i) {
        uintptr_t ent[2] = {};
        char cls[96] = "?";
        SafeRead((void*)(tab + i * 0x10), ent, 16);
        if (ent[0] > 0x10000) GetClassNameFP(ent[0], cls, sizeof(cls));
        printf("[POPUP]     [%d] objet 0x%llX (%s) fonction Ghidra 0x%llX\n", i, (unsigned long long)ent[0], cls,
            (unsigned long long)(ent[1] - base + 0x140000000ULL));
    }
}

static void hkClicPopup(uintptr_t panneau, uintptr_t evt, uintptr_t p3, uintptr_t p4)
{
    int32_t bouton = 0;
    SafeRead((void*)(evt + 0x24), &bouton, 4);
    printf("[POPUP] clic (bouton 0x%X) sur une popup\n", bouton);
    ListerEcouteurs("evenement clic gauche (panneau +0xD0)", panneau + 0xD0);
    ListerEcouteurs("evenement clic droit (panneau +0x108)", panneau + 0x108);
    oClicPopup(panneau, evt, p3, p4);
}

void InstallTracePopups(uintptr_t modBase)
{
    void* c = (void*)(modBase + 0x1AAB820);
    bool ok = MH_CreateHook(c, (void*)&hkAjouterPopup, (void**)&oAjouterPopup) == MH_OK && MH_EnableHook(c) == MH_OK;
    printf("[POPUP] hook ajout au panneau des notifications (+1AAB820) %s\n", ok ? "OK" : "KO");
    void* k = (void*)(modBase + 0x1AAD120);
    bool okK = MH_CreateHook(k, (void*)&hkClicPopup, (void**)&oClicPopup) == MH_OK && MH_EnableHook(k) == MH_OK;
    printf("[POPUP] hook clic sur une popup (+1AAD120) %s\n", okK ? "OK" : "KO");
    void* t = (void*)(modBase + 0x10260C0);
    bool okT = MH_CreateHook(t, (void*)&hkPoserTuileEnfant, (void**)&oPoserTuileEnfant) == MH_OK && MH_EnableHook(t) == MH_OK;
    printf("[POPUP] hook pose de tuile sur un enfant (+10260C0) %s\n", okT ? "OK" : "KO");
}

static bool EstNotreEntree(uintptr_t e)
{
    for (int i = 0; i < g_nEntreesAP; ++i) if (e && g_entreesAP[i].entree == e) return true;
    return false;
}

// Icone PROPRE au message (comme le message du jeu TechnologyResearched :
// message +0x20 = 1, +0x28 = UITextureTile de l'atlas de l'arbre). Releve du
// 2026-10-28 : l'icone de l'entree (+0x78) n'est pas celle du cercle (toutes
// les entrees du jeu ont la case (1,9), et nos messages montraient la loupe
// par defaut) ; les messages a icone portent soit une icone propre, soit un
// contexte (entite, ex. SiteExplored). On recopie la tuile de notre entree
// (texture "UI/Tiles/Icons.dds" + case + nombre de cases) dans le message.
// La texture est une chaine du moteur a comptage de references : copiee par
// la fonction de copie du moteur (FUN_140e4e990(destination, source), celle
// de FUN_141687ac0), jamais par simple ecriture du pointeur (regle n. 2).
constexpr int MSG_ICONE_PROPRE = 0x20;   // octet : icone propre presente
constexpr int MSG_ICONE        = 0x28;   // UITextureTile (0x18)
typedef void(*CopierNomTexture_t)(uintptr_t dest, uintptr_t src);

// Tuile d'icone d'une techno de l'arbre : techno +0x78 (texture
// "UI/Tiles/tech_tree_atlas.dds", atlas #1179 ou nos icones occupent la
// colonne 0), +0x80 case, +0x88 nombre de cases (20 x 20). Les miniatures de
// batiment (case (0,2) sur une autre grille) sont ignorees : on prend une
// techno dont le nombre de cases est (20,20). 0 si aucune.
static uintptr_t TuileTechnoArbre()
{
    if (!EnsureTechSelf()) return 0;
    uintptr_t base = 0;
    uint32_t cnt = 0;
    SafeRead((void*)(g_selfTech + 0x2248), &base, 8);
    SafeRead((void*)(g_selfTech + 0x2250), &cnt, 4);
    if (base <= 0x10000 || cnt == 0 || cnt > 4096) return 0;
    for (uint32_t k = 0; k < cnt; ++k) {
        uintptr_t te = 0, tex = 0;
        int32_t v[4] = {};
        if (!SafeRead((void*)(base + (uintptr_t)k * 0x10), &te, 8) || te <= 0x10000) continue;
        if (!SafeRead((void*)(te + 0x78), &tex, 8) || tex <= 0x10000 ||
            !SafeRead((void*)(te + 0x80), v, sizeof(v))) continue;
        if (v[2] == 20 && v[3] == 20) return te + 0x78;
    }
    return 0;
}

// Essai 2 (2026-10-28) : avec la texture "UI/Tiles/Icons.dds", le cercle
// garde la loupe. On reproduit exactement TechnologyResearched : texture de
// l'atlas de l'arbre, case (techCol, techRow) de l'icone dans
// ARCHIPELAGO_ICONS_TECH.
static void PoserIconeMessage(uintptr_t message, const char* icone)
{
    const TechIcons* ic = nullptr;
    for (const TechIcons& t : ARCHIPELAGO_ICONS_TECH) if (!strcmp(t.nom, icone)) ic = &t;
    uintptr_t source = TuileTechnoArbre();
    uintptr_t deja = 0;
    if (!ic || !source || !SafeRead((void*)(message + MSG_ICONE), &deja, 8) || deja) {
        printf("[MSG] icone propre non posee (icone %s, tuile de techno 0x%llX, deja 0x%llX)\n",
            ic ? "connue" : "inconnue", (unsigned long long)source, (unsigned long long)deja);
        return;
    }
    int32_t caseEtNombre[4] = { ic->techCol, ic->techRow, 0, 0 };
    SafeRead((void*)(source + 0x10), &caseEtNombre[2], 8);
    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    CopierNomTexture_t copier = (CopierNomTexture_t)(base + 0xE4E990);
    __try {
        copier(message + MSG_ICONE, source);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[MSG] EXCEPTION 0x%08lX a la copie de la texture de l'icone\n", GetExceptionCode());
        return;
    }
    SafeWrite((void*)(message + MSG_ICONE + 8), caseEtNombre, 16);
    uint8_t un = 1;
    SafeWrite((void*)(message + MSG_ICONE_PROPRE), &un, 1);
    DecrireTuileMsg("icone propre posee sur le message", message + MSG_ICONE);
}

static uintptr_t EntreeDeNotre(const char* icone)
{
    if (g_entreeRefCopiee != g_msgEntreeRef) { g_nEntreesAP = 0; g_entreeRefCopiee = g_msgEntreeRef; }
    for (int i = 0; i < g_nEntreesAP; ++i) if (!strcmp(g_entreesAP[i].icone, icone)) return g_entreesAP[i].entree;
    if (!MoteurAlloc || !g_msgEntreeRef || g_nEntreesAP >= MAX_ENTREES_AP) return g_msgEntreeRef;

    uintptr_t e = (uintptr_t)MoteurAlloc(ENT_TAILLE);
    if (!e || !SafeRead((void*)g_msgEntreeRef, (void*)e, ENT_TAILLE)) return g_msgEntreeRef;

    int32_t type = 0;
    uint8_t imp = 0, cat[16] = {};
    SafeRead((void*)(g_msgEntreeRef + ENT_TYPE), &type, 4);
    SafeRead((void*)(g_msgEntreeRef + ENT_IMPORTANTE), &imp, 1);
    SafeRead((void*)(g_msgEntreeRef + ENT_CATEGORIE), cat, 16);
    printf("[MSG] entree d'origine 0x%llX : type %d, importante %d, categorie", (unsigned long long)g_msgEntreeRef, type, imp);
    for (int i = 0; i < 16; ++i) printf(" %02X", cat[i]);
    printf("\n");
    DecrireTuileMsg("image d'origine (+0x60)", g_msgEntreeRef + ENT_IMAGE);
    DecrireTuileMsg("icone d'origine (+0x78)", g_msgEntreeRef + ENT_ICONE);

    uint8_t zero[16] = {}, nonImportante = 0;
    SafeWrite((void*)(e + ENT_IMPORTANTE), &nonImportante, 1);
    SafeWrite((void*)(e + ENT_CATEGORIE), zero, 16);
    // Type 0 (Notification) au lieu de 1 (Openable notification) : capture de
    // l'auteur du 2026-10-28, nos popups (FrostpunkModalMessageNotification,
    // "ouvrables") montrent une LOUPE (case (12,4)) et un anneau propres a la
    // classe, l'icone de l'entree etant ignoree ; les notifications simples
    // (FrostpunkNotification, ex. OutOfCoal) affichent l'icone de leur entree.
    // Le type reste 1 (ouvrable) : exigence de l'auteur, le detail doit
    // s'ouvrir au clic. Essai A/B du 2026-10-28 (vanilla en type 0) : la
    // loupe reste aussi en notification simple, elle ne vient pas du type.
    // Test du 2026-10-28 : la texture du marqueur des bulles est
    // "UI\Tiles\Icons.dds" (barres inverses), celle de l'icone d'origine
    // "UI/Tiles/Icons.dds" : meme atlas, mais deux noms pour le moteur. On garde
    // donc la texture et le nombre de cases de l'entree d'origine et on ne
    // change que la case (celle de l'icone dans l'atlas des bulles).
    uint8_t tuile[0x18];
    if (TuileAtlasBulles(icone, tuile)) {
        SafeWrite((void*)(e + ENT_ICONE + 8), tuile + 8, 8);
        DecrireTuileMsg("icone de notre entree", e + ENT_ICONE);
    }
    else printf("[MSG] icone \"%s\" indisponible : icone d'origine gardee\n", icone);

    EntreeAP& n = g_entreesAP[g_nEntreesAP++];
    strncpy_s(n.icone, icone, _TRUNCATE);
    n.entree = e;
    printf("[MSG] entree AP \"%s\" = 0x%llX (copie de 0x%llX)\n", icone, (unsigned long long)e,
        (unsigned long long)g_msgEntreeRef);
    return e;
}

// A appeler UNIQUEMENT depuis le thread du jeu, hors evenement en cours.
void PompeNotifications()
{
    if (g_iconesAReposer) {
        g_iconesAReposer = false;
        ReposerIconesMessages("reprise du jeu apres chargement");
    }
    for (;;) {
        PopupEnAttente p;
        AcquireSRWLockExclusive(&g_verrouPopups);
        if (g_nbPopups == 0) { ReleaseSRWLockExclusive(&g_verrouPopups); return; }
        p = g_filePopups[g_debutPopups];
        g_debutPopups = (g_debutPopups + 1) % 16;
        g_nbPopups--;
        ReleaseSRWLockExclusive(&g_verrouPopups);
        EmettreNotificationMaintenant(p.titre, p.texte, p.icone);
    }
}

// Releve, une fois par nom, du thread qui execute un hook : sert a savoir
// quels hooks tournent sur le meme thread que le jeu (pompe possible en pause ?).
void NoterThread(const char* qui)
{
    static struct { const char* qui; DWORD tid; } vus[8];
    static int nVus = 0;
    for (int i = 0; i < nVus; ++i) if (vus[i].qui == qui) return;
    if (nVus >= 8) return;
    vus[nVus].qui = qui; vus[nVus].tid = GetCurrentThreadId(); nVus++;
    JournalImmediat("[THREAD] %-24s thread %lu\n", qui, GetCurrentThreadId());
}

static void EmettreNotificationMaintenant(const char* titre, const char* texte, const char* icone)
{
    if (!oCreerMessage || !EmettreMessage || !oPoseTitre || !oPoseTexte) {
        printf("[MSG] fabrique incomplete\n"); fflush(stdout); return;
    }
    if (!g_msgSystemeFab || !g_msgEntreeRef) {
        printf("[MSG] references absentes\n"); fflush(stdout); return;
    }

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);
    uintptr_t file = 0;
    SafeRead((void*)(base + 0x408E390), &file, 8);
    printf("[MSG] 1. file=%llX\n", (unsigned long long)file); fflush(stdout);
    if (!file) return;

    uintptr_t entree = EntreeDeNotre(icone);
    uintptr_t message = 0;
    oCreerMessage(g_msgSystemeFab, &message, entree, g_msgParamsRef);
    if (message) PoserIconeMessage(message, icone);
    printf("[MSG] 2. message=%llX\n", (unsigned long long)message); fflush(stdout);
    if (!message) return;

    uintptr_t modele = 0;
    SafeRead((void*)(g_msgEntreeRef + 0x50), &modele, 8);
    if (!ModeleCleValide(modele)) {
        uintptr_t t = message; message = 0;
        EmettreMessage(file, &t);
        printf("[MSG] emis avec les textes d'origine\n"); fflush(stdout);
        return;
    }

    // Id unique (sauvegarde AP) : suite du plus grand id du fichier.
    if (g_notifSuivante < 0) {
        int idMax = -1;
        ParcourirMessages(-1, nullptr, &idMax);
        g_notifSuivante = idMax + 1;
        printf("[MSG] ids des messages AP : a partir de %d\n", g_notifSuivante);
    }
    int id = g_notifSuivante++;
    NotifAP& n = g_notifs[id % NOTIFS_CACHE];
    memset(&n, 0, sizeof(n));
    n.id = id;
    strncpy_s(n.icone, icone ? icone : "icon_vanilla", _TRUNCATE);
    strncpy_s(n.titre, titre, _TRUNCATE);
    strncpy_s(n.texte, texte, _TRUNCATE);
    n.actif = true;
    SauverMessage(n);

    char cleT[48], cleD[48];
    _snprintf_s(cleT, sizeof(cleT), _TRUNCATE, "@AP/Notif/%d/T", id);
    _snprintf_s(cleD, sizeof(cleD), _TRUNCATE, "@AP/Notif/%d/D", id);

    // Une cle par message, allouee par le moteur et jamais liberee (CleMoteur,
    // groupes.cpp) : les anciens blocs statiques recycles (ApCle, 8 paires)
    // auraient reecrit la cle d'un message encore affiche... et sauvegarde.
    char* ct = (char*)CleMoteur(cleT, modele);
    char* cd = (char*)CleMoteur(cleD, modele);
    printf("[MSG] 3. cles pretes\n"); fflush(stdout);

    uintptr_t transfert = message;
    message = 0;

    if (ct) { uintptr_t p = (uintptr_t)ct; oPoseTitre(transfert, &p); }
    printf("[MSG] 4. titre pose\n"); fflush(stdout);
    if (cd) { uintptr_t p = (uintptr_t)cd; oPoseTexte(transfert, &p); }
    printf("[MSG] 5. texte pose\n"); fflush(stdout);

    EmettreMessage(file, &transfert);
    printf("[MSG] 6. notification %d emise\n", id); fflush(stdout);
}






