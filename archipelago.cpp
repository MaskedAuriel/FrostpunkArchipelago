// archipelago.cpp -- surface de communication Archipelago <-> jeu
#include "archipelago.h"
#include "mod_common.h"
#include "tech_brain.h"
#include "resources.h"
#include "ap_data.h"
#include "MessageManager.h"
#include "lois_ap.h"
#include "temperature.h"

// ============================================================================
//  Serveur -> jeu
// ============================================================================

bool ReceiveProgressive(int tree, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] progressive arbre %d de %s : partie pas prete, ignore\n",
            tree, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : la techno suivante de l'arbre passe a "recue".
    const char* tech = nullptr;
    if (!OnProgressiveReceived(tree, &tech)) {
        printf("[AP RECU] progressive arbre %d de %s : arbre epuise ou hors bornes\n",
            tree, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%s (from %s)", tech ? tech : "?", fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Technology received", texte);

    printf("[AP RECU] progressive arbre %d -> %s (de %s)\n",
        tree, tech ? tech : "?", fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveResource(const char* resource, int amount, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] %d %s de %s : partie pas prete, ignore\n",
            amount, resource, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : credit mis en attente, applique par le hook du setter.
    if (!Res_Add(resource, amount)) {
        printf("[AP RECU] %d %s de %s : credit refuse\n",
            amount, resource, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%+d %s (from %s)", amount, resource, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Resource received", texte);

    printf("[AP RECU] %+d %s (de %s)\n", amount, resource, fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveLawProgressive(const char* tree, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] loi, arbre %s de %s : partie pas prete, ignore\n",
            tree ? tree : "?", fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : l'arbre compte un item de plus ; la loi signee a ce rang
    //    recoit son effet (applique par LoisAPPompe, thread du jeu).
    int rang = 0;
    if (!LoisAPRecevoir(tree, &rang)) {
        printf("[AP RECU] loi, arbre %s de %s : arbre inconnu\n",
            tree ? tree : "?", fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 2. Popup.
    char texte[200];
    sprintf_s(texte, "%s #%d (from %s)", tree, rang, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Law received", texte);

    printf("[AP RECU] loi : arbre %s, item %d (de %s)\n", tree, rang, fromPlayer ? fromPlayer : "?");
    return true;
}

bool ReceiveTemperatureRise(int levels, const char* fromPlayer)
{
    if (!PartiePrete()) {
        printf("[AP RECU] montee de temperature (%+d) de %s : partie pas prete, ignore\n",
            levels, fromPlayer ? fromPlayer : "?");
        return false;
    }

    // 1. Effet : decalage Archipelago augmente ; recalcul par TemperaturePompe.
    TemperatureRecevoirMontee(levels);

    // 2. Popup.
    char texte[160];
    sprintf_s(texte, "%+d degrees (from %s)", levels * 10, fromPlayer ? fromPlayer : "?");
    AfficherNotificationAP("Temperature rise", texte);

    printf("[AP RECU] montee de temperature : %+d niveau(x) (de %s)\n", levels, fromPlayer ? fromPlayer : "?");
    return true;
}

// ============================================================================
//  Jeu -> serveur
// ============================================================================

void SendResearchCheck(uint64_t techHash)
{
    // 1. Ce que contient cette location (pour l'instant : savoir local).
    char texte[160];
    AP_LocationText(techHash, texte, sizeof(texte));

    // 2. Envoi.  TODO reseau : transmettre le check au serveur.
    printf("[AP CHECK] recherche %016llX -> %s\n", (unsigned long long)techHash, texte);

    // 3. Popup.
    AfficherNotificationAP("Check sent", texte);
}

void SendLocationCheck(int locationId)
{
    const ApCheck* ac = ApCheckById(locationId);
    const char* texte = ac ? ac->description : "(unknown content)";

    // 1. Envoi.  TODO reseau : transmettre le check au serveur.
    printf("[AP CHECK] location %d -> %s\n", locationId, texte);

    // 2. Popup.
    AfficherNotificationAP("Check sent", texte);

    // TEMPORAIRE : simule la reponse du serveur quand l'item est pour nous.
    if (ac && ac->arbreTest >= 0)
        ReceiveProgressive(ac->arbreTest, ac->joueur);
}

void SendLawCheck(const char* tree, int rank, const char* lawName, bool effectPending)
{
    // 1. Envoi.  TODO reseau : transmettre le check au serveur.
    printf("[AP CHECK] loi \"%s\" -> arbre %s, rang %d%s\n", lawName, tree, rank,
        effectPending ? " (effet en attente)" : "");

    // 2. Popup.
    char texte[200];
    sprintf_s(texte, "Law: %s (%s #%d)%s", lawName, tree, rank,
        effectPending ? " - effect pending" : "");
    AfficherNotificationAP("Check sent", texte);
}

void SendQuestCheck(const char* questName, const char* displayName)
{
    // 1. Envoi.  TODO reseau : transmettre le check au serveur.
    printf("[AP CHECK] quete \"%s\" (\"%s\") accomplie\n", questName, displayName);

    // 2. Popup.
    char texte[200];
    sprintf_s(texte, "Quest: %s", displayName && displayName[0] && displayName[0] != '(' ? displayName : questName);
    AfficherNotificationAP("Check sent", texte);
}

// Copie ASCII pour la console du jeu (UTF-8 non ASCII -> '?').
static void AsciiPourJournal(const char* s, char* out, int maxlen)
{
    int i = 0;
    for (; s && s[i] && i < maxlen - 1; ++i)
        out[i] = (s[i] >= 0x20 && s[i] < 0x7F) ? s[i] : '?';
    out[i] = 0;
}

void RequestConnection(const char* server, const char* slot, const char* password)
{
    // 1. Envoi.  TODO reseau : la classe reseau ouvrira la connexion ici.
    char s[256], j[256];
    AsciiPourJournal(server, s, sizeof(s));
    AsciiPourJournal(slot, j, sizeof(j));
    printf("[AP CONNEXION] demande : serveur \"%s\", slot \"%s\", mot de passe %s (%d octets)\n",
        s, j, (password && password[0]) ? "fourni" : "vide", password ? (int)strlen(password) : 0);
}
