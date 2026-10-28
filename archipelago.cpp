// archipelago.cpp -- surface de communication Archipelago <-> jeu
#include "archipelago.h"
#include "mod_common.h"
#include "tech_brain.h"
#include "resources.h"
#include "ap_data.h"
#include "MessageManager.h"

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
