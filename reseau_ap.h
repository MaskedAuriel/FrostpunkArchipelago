// reseau_ap.h
// ---------------------------------------------------------------------------
// Couche reseau Archipelago (docs/chantiers/reseau.md) : la "classe specifique
// pour toutes les interactions avec le serveur". WebSocket ecrit a la main sur
// Winsock, TLS par Schannel pour wss://, JSON par nlohmann/json, thread dedie.
//
// Elle ne touche JAMAIS un systeme du jeu : elle n'appelle que la surface
// archipelago.h (ReceiveItem, ApAppliquerSlotData, ApDefinirSession...), dont
// les effets passent par les files et les pompes du thread du jeu.
//
// Seul archipelago.cpp (et le demarrage, version.cpp) appelle ces fonctions.
// ---------------------------------------------------------------------------
#pragma once
#include <cstdint>

// Demarrage (PayloadThread) : relit la derniere session (mods\ap_session.txt :
// dossier de la graine et du slot), y reapplique le slot_data et les items deja
// recus (deblocages de scenarios), puis lance le thread reseau. Aucune
// connexion tant que le joueur n'a pas clique sur Connect.
void ReseauDemarrer();

// Bouton Connect : (re)connexion a 'serveur' ("hote:port", "ws://hote:port",
// "wss://hote:port" ; sans prefixe : wss:// puis ws:// ; port 38281 par
// defaut). Remplace une connexion en cours. Tout thread.
void ReseauConnecter(const char* serveur, const char* slot, const char* motDePasse);

// Location cochee en jeu : notee dans la session (checks.txt), envoyee tout de
// suite si connecte, sinon a la (re)connexion. Sans doublon. Tout thread.
void ReseauEnvoyerLocation(int64_t locationId);

// Goal atteint : StatusUpdate CLIENT_GOAL (30), renvoye a chaque connexion
// tant que ApGoalAtteint() est vrai. Tout thread.
void ReseauEnvoyerGoal();

// Scouts : contenu des locations du slot (LocationScouts / LocationInfo a
// chaque connexion, garde dans scouts.txt de la session). Transmis au jeu
// par ApNoterScout (archipelago.h) : a la relecture de la session puis a
// chaque LocationInfo. Pas d'acces direct depuis le jeu.
