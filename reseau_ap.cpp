// reseau_ap.cpp -- couche reseau Archipelago (docs/chantiers/reseau.md).
//
// Transport : WebSocket ecrit a la main sur Winsock (decision de l'auteur,
// 2026-11-04) : poignee de main HTTP, trames masquees cote client, ping/pong,
// fragmentation. Pas de compression par message (le serveur l'accepte,
// deprecie).
// TLS (wss://) : Schannel, natif Windows (decision de l'auteur, 2026-11-04,
// les serveurs publics comme archipelago.gg n'acceptent que wss://). Sans
// prefixe : wss:// puis ws://, comme le client officiel ; le mode qui marche
// est retenu pour les reconnexions de la meme demande.
// JSON : nlohmann/json 3.11.3 (nlohmann\json.hpp, inclus ICI SEULEMENT).
//
// Threads : tout se passe sur le thread reseau. Il n'appelle que la surface
// archipelago.h ; les effets en jeu passent par les files et les pompes du
// thread du jeu. Les appels venus du jeu (ReseauEnvoyerLocation, ...) ne font
// que noter et mettre en file, sous verrou.
//
// Persistance (dossier de session mods\ap_<graine>_<slot>\, ApCheminSession) :
//   index.txt      index du prochain item attendu (ReceivedItems)
//   checks.txt     locations cochees (une par ligne), renvoyees a la connexion
//   slot_data.json slot_data du dernier Connected (reapplique au demarrage)
// et, hors session : mods\ap_session.txt (derniere session), mods\ap_uuid.txt,
// mods\ap_datapackage\<jeu>_<checksum>.json (noms des items des autres jeux).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <algorithm>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include "nlohmann/json.hpp"
#include "archipelago.h"
#include "reseau_ap.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")

using json = nlohmann::json;

static const char* const JEU = "Frostpunk";
static const char* const PORT_DEFAUT = "38281";

// ============================================================================
//  Utilitaires
// ============================================================================

// Copie ASCII pour la console du jeu (UTF-8 non ASCII -> '?').
static std::string Ascii(const std::string& s)
{
    std::string r = s;
    for (char& c : r) if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) c = '?';
    return r;
}

// Texte affiche en jeu : nos popups elargissent chaque OCTET en caractere
// (hooks_loc.cpp, ApLocNotif) : UTF-8 -> Latin-1, '?' au-dela de U+00FF.
static std::string Latin1(const std::string& utf8)
{
    if (utf8.empty()) return utf8;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (n <= 0) return Ascii(utf8);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
    std::string r;
    r.reserve(n);
    for (wchar_t c : w) r += (c >= 0x20 && c <= 0xFF) ? (char)c : '?';
    return r;
}

// Nom de fichier sur : lettres, chiffres, '-', '_' ; le reste -> '_'.
static std::string NomSur(const std::string& s)
{
    std::string r;
    for (char c : s) r += (isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    return r.empty() ? "_" : r.substr(0, 64);
}

static std::string DossierMods()
{
    char base[MAX_PATH] = {};
    if (!GetModuleFileNameA(nullptr, base, MAX_PATH)) return "";
    for (int i = (int)strlen(base) - 1; i >= 0; --i)
        if (base[i] == '\\') { base[i + 1] = 0; break; }
    return std::string(base) + "mods\\";
}

static bool LireFichier(const std::string& chemin, std::string& out)
{
    FILE* f = nullptr;
    if (fopen_s(&f, chemin.c_str(), "rb") != 0 || !f) return false;
    out.clear();
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

static bool EcrireFichier(const std::string& chemin, const std::string& contenu)
{
    FILE* f = nullptr;
    if (fopen_s(&f, chemin.c_str(), "wb") != 0 || !f) return false;
    fwrite(contenu.data(), 1, contenu.size(), f);
    fclose(f);
    return true;
}

static std::string CheminSession(const char* fichier)
{
    char chemin[MAX_PATH];
    return ApCheminSession(chemin, sizeof(chemin), fichier) ? std::string(chemin) : std::string();
}

static std::string Base64(const uint8_t* d, size_t n)
{
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        r += T[(v >> 18) & 63];
        r += T[(v >> 12) & 63];
        r += i + 1 < n ? T[(v >> 6) & 63] : '=';
        r += i + 2 < n ? T[v & 63] : '=';
    }
    return r;
}

// ============================================================================
//  Classe reseau
// ============================================================================

// Contenu d'une location scoutee (chaines en Latin-1, texte en jeu).
struct ReseauScout {
    int64_t item;          // id de l'item (dans le jeu du destinataire)
    int     joueur;        // slot du destinataire
    int     flags;         // 1 progression, 2 useful, 4 trap
    bool    pourMoi;       // destinataire = ce slot
    char    nomItem[128];
    char    nomJoueur[64];
};

class ReseauAP {
public:
    void Demarrer();
    void Connecter(const std::string& serveur, const std::string& slot, const std::string& mdp);
    void EnvoyerLocation(int64_t id);
    void EnvoyerGoal();
    bool LocationFaite(int64_t id);

private:
    // --- Scouts (thread reseau ; transmis au jeu par ApNoterScout)
    SRWLOCK                                  m_verrouScouts = SRWLOCK_INIT;
    std::unordered_map<int64_t, ReseauScout> m_scouts;
    void TransmettreScouts(const std::vector<int64_t>& locations);
    // --- Demande de connexion (ecrite par le jeu, lue par le thread reseau)
    struct Demande { std::string serveur, slot, mdp; int generation = 0; };
    SRWLOCK  m_verrouDemande = SRWLOCK_INIT;
    Demande  m_demande;
    HANDLE   m_reveil = nullptr;   // evenement : nouvelle demande / nouvel envoi

    // --- Checks et goal (ecrits par le jeu, envoyes par le thread reseau)
    SRWLOCK                     m_verrouChecks = SRWLOCK_INIT;
    std::unordered_set<int64_t> m_checks;      // toutes les locations cochees de la session
    std::vector<int64_t>        m_aEnvoyer;    // cochees depuis le dernier envoi
    bool                        m_goalAEnvoyer = false;

    // --- Transport (thread reseau seulement)
    SOCKET      m_sock = INVALID_SOCKET;
    std::string m_tampon;      // octets recus, pas encore decoupes en trames
    std::string m_message;     // message fragmente en cours
    std::mt19937 m_alea{ std::random_device{}() };

    // --- TLS par Schannel, wss:// (thread reseau seulement)
    bool        m_tls = false;            // socket courante chiffree
    bool        m_credOk = false, m_ctxOk = false;
    CredHandle  m_cred = {};
    CtxtHandle  m_ctx = {};
    SecPkgContext_StreamSizes m_tailles = {};
    std::string m_tlsEntree;              // octets chiffres recus, pas encore dechiffres
    int         m_modeRetenu = 0;         // adresse sans prefixe : 0 inconnu, 1 wss, 2 ws

    // --- Etat du protocole (thread reseau seulement)
    std::string m_mdp, m_slotNom, m_serveur;
    std::string m_graine;
    std::string m_uuid;
    std::string m_dossier;     // dossier de session courant ("" : aucun)
    bool        m_connecte = false;   // Connected recu sur cette socket
    bool        m_refuse = false;     // ConnectionRefused : plus de reconnexion
    bool        m_dejaConnecte = false; // une connexion a abouti pour cette demande
    int         m_slot = -1, m_equipe = 0;
    int         m_indexSuivant = 0;   // prochain index de ReceivedItems attendu
    std::unordered_map<int, std::string> m_alias;   // slot -> nom du joueur
    std::unordered_map<int, std::string> m_jeuDuSlot;
    struct Noms { std::unordered_map<int64_t, std::string> items, locations; };
    std::unordered_map<std::string, Noms> m_noms;    // jeu -> noms (DataPackage)
    std::unordered_map<std::string, std::string> m_sommes; // jeu -> checksum attendu

    static DWORD WINAPI Thread(LPVOID p) { ((ReseauAP*)p)->Boucle(); return 0; }
    void Boucle();
    bool Session(const Demande& d);

    // transport
    int  Ouvrir(const std::string& hote, const std::string& port, bool tls, std::string& erreur);
    bool OuvrirTcp(const std::string& hote, const std::string& port, std::string& erreur);
    bool TlsPoignee(const std::string& hote, std::string& erreur);
    bool PoigneeWebSocket(const std::string& hote, const std::string& port, std::string& erreur);
    void Fermer();
    bool EnvoyerBrut(const char* d, size_t n);
    bool EnvoyerTout(const char* d, size_t n);
    int  RecevoirOctets(std::string& sortie);   // -1 fermee, -2 erreur, sinon octets clairs ajoutes
    int  Dechiffrer(std::string& sortie);       // -2 erreur, sinon octets clairs ajoutes
    bool EnvoyerTrame(uint8_t opcode, const std::string& donnees);
    bool Recevoir();          // false : connexion perdue ou fermee
    bool DecouperTrames();    // false : trame de fermeture

    // protocole
    void Envoyer(const json& paquets);
    void Traiter(const std::string& texte);
    void Paquet(const json& p);
    void SurRoomInfo(const json& p);
    void SurDataPackage(const json& p);
    void SurConnected(const json& p);
    void SurRefus(const json& p);
    void SurReceivedItems(const json& p);
    void SurPrintJSON(const json& p);
    void SurLocationInfo(const json& p);
    void EnvoyerEnAttente();
    void ChargerScouts();

    // session et noms
    void ChoisirSession(const std::string& dossier);
    void SauverIndex();
    void ChargerUuid();
    bool ChargerDataPackage(const std::string& jeu, const std::string& somme);
    void AppliquerSlotData(const json& sd);
    std::string NomJoueur(int slot);
    std::string NomItem(int64_t id, int slotJeu);
    std::string NomLocation(int64_t id, int slotJeu);
};

static ReseauAP g_reseau;

// ============================================================================
//  Demarrage, demandes du jeu
// ============================================================================

void ReseauAP::Demarrer()
{
    m_reveil = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) printf("[AP NET] WSAStartup KO\n");
    ChargerUuid();

    // Derniere session : ses fichiers restent la reference hors connexion
    // (items deja recus, checks a renvoyer, slot_data).
    std::string contenu, dossier;
    if (LireFichier(DossierMods() + "ap_session.txt", contenu)) {
        size_t p = contenu.find("dossier=");
        if (p != std::string::npos) {
            dossier = contenu.substr(p + 8);
            dossier = dossier.substr(0, dossier.find_first_of("\r\n"));
        }
    }
    if (!dossier.empty()) {
        printf("[AP NET] derniere session : mods\\%s\n", Ascii(dossier).c_str());
        ChoisirSession(dossier);
        std::string sd;
        if (LireFichier(CheminSession("slot_data.json"), sd)) {
            json j = json::parse(sd, nullptr, false);
            if (!j.is_discarded()) AppliquerSlotData(j);
        }
    }
    else printf("[AP NET] aucune session connue : connexion requise (bouton Connect)\n");

    CreateThread(nullptr, 0, &ReseauAP::Thread, this, 0, nullptr);
}

void ReseauAP::Connecter(const std::string& serveur, const std::string& slot, const std::string& mdp)
{
    AcquireSRWLockExclusive(&m_verrouDemande);
    m_demande.serveur = serveur;
    m_demande.slot = slot;
    m_demande.mdp = mdp;
    ++m_demande.generation;
    ReleaseSRWLockExclusive(&m_verrouDemande);
    if (m_reveil) SetEvent(m_reveil);
}

void ReseauAP::EnvoyerLocation(int64_t id)
{
    AcquireSRWLockExclusive(&m_verrouChecks);
    bool nouveau = m_checks.insert(id).second;
    if (nouveau) {
        m_aEnvoyer.push_back(id);
        FILE* f = nullptr;
        std::string chemin = CheminSession("checks.txt");
        if (!chemin.empty() && fopen_s(&f, chemin.c_str(), "a") == 0 && f) {
            fprintf(f, "%lld\n", (long long)id);
            fclose(f);
        }
        else printf("[AP NET] ecriture de checks.txt impossible\n");
    }
    ReleaseSRWLockExclusive(&m_verrouChecks);
    printf("[AP NET] location %lld %s\n", (long long)id, nouveau ? "en file d'envoi" : "deja cochee");
    if (m_reveil) SetEvent(m_reveil);
}

// Location deja cochee : envoyee dans cette session (checks.txt) ou connue
// du serveur (checked_locations de Connected / RoomUpdate).
bool ReseauAP::LocationFaite(int64_t id)
{
    AcquireSRWLockShared(&m_verrouChecks);
    bool faite = m_checks.count(id) != 0;
    ReleaseSRWLockShared(&m_verrouChecks);
    return faite;
}

void ReseauAP::EnvoyerGoal()
{
    AcquireSRWLockExclusive(&m_verrouChecks);
    m_goalAEnvoyer = true;
    ReleaseSRWLockExclusive(&m_verrouChecks);
    if (m_reveil) SetEvent(m_reveil);
}

// ============================================================================
//  Boucle du thread : connexion, perte, reconnexion
// ============================================================================

void ReseauAP::Boucle()
{
    int generationVue = 0;
    DWORD prochainEssai = 0, delai = 0;
    for (;;) {
        Demande d;
        AcquireSRWLockShared(&m_verrouDemande);
        d = m_demande;
        ReleaseSRWLockShared(&m_verrouDemande);

        if (d.generation == 0) { WaitForSingleObject(m_reveil, 1000); continue; }
        if (d.generation != generationVue) {
            generationVue = d.generation;
            m_refuse = false;
            m_dejaConnecte = false;
            m_modeRetenu = 0;
            delai = 0;
            prochainEssai = 0;
        }
        if (m_refuse || GetTickCount() < prochainEssai) { WaitForSingleObject(m_reveil, 250); continue; }

        bool aboutie = Session(d);
        if (aboutie) delai = 0;
        // Reprise : 5 s, puis 10, 20, 30 s au plus.
        delai = delai ? std::min(delai * 2, (DWORD)30000) : 5000;
        prochainEssai = GetTickCount() + delai;
        if (!m_refuse)
            printf("[AP NET] nouvel essai de connexion dans %lu s\n", (unsigned long)(delai / 1000));
    }
}

// Une connexion complete. Rend la main a sa perte (ou a une nouvelle demande).
// true si Connected a ete recu.
bool ReseauAP::Session(const Demande& d)
{
    // Adresse : [ws://|wss://]hote[:port][/...]
    std::string adr = d.serveur;
    while (!adr.empty() && isspace((unsigned char)adr.back())) adr.pop_back();
    while (!adr.empty() && isspace((unsigned char)adr.front())) adr.erase(0, 1);
    int schema = 0;   // 0 sans prefixe (wss puis ws), 1 wss, 2 ws
    if (_strnicmp(adr.c_str(), "wss://", 6) == 0) { schema = 1; adr = adr.substr(6); }
    else if (_strnicmp(adr.c_str(), "ws://", 5) == 0) { schema = 2; adr = adr.substr(5); }
    adr = adr.substr(0, adr.find('/'));
    std::string hote = adr, port = PORT_DEFAUT;
    size_t dp = adr.rfind(':');
    if (dp != std::string::npos && adr.find(']') == std::string::npos) {
        hote = adr.substr(0, dp);
        port = adr.substr(dp + 1);
    }
    if (hote.size() > 2 && hote.front() == '[' && hote.back() == ']') hote = hote.substr(1, hote.size() - 2);
    if (hote.empty()) hote = "localhost";

    m_serveur = hote + ":" + port;
    m_slotNom = d.slot;
    m_mdp = d.mdp;
    m_connecte = false;

    // Modes essayes : celui du prefixe, sinon celui qui a marche pour cette
    // demande, sinon wss:// puis ws://.
    int essais[2] = { schema, 0 };
    if (!schema) {
        if (m_modeRetenu) essais[0] = m_modeRetenu;
        else { essais[0] = 1; essais[1] = 2; }
    }
    bool ouvert = false;
    for (int mode : essais) {
        if (!mode) break;
        const char* nom = mode == 1 ? "wss" : "ws";
        printf("[AP NET] connexion a %s://%s (slot \"%s\")...\n", nom, Ascii(m_serveur).c_str(), Ascii(m_slotNom).c_str());
        std::string erreur;
        int r = Ouvrir(hote, port, mode == 1, erreur);
        if (r == 0) {
            ouvert = true;
            if (!schema) m_modeRetenu = mode;
            break;
        }
        printf("[AP NET] echec en %s:// : %s\n", nom, erreur.c_str());
        if (r == 1) break;   // DNS ou TCP : l'autre mode n'y changerait rien
    }
    if (!ouvert) {
        if (!m_dejaConnecte) {
            static int generationSignalee = 0;
            if (generationSignalee != d.generation) {
                generationSignalee = d.generation;
                std::string t = "Cannot reach " + Latin1(m_serveur) + ", retrying...";
                ApMessageReseau("Archipelago server unreachable", t.c_str());
            }
        }
        return false;
    }
    printf("[AP NET] WebSocket ouvert%s, attente de RoomInfo\n", m_tls ? " (TLS)" : "");

    // Trames arrivees avec la reponse 101 : traitees avant d'attendre la socket.
    bool aboutie = false;
    bool vivant = m_tampon.empty() || DecouperTrames();
    if (vivant && m_connecte) { aboutie = true; EnvoyerEnAttente(); }
    while (vivant) {
        // Nouvelle demande : on ferme et on repart avec elle.
        AcquireSRWLockShared(&m_verrouDemande);
        bool changee = m_demande.generation != d.generation;
        ReleaseSRWLockShared(&m_verrouDemande);
        if (changee) {
            printf("[AP NET] nouvelle demande de connexion : fermeture de la session\n");
            break;
        }
        if (m_refuse) break;

        fd_set lecture;
        FD_ZERO(&lecture);
        FD_SET(m_sock, &lecture);
        timeval attente = { 0, 100 * 1000 };
        int r = select(0, &lecture, nullptr, nullptr, &attente);
        if (r == SOCKET_ERROR) { printf("[AP NET] select : erreur %d\n", WSAGetLastError()); break; }
        if (r > 0 && !Recevoir()) break;
        if (m_connecte) { aboutie = true; EnvoyerEnAttente(); }
    }

    bool perdue = m_connecte;
    Fermer();
    if (perdue && !m_refuse) {
        printf("[AP NET] connexion perdue\n");
        ApMessageReseau("Disconnected", "Connection to the Archipelago server lost, reconnecting...");
    }
    m_connecte = false;
    return aboutie;
}

// ============================================================================
//  Transport : TCP + WebSocket (RFC 6455, cote client)
// ============================================================================

// 0 : WebSocket ouvert ; 1 : echec DNS / TCP ; 2 : echec TLS ou WebSocket.
int ReseauAP::Ouvrir(const std::string& hote, const std::string& port, bool tls, std::string& erreur)
{
    if (!OuvrirTcp(hote, port, erreur)) return 1;
    if (tls && !TlsPoignee(hote, erreur)) { Fermer(); return 2; }
    if (!PoigneeWebSocket(hote, port, erreur)) { Fermer(); return 2; }
    return 0;
}

bool ReseauAP::OuvrirTcp(const std::string& hote, const std::string& port, std::string& erreur)
{
    addrinfo indices = {}, *liste = nullptr;
    indices.ai_family = AF_UNSPEC;
    indices.ai_socktype = SOCK_STREAM;
    indices.ai_protocol = IPPROTO_TCP;
    int e = getaddrinfo(hote.c_str(), port.c_str(), &indices, &liste);
    if (e != 0 || !liste) { erreur = "adresse introuvable (getaddrinfo " + std::to_string(e) + ")"; return false; }

    // Chaque adresse a son tour (localhost : ::1 puis 127.0.0.1), 5 s chacune.
    for (addrinfo* a = liste; a && m_sock == INVALID_SOCKET; a = a->ai_next) {
        SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        u_long nonBloquant = 1;
        ioctlsocket(s, FIONBIO, &nonBloquant);
        bool ok = connect(s, a->ai_addr, (int)a->ai_addrlen) == 0;
        if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set ecriture, erreurs;
            FD_ZERO(&ecriture); FD_SET(s, &ecriture);
            FD_ZERO(&erreurs);  FD_SET(s, &erreurs);
            timeval t = { 5, 0 };
            if (select(0, nullptr, &ecriture, &erreurs, &t) > 0 && FD_ISSET(s, &ecriture)) {
                int soErr = 0, l = sizeof(soErr);
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soErr, &l);
                ok = soErr == 0;
            }
        }
        if (!ok) { closesocket(s); continue; }
        nonBloquant = 0;
        ioctlsocket(s, FIONBIO, &nonBloquant);
        BOOL sansDelai = TRUE;
        setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&sansDelai, sizeof(sansDelai));
        m_sock = s;
    }
    freeaddrinfo(liste);
    if (m_sock == INVALID_SOCKET) { erreur = "connexion TCP refusee ou expiree"; return false; }
    return true;
}

static std::string CodeSspi(SECURITY_STATUS st)
{
    char t[16];
    sprintf_s(t, "0x%08lX", (unsigned long)st);
    return t;
}

// Poignee de main TLS (Schannel). Certificat verifie par Windows
// (SCH_CRED_AUTO_CRED_VALIDATION, nom = 'hote', SNI). SCHANNEL_CRED : TLS 1.2
// au plus (TLS 1.3 demanderait SCH_CREDENTIALS) ; archipelago.gg l'accepte.
bool ReseauAP::TlsPoignee(const std::string& hote, std::string& erreur)
{
    SCHANNEL_CRED sc = {};
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    sc.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
    TimeStamp expiration;
    SECURITY_STATUS st = AcquireCredentialsHandleA(nullptr, (SEC_CHAR*)UNISP_NAME_A, SECPKG_CRED_OUTBOUND,
        nullptr, &sc, nullptr, nullptr, &m_cred, &expiration);
    if (st != SEC_E_OK) { erreur = "TLS : AcquireCredentialsHandle " + CodeSspi(st); return false; }
    m_credOk = true;

    const ULONG DRAPEAUX = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY |
        ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    std::string entree;   // octets du serveur pas encore consommes par Schannel
    DWORD limite = GetTickCount() + 10000;
    bool premier = true;
    for (;;) {
        SecBuffer bufEnt[2] = {};
        bufEnt[0].BufferType = SECBUFFER_TOKEN;
        bufEnt[0].pvBuffer = entree.empty() ? nullptr : &entree[0];
        bufEnt[0].cbBuffer = (ULONG)entree.size();
        bufEnt[1].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc descEnt = { SECBUFFER_VERSION, 2, bufEnt };
        SecBuffer bufSor[1] = {};
        bufSor[0].BufferType = SECBUFFER_TOKEN;
        SecBufferDesc descSor = { SECBUFFER_VERSION, 1, bufSor };
        ULONG attributs = 0;
        st = InitializeSecurityContextA(&m_cred, premier ? nullptr : &m_ctx, (SEC_CHAR*)hote.c_str(), DRAPEAUX,
            0, 0, premier ? nullptr : &descEnt, 0, premier ? &m_ctx : nullptr, &descSor, &attributs, &expiration);
        if (premier) {
            premier = false;
            if (st == SEC_E_OK || st == SEC_I_CONTINUE_NEEDED) m_ctxOk = true;
        }
        // Jeton a envoyer (aussi en cas d'erreur : alerte TLS).
        if (bufSor[0].pvBuffer) {
            bool ok = bufSor[0].cbBuffer == 0 || EnvoyerBrut((const char*)bufSor[0].pvBuffer, bufSor[0].cbBuffer);
            FreeContextBuffer(bufSor[0].pvBuffer);
            if (!ok) { erreur = "TLS : envoi de la poignee de main"; return false; }
        }
        if (st != SEC_E_INCOMPLETE_MESSAGE) {
            if (st != SEC_E_OK && st != SEC_I_CONTINUE_NEEDED) {
                erreur = "TLS : poignee de main refusee (" + CodeSspi(st) +
                    (st == (SECURITY_STATUS)0x80090325 ? ", certificat non reconnu" :
                     st == (SECURITY_STATUS)0x80090322 ? ", nom du certificat different" : "") + ")";
                return false;
            }
            if (bufEnt[1].BufferType == SECBUFFER_EXTRA && bufEnt[1].cbBuffer <= entree.size())
                entree = entree.substr(entree.size() - bufEnt[1].cbBuffer);
            else
                entree.clear();
            if (st == SEC_E_OK) break;
            if (!entree.empty()) continue;   // encore des octets a donner a Schannel
        }
        // Lecture de la suite.
        if (GetTickCount() > limite) { erreur = "TLS : pas de reponse a la poignee de main"; return false; }
        fd_set lecture;
        FD_ZERO(&lecture); FD_SET(m_sock, &lecture);
        timeval t = { 0, 200 * 1000 };
        if (select(0, &lecture, nullptr, nullptr, &t) <= 0) continue;
        char buf[16384];
        int n = recv(m_sock, buf, sizeof(buf), 0);
        if (n <= 0) { erreur = "TLS : connexion fermee pendant la poignee de main (serveur ws:// ?)"; return false; }
        entree.append(buf, n);
    }

    st = QueryContextAttributesA(&m_ctx, SECPKG_ATTR_STREAM_SIZES, &m_tailles);
    if (st != SEC_E_OK) { erreur = "TLS : QueryContextAttributes " + CodeSspi(st); return false; }
    SecPkgContext_ConnectionInfo info = {};
    QueryContextAttributesA(&m_ctx, SECPKG_ATTR_CONNECTION_INFO, &info);
    printf("[AP NET] TLS etabli (protocole 0x%lX)\n", (unsigned long)info.dwProtocol);
    m_tls = true;
    m_tlsEntree = entree;   // donnees applicatives deja arrivees
    return true;
}

// Poignee de main HTTP -> 101 Switching Protocols (sur TCP ou sur TLS).
bool ReseauAP::PoigneeWebSocket(const std::string& hote, const std::string& port, std::string& erreur)
{
    uint8_t cle[16];
    for (uint8_t& c : cle) c = (uint8_t)m_alea();
    std::string requete =
        "GET / HTTP/1.1\r\n"
        "Host: " + hote + ":" + port + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + Base64(cle, 16) + "\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent: Frostpunk-Archipelago\r\n\r\n";
    if (!EnvoyerTout(requete.data(), requete.size())) { erreur = "envoi de la poignee de main"; return false; }

    std::string reponse;
    if (m_tls && !m_tlsEntree.empty() && Dechiffrer(reponse) < 0) { erreur = "TLS : dechiffrement"; return false; }
    DWORD limite = GetTickCount() + 5000;
    size_t fin = std::string::npos;
    while ((fin = reponse.find("\r\n\r\n")) == std::string::npos) {
        if (GetTickCount() > limite || reponse.size() > 16384) { erreur = "pas de reponse a la poignee de main"; return false; }
        fd_set lecture;
        FD_ZERO(&lecture); FD_SET(m_sock, &lecture);
        timeval t = { 0, 200 * 1000 };
        if (select(0, &lecture, nullptr, nullptr, &t) <= 0) continue;
        int n = RecevoirOctets(reponse);
        if (n == -1) {
            erreur = m_tls ? "connexion fermee pendant la poignee de main"
                           : "connexion fermee pendant la poignee de main (serveur wss:// ?)";
            return false;
        }
        if (n < 0) { erreur = "lecture de la reponse a la poignee de main"; return false; }
    }
    std::string entete = reponse.substr(0, fin);
    std::string ligne1 = entete.substr(0, entete.find("\r\n"));
    if (ligne1.find(" 101") == std::string::npos) {
        erreur = "le serveur refuse le WebSocket : \"" + Ascii(ligne1) + "\"";
        return false;
    }
    m_tampon = reponse.substr(fin + 4);   // trames deja arrivees
    m_message.clear();
    return true;
}

void ReseauAP::Fermer()
{
    if (m_sock != INVALID_SOCKET) {
        shutdown(m_sock, SD_BOTH);
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    if (m_ctxOk) { DeleteSecurityContext(&m_ctx); m_ctxOk = false; }
    if (m_credOk) { FreeCredentialsHandle(&m_cred); m_credOk = false; }
    m_tls = false;
    m_tlsEntree.clear();
    m_tampon.clear();
    m_message.clear();
}

bool ReseauAP::EnvoyerBrut(const char* d, size_t n)
{
    while (n) {
        int e = send(m_sock, d, (int)std::min(n, (size_t)65536), 0);
        if (e <= 0) return false;
        d += e;
        n -= e;
    }
    return true;
}

// Envoi des octets clairs : chiffres en enregistrements TLS si wss://.
bool ReseauAP::EnvoyerTout(const char* d, size_t n)
{
    if (!m_tls) return EnvoyerBrut(d, n);
    std::vector<char> paquet(m_tailles.cbHeader + m_tailles.cbMaximumMessage + m_tailles.cbTrailer);
    while (n) {
        ULONG part = (ULONG)std::min(n, (size_t)m_tailles.cbMaximumMessage);
        memcpy(paquet.data() + m_tailles.cbHeader, d, part);
        SecBuffer b[4] = {};
        b[0].BufferType = SECBUFFER_STREAM_HEADER;
        b[0].pvBuffer = paquet.data();
        b[0].cbBuffer = m_tailles.cbHeader;
        b[1].BufferType = SECBUFFER_DATA;
        b[1].pvBuffer = paquet.data() + m_tailles.cbHeader;
        b[1].cbBuffer = part;
        b[2].BufferType = SECBUFFER_STREAM_TRAILER;
        b[2].pvBuffer = paquet.data() + m_tailles.cbHeader + part;
        b[2].cbBuffer = m_tailles.cbTrailer;
        b[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc = { SECBUFFER_VERSION, 4, b };
        SECURITY_STATUS st = EncryptMessage(&m_ctx, 0, &desc, 0);
        if (st != SEC_E_OK) { printf("[AP NET] TLS : EncryptMessage %s\n", CodeSspi(st).c_str()); return false; }
        if (!EnvoyerBrut(paquet.data(), (size_t)b[0].cbBuffer + b[1].cbBuffer + b[2].cbBuffer)) return false;
        d += part;
        n -= part;
    }
    return true;
}

// Une lecture de la socket (prete) ; octets clairs ajoutes a 'sortie'. En TLS,
// tous les enregistrements complets sont dechiffres (rien ne reste en attente
// cote clair : select() sur la socket suffit).
int ReseauAP::RecevoirOctets(std::string& sortie)
{
    char buf[65536];
    int n = recv(m_sock, buf, sizeof(buf), 0);
    if (n == 0) return -1;
    if (n < 0) { printf("[AP NET] recv : erreur %d\n", WSAGetLastError()); return -2; }
    if (!m_tls) { sortie.append(buf, n); return n; }
    m_tlsEntree.append(buf, n);
    return Dechiffrer(sortie);
}

int ReseauAP::Dechiffrer(std::string& sortie)
{
    int ajoutes = 0;
    while (!m_tlsEntree.empty()) {
        SecBuffer b[4] = {};
        b[0].BufferType = SECBUFFER_DATA;
        b[0].pvBuffer = &m_tlsEntree[0];
        b[0].cbBuffer = (ULONG)m_tlsEntree.size();
        b[1].BufferType = b[2].BufferType = b[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc = { SECBUFFER_VERSION, 4, b };
        SECURITY_STATUS st = DecryptMessage(&m_ctx, &desc, 0, nullptr);
        if (st == SEC_E_INCOMPLETE_MESSAGE) break;   // enregistrement pas encore complet
        if (st == SEC_I_CONTEXT_EXPIRED) { printf("[AP NET] TLS : fermeture par le serveur\n"); return -2; }
        if (st != SEC_E_OK) {
            // SEC_I_RENEGOTIATE (0x00090321) : renegociation / messages TLS 1.3,
            // non geres (TLS 1.2 avec SCHANNEL_CRED).
            printf("[AP NET] TLS : DecryptMessage %s\n", CodeSspi(st).c_str());
            return -2;
        }
        std::string reste;
        for (int i = 1; i < 4; ++i) {
            if (b[i].BufferType == SECBUFFER_DATA && b[i].cbBuffer) {
                sortie.append((const char*)b[i].pvBuffer, b[i].cbBuffer);
                ajoutes += (int)b[i].cbBuffer;
            }
            else if (b[i].BufferType == SECBUFFER_EXTRA && b[i].cbBuffer)
                reste.assign((const char*)b[i].pvBuffer, b[i].cbBuffer);
        }
        m_tlsEntree.swap(reste);   // dechiffrement en place : copies faites avant
    }
    return ajoutes;
}

// Trame client : FIN, masque obligatoire.
bool ReseauAP::EnvoyerTrame(uint8_t opcode, const std::string& donnees)
{
    if (m_sock == INVALID_SOCKET) return false;
    std::string t;
    size_t n = donnees.size();
    t += (char)(0x80 | opcode);
    if (n < 126) t += (char)(0x80 | n);
    else if (n < 65536) { t += (char)(0x80 | 126); t += (char)(n >> 8); t += (char)n; }
    else { t += (char)(0x80 | 127); for (int i = 7; i >= 0; --i) t += (char)((uint64_t)n >> (i * 8)); }
    uint8_t masque[4];
    for (uint8_t& m : masque) m = (uint8_t)m_alea();
    t.append((const char*)masque, 4);
    size_t debut = t.size();
    t += donnees;
    for (size_t i = 0; i < n; ++i) t[debut + i] ^= masque[i & 3];
    return EnvoyerTout(t.data(), t.size());
}

bool ReseauAP::Recevoir()
{
    int n = RecevoirOctets(m_tampon);
    if (n == -1) { printf("[AP NET] le serveur a ferme la connexion\n"); return false; }
    if (n < 0) return false;   // erreur deja au journal
    return DecouperTrames();
}

bool ReseauAP::DecouperTrames()
{
    for (;;) {
        if (m_tampon.size() < 2) return true;
        uint8_t b0 = (uint8_t)m_tampon[0], b1 = (uint8_t)m_tampon[1];
        bool fin = (b0 & 0x80) != 0, masque = (b1 & 0x80) != 0;
        uint8_t op = b0 & 0x0F;
        uint64_t len = b1 & 0x7F;
        size_t pos = 2;
        if (len == 126) {
            if (m_tampon.size() < 4) return true;
            len = ((uint8_t)m_tampon[2] << 8) | (uint8_t)m_tampon[3];
            pos = 4;
        }
        else if (len == 127) {
            if (m_tampon.size() < 10) return true;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | (uint8_t)m_tampon[2 + i];
            pos = 10;
        }
        uint8_t m[4] = {};
        if (masque) {
            if (m_tampon.size() < pos + 4) return true;
            memcpy(m, m_tampon.data() + pos, 4);
            pos += 4;
        }
        if (m_tampon.size() < pos + len) return true;
        std::string charge = m_tampon.substr(pos, (size_t)len);
        m_tampon.erase(0, pos + (size_t)len);
        if (masque) for (size_t i = 0; i < charge.size(); ++i) charge[i] ^= m[i & 3];

        switch (op) {
        case 0x8:   // fermeture : on repond, puis on coupe
            printf("[AP NET] trame de fermeture du serveur\n");
            EnvoyerTrame(0x8, charge.substr(0, 2));
            return false;
        case 0x9:   // ping -> pong
            EnvoyerTrame(0xA, charge);
            break;
        case 0xA:
            break;
        case 0x1: case 0x2:
            m_message = charge;
            if (fin) { Traiter(m_message); m_message.clear(); }
            break;
        case 0x0:
            m_message += charge;
            if (fin) { Traiter(m_message); m_message.clear(); }
            break;
        default:
            printf("[AP NET] trame d'opcode %u ignoree\n", op);
        }
    }
}

// ============================================================================
//  Protocole Archipelago
// ============================================================================

void ReseauAP::Envoyer(const json& paquets)
{
    std::string texte = paquets.dump();
    if (!EnvoyerTrame(0x1, texte)) printf("[AP NET] envoi impossible (%zu octets)\n", texte.size());
}

void ReseauAP::Traiter(const std::string& texte)
{
    json j = json::parse(texte, nullptr, false);
    if (j.is_discarded() || !j.is_array()) {
        printf("[AP NET] message illisible (%zu octets)\n", texte.size());
        return;
    }
    for (const json& p : j) {
        try { Paquet(p); }
        catch (const std::exception& e) {
            printf("[AP NET] paquet \"%s\" : exception %s\n",
                p.is_object() ? Ascii(p.value("cmd", std::string("?"))).c_str() : "?", Ascii(e.what()).c_str());
        }
    }
}

void ReseauAP::Paquet(const json& p)
{
    std::string cmd = p.value("cmd", std::string());
    if (cmd == "RoomInfo") SurRoomInfo(p);
    else if (cmd == "DataPackage") SurDataPackage(p);
    else if (cmd == "Connected") SurConnected(p);
    else if (cmd == "ConnectionRefused") SurRefus(p);
    else if (cmd == "ReceivedItems") SurReceivedItems(p);
    else if (cmd == "PrintJSON") SurPrintJSON(p);
    else if (cmd == "LocationInfo") SurLocationInfo(p);
    else if (cmd == "RoomUpdate") {
        if (p.contains("checked_locations")) {
            AcquireSRWLockExclusive(&m_verrouChecks);
            for (const json& j : p["checked_locations"]) m_checks.insert(j.get<int64_t>());
            ReleaseSRWLockExclusive(&m_verrouChecks);
        }
        if (p.contains("players"))
            for (const json& j : p["players"]) m_alias[j.value("slot", 0)] = j.value("alias", std::string());
    }
    else if (cmd == "InvalidPacket")
        printf("[AP NET] InvalidPacket (%s) : %s\n", Ascii(p.value("original_cmd", std::string("?"))).c_str(),
            Ascii(p.value("text", std::string())).c_str());
    else if (cmd != "Bounced")
        printf("[AP NET] paquet \"%s\" ignore\n", Ascii(cmd).c_str());
}

void ReseauAP::SurRoomInfo(const json& p)
{
    m_graine = p.value("seed_name", std::string());
    const json& v = p.contains("version") ? p["version"] : json::object();
    printf("[AP NET] RoomInfo : serveur %d.%d.%d, graine \"%s\", mot de passe %s\n",
        v.value("major", 0), v.value("minor", 0), v.value("build", 0), Ascii(m_graine).c_str(),
        p.value("password", false) ? "requis" : "non requis");

    // Noms des items / locations de chaque jeu de la salle : cache local par
    // checksum, sinon GetDataPackage (envoye AVANT Connect, traite dans l'ordre).
    json manquants = json::array();
    if (p.contains("datapackage_checksums"))
        for (auto it = p["datapackage_checksums"].begin(); it != p["datapackage_checksums"].end(); ++it) {
            std::string jeu = it.key(), somme = it.value().get<std::string>();
            m_sommes[jeu] = somme;
            if (!ChargerDataPackage(jeu, somme)) manquants.push_back(jeu);
        }

    json paquets = json::array();
    if (!manquants.empty()) {
        printf("[AP NET] DataPackage demande pour %zu jeu(x)\n", manquants.size());
        paquets.push_back({ {"cmd", "GetDataPackage"}, {"games", manquants} });
    }
    paquets.push_back({
        {"cmd", "Connect"},
        {"password", m_mdp},
        {"game", JEU},
        {"name", m_slotNom},
        {"uuid", m_uuid},
        {"version", { {"major", 0}, {"minor", 6}, {"build", 4}, {"class", "Version"} }},
        {"items_handling", 7},     // autres mondes + le notre + inventaire de depart
        {"tags", json::array({ "AP" })},
        {"slot_data", true},
    });
    Envoyer(paquets);
    printf("[AP NET] Connect envoye (slot \"%s\")\n", Ascii(m_slotNom).c_str());
}

void ReseauAP::SurDataPackage(const json& p)
{
    if (!p.contains("data") || !p["data"].contains("games")) return;
    std::string dossier = DossierMods() + "ap_datapackage\\";
    CreateDirectoryA(dossier.c_str(), nullptr);
    for (auto it = p["data"]["games"].begin(); it != p["data"]["games"].end(); ++it) {
        const std::string jeu = it.key();
        const json& g = it.value();
        Noms& n = m_noms[jeu];
        n.items.clear();
        n.locations.clear();
        if (g.contains("item_name_to_id"))
            for (auto i = g["item_name_to_id"].begin(); i != g["item_name_to_id"].end(); ++i)
                n.items[i.value().get<int64_t>()] = i.key();
        if (g.contains("location_name_to_id"))
            for (auto i = g["location_name_to_id"].begin(); i != g["location_name_to_id"].end(); ++i)
                n.locations[i.value().get<int64_t>()] = i.key();
        std::string somme = g.value("checksum", m_sommes[jeu]);
        if (!somme.empty())
            EcrireFichier(dossier + NomSur(jeu) + "_" + NomSur(somme) + ".json", g.dump());
        printf("[AP NET] DataPackage \"%s\" : %zu items, %zu locations\n", Ascii(jeu).c_str(),
            n.items.size(), n.locations.size());
    }
}

bool ReseauAP::ChargerDataPackage(const std::string& jeu, const std::string& somme)
{
    if (somme.empty()) return false;
    std::string contenu;
    if (!LireFichier(DossierMods() + "ap_datapackage\\" + NomSur(jeu) + "_" + NomSur(somme) + ".json", contenu))
        return false;
    json g = json::parse(contenu, nullptr, false);
    if (g.is_discarded()) return false;
    Noms& n = m_noms[jeu];
    n.items.clear();
    n.locations.clear();
    if (g.contains("item_name_to_id"))
        for (auto i = g["item_name_to_id"].begin(); i != g["item_name_to_id"].end(); ++i)
            n.items[i.value().get<int64_t>()] = i.key();
    if (g.contains("location_name_to_id"))
        for (auto i = g["location_name_to_id"].begin(); i != g["location_name_to_id"].end(); ++i)
            n.locations[i.value().get<int64_t>()] = i.key();
    return true;
}

void ReseauAP::SurConnected(const json& p)
{
    m_equipe = p.value("team", 0);
    m_slot = p.value("slot", -1);
    if (p.contains("players"))
        for (const json& j : p["players"]) m_alias[j.value("slot", 0)] = j.value("alias", std::string());
    if (p.contains("slot_info"))
        for (auto it = p["slot_info"].begin(); it != p["slot_info"].end(); ++it)
            m_jeuDuSlot[atoi(it.key().c_str())] = it.value().value("game", std::string());

    // Session = graine + slot : ses fichiers (items recus, checks, victoires...).
    ChoisirSession("ap_" + NomSur(m_graine) + "_" + std::to_string(m_slot));
    EcrireFichier(DossierMods() + "ap_session.txt", "dossier=" + m_dossier + "\r\n");

    size_t nCoches = p.contains("checked_locations") ? p["checked_locations"].size() : 0;
    size_t nManquants = p.contains("missing_locations") ? p["missing_locations"].size() : 0;
    printf("[AP NET] Connected : equipe %d, slot %d (\"%s\"), %zu location(s) cochee(s), %zu restante(s), index %d\n",
        m_equipe, m_slot, Ascii(NomJoueur(m_slot)).c_str(), nCoches, nManquants, m_indexSuivant);

    if (p.contains("slot_data") && p["slot_data"].is_object()) {
        EcrireFichier(CheminSession("slot_data.json"), p["slot_data"].dump());
        AppliquerSlotData(p["slot_data"]);
    }

    // Checks faits hors connexion (ou jamais confirmes) : tous ceux de la
    // session que le serveur n'a pas.
    std::unordered_set<int64_t> serveur;
    if (p.contains("checked_locations"))
        for (const json& j : p["checked_locations"]) serveur.insert(j.get<int64_t>());
    AcquireSRWLockExclusive(&m_verrouChecks);
    m_aEnvoyer.clear();
    for (int64_t id : m_checks) if (!serveur.count(id)) m_aEnvoyer.push_back(id);
    // Cochees ailleurs (autre machine, sauvegarde precedente...) : connues
    // aussi, pour l'affichage "deja fait" (ReseauLocationFaite).
    for (int64_t id : serveur) m_checks.insert(id);
    if (ApGoalAtteint()) m_goalAEnvoyer = true;
    ReleaseSRWLockExclusive(&m_verrouChecks);

    // Scouts : contenu de TOUTES les locations du slot (cochees ou non), sans
    // creer de hint ; reponse LocationInfo.
    json toutes = json::array();
    for (const char* cle : { "missing_locations", "checked_locations" })
        if (p.contains(cle))
            for (const json& j : p[cle]) toutes.push_back(j);
    if (!toutes.empty()) {
        json paquets = json::array();
        paquets.push_back({ {"cmd", "LocationScouts"}, {"locations", toutes}, {"create_as_hint", 0} });
        Envoyer(paquets);
        printf("[AP NET] LocationScouts : %zu location(s)\n", toutes.size());
    }

    m_connecte = true;
    std::string t = "Slot " + Latin1(NomJoueur(m_slot)) + " on " + Latin1(m_serveur);
    ApMessageReseau(m_dejaConnecte ? "Reconnected to Archipelago" : "Connected to Archipelago", t.c_str());
    m_dejaConnecte = true;
}

void ReseauAP::SurRefus(const json& p)
{
    std::string erreurs;
    if (p.contains("errors"))
        for (const json& e : p["errors"]) erreurs += (erreurs.empty() ? "" : ", ") + e.get<std::string>();
    if (erreurs.empty()) erreurs = "unknown reason";
    printf("[AP NET] ConnectionRefused : %s\n", Ascii(erreurs).c_str());
    ApMessageReseau("Connection refused", erreurs.c_str());
    m_refuse = true;   // pas de reconnexion : il faut une nouvelle demande
}

// ReceivedItems : 'index' = position du premier item dans la liste complete
// des items du slot. Index 0 = liste complete (connexion, Sync) : les items
// deja comptes dans la session (index.txt) sont sautes. Trou -> Sync.
void ReseauAP::SurReceivedItems(const json& p)
{
    int index = p.value("index", 0);
    const json& items = p.contains("items") ? p["items"] : json::array();
    if (index > m_indexSuivant) {
        printf("[AP NET] ReceivedItems index %d, attendu %d : Sync\n", index, m_indexSuivant);
        json paquets = json::array();
        paquets.push_back({ {"cmd", "Sync"} });
        Envoyer(paquets);
        return;
    }
    int nouveaux = 0;
    for (size_t k = 0; k < items.size(); ++k) {
        int i = index + (int)k;
        if (i < m_indexSuivant) continue;   // deja recu (session)
        const json& it = items[k];
        int64_t id = it.value("item", (int64_t)0);
        int de = it.value("player", 0);
        std::string nom = NomJoueur(de);
        printf("[AP NET] item #%d : %lld de %s\n", i, (long long)id, Ascii(nom).c_str());
        ReceiveItem(id, Latin1(nom).c_str());
        m_indexSuivant = i + 1;
        SauverIndex();
        ++nouveaux;
    }
    printf("[AP NET] ReceivedItems index %d, %zu item(s), %d nouveau(x), prochain index %d\n",
        index, items.size(), nouveaux, m_indexSuivant);
}

// PrintJSON : tout au journal ; en popup, seulement les items qui concernent
// ce slot (decision de l'auteur, 2026-11-04) : recus d'un autre joueur, ou
// trouves ici pour un autre joueur.
void ReseauAP::SurPrintJSON(const json& p)
{
    std::string texte;
    if (p.contains("data"))
        for (const json& part : p["data"]) {
            std::string type = part.value("type", std::string("text"));
            std::string t = part.value("text", std::string());
            int joueur = part.value("player", 0);
            if (type == "player_id") t = NomJoueur(atoi(t.c_str()));
            else if (type == "item_id") t = NomItem(_atoi64(t.c_str()), joueur);
            else if (type == "location_id") t = NomLocation(_atoi64(t.c_str()), joueur);
            texte += t;
        }
    std::string type = p.value("type", std::string());
    printf("[AP MSG] %s%s\n", type.empty() ? "" : ("(" + Ascii(type) + ") ").c_str(), Ascii(texte).c_str());

    // Plus de message en jeu pour ItemSend (bug du 2026-11-04 : messages en
    // DOUBLE). Item trouve ici pour un autre joueur : deja dit par "Check
    // sent" (<check> -> <joueur> - <item>, d'apres le scout). Item recu d'un
    // autre joueur : deja dit par le message de son application ("Technology
    // received ... from", convoi "Item received", loi, temperature, piege).
    // Le journal [AP MSG] ci-dessus garde tout.
}

// LocationInfo : contenu des locations scoutees. Gardees en memoire (lues
// par le jeu, ReseauInfoLocation) et dans scouts.txt de la session :
// "<location>\t<item>\t<joueur>\t<flags>\t<nom de l'item>\t<nom du joueur>"
// (UTF-8), fichier reecrit en entier.
void ReseauAP::SurLocationInfo(const json& p)
{
    if (!p.contains("locations")) return;
    int n = 0, pourMoi = 0;
    std::vector<int64_t> recues;
    AcquireSRWLockExclusive(&m_verrouScouts);
    for (const json& it : p["locations"]) {
        ReseauScout s = {};
        int64_t loc = it.value("location", (int64_t)0);
        recues.push_back(loc);
        s.item = it.value("item", (int64_t)0);
        s.joueur = it.value("player", 0);
        s.flags = it.value("flags", 0);
        s.pourMoi = s.joueur == m_slot;
        strncpy_s(s.nomItem, Latin1(NomItem(s.item, s.joueur)).c_str(), _TRUNCATE);
        strncpy_s(s.nomJoueur, Latin1(NomJoueur(s.joueur)).c_str(), _TRUNCATE);
        m_scouts[loc] = s;
        ++n;
        pourMoi += s.pourMoi ? 1 : 0;
    }
    // Sauvegarde (noms en UTF-8 relus a la source, tabulations retirees).
    std::string contenu;
    for (const auto& kv : m_scouts) {
        std::string item = NomItem(kv.second.item, kv.second.joueur), joueur = NomJoueur(kv.second.joueur);
        for (char& c : item) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        for (char& c : joueur) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
        contenu += std::to_string(kv.first) + "\t" + std::to_string(kv.second.item) + "\t" +
            std::to_string(kv.second.joueur) + "\t" + std::to_string(kv.second.flags) + "\t" +
            item + "\t" + joueur + "\n";
    }
    size_t total = m_scouts.size();
    ReleaseSRWLockExclusive(&m_verrouScouts);
    if (!EcrireFichier(CheminSession("scouts.txt"), contenu)) printf("[AP NET] ecriture de scouts.txt impossible\n");
    printf("[AP NET] LocationInfo : %d location(s) (%d item(s) pour ce slot), %zu connue(s) en tout\n",
        n, pourMoi, total);
    TransmettreScouts(recues);
}

// scouts.txt de la session (thread reseau ou demarrage).
void ReseauAP::ChargerScouts()
{
    // Slot de la session : suffixe du dossier "ap_<graine>_<slot>".
    size_t us = m_dossier.rfind('_');
    int monSlot = us == std::string::npos ? -1 : atoi(m_dossier.c_str() + us + 1);
    std::string contenu;
    AcquireSRWLockExclusive(&m_verrouScouts);
    m_scouts.clear();
    if (LireFichier(CheminSession("scouts.txt"), contenu)) {
        size_t debut = 0;
        while (debut < contenu.size()) {
            size_t fin = contenu.find('\n', debut);
            if (fin == std::string::npos) fin = contenu.size();
            std::string ligne = contenu.substr(debut, fin - debut);
            debut = fin + 1;
            std::string champs[6];
            size_t a = 0;
            for (int k = 0; k < 6; ++k) {
                size_t b = k < 5 ? ligne.find('\t', a) : std::string::npos;
                champs[k] = ligne.substr(a, b == std::string::npos ? std::string::npos : b - a);
                if (b == std::string::npos) { if (k < 5) champs[0].clear(); break; }
                a = b + 1;
            }
            int64_t loc = _atoi64(champs[0].c_str());
            if (!loc) continue;
            ReseauScout s = {};
            s.item = _atoi64(champs[1].c_str());
            s.joueur = atoi(champs[2].c_str());
            s.flags = atoi(champs[3].c_str());
            s.pourMoi = s.joueur == monSlot;
            strncpy_s(s.nomItem, Latin1(champs[4]).c_str(), _TRUNCATE);
            strncpy_s(s.nomJoueur, Latin1(champs[5]).c_str(), _TRUNCATE);
            m_scouts[loc] = s;
        }
    }
    std::vector<int64_t> toutes;
    for (const auto& kv : m_scouts) toutes.push_back(kv.first);
    ReleaseSRWLockExclusive(&m_verrouScouts);
    printf("[AP NET] %zu scout(s) relu(s) dans la session\n", toutes.size());
    TransmettreScouts(toutes);
}

// Scouts -> jeu (surface archipelago.h). Hors verrou : ApNoterScout copie
// les chaines.
void ReseauAP::TransmettreScouts(const std::vector<int64_t>& locations)
{
    std::vector<std::pair<int64_t, ReseauScout>> copie;
    AcquireSRWLockShared(&m_verrouScouts);
    for (int64_t loc : locations) {
        auto it = m_scouts.find(loc);
        if (it != m_scouts.end()) copie.push_back(*it);
    }
    ReleaseSRWLockShared(&m_verrouScouts);
    for (const auto& kv : copie)
        ApNoterScout(kv.first, kv.second.item, kv.second.nomItem, kv.second.nomJoueur,
            kv.second.flags, kv.second.pourMoi);
}

// Checks et goal en attente (thread reseau, connecte).
void ReseauAP::EnvoyerEnAttente()
{
    std::vector<int64_t> ids;
    bool goal;
    AcquireSRWLockExclusive(&m_verrouChecks);
    ids.swap(m_aEnvoyer);
    goal = m_goalAEnvoyer;
    m_goalAEnvoyer = false;
    ReleaseSRWLockExclusive(&m_verrouChecks);
    if (ids.empty() && !goal) return;

    json paquets = json::array();
    if (!ids.empty()) paquets.push_back({ {"cmd", "LocationChecks"}, {"locations", ids} });
    if (goal) paquets.push_back({ {"cmd", "StatusUpdate"}, {"status", 30} });
    Envoyer(paquets);
    printf("[AP NET] envoye : %zu location(s)%s\n", ids.size(), goal ? " + goal (StatusUpdate 30)" : "");
}

// ============================================================================
//  Session, slot_data, noms
// ============================================================================

void ReseauAP::ChoisirSession(const std::string& dossier)
{
    if (dossier == m_dossier) return;
    m_dossier = dossier;
    ApDefinirSession(dossier.c_str());   // fichiers des items, victoires... ; deblocages rejoues

    std::string contenu;
    m_indexSuivant = LireFichier(CheminSession("index.txt"), contenu) ? atoi(contenu.c_str()) : 0;

    AcquireSRWLockExclusive(&m_verrouChecks);
    m_checks.clear();
    m_aEnvoyer.clear();
    if (LireFichier(CheminSession("checks.txt"), contenu)) {
        size_t debut = 0;
        while (debut < contenu.size()) {
            size_t fin = contenu.find('\n', debut);
            if (fin == std::string::npos) fin = contenu.size();
            int64_t id = _atoi64(contenu.substr(debut, fin - debut).c_str());
            if (id) m_checks.insert(id);
            debut = fin + 1;
        }
    }
    size_t n = m_checks.size();
    ReleaseSRWLockExclusive(&m_verrouChecks);
    printf("[AP NET] session mods\\%s : prochain item %d, %zu location(s) cochee(s)\n",
        Ascii(dossier).c_str(), m_indexSuivant, n);
    ChargerScouts();
}

void ReseauAP::SauverIndex()
{
    if (!EcrireFichier(CheminSession("index.txt"), std::to_string(m_indexSuivant) + "\r\n"))
        printf("[AP NET] ecriture de index.txt impossible\n");
}

void ReseauAP::ChargerUuid()
{
    std::string chemin = DossierMods() + "ap_uuid.txt", contenu;
    if (LireFichier(chemin, contenu)) {
        m_uuid = contenu.substr(0, contenu.find_first_of("\r\n"));
        if (!m_uuid.empty()) return;
    }
    char hex[40];
    sprintf_s(hex, "%08x%08x%08x%08x", (unsigned)m_alea(), (unsigned)m_alea(), (unsigned)m_alea(), (unsigned)m_alea());
    m_uuid = hex;
    EcrireFichier(chemin, m_uuid + "\r\n");
}

// slot_data -> ApSlotData (chaines gardees en statique : la structure ne
// porte que des pointeurs).
void ReseauAP::AppliquerSlotData(const json& sd)
{
    static std::string scenarios[6], depart, difficulte;
    ApSlotData d = {};
    if (sd.contains("scenarios") && sd["scenarios"].is_array())
        for (const json& s : sd["scenarios"]) {
            if (d.nScenarios >= 6 || !s.is_string()) continue;
            scenarios[d.nScenarios] = s.get<std::string>();
            d.scenarios[d.nScenarios] = scenarios[d.nScenarios].c_str();
            ++d.nScenarios;
        }
    depart = sd.value("starting_scenario", std::string());
    difficulte = sd.value("goal_difficulty", std::string());
    d.startingScenario = depart.c_str();
    d.goalDifficulty = difficulte.c_str();
    d.excludePurposePolitics = sd.value("exclude_purpose_politics", false);
    d.deliveryHours = sd.value("delivery_hours", 24);
    printf("[AP NET] slot_data : %d scenario(s), depart \"%s\", difficulte \"%s\", livraison %d h\n",
        d.nScenarios, Ascii(depart).c_str(), Ascii(difficulte).c_str(), d.deliveryHours);
    ApAppliquerSlotData(d);
}

std::string ReseauAP::NomJoueur(int slot)
{
    if (slot == 0) return "Archipelago";
    auto it = m_alias.find(slot);
    return it != m_alias.end() && !it->second.empty() ? it->second : "Player " + std::to_string(slot);
}

std::string ReseauAP::NomItem(int64_t id, int slotJeu)
{
    auto j = m_jeuDuSlot.find(slotJeu);
    if (j != m_jeuDuSlot.end()) {
        auto n = m_noms.find(j->second);
        if (n != m_noms.end()) {
            auto i = n->second.items.find(id);
            if (i != n->second.items.end()) return i->second;
        }
    }
    return "Item " + std::to_string(id);
}

std::string ReseauAP::NomLocation(int64_t id, int slotJeu)
{
    auto j = m_jeuDuSlot.find(slotJeu);
    if (j != m_jeuDuSlot.end()) {
        auto n = m_noms.find(j->second);
        if (n != m_noms.end()) {
            auto i = n->second.locations.find(id);
            if (i != n->second.locations.end()) return i->second;
        }
    }
    return "Location " + std::to_string(id);
}

// ============================================================================
//  Fonctions exportees
// ============================================================================

void ReseauDemarrer() { g_reseau.Demarrer(); }

void ReseauConnecter(const char* serveur, const char* slot, const char* motDePasse)
{
    g_reseau.Connecter(serveur ? serveur : "", slot ? slot : "", motDePasse ? motDePasse : "");
}

void ReseauEnvoyerLocation(int64_t locationId) { g_reseau.EnvoyerLocation(locationId); }

void ReseauEnvoyerGoal() { g_reseau.EnvoyerGoal(); }

bool ReseauLocationFaite(int64_t locationId) { return g_reseau.LocationFaite(locationId); }