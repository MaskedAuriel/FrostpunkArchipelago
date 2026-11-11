#pragma once
#include <cstdint>
#include "mod_common.h"
#include "MinHook.h"
#include "hooks_loc.h"


void InstallNotifHooks(uintptr_t modBase);
// Met la popup en file (n'importe quel thread). Elle est emise par
// PompeNotifications, depuis le thread du jeu.
// Popup a gauche de l'ecran. 'icone' : nom dans ARCHIPELAGO_ICONS_TECH (case de
// l'atlas des bulles) ; la popup n'est pas "importante" (pas d'orange).
void AfficherNotificationAP(const char* titre, const char* texte, const char* icone = "icon_vanilla");
void PompeNotifications();          // thread du jeu uniquement, hors evenement
void InstallTracePopups(uintptr_t modBase);   // [POPUP] popups ajoutees au panneau
void CouleursMessagesTick();   // boucle du mod : maintient la couleur de nos messages (2 s apres l'ajout)
void MessagesAvantChargement(); // thread du jeu, entree de LoadGame : controles de la partie precedente oublies
void MessagesApresChargement(); // thread du jeu, sortie de LoadGame : icones des messages AP a reposer
void NoterThread(const char* qui);   // diagnostic : thread d'un hook (une fois par nom)
void InitFabriqueMessage(uintptr_t modBase);
void MessageSetReferences(uintptr_t* systemes, int nSys, uintptr_t* entrees, int nEnt);
const char* ApNotifTexte(int id, bool titre);
uintptr_t MessageEntreeRef();   // entree "CitizensDied" (0 si pas encore resolue)
void InstallCaptureEntrees(uintptr_t modBase);   // hook du ctor +0x19E68B0
int  EntreesMessageCaptures(uintptr_t* out, int maxOut);   // entrees vivantes
void RapportCapturesEntrees();