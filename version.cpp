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
#include "sauvegardes_ap.h"
#include "groupes.h"
#include "evenements.h"
#include "dilemmes_ap.h"
#include "lois.h"
#include "lois_ap.h"
#include "menu_ap.h"
#include "historique_ap.h"
#include "scenarios_ap.h"
#include "victoire.h"
#include "temperature.h"
#include "quetes.h"
#include "reseau_ap.h"
#include "schema_reflexion.h"

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
// Build de release (/DAP_RELEASE, release\construire_release.ps1, 2026-11-07) :
// pas de console, journal dans <dossier du jeu>\mods\logs\console.log, debug
// neutralise. Build de dev : inchangee (journal dans le dossier du projet).
#ifdef AP_RELEASE
static wchar_t JOURNAL_DOSSIER[MAX_PATH] = L"";
static wchar_t JOURNAL_FICHIER[MAX_PATH] = L"";
static void CheminsJournalRelease()
{
	wchar_t base[MAX_PATH] = {};
	GetModuleFileNameW(NULL, base, MAX_PATH);
	for (int i = (int)wcslen(base) - 1; i >= 0; --i)
		if (base[i] == L'\\') { base[i + 1] = 0; break; }
	wchar_t mods[MAX_PATH];
	swprintf_s(mods, MAX_PATH, L"%smods", base);
	CreateDirectoryW(mods, NULL);
	swprintf_s(JOURNAL_DOSSIER, MAX_PATH, L"%smods\\logs", base);
	swprintf_s(JOURNAL_FICHIER, MAX_PATH, L"%smods\\logs\\console.log", base);
}
#else
static const wchar_t* JOURNAL_DOSSIER = L"C:\\Users\\Simon\\Desktop\\Visual\\Frost\\DLL\\logs";
static const wchar_t* JOURNAL_FICHIER = L"C:\\Users\\Simon\\Desktop\\Visual\\Frost\\DLL\\logs\\console.log";
#endif

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
	// immediate a disparu ainsi le 2026-10-27).
	// 2026-11-11 (demande de l'auteur : le journal d'une session Refugees a ete
	// ecrase au redemarrage du jeu) : l'ancien journal est garde sous
	// console_precedent.log (un seul niveau), au lieu d'etre efface.
	wchar_t precedent[MAX_PATH];
	swprintf_s(precedent, MAX_PATH, L"%s\\console_precedent.log", JOURNAL_DOSSIER);
	if (!MoveFileExW(JOURNAL_FICHIER, precedent, MOVEFILE_REPLACE_EXISTING))
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
// Releve de plantage (2026-11-10) : deux crashs c0000374 (corruption du tas
// Windows, ntdll) sur Steam, journal coupe, aucun minidump ni debogueur.
// Gestionnaire d'exceptions vectorise : sur une exception FATALE (tas
// corrompu c0000374, depassement de pile c0000409), la pile d'appels est
// ecrite AUSSITOT dans le journal (JournalImmediat, sans allocation) : module
// + decalage de chaque cadre. Une seule fois par processus.
// Violations d'acces (2026-11-11, deux AV Steam dans des Dynarray du moteur,
// +0xE087C1 et +0x1511CF5, sans pile) : relevees seulement si l'adresse
// fautive est DANS Frostpunk.exe (les SafeRead du mod fautent dans
// version.dll), 4 au plus par processus, registres compris. Une AV attrapee
// par un __try du mod autour d'une fonction du jeu (ex. pose d'icone) peut
// donc apparaitre : elle est suivie de la ligne d'EXCEPTION du mod.
static LONG CALLBACK ReleveCrash(PEXCEPTION_POINTERS e)
{
	DWORD code = e && e->ExceptionRecord ? e->ExceptionRecord->ExceptionCode : 0;
	if (code == 0xC0000005) {
		static volatile LONG nAV = 0;
		uintptr_t adr = (uintptr_t)e->ExceptionRecord->ExceptionAddress;
		HMODULE jeu = GetModuleHandleW(NULL);
		HMODULE m = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)adr, &m) || m != jeu) return EXCEPTION_CONTINUE_SEARCH;
		if (InterlockedIncrement(&nAV) > 4) return EXCEPTION_CONTINUE_SEARCH;
		const CONTEXT* c = e->ContextRecord;
		JournalImmediat("\n[CRASH] violation d'acces a Frostpunk.exe+0x%llX (%s 0x%llX), thread %lu\n",
			(unsigned long long)(adr - (uintptr_t)jeu),
			e->ExceptionRecord->NumberParameters >= 2 && e->ExceptionRecord->ExceptionInformation[0] ? "ecriture" : "lecture",
			(unsigned long long)(e->ExceptionRecord->NumberParameters >= 2 ? e->ExceptionRecord->ExceptionInformation[1] : 0),
			GetCurrentThreadId());
		JournalImmediat("[CRASH]   rax=%llX rbx=%llX rcx=%llX rdx=%llX rsi=%llX rdi=%llX\n",
			c->Rax, c->Rbx, c->Rcx, c->Rdx, c->Rsi, c->Rdi);
		JournalImmediat("[CRASH]   r8=%llX r9=%llX r10=%llX r11=%llX rsp=%llX\n", c->R8, c->R9, c->R10, c->R11, c->Rsp);
		// Diagnostic (2026-11-15, Last Autumn, deux crashs au retour d'un
		// eclaireur charge) : file de production d'un batiment liberee, lue par
		// FUN_1417e5160 (Epic +0x17E51E0, Steam +0x16801E0 : add edi,[rcx+8]).
		// rax = FrostpunkProductionComponent, rsi = entree d'etat (+0 entite).
		{
			uintptr_t off = adr - (uintptr_t)jeu;
			if (off == 0x17E51E0 || off == 0x16801E0) {
				char cls[96] = "?", desc[300] = "?";
				GetClassNameFP(c->Rax, cls, sizeof(cls));
				uintptr_t ent = 0, enCours = 0, file = 0;
				int32_t nFile = -1;
				float prog = 0.f;
				SafeRead((void*)(c->Rax + 0x18), &ent, 8);
				SafeRead((void*)(c->Rax + 0x30), &enCours, 8);
				SafeRead((void*)(c->Rax + 0x38), &file, 8);
				SafeRead((void*)(c->Rax + 0x40), &nFile, 4);
				SafeRead((void*)(c->Rax + 0x60), &prog, 4);
				if (ent > 0x10000) DecrireEntiteContexte(ent, desc, sizeof(desc));
				JournalImmediat("[CRASH]   composant 0x%llX %s : entite %s\n", (unsigned long long)c->Rax, cls, desc);
				JournalImmediat("[CRASH]   +0x30 en cours 0x%llX, file +0x38 0x%llX (%d entree(s)), +0x60 %.3f\n",
					(unsigned long long)enCours, (unsigned long long)file, nFile, prog);
				uintptr_t entEtat = 0;
				if (SafeRead((void*)c->Rsi, &entEtat, 8) && entEtat > 0x10000 && entEtat != ent) {
					DecrireEntiteContexte(entEtat, desc, sizeof(desc));
					JournalImmediat("[CRASH]   entree d'etat : entite %s\n", desc);
				}
			}
		}
		// Adresses de retour probables : valeurs de la pile qui tombent dans
		// Frostpunk.exe ou version.dll, precedees d'un CALL (E8 / FF xx).
		HMODULE mod = nullptr;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&ReleveCrash, &mod);
		const uintptr_t* pile = (const uintptr_t*)c->Rsp;
		int vus = 0;
		for (int k = 0; k < 512 && vus < 24; ++k) {
			uintptr_t v = 0;
			if (!SafeRead((void*)(pile + k), &v, 8) || v < 0x10000) continue;
			HMODULE mv = nullptr;
			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					(LPCSTR)v, &mv) || (mv != jeu && mv != mod)) continue;
			uint8_t av[6] = {};
			if (!SafeRead((void*)(v - 6), av, 6)) continue;
			if (av[1] != 0xE8 && av[4] != 0xFF && av[3] != 0xFF) continue;
			JournalImmediat("[CRASH]   pile +0x%03X : %s+0x%llX\n", k * 8, mv == jeu ? "Frostpunk.exe" : "version.dll",
				(unsigned long long)(v - (uintptr_t)mv));
			++vus;
		}
		JournalImmediat("[CRASH] fin du releve\n");
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if (code != 0xC0000374 && code != 0xC0000409) return EXCEPTION_CONTINUE_SEARCH;
	static volatile LONG fait = 0;
	if (InterlockedExchange(&fait, 1)) return EXCEPTION_CONTINUE_SEARCH;
	JournalImmediat("\n[CRASH] exception 0x%08lX a 0x%p, thread %lu\n", code,
		e->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
	void* cadres[62] = {};
	USHORT n = RtlCaptureStackBackTrace(0, 62, cadres, nullptr);
	for (USHORT i = 0; i < n; ++i) {
		HMODULE m = nullptr;
		char chemin[MAX_PATH] = "?";
		const char* nom = chemin;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)cadres[i], &m) && m) {
			GetModuleFileNameA(m, chemin, MAX_PATH);
			for (const char* p = chemin; *p; ++p) if (*p == '\\') nom = p + 1;
		}
		JournalImmediat("[CRASH]   %2u  %s+0x%llX\n", (unsigned)i, nom,
			(unsigned long long)((uintptr_t)cadres[i] - (uintptr_t)m));
	}
	JournalImmediat("[CRASH] fin de la pile\n");
	return EXCEPTION_CONTINUE_SEARCH;
}

// Pile d'appels courante dans le journal (module + decalage), sans allocation.
static void ReleverPileCourante(const char* motif)
{
	JournalImmediat("\n[CRASH] %s, thread %lu\n", motif, GetCurrentThreadId());
	void* cadres[62] = {};
	USHORT n = RtlCaptureStackBackTrace(1, 62, cadres, nullptr);
	for (USHORT i = 0; i < n; ++i) {
		HMODULE m = nullptr;
		char chemin[MAX_PATH] = "?";
		const char* nom = chemin;
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)cadres[i], &m) && m) {
			GetModuleFileNameA(m, chemin, MAX_PATH);
			for (const char* p = chemin; *p; ++p) if (*p == '\\') nom = p + 1;
		}
		JournalImmediat("[CRASH]   %2u  %s+0x%llX\n", (unsigned)i, nom,
			(unsigned long long)((uintptr_t)cadres[i] - (uintptr_t)m));
	}
	JournalImmediat("[CRASH] fin de la pile\n");
}

// 4e crash (2026-11-10, arrivee des refugies) : c0000409 dans ucrtbase.dll
// (CRT DU JEU : la DLL a son CRT statique). C'est un arret immediat
// (__fastfail), invisible du gestionnaire vectorise : parametre invalide
// d'une fonction du CRT, ou abort() / terminate(). Gestionnaires poses dans
// l'ucrtbase du jeu (GetProcAddress) : ils relevent la pile puis laissent
// le CRT arreter le jeu comme avant (comportement inchange).
typedef void(__cdecl* GestionnaireParam_t)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t);
typedef GestionnaireParam_t(__cdecl* PoserGestionnaireParam_t)(GestionnaireParam_t);
typedef void(__cdecl* GestionnaireSignal_t)(int);
typedef GestionnaireSignal_t(__cdecl* PoserSignal_t)(int, GestionnaireSignal_t);
static GestionnaireParam_t g_ancienParam = nullptr;

static void __cdecl ParametreInvalideJeu(const wchar_t* e, const wchar_t* f, const wchar_t* fi, unsigned l, uintptr_t r)
{
	static volatile LONG n = 0;
	if (InterlockedIncrement(&n) <= 3) ReleverPileCourante("parametre invalide d'une fonction du CRT du jeu (ucrtbase)");
	if (g_ancienParam) g_ancienParam(e, f, fi, l, r);
	else __fastfail(5 /* FAST_FAIL_INVALID_ARG */);
}

static void __cdecl AbortJeu(int)
{
	static volatile LONG n = 0;
	if (InterlockedIncrement(&n) <= 1) ReleverPileCourante("abort() / terminate() (SIGABRT, ucrtbase)");
}

static void InstallerRelevesCRTJeu()
{
	HMODULE ucrt = GetModuleHandleA("ucrtbase.dll");
	if (!ucrt) { printf("[CRASH] ucrtbase.dll absent : pas de releve des arrets du CRT\n"); return; }
	PoserGestionnaireParam_t poserParam = (PoserGestionnaireParam_t)GetProcAddress(ucrt, "_set_invalid_parameter_handler");
	PoserSignal_t poserSignal = (PoserSignal_t)GetProcAddress(ucrt, "signal");
	if (poserParam) g_ancienParam = poserParam(&ParametreInvalideJeu);
	if (poserSignal) poserSignal(22 /* SIGABRT */, &AbortJeu);
	printf("[CRASH] releves du CRT du jeu : parametre invalide %s, SIGABRT %s\n", poserParam ? "OK" : "KO",
		poserSignal ? "OK" : "KO");
}

// ---------------------------------------------------------------------------
DWORD WINAPI PayloadThread(LPVOID)
{
	FILE* f = nullptr;
#ifdef AP_RELEASE
	// Release : pas de fenetre console. stdout pointe sur NUL pour avoir un
	// descripteur valide, que DupliquerConsoleVersJournal remplace par le tube
	// (le relais n'ecrit alors que dans le journal : CONOUT$ est introuvable).
	CheminsJournalRelease();
	freopen_s(&f, "NUL", "w", stdout);
#else
	AllocConsole();
	freopen_s(&f, "CONOUT$", "w", stdout);
#endif
	setvbuf(stdout, NULL, _IONBF, 0);
	DupliquerConsoleVersJournal();
	g_consolePrete = true;

	uintptr_t modBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(NULL));

	// Version de l'exe (versions_jeu.h, 2026-11-07) : Epic ou Steam. Exe
	// inconnu : aucun hook, le jeu tourne en vanilla (adresses inconnues =
	// crash immediat au premier hook).
	JournalVersionJeu();
	if (!VersionJeuSupportee()) return 0;
	AddVectoredExceptionHandler(1, &ReleveCrash);   // releve de plantage (2026-11-10)
	InstallerRelevesCRTJeu();                       // arrets immediats du CRT du jeu (2026-11-10)

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
	InstallHooksExpeditions(modBase);   // marqueurs et tirage fixe des sites
	InstallMenuAP(modBase);   // ecran de connexion Archipelago du menu principal
	InstallHistoriqueAP(modBase);   // historique de la session AP (HUD en jeu)
	InstallHooksScenarios(modBase);   // scenarios verrouilles tant qu'Archipelago ne les debloque pas
	InstallHooksVictoire(modBase);    // fin de partie : checks de victoire selon le mode
	InstallHooksSauvegardes(modBase); // liste des sauvegardes : celles de la session Archipelago seulement


	ScootingPayloadsInit();   // tous les sites AP (known_ap_ids.h) deviennent des checks
	ApChargerTechnosDeTest();
	// Reseau Archipelago : derniere session (slot_data, scenarios debloques par
	// les items deja recus), puis thread reseau ; connexion au clic sur Connect.
	ReseauDemarrer();
	InstallResourceHooks(modBase);
	InstallResearchHooks(modBase);
	InstallLocHooks(modBase);	
	InstallScenarioHooks(modBase);
	InitAllocateur(modBase);
	InitFabriqueMessage(modBase);
	InstallNotifHooks(modBase);
		
	while (true) {		
		// Touches de debug (F3-F9, F11) retirees le 2026-11-04 a la demande de l'auteur.
		// F11 (demande de l'auteur, 2026-11-06, pour tester les quetes comme
		// 006) : terminer avec succes la quete d'histoire en cours (thread du
		// jeu, QuetesPompe).
#ifndef AP_RELEASE
		if (ToucheAppuyee(VK_F11)) QuetesDemanderFinQueteEnCours();
#endif
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
#ifndef AP_RELEASE
			RapportSubstitutions();        // DIAG : chargements des atlas substitues
			SchemaReflexionTick();         // Big map : schema de la reflexion (une fois)
#endif
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
