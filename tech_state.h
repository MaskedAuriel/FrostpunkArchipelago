// tech_state.h
// ---------------------------------------------------------------------------
// Acces au tableau des technos du jeu (g_selfTech + 0x2248) : lire et ecrire
// l'etat d'une techno, la retrouver par hash ou par segment de cle de
// localisation, parcourir toutes les technos.
//
// Etats du jeu :  0 = non recherchee/pas d'effet  |  1 = en cours  |  2 = effet applique
// (l'etat 3 n'est plus utilise : la verite logique vit dans tech_runtime)
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"
#include "tech_runtime.h"

// --- Acces par HASH ---------------------------------------------------------
int32_t   TechStateByHash(uint64_t hash);                 // -1 si introuvable
bool      WriteTechStateByHash(uint64_t hash, int32_t state);
uintptr_t FindTechPtrByHash(uint64_t hash);               // 0 si introuvable

// --- Acces par POINTEUR d'objet techno (utile dans les hooks) ---------------
int32_t   TechStateByPtr(uintptr_t self, uintptr_t techPtr);
bool      WriteTechStateByPtr(uintptr_t self, uintptr_t techPtr, int32_t state);
bool      IsInProgress(uintptr_t self, uintptr_t techPtr); // techno == recherche active
uint64_t  HashOfTechPtr(uintptr_t techPtr);                // 0 si illisible

void     InternalToSeg(const char* internal, char* out, int maxlen);// "H 1_X" -> "X"
// --- Parcours ---------------------------------------------------------------
int  ForEachTech(void (*fn)(uint64_t hash));   // renvoie le nombre visite
void DumpAllStates();                          // diagnostic : repartition des etats
void DumpTechNames();
static bool ReadAscii(uintptr_t addr, char* out, int maxlen);
void DumpTechStrings(const char* motif);
bool TechLocKey(uintptr_t techPtr, char* out, int maxlen);
bool LookupLocKey(const char* localKey, uint64_t* outHash, int* outSlot);
bool TechLocKeyAt(uintptr_t techPtr, int off, char* out, int maxlen);
static uint32_t Fnv32(const char* s);
static void BuildLocKeyTable();
bool StrEq(const char* a, const char* b);
void DimensionsAtlasTechnos();