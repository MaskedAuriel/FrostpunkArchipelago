// hooks_research.h
// ---------------------------------------------------------------------------
// Les quatre hooks du systeme de recherche :
//   CanResearch      (0x1326A60) : autorise / refuse le clic
//   ResearchStart    (0x1329610) : demarrage
//   ResearchComplete (0x1327120) : completion -> location check
//   CancelResearch   (0x1326C80) : annulation
// ---------------------------------------------------------------------------
#pragma once
#include "mod_common.h"
#include "IconSubManager.h"

// Cree et active les hooks. MH_Initialize doit avoir ete appele avant.
void InstallResearchHooks(uintptr_t modBase);

// Techno recue : effets du jeu appliques comme a une recherche terminee
// (AddResearchedTechnology : etat 2, effets, ecouteurs). THREAD DU JEU.
// true si la techno est a l'etat 2 apres l'appel (ou l'etait deja).
bool TechAppliquerEffets(uint64_t hash);
