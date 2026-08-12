// known_techs.h  --  Table des technologies de Frostpunk
// ---------------------------------------------------------------------------
// Cle stable = hash 64 bits (a techno+0x20, invariant entre sessions/parties).
// name         = nom interne du jeu (chaine a desc+0x00) -- stable, unique.
// displayName  = nom lisible que TU choisis pour l'afficher ("???" a remplir).
//
// index n'est PAS stable entre parties (l'ordre du tableau peut varier) : il
// n'est la que pour reference/tri. L'identification se fait sur le HASH.
// ---------------------------------------------------------------------------

#pragma once
#include <cstdint>
#include <cstring>

struct TechInfo {    
    uint64_t    hash;         // identifiant STABLE -- cle de recherche
    const char* name; 
    int progressiveTree;
    int progressiveTreePosition;
};

struct TechGroup { const TechInfo* items; int count; const char* label; };


static const TechInfo KNOWN_TECHS_MAIN_HEATING[] = {
{ 0xA0E0D2727D0B336AULL, "SteamHub", 0, 0},
{ 0x8ACF1A3F6CD1A7FBULL, "Steamhub Upgrade - range", 0, 1},
{ 0x8169FDE4BB9BB141ULL, "Steamhub Upgrade - less coal", 0, 2},
{ 0xA55C1E23258CB056ULL, "Generator Upgrade - power 1", 1, 0},
{ 0xBDF0CF1D41987C65ULL, "Generator Upgrade - power 2", 1, 1},
{ 0xB8DD400DADCDEF8EULL, "Generator Upgrade - power 3", 1, 2 },
{ 0xAAE37A9B8A8B25F1ULL, "Generator Upgrade - less coal", 2, 0},
{ 0xB6C37AB9E60FF207ULL, "Generator Upgrade - less coal 2", 2, 1},
{ 0xA156A81B7E067BF5ULL, "Generator Upgrade - range 1", 3, 0},
{ 0x94BF1A6EF2603AE0ULL, "Generator Upgrade - range 2", 3, 1},
{ 0xB800B652FE01D119ULL, "Generator Upgrade - range 3", 3, 2},
{ 0x8F0A051638529900ULL, "Generator Overdrive 1", 4, 0},
{ 0x91D2CE4C68CE4109ULL, "Generator Upgrade - less stress", 4, 1},
{ 0x85BCE42D5F9E4F3BULL, "Boilers", 5, 0},
{ 0xB2366AD179BD4D26ULL, "Boiler Upgrade", 5, 1},
{ 0xBA712FF32CF9CAAEULL, "Boiler Upgrade 2", 5,2},
{ 0x8095FB143BBF82CCULL, "Boiler Upgrade - less coal", 6, 0},
};
static const TechInfo KNOWN_TECHS_WINTERHOME_HEATING[] = {
{ 0xA0E0D2727D0B336AULL, "SteamHub", 0, 0},
{ 0x8ACF1A3F6CD1A7FBULL, "Steamhub Upgrade - range", 0, 1},
{ 0x8169FDE4BB9BB141ULL, "Steamhub Upgrade - less coal", 0, 2},
{ 0xA55C1E23258CB056ULL, "Generator Upgrade - power 1", 1, 0},
{ 0xBDF0CF1D41987C65ULL, "Generator Upgrade - power 2", 1, 1},
{ 0xB8DD400DADCDEF8EULL, "Generator Upgrade - power 3", 1, 2 },
{ 0xAAE37A9B8A8B25F1ULL, "Generator Upgrade - less coal", 2, 0},
{ 0xB6C37AB9E60FF207ULL, "Generator Upgrade - less coal 2", 2, 1},
{ 0xA156A81B7E067BF5ULL, "Generator Upgrade - range 1", 3, 0},
{ 0x94BF1A6EF2603AE0ULL, "Generator Upgrade - range 2", 3, 1},
{ 0xB800B652FE01D119ULL, "Generator Upgrade - range 3", 3, 2},
{ 0x8F0A051638529900ULL, "Generator Overdrive 1", 4, 0},
{ 0x91D2CE4C68CE4109ULL, "Generator Upgrade - less stress", 4, 1},
{ 0x85BCE42D5F9E4F3BULL, "Boilers", 5, 0},
{ 0xB2366AD179BD4D26ULL, "Boiler Upgrade", 5, 1},
{ 0xBA712FF32CF9CAAEULL, "Boiler Upgrade 2", 5,2},
{ 0x8095FB143BBF82CCULL, "Boiler Upgrade - less coal", 6, 0},
{ 0xA24E361D87891912ULL, "E RepairStation",                      37, 0 },
{ 0x9004FB1D2E95369FULL, "E RC Efficiency 1",                    37, 1 },
{ 0x9BAE7B748D5B7820ULL, "E RC Efficiency 2",                    37, 2 },
{ 0x9A306E92F391690AULL, "E RC Efficiency 3",                    37, 3 },
{ 0xA54820F7892B9652ULL, "E RC Ability",                         38, 0 },
{ 0x8A15192C335D2FECULL, "E RC Ability 2",                       38, 1 },
{ 0xBC7C654BF3D6E484ULL, "E RC Less workers",                    39, 0 },
};
static const TechInfo KNOWN_TECHS_EDGE_HEATING[] = {
{ 0x85BCE42D5F9E4F3BULL, "Boilers", 5, 0},
{ 0xB2366AD179BD4D26ULL, "Boiler Upgrade", 5, 1},
{ 0xBA712FF32CF9CAAEULL, "Boiler Upgrade 2", 5,2},
{ 0x8095FB143BBF82CCULL, "Boiler Upgrade - less coal", 6, 0},
{ 0xAC4103808CBCB5B8ULL, "HC Brazier",                           70, 0 },
{ 0x87BA3B1685C7F1D1ULL, "HC BrazierPower",                      70, 1 },
{ 0xB6F142D34017C5E2ULL, "HC BrazierRange",                      71, 0 },
{ 0xB81384D36C8EEABFULL, "HC BrazierCoalOpt1",                   72, 0 },
{ 0xB4E312D3864D6BE2ULL, "HC BrazierCoalOpt2",                   72, 1 },
};

static const TechInfo KNOWN_TECHS_MAIN_EXPLORATION[] = {
{ 0xBD91C0E32EE8A6B2ULL, "Beacon",                               7, 0 },
{ 0xA16CF8DC50B2727AULL, "ExpeditionCenter",                     8, 0 },
{ 0xB7D0B9057487D146ULL, "ExpeditionUpgrade",                    8, 1 },
{ 0xBFB9EC0F1190EBC8ULL, "ScoutsLimit",                          9, 0 },
{ 0x903BF385083D378FULL, "ScoutsLimit2",                         9, 1 },
{ 0x980DA1A7BA055FADULL, "ScoutsSpeed",                          10, 0 },
{ 0xAFDDF739B5C4D799ULL, "ScoutsSpeed 2",                        10, 1 },
{ 0xA3E95C4CEB6C2B7FULL, "Factory",                              11, 0 },
{ 0xAD20BB9845A5DDC5ULL, "Cheaper Automatons",                   11, 1 },
{ 0x973B35A4E76361EEULL, "Automatons no Steamcores",             11, 2 },
{ 0x981F59F3146CF4C6ULL, "Faster Augmentation Parts production", 12, 0 },
{ 0x8AFE2FDE0DF97D69ULL, "Cheaper Augmentation Parts",           12, 1 },
{ 0x9E16C1962C1325C6ULL, "Automaton Efficiency",                 13, 0 },
{ 0xA181975459F6C673ULL, "Automaton Efficiency 2",               13, 1 },
{ 0xB0131D084E15541AULL, "Automatons Efficiency 3",              13, 2 },
{ 0xB12F86943A6036E4ULL, "Automaton Medical",                    14, 0 },
{ 0x85C386E583A32865ULL, "Automatons Medical 2",                 14, 1 },
{ 0xA01F9F7DBAB21917ULL, "Automaton Engineers",                  79, 0 },
};
static const TechInfo KNOWN_TECHS_WINTERHOME_EXPLORATION[] = {
{ 0xBD91C0E32EE8A6B2ULL, "Beacon",                               7, 0 },
{ 0xA16CF8DC50B2727AULL, "ExpeditionCenter",                     8, 0 },
{ 0xAF702239C79A412BULL, "ExpeditionUpgradeWinterhome",          8, 1 },
{ 0xBFB9EC0F1190EBC8ULL, "ScoutsLimit",                          9, 0 },
{ 0x903BF385083D378FULL, "ScoutsLimit2",                         9, 1 },
{ 0x980DA1A7BA055FADULL, "ScoutsSpeed",                          10, 0 },
{ 0xAFDDF739B5C4D799ULL, "ScoutsSpeed 2",                        10, 1 },
{ 0xA3E95C4CEB6C2B7FULL, "Factory",                              11, 0 },
{ 0xAD20BB9845A5DDC5ULL, "Cheaper Automatons",                   11, 1 },
{ 0x973B35A4E76361EEULL, "Automatons no Steamcores",             11, 2 },
{ 0x981F59F3146CF4C6ULL, "Faster Augmentation Parts production", 12, 0 },
{ 0x8AFE2FDE0DF97D69ULL, "Cheaper Augmentation Parts",           12, 1 },
{ 0x9E16C1962C1325C6ULL, "Automaton Efficiency",                 13, 0 },
{ 0xA181975459F6C673ULL, "Automaton Efficiency 2",               13, 1 },
{ 0xB0131D084E15541AULL, "Automatons Efficiency 3",              13, 2 },
{ 0xB12F86943A6036E4ULL, "Automaton Medical",                    14, 0 },
{ 0x85C386E583A32865ULL, "Automatons Medical 2",                 14, 1 },
{ 0xA01F9F7DBAB21917ULL, "Automaton Engineers",                  15, 0 },
};
static const TechInfo KNOWN_TECHS_ARKS_EXPLORATION[] = {
{ 0xBD91C0E32EE8A6B2ULL, "Beacon",                               7, 0 },
{ 0x9843404FB6931C76ULL, "AutomatonScouts_TechHeaven",           7, 1 },
{ 0xBFB9EC0F1190EBC8ULL, "ScoutsLimit",                          9, 0 },
{ 0x903BF385083D378FULL, "ScoutsLimit2",                         9, 1 },
{ 0x980DA1A7BA055FADULL, "ScoutsSpeed",                          10, 0 },
{ 0xAFDDF739B5C4D799ULL, "ScoutsSpeed 2",                        10, 1 },
{ 0xA3E95C4CEB6C2B7FULL, "Factory",                              11, 0 },
{ 0xAD20BB9845A5DDC5ULL, "Cheaper Automatons",                   11, 1 },
{ 0x973B35A4E76361EEULL, "Automatons no Steamcores",             11, 2 },
{ 0x981F59F3146CF4C6ULL, "Faster Augmentation Parts production", 12, 0 },
{ 0x8AFE2FDE0DF97D69ULL, "Cheaper Augmentation Parts",           12, 1 },
{ 0x9E16C1962C1325C6ULL, "Automaton Efficiency",                 13, 0 },
{ 0xA181975459F6C673ULL, "Automaton Efficiency 2",               13, 1 },
{ 0xB0131D084E15541AULL, "Automatons Efficiency 3",              13, 2 },
{ 0xB12F86943A6036E4ULL, "Automaton Medical",                    14, 0 },
{ 0x85C386E583A32865ULL, "Automatons Medical 2",                 14, 1 },
{ 0xA01F9F7DBAB21917ULL, "Automaton Engineers",                  15, 0 },
};
static const TechInfo KNOWN_TECHS_EDGE_EXPLORATION[] = {
{ 0xBD91C0E32EE8A6B2ULL, "Beacon",                               7, 0 },
{ 0xBFB9EC0F1190EBC8ULL, "ScoutsLimit",                          9, 0 },
{ 0x903BF385083D378FULL, "ScoutsLimit2",                         9, 1 },
{ 0x980DA1A7BA055FADULL, "ScoutsSpeed",                          10, 0 },
{ 0xAFDDF739B5C4D799ULL, "ScoutsSpeed 2",                        10, 1 },
{ 0xA3E95C4CEB6C2B7FULL, "Factory",                              11, 0 },
{ 0x981F59F3146CF4C6ULL, "Faster Augmentation Parts production", 12, 0 },
{ 0x8AFE2FDE0DF97D69ULL, "Cheaper Augmentation Parts",           12, 1 },
};

static const TechInfo KNOWN_TECHS_MAIN_RESSOURCES[] = {
{ 0x8BC47F368138DA92ULL, "Gathering Upgrade",                    15, 0 },
{ 0xB69B51DB31F4B001ULL, "Gathering Insulation",                 15, 1 },
{ 0x851032DABCC703AEULL, "Gathering Insulation 2",               15, 2 },
{ 0xB300360E7D7A3295ULL, "ResourceDepotUpgrade",                 16, 0 },
{ 0x96031EA6A430BF85ULL, "Resource Depot Big",                   16, 1 },
{ 0xBF4935A464F03A66ULL, "Gathering upgrade 2",                  17, 0 },
{ 0x86A71762D9705649ULL, "CoalThumper",                          18, 0 },
{ 0xB5B909AE4C1E54F6ULL, "SteamCoalThumper",                     18, 1 },
{ 0x86CCBA956FEBC7DEULL, "Coal Thumper Upgrade",                 18, 2 },
{ 0xA3796F28B9AC2A23ULL, "Coal Mine",                            19, 0 },
{ 0xB82524B0D19D88D5ULL, "Adv Coal Mine",                        19, 1 },
{ 0xAB63782F194E8021ULL, "Steam Coal Mine",                      19, 2 },
{ 0xA5A0DE87E8514928ULL, "Coal mine upgrade 1",                  20, 0 },
{ 0x960C27264EA17CF9ULL, "Coal Mine upgrade 2",                  20, 1 },
{ 0xBEC33A80C6853BA3ULL, "CharcoalKiln",                         21, 0 },
{ 0xBBD8663CFA3F3D7DULL, "Kiln upgrade",                         21, 1 },
{ 0x87221793ACD9ED8AULL, "Kiln upgrade 2",                       21, 2 },
{ 0xA9F39DF2AEEB21B1ULL, "Sawmill",                              22, 0 },
{ 0xAC019A23402AB883ULL, "Steam Sawmill",                        22, 1 },
{ 0x9D7F762601622CA6ULL, "Sawmill Upgrade",                      23, 0 },
{ 0xBC770E739218DAB4ULL, "Sawmill Upgrade - range",              23, 1 },
{ 0xB3C7DF784DC97A83ULL, "WallDrill",                            24, 0 },
{ 0x9DA3ABAE1FA72621ULL, "Adv Wall Drill",                       24, 1 },
{ 0x88698E693179DD2DULL, "Steam WallDrill",                      24, 2 },
{ 0x87856A059CE65971ULL, "Steel Mine",                           25, 0 },
{ 0x884E0D684DAAB02DULL, "Adv Steel Mine",                       25, 1 },
{ 0xA9AC3601AE5EA23CULL, "Steam Steel Mine",                     25, 2 },
};
static const TechInfo KNOWN_TECHS_EDGE_RESSOURCES[] = {
{ 0x8BC47F368138DA92ULL, "Gathering Upgrade",                    15, 0 },
{ 0xB69B51DB31F4B001ULL, "Gathering Insulation",                 15, 1 },
{ 0x851032DABCC703AEULL, "Gathering Insulation 2",               15, 2 },
{ 0xB300360E7D7A3295ULL, "ResourceDepotUpgrade",                 16, 0 },
{ 0x96031EA6A430BF85ULL, "Resource Depot Big",                   16, 1 },
{ 0xBF4935A464F03A66ULL, "Gathering upgrade 2",                  17, 0 },
{ 0x86A71762D9705649ULL, "CoalThumper",                          18, 0 },
{ 0xB5B909AE4C1E54F6ULL, "SteamCoalThumper",                     18, 1 },
{ 0x86CCBA956FEBC7DEULL, "Coal Thumper Upgrade",                 18, 2 },
{ 0xA3796F28B9AC2A23ULL, "Coal Mine",                            19, 0 },
{ 0xB82524B0D19D88D5ULL, "Adv Coal Mine",                        19, 1 },
{ 0xAB63782F194E8021ULL, "Steam Coal Mine",                      19, 2 },
{ 0xA5A0DE87E8514928ULL, "Coal mine upgrade 1",                  20, 0 },
{ 0x960C27264EA17CF9ULL, "Coal Mine upgrade 2",                  20, 1 },
{ 0xBEC33A80C6853BA3ULL, "CharcoalKiln",                         21, 0 },
{ 0xBBD8663CFA3F3D7DULL, "Kiln upgrade",                         21, 1 },
{ 0x87221793ACD9ED8AULL, "Kiln upgrade 2",                       21, 2 },
{ 0xA9F39DF2AEEB21B1ULL, "Sawmill",                              22, 0 },
{ 0xAC019A23402AB883ULL, "Steam Sawmill",                        22, 1 },
{ 0x9D7F762601622CA6ULL, "Sawmill Upgrade",                      23, 0 },
{ 0xBC770E739218DAB4ULL, "Sawmill Upgrade - range",              23, 1 },
{ 0xB3C7DF784DC97A83ULL, "WallDrill",                            24, 0 },
{ 0x9DA3ABAE1FA72621ULL, "Adv Wall Drill",                       24, 1 },
{ 0x88698E693179DD2DULL, "Steam WallDrill",                      24, 2 },
{ 0xBAD9632D2482EB30ULL, "R VaultEfficiencyBoost",               73, 0 },
{ 0xA0F1BE0F4175B30FULL, "R VaultUpgrade1",                      74, 0 },
{ 0xBF5C4DDAD1CAFE3BULL, "R VaultUpgrade2",                      74, 1 },
{ 0x876F9433DD7C12DAULL, "R VaultUpgrade3",                      74, 2 },
{ 0xA6B7DCE3504E6494ULL, "R VaultMultitasking",                  75, 0 },
{ 0x98356E3CE54E08ADULL, "R VaultInsulation",                    76, 0 },
{ 0xA0606FEF83CB14C8ULL, "R VaultSteamCoresUpgrade",             77, 0 },
{ 0x95E69640FB2830D4ULL, "E Transport Depot",                    78, 0 },
};

static const TechInfo KNOWN_TECHS_MAIN_FOOD[] = {
{ 0xBF7153CF70731641ULL, "Plantation",                           26, 0 },
{ 0xB49A95770CB83298ULL, "Steam Plantation",                     26, 1 },
{ 0xA92780AFF69180BFULL, "Plantation upgrade",                   26, 2 },
{ 0x95DCED5FD8466387ULL, "Plantation Insulation",                27, 0 },
{ 0x94FCD02E48F30228ULL, "Plantation Insulation 2",              27, 1 },
{ 0xBD90D178518E872CULL, "HuntersUpgrade",                       28, 0 },
{ 0x90F460E6380DFD7AULL, "Flying Hunters",                       28, 1 },
{ 0x93B19C3058E896DCULL, "FlyingHuntersUpgrade",                 28, 2 },
{ 0x9781BAE37ABD9B94ULL, "Hunters Upgrade - less workers",       29, 0 },
{ 0xAEBE6A8DC9857616ULL, "MedPost Upgrade",                      30, 0 },
{ 0xAE0D5029C4FAFA6AULL, "Steam healthcare",                     30, 1 },
{ 0xA8E5A152E5F619C4ULL, "Infirmary Upgrade",                    30, 2 },
{ 0x9EB2A329D39949C0ULL, "HealthCare Buildings - warmer",        31, 0 },
{ 0x881F9801EF69831FULL, "Healthcare Buildings 2 - warmer",      31, 1 },
{ 0xB5C1BFCACE2F518AULL, "Infirmary Upgrade - less workers",     32, 0 },
{ 0x9479CC7A027A3F61ULL, "Bunkhouse",                            33, 0 },
{ 0x8226B4149A4C642AULL, "House",                                33, 1 },
{ 0xA751C3903745ABD5ULL, "House Insulation",                     33, 2 },
{ 0x9E93A93F1C0C2C51ULL, "Cantinee - warmer",                    34, 0 },
{ 0xBFB89F7354D04AE2ULL, "House Upgrade - cheaper",              35, 0 },
};
static const TechInfo KNOWN_TECHS_EDGE_FOOD[] = {
{ 0xAEBE6A8DC9857616ULL, "MedPost Upgrade",                      30, 0 },
{ 0xAE0D5029C4FAFA6AULL, "Steam healthcare",                     30, 1 },
{ 0xA8E5A152E5F619C4ULL, "Infirmary Upgrade",                    30, 2 },
{ 0x9EB2A329D39949C0ULL, "HealthCare Buildings - warmer",        31, 0 },
{ 0x881F9801EF69831FULL, "Healthcare Buildings 2 - warmer",      31, 1 },
{ 0xB5C1BFCACE2F518AULL, "Infirmary Upgrade - less workers",     32, 0 },
{ 0x9479CC7A027A3F61ULL, "Bunkhouse",                            33, 0 },
{ 0x8226B4149A4C642AULL, "House",                                33, 1 },
{ 0xA751C3903745ABD5ULL, "House Insulation",                     33, 2 },
{ 0x9E93A93F1C0C2C51ULL, "Cantinee - warmer",                    34, 0 },
{ 0xBFB89F7354D04AE2ULL, "House Upgrade - cheaper",              35, 0 },
};
static const TechInfo KNOWN_TECHS_MAIN_TIER[] = {
{ 0xBE5B5F3661B74AAEULL, "TierUnlock1",                          36, 0 },
{ 0x99F67FD0200252EFULL, "TierUnlock2",                          36, 1 },
{ 0x9D18A6629DC54E67ULL, "TierUnlock3",                          36, 2 },
{ 0x9F5C9E5513755596ULL, "TierUnlock4",                          36, 3 },
{ 0x8D5D8F90ED24296EULL, "TierUnlock5",                          36, 4 },
};

static const TechInfo KNOWN_TECHS_ALL_AUTUMN[] = {
{ 0x9A3EB5C5736C5CF8ULL, "HC Brazier",                           40, 0 },
{ 0x8BA6E8ADEAA56A13ULL, "HC BrazierPower1",                     40, 1 },
{ 0xBE2A8E7B932D9C2DULL, "HC BrazierRange1",                     41, 0 },
{ 0x84FFDAF28948F16BULL, "HC BrazierCoalOpt1",                   42, 0 },
{ 0xA21A49B6CC78F2C0ULL, "HC BrazierCoalOpt2",                   42, 1 },
{ 0x8F3095A50ABE5B8CULL, "HH Heater",                            43, 0 },
{ 0x98973939A041BFB3ULL, "HH Heater Upgrade",                    43, 1 },
{ 0x95884318394691F1ULL, "HH Heater Upgrade - less coal",        43, 2 },
{ 0xAD9A3CCFA5A5E216ULL, "HI HealthCare Insulation1",            44, 0 },
{ 0x89E11B6D8D3EBB34ULL, "HI Healthcare Insulation2",            44, 1 },
{ 0xB64F631E2C554692ULL, "GS ScaffoldingManufacture",            45, 0 },
{ 0xA73DE9D28A567C7DULL, "GS ManufactureCheaperMaterials",       45, 1 },
{ 0xA585F6C76A51C68DULL, "GS Manufacture safety improve",        45, 2 },
{ 0xA3744892FFB106B8ULL, "GS ProtectiveStructure1",              46, 0 },
{ 0x9B1DFC4166BDF1F2ULL, "GS ProtectiveStructure2",              46, 1 },
{ 0x869CB004219D9B9FULL, "GM SteelPlateForge",                   47, 0 },
{ 0xB340712E1AA67558ULL, "GM Forge safety improve",              47, 1 },
{ 0xBB76809681658ED6ULL, "GM ForgeCheaperMaterials",             48, 0 },
{ 0xA6AB1E204FB27849ULL, "GG SteamMill",                         49, 0 },
{ 0x8626EF58D76A77AAULL, "GG Steam Mill safety improve",         49, 1 },
{ 0xB52481405FA109E4ULL, "GG SteamMillCheaperMaterials",         49, 2 },
{ 0x907F270A35547E8EULL, "GG Supercharge1",                      50, 0 },
{ 0xA3DF3639D1906909ULL, "GG Supercharge2",                      50, 1 },
{ 0xB7DB8BC2F8AA8D18ULL, "GF Air Scrubbers1",                    51, 0 },
{ 0xAB7A0E6FE556795CULL, "GF Air Scrubbers2",                    51, 1 },
{ 0x91D3615B9CA9BD25ULL, "GF Air Scrubbers3",                    51, 2 },
{ 0x9E446E15CA7F3829ULL, "GF Air ScrubbersOptimalization",       52, 0 },
{ 0xBF2E2AB5620FFB4CULL, "GF Air ScrubbersOptimalization2",      52, 1 },
{ 0xB1E32EC5BA936484ULL, "GU GeneratorUpgrade1",                 53, 0 },
{ 0x8F91A5E56C1F7808ULL, "GU GeneratorUpgrade2",                 53, 1 },
{ 0x8E6360937ECF4770ULL, "GU GeneratorUpgrade3",                 53, 2 },
{ 0x84CE92C6D1B6B9A6ULL, "FF Foragers",                          54, 0 },
{ 0xB2EBD63D29F02BAAULL, "FF Foragers_+1",                       55, 0 },
{ 0xA5C87129AA78EB4CULL, "FF Foragers_+2",                       55, 1 },
{ 0xBB86A865575F0898ULL, "FF ForagersSpeedUpgrade",              56, 0 },
{ 0xBC30C632069AB76BULL, "FF ForagersSpeedUpgrade2",             56, 1 },
{ 0x9926B4EED59BBBBDULL, "FH Fishing Harbor",                    57, 0 },
{ 0xB3BCC2FDE230AD82ULL, "FH Fishing Harbor2",                   57, 1 },
{ 0x8F721C114E1F5EA1ULL, "FH Fishing Harbor_efficiencyBoost",    58, 0 },
{ 0xB25899C67C3E13C7ULL, "FM MedPost Upgrade",                   59, 0 },
{ 0x9BEFF8545E4A0C48ULL, "FM Steam healthcare",                  59, 1 },
{ 0xA6C2C6A453C1036FULL, "FM Infirmary Upgrade - less workers",  59, 2 },
{ 0x92BD99FA0B8CF5FDULL, "FM Infirmary Upgrade",                 60, 0 },
{ 0x8BA655C00C38E5DBULL, "RD Docksv1",                           61, 0 },
{ 0xBC42749C23D732DEULL, "RD Docksv2",                           61, 1 },
{ 0xA45EC7F7C4042BA7ULL, "RD Docksv3",                           61, 2 },
{ 0xAB3378C1C0F63FDFULL, "RU URS",                               62, 0 },
{ 0x9DA9438616748A1BULL, "RU URS_Steam",                         63, 0 },
{ 0xA189AA1BA1E45B98ULL, "RU URS_Faster gathering",              63, 1 },
{ 0x82F3957C59928F95ULL, "RU URS_Advanced",                      63, 2 },
{ 0xA4A3B3D1583AE97DULL, "RD Docks faster reload",               64, 0 },
{ 0x8EDC34ADC0A71BD9ULL, "RG Gathering Upgrade",                 65, 0 },
{ 0xA383E8E076BDAC46ULL, "RG Gathering upgrade 2",               65, 1 },
{ 0x9CA531E645AA9138ULL, "RG ResourceDepotUpgrade",              66, 0 },
{ 0x9767F8931DCF733EULL, "RG Resource Depot Big",                66, 1 },
{ 0x9B8662D8557103EFULL, "RW Sawmill",                           67, 0 },
{ 0x853470E14031EC9DULL, "RW Steam Sawmill",                     67, 1 },
{ 0xB3E26DFFB50E015EULL, "RW Sawmill Upgrade",                   68, 0 },
{ 0x9222D9DF836D090AULL, "RW Sawmill Upgrade - range",           68, 1 },
{ 0x9D93FA7554513F8DULL, "RC CharcoalKiln",                      69, 0 },
{ 0x9A9531B0F90E6D29ULL, "RC Kiln upgrade",                      69, 1 },
{ 0x9A8EA035E1F6BD7DULL, "RC Kiln upgrade 2",                    69, 2 },
};



#define TECH_GROUP(arr, lbl) { arr, (int)(sizeof(arr) / sizeof((arr)[0])), lbl }

static const TechGroup KNOWN_TECH_GROUPS[] = {
    TECH_GROUP(KNOWN_TECHS_MAIN_HEATING,          "Main/Heater"),
    TECH_GROUP(KNOWN_TECHS_WINTERHOME_HEATING,    "Winterhome/Heater"),
    TECH_GROUP(KNOWN_TECHS_EDGE_HEATING,          "Edge/Heater"),
    TECH_GROUP(KNOWN_TECHS_MAIN_EXPLORATION,      "Main/Exploration"),
    TECH_GROUP(KNOWN_TECHS_WINTERHOME_EXPLORATION,"Winterhome/Exploration"),
    TECH_GROUP(KNOWN_TECHS_ARKS_EXPLORATION,      "Arks/Exploration"),
    TECH_GROUP(KNOWN_TECHS_EDGE_EXPLORATION,      "Edge/Exploration"),
    TECH_GROUP(KNOWN_TECHS_MAIN_RESSOURCES,       "Main/Ressources"),
    TECH_GROUP(KNOWN_TECHS_EDGE_RESSOURCES,       "Edge/Ressources"),
    TECH_GROUP(KNOWN_TECHS_MAIN_FOOD,             "Main/Food"),
    TECH_GROUP(KNOWN_TECHS_EDGE_FOOD,             "Edge/Food"),
    TECH_GROUP(KNOWN_TECHS_MAIN_TIER,             "Main/Tier"),
    TECH_GROUP(KNOWN_TECHS_ALL_AUTUMN,            "Builders"),
};

static const int KNOWN_TECH_GROUP_COUNT =
(int)(sizeof(KNOWN_TECH_GROUPS) / sizeof(KNOWN_TECH_GROUPS[0]));


static int KnownTechsTotal()
{
    int n = 0;
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) n += KNOWN_TECH_GROUPS[g].count;
    return n;
}

// Recherche par hash dans TOUS les groupes (c'etait le bug : seul le groupe
// TIER etait parcouru, soit 5 technos sur 172).
static const TechInfo* TechByHash(uint64_t hash)
{
    for (int g = 0; g < KNOWN_TECH_GROUP_COUNT; ++g) {
        const TechGroup& grp = KNOWN_TECH_GROUPS[g];
        for (int i = 0; i < grp.count; ++i)
            if (grp.items[i].hash == hash) return &grp.items[i];
    }
    return nullptr;
}