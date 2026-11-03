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
#include "IconSubManager.h"
#include "MessageManager.h"
#include "archipelago.h"
#include "groupes.h"
#include "evenements.h"
#include "dilemmes_ap.h"
#include "lois.h"
#include "lois_ap.h"
#include "menu_ap.h"
#include "scenarios_ap.h"
#include "victoire.h"
#include "temperature.h"
#include "quetes.h"

#include <io.h>
#include <fcntl.h>
#include <cstdarg>

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



// --- Journal : copie de tout ce qui s'ecrit dans la console -----------------
// stdout est redirige vers un tube ; un thread relais recopie chaque octet a la
// fois dans la console et dans le fichier journal (ecrase a chaque lancement).
static const wchar_t* JOURNAL_DOSSIER = L"C:\\Users\\Simon\\Desktop\\Visual\\Frost\\DLL\\logs";
static const wchar_t* JOURNAL_FICHIER = L"C:\\Users\\Simon\\Desktop\\Visual\\Frost\\DLL\\logs\\console.log";

static HANDLE g_tubeLecture = NULL;
static HANDLE g_consoleSortie = INVALID_HANDLE_VALUE;
static HANDLE g_journal = INVALID_HANDLE_VALUE;

static DWORD WINAPI RelaisConsole(LPVOID)
{
	char buf[4096];
	DWORD lu = 0, ecrit = 0;
	while (ReadFile(g_tubeLecture, buf, sizeof(buf), &lu, NULL) && lu > 0) {
		if (g_consoleSortie != INVALID_HANDLE_VALUE) WriteFile(g_consoleSortie, buf, lu, &ecrit, NULL);
		if (g_journal != INVALID_HANDLE_VALUE)       WriteFile(g_journal, buf, lu, &ecrit, NULL);
	}
	return 0;
}

static void DupliquerConsoleVersJournal()
{
	g_consoleSortie = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	CreateDirectoryW(JOURNAL_DOSSIER, NULL);
	// Mode AJOUT (FILE_APPEND_DATA) : le relais du tube et JournalImmediat
	// ecrivent dans le meme fichier depuis deux threads. Sans ce mode, ils
	// partagent une position d'ecriture et peuvent s'ecraser (une ligne
	// immediate a disparu ainsi le 2026-10-27). L'ancien journal est efface.
	DeleteFileW(JOURNAL_FICHIER);
	g_journal = CreateFileW(JOURNAL_FICHIER, FILE_APPEND_DATA | SYNCHRONIZE, FILE_SHARE_READ, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD erreurJournal = GetLastError();

	HANDLE ecriture = NULL;
	if (!CreatePipe(&g_tubeLecture, &ecriture, NULL, 1 << 20)) {
		printf("[LOG] creation du tube KO -- pas de journal\n");
		return;
	}
	int fd = _open_osfhandle((intptr_t)ecriture, _O_WRONLY | _O_TEXT);
	if (fd < 0 || _dup2(fd, _fileno(stdout)) != 0) {
		printf("[LOG] redirection de stdout KO -- pas de journal\n");
		return;
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	CreateThread(NULL, 0, RelaisConsole, NULL, 0, NULL);

	if (g_journal == INVALID_HANDLE_VALUE)
		printf("[LOG] fichier journal KO (erreur %lu)\n", erreurJournal);
	else
		printf("[LOG] journal : %ls\n", JOURNAL_FICHIER);
}

// Ecriture SYNCHRONE dans le journal et la console, sans passer par le tube :
// la ligne est sur disque avant le retour, meme si le jeu plante juste apres
// (le relais du tube, lui, perd les dernieres lignes en cas de crash).
// A reserver aux lignes critiques : l'ordre avec les printf n'est pas garanti.
void JournalImmediat(const char* fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (n <= 0) return;
	if (n >= (int)sizeof(buf)) n = (int)sizeof(buf) - 1;
	DWORD ecrit = 0;
	if (g_journal != INVALID_HANDLE_VALUE)       WriteFile(g_journal, buf, (DWORD)n, &ecrit, NULL);
	if (g_consoleSortie != INVALID_HANDLE_VALUE) WriteFile(g_consoleSortie, buf, (DWORD)n, &ecrit, NULL);
}

// ---------------------------------------------------------------------------
DWORD WINAPI PayloadThread(LPVOID)
{
	AllocConsole();
	FILE* f = nullptr;
	freopen_s(&f, "CONOUT$", "w", stdout);
	setvbuf(stdout, NULL, _IONBF, 0);
	DupliquerConsoleVersJournal();
	g_consolePrete = true;

	uintptr_t modBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));

	if (MH_Initialize() != MH_OK) {
		printf("[!] MH_Initialize KO -- aucun hook installe\n");
		return 0;
	}

	// TEST : capture des templates a leur construction, au plus tot pour ne
	// pas rater ceux crees au demarrage (entrees de message, 6 gabarits).
	InstallCaptureGabarits(modBase);
	InstallCaptureEntrees(modBase);
	InstallTraceCreationEntites(modBase);
	InstallCaptureTemplatesGroupes(modBase);
	// Substitution des atlas AU PLUS TOT (2026-11-03) : le jeu charge ses
	// textures pendant l'installation des hooks. Posee apres tous les autres,
	// elle arrivait apres la lecture de UI/Tiles/Icons.texture (#2350, atlas
	// des bulles et des lois) : plus aucune icone AP dans le livre des lois.
	ChargerSubstitutions();
	InstallNtHooks();
	InstallHooksEvenements(modBase);
	InstallTraceArchives(modBase);
	InstallTracePopups(modBase);
	InstallTraceChargement(modBase);
	InstallHooksLoisAP(modBase);
	InstallHooksTemperature(modBase);   // montees de temperature Archipelago
	InstallHooksQuetes(modBase);        // trace des demarrages de quete
	InstallHooksExpeditions(modBase);   // DEBUG : trajets d'expedition de 15 min
	InstallMenuAP(modBase);   // ecran de connexion Archipelago du menu principal
	InstallHooksScenarios(modBase);   // scenarios verrouilles tant qu'Archipelago ne les debloque pas
	InstallHooksVictoire(modBase);    // fin de partie : checks de victoire selon le mode


	ApChargerChecksDeTest();
	ApChargerTechnosDeTest();
	ScenariosRecevoirItemsDeTest();   // TEMPORAIRE : items de scenario de depart (les 6)
	InstallResourceHooks(modBase);
	InstallResearchHooks(modBase);
	InstallLocHooks(modBase);	
	InstallScenarioHooks(modBase);
	InitAllocateur(modBase);
	InitFabriqueMessage(modBase);
	InstallNotifHooks(modBase);
		
	while (true) {		
		if (ToucheAppuyee(VK_F4) & 1) {
			printf("[F4] appui detecte\n");
			// Releve des lois : arbres, prerequis, reglages des scenarios.
			// (Remplace ResetSeenKeys, reste de l'ancienne fenetre de releve
			// des cles.)
			LoisDemanderReleve();   // execute sur le thread du jeu (hors pause)
			printf("[F4] End\n");

		}

		if (ToucheAppuyee(VK_F5) & 1) {
			printf("[F5] appui detecte\n");
			// Test des lois AP : un item progressif pour l'arbre de la loi
			// en attente la plus ancienne (remplace BaselineKeys, reste de
			// l'ancienne fenetre de releve des cles).
			LoisAPDemanderTest();   // execute sur le thread du jeu (hors pause)
			printf("[F5] End\n");
		}
		if (ToucheAppuyee(VK_F6) & 1) {
			printf("[F6] appui detecte\n");
			// Test : piege Archipelago par dilemme (perdre 50 charbon ou 50
			// acier). Remplace l'ancienne fenetre de releve des cles
			// (OpenKeyLogWindow), F10 etant le menu des options du jeu.
			DilemmeAPDemander("trap_coal_or_steel");
			printf("[F6] End\n");
		}
		
		if (ToucheAppuyee(VK_F7) & 1) {
			printf("[F7] appui detecte\n");
			// Test de la couche Archipelago : une progressive au hasard.
			int arbre = ArbreProgressifAuHasard();
			if (arbre < 0) printf("[F7] tous les arbres sont epuises\n");
			else ReceiveProgressive(arbre, "Test F7");
			printf("[F7] End\n");
		}

		if (ToucheAppuyee(VK_F8) & 1) {
			printf("[F8] appui detecte\n");
			// Test : un item "Temperature rise" de 1 niveau (+10 degres).
			// Remplace le rapprochement du premier groupe (valide), sur
			// decision de l'auteur (2026-11-01).
			ReceiveTemperatureRise(1, "Test F8");
			printf("[F8] End\n");
		}

		if (ToucheAppuyee(VK_F9) & 1) {
			bool maj = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
			bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
			if (maj || ctrl) {
				// Test de la fin de partie (2026-11-01) : Maj+F9 = victoire,
				// Ctrl+F9 = defaite, par EndGame du jeu, sur le thread du jeu.
				printf("[F9] %s+F9 : fin de partie de test (%s)\n", maj ? "Maj" : "Ctrl", maj ? "victoire" : "defaite");
				VictoireDemanderTest(maj);
			}
			else {
				printf("[F9] appui detecte\n");
				// Releve des quetes (repertoires, quetes, objectifs, effets,
				// evenements), sur le thread du jeu. Le releve de temperature,
				// exploite, n'est plus lie a une touche (2026-11-01).
				QuetesDemanderReleve();
			}
			printf("[F9] End\n");
		}

		// Pas de F10 : c'est la touche du menu des options du jeu.
		if (ToucheAppuyee(VK_F3) & 1) {
			printf("[F3] appui detecte\n");
			// Test : creation d'un groupe de survivants (copie du premier
			// groupe existant), executee sur le thread du jeu.
			GroupesDemanderCreation();
			printf("[F3] End\n");
		}

		if (ToucheAppuyee(VK_F11) & 1) {
			printf("[F11] appui detecte\n");
			// Test de la couche Archipelago : ressources recues, dont une que
			// le jeu touche rarement (coeurs de vapeur).
			ReceiveResource("wood", 500, "F11 test");
			ReceiveResource("steamcore", 1, "F11 test");
			printf("[F11] End\n");
		}

		static DWORD prochain = 0;
		if (GetTickCount() > prochain) {
			
			prochain = GetTickCount() + 1000;
			CHRONO(EnsureTechSelf());
			CHRONO(EnsureResSelf());
			CHRONO(PartieTick());          // chargements + references globales
			CHRONO(CaptureRunState());
			CHRONO(TechPersistanceTick()); // technos : sauvegarde locale de la partie
			CHRONO(ScootingTick());
			CHRONO(SurveillerArbreTechno());
			RapportSubstitutions();        // DIAG : chargements des atlas substitues
		}
		CouleursMessagesTick();   // couleur de nos messages, 2 s apres leur ajout
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
