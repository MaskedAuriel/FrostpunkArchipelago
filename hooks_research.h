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

// Cree et active les hooks. MH_Initialize doit avoir ete appele avant.
void InstallResearchHooks(uintptr_t modBase);
