// hooks_loc.h
// ---------------------------------------------------------------------------
// Hook de localisation (0xFEB340) : ajoute " [Researched]" au nom affiche de
// toute techno dont researched == true.
//
// Mecanisme : traduire() renvoie un pointeur vers une STRUCTURE dont le premier
// champ pointe vers les caracteres UTF-16. On ne reecrit PAS la table (le
// buffer d'origine deborderait) : on redirige ce pointeur vers un buffer a nous.
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"

void InstallLocHooks(uintptr_t modBase);
void OpenKeyLogWindow(int ms);
void DumpWide(const char* key, uintptr_t ret);

wchar_t* ApDescBuffer(const char* seg, uint64_t hash);
bool EndsWith(const char* s, const char* suf);
void OpenKeyLogWindow(int ms);
static void LogKey(const char* key);
void ResetSeenKeys();
void BaselineKeys(int ms);
static bool SeenAdd(const char* k);
extern volatile bool g_apTexteActif;
void DumpClesDilemme(const char* k, uintptr_t ret);
wchar_t* ApChaine(const wchar_t* texte, uintptr_t modele, int slot);

bool TexteDescriptionSite(const char* k, uintptr_t ret);
static void DumpClesLandmark(const char* k, uintptr_t ret);