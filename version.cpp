// version.cpp  --  Proxy DLL pour version.dll  (point d'entree du mod)
// ---------------------------------------------------------------------------
// Compile en une "version.dll" a poser A COTE de Frostpunk.exe. Quand le jeu la
// charge, elle :
//   1. renvoie tous les appels legitimes vers la VRAIE version.dll (System32) ;
//   2. lance PayloadThread, qui installe les hooks du mod.
//
// Ce fichier ne contient plus QUE le squelette DLL et le cablage. Toute la
// logique vit dans les modules (mod_common, tech_state, tech_runtime,
// tech_brain, hooks_research, hooks_loc, resources).
// ---------------------------------------------------------------------------

#include "mod_common.h"
#include "tech_state.h"
#include "tech_runtime.h"
#include "tech_brain.h"
#include "hooks_research.h"
#include "hooks_loc.h"
#include "resources.h"
#include "MinHook.h"
#include "known_techs.h"
#include "scenario.h"
#include "scooting.h"
#include "ap_data.h"
#include "ap_ressources.h"

#pragma comment(lib, "user32.lib")

// --- Renvoi des 17 exports de version.dll vers la vraie DLL systeme ---------
#pragma comment(linker, "/export:GetFileVersionInfoA=C:\\Windows\\System32\\version.GetFileVersionInfoA")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=C:\\Windows\\System32\\version.GetFileVersionInfoByHandle")
#pragma comment(linker, "/export:GetFileVersionInfoExA=C:\\Windows\\System32\\version.GetFileVersionInfoExA")
#pragma comment(linker, "/export:GetFileVersionInfoExW=C:\\Windows\\System32\\version.GetFileVersionInfoExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoW=C:\\Windows\\System32\\version.GetFileVersionInfoW")
#pragma comment(linker, "/export:VerFindFileA=C:\\Windows\\System32\\version.VerFindFileA")
#pragma comment(linker, "/export:VerFindFileW=C:\\Windows\\System32\\version.VerFindFileW")
#pragma comment(linker, "/export:VerInstallFileA=C:\\Windows\\System32\\version.VerInstallFileA")
#pragma comment(linker, "/export:VerInstallFileW=C:\\Windows\\System32\\version.VerInstallFileW")
#pragma comment(linker, "/export:VerLanguageNameA=C:\\Windows\\System32\\version.VerLanguageNameA")
#pragma comment(linker, "/export:VerLanguageNameW=C:\\Windows\\System32\\version.VerLanguageNameW")
#pragma comment(linker, "/export:VerQueryValueA=C:\\Windows\\System32\\version.VerQueryValueA")
#pragma comment(linker, "/export:VerQueryValueW=C:\\Windows\\System32\\version.VerQueryValueW")


void CheckLocKeys()
{
	int ok = 0, ko = 0;
	uintptr_t base = 0; int count = 0;
	SafeRead((void*)(g_selfTech + TECH_LIST_OFF), &base, sizeof(base));
	SafeRead((void*)(g_selfTech + TECH_COUNT_OFF), &count, sizeof(count));
	if (base <= 0x10000 || count <= 0) { printf("[LOCK] table KO\n"); return; }

	for (int i = 0; i < count; ++i) {
		uintptr_t entry = base + (uintptr_t)i * TECH_ENTRY_SIZE, tech = 0;
		if (!SafeRead((void*)(entry + TECH_ENTRY_PTR), &tech, 8) || tech <= 0x10000) continue;
		char k[96];
		if (TechLocKey(tech, k, sizeof(k))) { ok++; }
		else {
			ko++;
			uintptr_t desc = 0; char internal[64] = { 0 };
			if (SafeRead((void*)(tech + TECH_DESC_OFF), &desc, 8) && desc > 0x10000)
				SafeRead((void*)desc, internal, sizeof(internal) - 1);
			printf("  [SANS CLE] %s\n", internal);
		}
	}
	printf("[LOCK] cles lues : %d ok, %d sans cle\n", ok, ko);
}



// ---------------------------------------------------------------------------
DWORD WINAPI PayloadThread(LPVOID)
{
	AllocConsole();
	FILE* f = nullptr;
	freopen_s(&f, "CONOUT$", "w", stdout);
	setvbuf(stdout, NULL, _IONBF, 0);
	g_consolePrete = true;

	uintptr_t modBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));

	if (MH_Initialize() != MH_OK) {
		printf("[!] MH_Initialize KO -- aucun hook installe\n");
		return 0;
	}	

	ApChargerChecksDeTest();
	InstallResourceHooks(modBase);
	InstallResearchHooks(modBase);
	InstallLocHooks(modBase);
	InstallScenarioHooks(modBase);
	ChargerSubstitutions();
	InstallNtHooks();


	while (true) {		
		if (GetAsyncKeyState(VK_F4) & 1) {
			printf("[F4] appui detecte\n");
			// 1) Le vocabulaire complet de l'exploration.
			ScanClassesDistinct("Expedition", 0);
			ScanClassesDistinct("WorldMap", 0);

			// 2) Le mecanisme de decouverte : qui porte une propriete "Reveal" ?
			FindPropertyByName("Reveal");
			FindPropertyByName("Discover");
		}

		if (GetAsyncKeyState(VK_F5) & 1) {
			printf("[F5] appui detecte\n");
			FindPropertyByName("Selected");
			printf("[F5] End\n");
			
		}
		if (GetAsyncKeyState(VK_F6) & 1) {
			printf("[F6] appui detecte\n");
			g_apTexteActif = !g_apTexteActif;
			printf("[TEST] textes AP : %s\n", g_apTexteActif ? "ON" : "OFF");
			printf("[F6] End\n");
		}
		
		if (GetAsyncKeyState(VK_F7) & 1) {
			printf("[F7] appui detecte\n");			
			g_apDonneesActif = !g_apDonneesActif;
			printf("[TEST] donnees AP : %s\n", g_apDonneesActif ? "ON" : "OFF");
			printf("[F7] End\n");
		}

		if (GetAsyncKeyState(VK_F8) & 1) {
			printf("[F8] appui detecte\n");
			g_apPorteurActif = !g_apPorteurActif;
			printf("[TEST] porteur AP : %s\n", g_apPorteurActif ? "ON" : "OFF");
			printf("[F8] End\n");
		}

		if (GetAsyncKeyState(VK_F11) & 1) {
			
		}

		static DWORD prochain = 0;
		if (GetTickCount() > prochain) {
			prochain = GetTickCount() + 1000;
			EnsureTechSelf();
			EnsureResSelf();
			CaptureRunState();

			CaptureRunState();

			// Seulement quand une partie est en place : jamais pendant un chargement.		
				
			ScootingTick();

			/*static bool dejaSignale = false;
			bool vic = ScenarioJustWon();
			if (vic && !dejaSignale && g_run.valid) {
				dejaSignale = true;
				printf("[VICTOIRE] %s remporte\n", g_run.scenario);
				for (int i = 0; i < g_run.diffCount; ++i)
					printf("   %s = niveau %d/%d\n",
						g_run.diff[i].name, g_run.diff[i].level, g_run.diff[i].levelCount);
			}
			if (!vic) dejaSignale = false;*/
		}
		Sleep(16);            // evite de saturer un coeur du CPU
	}
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hModule);
		CreateThread(NULL, 0, PayloadThread, NULL, 0, NULL);
	}
	return TRUE;
}
