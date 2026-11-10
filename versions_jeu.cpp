// versions_jeu.cpp -- Detection de l'exe et traduction des adresses Epic.
// Voir versions_jeu.h. Table Steam generee le 2026-11-07 (session de
// portage : appariement Epic / Steam, scripts dans docs/chantiers/steam.md).
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include "versions_jeu.h"

namespace {

struct Correspondance { uintptr_t epic, steam; };

// Triee par adresse Epic (recherche dichotomique). Commentaire : premier
// usage dans le mod -- methode d'appariement et score.
const Correspondance VERSION_STEAM[] = {
    { 0x0E1A110, 0x0E1DEA0 },   // menu_ap.cpp:71 -- feuille (motif d'octets)
    { 0x0E1A120, 0x0E1DEB0 },   // menu_ap.cpp:70 -- exact 1.00
    { 0x0E1A260, 0x0E1DFF0 },   // menu_ap.cpp:72 -- exact 1.00
    { 0x0E1CE20, 0x0E20BB0 },   // groupes.cpp:675 -- feuille (motif d'octets)
    { 0x0E1D180, 0x0E20F10 },   // groupes.cpp:1501 -- feuille (motif d'octets)
    { 0x0E27BF0, 0x0E2BA90 },   // mod_common.cpp:1985 -- feuille (motif d'octets)
    { 0x0E27C00, 0x0E2BAA0 },   // scooting.cpp:1619 -- voisinage(5 cand) 1.00
    { 0x0E4E990, 0x0E52830 },   // MessageManager.cpp:986 -- exact 1.00
    { 0x0E4EAB0, 0x0E52950 },   // MessageManager.cpp:532 -- exact 1.00
    { 0x0E4F9A0, 0x0E53840 },   // groupes.cpp:677 -- exact 1.00
    { 0x0E4FA20, 0x0E538C0 },   // MessageManager.cpp:531 -- exact 1.00
    { 0x0E51FA0, 0x0E58E40 },   // mod_common.cpp:1414 -- exact 1.00
    { 0x0FEB340, 0x0E84840 },   // mod_common.h:145 -- exact 1.00
    { 0x0FEB760, 0x0E84C60 },   // quetes.cpp:155 -- exact 1.00
    { 0x1016250, 0x0EAFE40 },   // groupes.cpp:37 -- exact 1.00
    { 0x1017230, 0x0EB0E20 },   // scooting.cpp:1580 -- exact 1.00
    { 0x1023D70, 0x0EBD960 },   // lois_ap.cpp:1090 -- exact 1.00
    { 0x10240D0, 0x0EBDCC0 },   // menu_ap.cpp:1287 -- exact 1.00
    { 0x1025CC0, 0x0EBF8B0 },   // groupes.cpp:1496 -- feuille (motif d'octets)
    { 0x1025FF0, 0x0EBFBE0 },   // menu_ap.cpp:73 -- exact 1.00
    { 0x10260C0, 0x0EBFCB0 },   // MessageManager.cpp:919 -- exact 1.00
    { 0x10261E0, 0x0EBFDD0 },   // menu_ap.cpp:68 -- exact 1.00
    { 0x10266A0, 0x0EC0290 },   // menu_ap.cpp:63 -- exact 1.00
    { 0x10297F0, 0x0EC33E0 },   // MessageManager.cpp:632 -- exact 1.00
    { 0x102A3E0, 0x0EC3FD0 },   // lois_ap.cpp:1068 -- exact 1.00
    { 0x102B170, 0x0EC4E40 },   // lois_ap.cpp:1066 -- exact 1.00
    { 0x102F720, 0x0EC93F0 },   // menu_ap.cpp:77 -- exact 1.00
    { 0x1030E90, 0x0ECAB60 },   // menu_ap.cpp:69 -- exact 1.00
    { 0x1035C80, 0x0ECF950 },   // menu_ap.cpp:61 -- exact 1.00
    { 0x1035CF0, 0x0ECF9C0 },   // scooting.cpp:1668 -- voisinage(1 cand) 1.00
    { 0x1035DD0, 0x0ECFAA0 },   // groupes.cpp:62 -- exact 1.00
    { 0x105A1C0, 0x0EF4B80 },   // menu_ap.cpp:64 -- exact 1.00
    { 0x105A3A0, 0x0EF4D60 },   // menu_ap.cpp:1286 -- exact 1.00
    { 0x105AC90, 0x0EF5650 },   // lois_ap.cpp:1074 -- exact 1.00
    { 0x1079AA0, 0x0F14060 },   // hooks_loc.cpp:487 -- exact 1.00
    { 0x1107140, 0x0FA0960 },   // IconSubManager.cpp:649 -- exact 1.00
    { 0x1107360, 0x0FA0B80 },   // lois_ap.cpp:1097 -- exact 1.00
    { 0x110C780, 0x0FA5FA0 },   // lois_ap.cpp:1090 -- exact 1.00
    { 0x11180B0, 0x0FB18D0 },   // scooting.cpp:1838 -- exact 1.00
    { 0x1119140, 0x0FB2960 },   // menu_ap.cpp:62 -- exact 1.00
    { 0x111BD70, 0x0FB5590 },   // IconSubManager.cpp:650 -- feuille (motif d'octets)
    { 0x111BEF0, 0x0FB5710 },   // lois_ap.cpp:1368 -- exact 1.00
    { 0x111C4F0, 0x0FB5D10 },   // scooting.cpp:1676 -- exact 1.00
    { 0x111D100, 0x0FB6920 },   // groupes.cpp:63 -- exact 1.00
    { 0x1129C90, 0x0FC34B0 },   // menu_ap.cpp:79 -- voisinage(1 cand) 0.90
    { 0x112A360, 0x0FC3B80 },   // menu_ap.cpp:67 -- exact 1.00
    { 0x112A920, 0x0FC4140 },   // menu_ap.cpp:66 -- exact 1.00
    { 0x112AC10, 0x0FC4430 },   // menu_ap.cpp:78 -- exact 1.00
    { 0x11FD800, 0x1096C10 },   // menu_ap.cpp:76 -- exact 1.00
    { 0x1265DF0, 0x10FF200 },   // mod_common.cpp:1484 -- exact 1.00
    { 0x1267C50, 0x1101060 },   // scenario.cpp:105 -- exact 1.00
    { 0x126C9F0, 0x1105E00 },   // mod_common.cpp:1486 -- exact 1.00
    { 0x1283EA0, 0x111D2B0 },   // scenarios_ap.cpp:85 -- exact 1.00
    { 0x1283F90, 0x111D3A0 },   // sauvegardes_ap.cpp:29 -- appels de FUN_1412878d0 identiques (+0x89D), 2026-11-10
    { 0x1284EB0, 0x111E2C0 },   // sauvegardes_ap.cpp:28 -- seul appelant de 0x1C29F80, meme taille, 2026-11-10
    { 0x1286840, 0x111FC50 },   // mod_common.cpp:1485 -- exact 1.00
    { 0x12892F0, 0x11226F0 },   // sauvegardes_ap.cpp:20 -- exact 1.00
    { 0x12EC200, 0x1186AC0 },   // groupes.cpp:674 -- getter de type (frostpunkselectioncomponent.h), corrige le 2026-11-10
    { 0x1301490, 0x119BCE0 },   // victoire.cpp:46 -- exact 1.00
    { 0x131DE20, 0x11B8670 },   // scooting.cpp:2243 -- exact 1.00
    { 0x1325E40, 0x11C0690 },   // hooks_research.cpp:34 -- exact 1.00
    { 0x1326A60, 0x11C12B0 },   // mod_common.h:141 -- exact 1.00
    { 0x1326C80, 0x11C14D0 },   // mod_common.h:144 -- exact 1.00
    { 0x1327120, 0x11C1970 },   // mod_common.h:143 -- exact 1.00
    { 0x1329610, 0x11C3E60 },   // mod_common.h:142 -- exact 1.00
    { 0x132F260, 0x11C9AB0 },   // temperature.cpp:877 -- exact 1.00
    { 0x132FD90, 0x11CA5E0 },   // scooting.cpp:253 -- getter de type (frostpunkworldmapentitycomponent.h), corrige le 2026-11-10
    { 0x1331940, 0x11CC190 },   // groupes.cpp:1446 -- exact 1.00
    { 0x1332CD0, 0x11CD520 },   // groupes.cpp:1445 -- exact 1.00
    { 0x139E930, 0x1239180 },   // mod_common.h:140 -- voisinage(2 cand) 1.00
    { 0x148C130, 0x1326980 },   // scooting.cpp:1837 -- getter de type (frostpunkexpeditionsitecomponent.h), corrige le 2026-11-10
    { 0x1518B70, 0x13B33C0 },   // scenarios_ap.cpp:323 -- exact 1.00
    { 0x1559730, 0x13F4070 },   // temperature.cpp:730 -- exact 1.00
    { 0x167C580, 0x1517520 },   // MessageManager.cpp:98 -- exact 1.00
    { 0x1685A50, 0x15209F0 },   // MessageManager.cpp:90 -- exact 1.00
    { 0x16896B0, 0x1524650 },   // MessageManager.cpp:118 -- exact 1.00
    { 0x168DC50, 0x1528BF0 },   // scooting.cpp:2225 -- exact 1.00
    { 0x169AEE0, 0x1535E80 },   // quetes.cpp:947 -- exact 1.00
    { 0x16A71F0, 0x1542190 },   // lois_ap.cpp:129 -- voisinage(2 cand) 0.98
    { 0x16A83B0, 0x1543350 },   // IconSubManager.cpp:555 -- exact 1.00
    { 0x16B7350, 0x15522F0 },   // groupes.cpp:1493 -- exact 1.00
    { 0x16C5EC0, 0x1560E60 },   // quetes.cpp:1144 -- voisinage(3 cand) 1.00
    { 0x16D22A0, 0x156D240 },   // quetes.cpp:1143 -- exact 1.00
    { 0x16D2710, 0x156D6B0 },   // quetes.cpp:1381 -- exact 1.00
    { 0x16E8570, 0x1583570 },   // quetes.cpp:936 -- exact 1.00
    { 0x16FD350, 0x1598350 },   // lois_ap.cpp:42 -- exact 1.00
    { 0x16FDBC0, 0x1598BC0 },   // lois_ap.cpp:43 -- exact 1.00
    { 0x16FECC0, 0x1599CC0 },   // lois_ap.cpp (etat brut d'une loi) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x16FF990, 0x159A990 },   // lois_ap.cpp (loi signee ?) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x173FE40, 0x15DAE40 },   // mod_common.h:139 -- exact 1.00
    { 0x17B9B70, 0x1654B70 },   // evenements.cpp:46 -- exact 1.00
    { 0x17CDE10, 0x1668E10 },   // evenements.cpp:65 -- exact 1.00
    { 0x17CFBF0, 0x166ABF0 },   // evenements.cpp:943 -- feuille (motif d'octets)
    { 0x17CFC40, 0x166AC40 },   // evenements.cpp:826 -- feuille (motif d'octets)
    { 0x17CFD70, 0x166AD70 },   // evenements.cpp:944 -- exact 1.00
    { 0x17D16D0, 0x166C6D0 },   // evenements.cpp:827 -- exact 1.00
    { 0x17D40C0, 0x166F0C0 },   // evenements.cpp:445 -- exact 1.00
    { 0x1812C10, 0x16ADC10 },   // temperature.cpp:575 -- exact 1.00
    { 0x1884960, 0x171F960 },   // groupes.cpp:320 -- voisinage(2 cand) 1.00
    { 0x18B1CC0, 0x174CCC0 },   // groupes.cpp:319 -- exact 1.00
    { 0x18B2870, 0x174D870 },   // groupes.cpp:347 -- exact 1.00
    { 0x18EF2F0, 0x178A1C0 },   // scooting.cpp:2239 -- exact 1.00
    { 0x18F2A30, 0x178D900 },   // scooting.cpp:2235 -- exact 1.00
    { 0x1916F90, 0x17B1E60 },   // IconSubManager.cpp:307 -- exact 1.00
    { 0x19E68B0, 0x1883300 },   // MessageManager.cpp:335 -- exact 1.00
    { 0x19E9560, 0x1885FB0 },   // mod_common.cpp:2689 -- voisinage(2 cand) 0.98
    { 0x1A0EB70, 0x18AB5F0 },   // mod_common.cpp:2690 -- exact 1.00
    { 0x1A0F1C0, 0x18ABC40 },   // groupes.cpp:672 -- exact 1.00
    { 0x1A0F2A0, 0x18ABD20 },   // groupes.cpp:673 -- exact 1.00
    { 0x1A31660, 0x18CE0E0 },   // groupes.cpp:1494 -- voisinage(1 cand) 1.00
    { 0x1A316B0, 0x18CE130 },   // groupes.cpp:1495 -- exact 1.00
    { 0x1A33780, 0x18D0200 },   // evenements.cpp:945 -- exact 1.00
    { 0x1AA9C00, 0x1946680 },   // MessageManager.cpp:105 -- exact 1.00
    { 0x1AA9D70, 0x19467F0 },   // MessageManager.cpp:104 -- exact 1.00
    { 0x1AA9DF0, 0x1946870 },   // MessageManager.cpp:94 -- exact 1.00
    { 0x1AAB820, 0x19482A0 },   // MessageManager.cpp:913 -- exact 1.00
    { 0x1AAD120, 0x1949BA0 },   // MessageManager.cpp:916 -- exact 1.00
    { 0x1B28950, 0x19C5B80 },   // menu_ap.cpp:59 -- voisinage(3 cand) 0.95
    { 0x1B28F10, 0x19C61C0 },   // menu_ap.cpp:1285 -- voisinage(3 cand) 0.53
    { 0x1B2A9C0, 0x19C9140 },   // mod_common.h:217 -- voisinage(2 cand) 0.68
    { 0x1B2CB40, 0x19CB3B0 },   // scenarios_ap.cpp:86 -- exact 1.00
    { 0x1B2DCC0, 0x19CC530 },   // mod_common.h:216 -- exact 1.00
    { 0x1B2E8C0, 0x19CD130 },   // scenarios_ap.cpp:322 -- exact 1.00
    { 0x1BB2B60, 0x1A53880 },   // IconSubManager.cpp:554 -- exact 1.00
    { 0x1BB2D30, 0x1A53A50 },   // IconSubManager.cpp:554 -- exact 1.00
    { 0x1BBB480, 0x1A5C1A0 },   // lois_ap.cpp (donnees d'un noeud du livre) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x1BBC320, 0x1A5D040 },   // lois_ap.cpp:993 -- voisinage(1 cand) 0.94
    { 0x1BC1440, 0x1A62180 },   // lois_ap.cpp:1455 -- exact 1.00
    { 0x1BC2020, 0x1A62D60 },   // lois_ap.cpp (visuel d'un noeud du livre) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x1BC2640, 0x1A63380 },   // lois_ap.cpp (etat d'un noeud du livre) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x1BC5100, 0x1A65E40 },   // hooks_research.cpp (noeud disponible, arbre) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x1BC5810, 0x1A66550 },   // IconSubManager.cpp:553 -- exact 1.00
    { 0x1BCA5A0, 0x1A6B2E0 },   // hooks_research.cpp (rafraichissement d'un noeud, arbre) -- voisinage, octets identiques hors rip (2026-11-10)
    { 0x1BCC1A0, 0x1A6CEE0 },   // temperature.cpp:731 -- exact 1.00
    { 0x1BE67F0, 0x1A87530 },   // IconSubManager.cpp:553 -- exact 1.00
    { 0x1C19460, 0x1ABA210 },   // evenements.cpp:942 -- exact 1.00
    { 0x1C1A0E0, 0x1ABAE90 },   // scooting.cpp:2229 -- exact 1.00
    { 0x1C2B6E0, 0x1ACC490 },   // groupes.cpp:1492 -- exact 1.00
    { 0x1CDEE60, 0x1C29F80 },   // sauvegardes_ap.cpp:21 -- voisinage(8 cand) 0.98
    { 0x1CE67F0, 0x1C32000 },   // groupes.cpp:1500 -- exact 1.00
    { 0x2929200, 0x289A200 },   // lois_ap.cpp:1098 -- global (references exactes)
    { 0x293FDE8, 0x289BB48 },   // lois_ap.cpp:48 -- global (references exactes)
    { 0x2A12D60, 0x2972420 },   // scooting.cpp:1803 -- global (references exactes)
    { 0x2A12D90, 0x2972450 },   // lois_ap.cpp:1098 -- global (references exactes)
    { 0x2A22E20, 0x29824E0 },   // groupes.cpp:1499 -- global (references exactes)
    { 0x2A25A30, 0x2982530 },   // quetes.cpp:155 -- global (references exactes)
    { 0x2A28D80, 0x2985988 },   // quetes.cpp:148 -- global (references exactes)
    { 0x2A2AFA4, 0x2987AA4 },   // scooting.cpp:1847 -- global (references exactes)
    { 0x2B2DE98, 0x2A8A998 },   // groupes.cpp:38 -- global (references exactes)
    { 0x2B2E710, 0x2A8B240 },   // groupes.cpp:61 -- global (references exactes)
    { 0x2BE0438, 0x2B3D018 },   // lois_ap.cpp:1090 -- global (references exactes)
    { 0x2BFD628, 0x2B5A1B8 },   // IconSubManager.cpp:651 -- global (references exactes)
    { 0x2C2FCA8, 0x2B8C808 },   // evenements.cpp:828 -- global (references exactes)
    { 0x2C30158, 0x2B8D3B8 },   // victoire.cpp:47 -- global (references exactes)
    { 0x2C30200, 0x2B8D460 },   // mod_common.h:150 -- global (references exactes)
    { 0x2C309E0, 0x2B8DC40 },   // mod_common.cpp:119 -- global (references exactes)
    { 0x2C30A20, 0x2B8DC80 },   // mod_common.cpp:118 -- global (references exactes)
    { 0x2C31B20, 0x2B8ED80 },   // mod_common.h:152 -- global (references exactes)
    { 0x2C31B48, 0x2B8EDA8 },   // scenario.cpp:302 -- global (references exactes)
    { 0x2C32A40, 0x2B8F800 },   // groupes.cpp:322 -- global (references exactes)
    { 0x2C34490, 0x2B91010 },   // lois.cpp:29 -- global (references exactes)
    { 0x2C38E90, 0x2B95A20 },   // victoire.cpp:48 -- global (references exactes)
    { 0x2C38FF0, 0x2B95B80 },   // groupes.cpp:323 -- global (references exactes)
    { 0x40615C0, 0x3FBE470 },   // MessageManager.cpp:434 -- global (references exactes)
    { 0x4062870, 0x3FBF720 },   // lois.cpp:70 -- global (references exactes)
    { 0x4062BA8, 0x3FBFA58 },   // IconSubManager.cpp:555 -- global (references exactes)
    { 0x4062D48, 0x3FBFBF8 },   // temperature.cpp:61 -- global (references exactes)
    { 0x4069880, 0x3FC6740 },   // evenements.cpp:1051 -- global (references exactes)
    { 0x4069970, 0x3FC6830 },   // evenements.cpp:47 -- global (references exactes)
    { 0x406AD18, 0x3FC7BD8 },   // groupes.cpp:1542 -- global (references exactes)
    { 0x406B4D0, 0x3FC8390 },   // temperature.cpp:59 -- global (references exactes)
    { 0x406DDA8, 0x3FCAC68 },   // evenements.cpp:1136 -- global (references exactes)
    { 0x4070460, 0x3FCD330 },   // lois.cpp:28 -- global (references exactes)
    { 0x4072078, 0x3FCEF48 },   // sites_contenu.cpp:63 -- global (references exactes)
    { 0x4072140, 0x3FCF010 },   // sites_contenu.cpp:64 -- global (references exactes)
    { 0x4074430, 0x3FD1300 },   // lois.cpp:1085 -- global (references exactes)
    { 0x4074548, 0x3FD1418 },   // IconSubManager.cpp:649 -- global (references exactes)
    { 0x4074550, 0x3FD1420 },   // IconSubManager.cpp:650 -- global (references exactes)
    { 0x407AE80, 0x3FD7D50 },   // quetes.cpp:40 -- global (references exactes)
    { 0x407BE20, 0x3FD8CF0 },   // recettes.cpp:34 -- global (references exactes)
    { 0x40806F8, 0x3FDD5C8 },   // sites_contenu.cpp:65 -- global (references exactes)
    { 0x4082EC0, 0x3FDFD90 },   // lois.cpp:57 -- global (references exactes)
    { 0x408B740, 0x3FE8628 },   // mod_common.cpp:327 -- global (references exactes)
    { 0x408E390, 0x3FEB280 },   // MessageManager.cpp:1093 -- global (references exactes)
    { 0x408EC18, 0x3FEBB08 },   // groupes.cpp:1497 -- global (references exactes)
    { 0x408F0E8, 0x3FEBFD8 },   // quetes.cpp:1380 -- global (references exactes)
    { 0x408FBD0, 0x3FECAC0 },   // lois.cpp:30 -- global (references exactes)
    { 0x40916D0, 0x3FEE5B8 },   // mod_common.cpp:119 -- global (references exactes)
    { 0x40935C8, 0x3FF04B8 },   // lois.cpp:1003 -- global (references exactes)
    { 0x4095A98, 0x3FF2978 },   // evenements.cpp:446 -- global (references exactes)
    { 0x409AC18, 0x3FF7AF8 },   // groupes.cpp:321 -- global (references exactes)
    { 0x40AF6F0, 0x400C630 },   // groupes.cpp:1498 -- global (references exactes)
    { 0x40B1B30, 0x40115C0 },   // MessageManager.cpp:278 -- global (references exactes)
};
const int N_STEAM = (int)(sizeof(VERSION_STEAM) / sizeof(VERSION_STEAM[0]));

// Signatures des exes (en-tete PE : TimeDateStamp, SizeOfImage).
constexpr DWORD EPIC_DATE  = 0x611CFD2D, EPIC_TAILLE  = 0x045A5000;
constexpr DWORD STEAM_DATE = 0x68E665D9, STEAM_TAILLE = 0x04508000;

DWORD g_date = 0, g_taille = 0;

VersionJeu Detecter()
{
    auto base = (const BYTE*)GetModuleHandleW(NULL);
    if (!base) return VersionJeu::Inconnue;
    auto dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return VersionJeu::Inconnue;
    auto nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return VersionJeu::Inconnue;
    g_date = nt->FileHeader.TimeDateStamp;
    g_taille = nt->OptionalHeader.SizeOfImage;
    if (g_date == EPIC_DATE && g_taille == EPIC_TAILLE)   return VersionJeu::Epic;
    if (g_date == STEAM_DATE && g_taille == STEAM_TAILLE) return VersionJeu::Steam;
    return VersionJeu::Inconnue;
}

// Adresses demandees mais absentes de la table (journal).
volatile LONG g_nManquantes = 0;
uintptr_t     g_manquantes[16] = {};

} // namespace

VersionJeu VersionDuJeu()
{
    static const VersionJeu v = Detecter();   // initialisation thread-safe (C++11)
    return v;
}

const char* NomVersionJeu()
{
    switch (VersionDuJeu()) {
    case VersionJeu::Epic:  return "Epic";
    case VersionJeu::Steam: return "Steam";
    default:                return "inconnue";
    }
}

bool VersionJeuSupportee() { return VersionDuJeu() != VersionJeu::Inconnue; }

uintptr_t Adr(uintptr_t rvaEpic)
{
    switch (VersionDuJeu()) {
    case VersionJeu::Epic:
        return rvaEpic;
    case VersionJeu::Steam: {
        int lo = 0, hi = N_STEAM - 1;
        while (lo <= hi) {
            int m = (lo + hi) / 2;
            if (VERSION_STEAM[m].epic == rvaEpic) return VERSION_STEAM[m].steam;
            if (VERSION_STEAM[m].epic < rvaEpic) lo = m + 1; else hi = m - 1;
        }
        LONG k = InterlockedIncrement(&g_nManquantes) - 1;
        if (k < 16) g_manquantes[k] = rvaEpic;
        return 0;
    }
    default:
        return 0;
    }
}

void JournalVersionJeu()
{
    printf("[VERSION] Frostpunk.exe : version %s (date 0x%08lX, taille 0x%08lX)\n",
        NomVersionJeu(), (unsigned long)g_date, (unsigned long)g_taille);
    if (!VersionJeuSupportee())
        printf("[VERSION] exe non reconnu : le mod reste INACTIF (aucun hook). "
               "Versions prises en charge : Epic (0x%08lX), Steam (0x%08lX).\n",
            (unsigned long)EPIC_DATE, (unsigned long)STEAM_DATE);
    LONG n = g_nManquantes;
    for (LONG i = 0; i < n && i < 16; ++i)
        printf("[VERSION] ADRESSE ABSENTE de la table %s : +0x%llX\n", NomVersionJeu(),
            (unsigned long long)g_manquantes[i]);
}

const wchar_t* FichierAtlasVersion(const wchar_t* fichierEpic)
{
    if (VersionDuJeu() == VersionJeu::Steam && wcscmp(fichierEpic, L"ap_buildings.gz") == 0)
        return L"ap_buildings_steam.gz";
    return fichierEpic;
}
