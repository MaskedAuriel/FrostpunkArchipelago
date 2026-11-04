#pragma once
// ---- temperature.h ----
#include <cstdint>
// Temperature et meteo du jeu (decompiles du 2026-11-01).
//   FrostpunkEnvironmentConfig (instance statique base + 0x406B4D0) :
//     +0x58 niveau de temperature par defaut (lien), +0x68 degres par defaut,
//     +0x6C degres par niveau, +0x90 niveaux de temperature, +0xF8 meteos
//   FrostpunkScenarioEntry +0x118 : chronologies meteo (liens)
//   FrostpunkScenariosConfig +0x1A0 : toutes les chronologies meteo
//   Systeme meteo (FrostpunkWeatherSystem) : *(base + 0x4062D48)

// F9 : demande le releve, execute par TemperaturePompe sur le thread du jeu
// (jeu hors pause). Lecture seule.
void TemperatureDemanderReleve();

// Thread du jeu (hook du setter de ressources, a cote des autres pompes) :
// releve F9, items de temperature en attente, recalcul apres chargement.
void TemperaturePompe();

// Montees de temperature Archipelago : hook du recalcul de la temperature
// globale (FUN_141812c10). Toute montee scriptee est retiree (baisses
// vanilla, decalage cumule) ; sauvegarde locale mods\ap_temperature_<GameId>.txt.
void InstallHooksTemperature(uintptr_t modBase);

// Item recu : +'niveaux' niveaux (10 degres chacun), cumulatif. Tout thread ;
// applique par TemperaturePompe (thread du jeu, hors pause).
void TemperatureRecevoirMontee(int niveaux);

// Niveaux deja recus dans la partie 'gid' (GameId), en attente compris. -1 si
// la sauvegarde locale de cette partie n'est pas relue. Thread du jeu (apres
// TemperaturePompe).
int TemperatureItemsRecus(const uint64_t gid[2]);

// Nom d'un acte de donnees (FrostpunkGameActEntry) et de son reglage, par GUID.
bool TemperatureNomActe(const uint64_t guid[2], char* out, int taille);
