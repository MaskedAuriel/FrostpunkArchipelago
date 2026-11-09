// Releve du systeme de reflexion du moteur, pour decoder les scenes binaires.
//
// Structures (Ghidra, revues le 2026-11-09) :
//   Registre des classes (rtti.cpp) : global modBase + 0x2A12DC8 -> objet
//            {+0x00 tableau de pointeurs de classes, +0x08 nombre (int),
//            +0x0C capacite}, trie par nom (_stricmp) ; rempli par
//            FUN_140e37a60, recherche par nom FUN_140e377c0, obtenu par
//            FUN_140e378d0. (0x408B740 n'est PAS le registre : c'est le
//            global d'UNE classe, FrostpunkCurrentScenarioParametersCheck-
//            Expression -- chaque classe a son propre global.)
//   Classe (vtable PropertyManager, 0xC0 octets, FUN_140e4d790) :
//            +0x20 derniere propriete ajoutee, +0x28 versionnee (int,
//            != 0 -> marqueur 0xDEADD00D + version dans le flux),
//            +0x30 proprietes dans l'ordre d'enregistrement (nombre +0x38),
//            +0x48 proprietes triees par nom (nombre +0x50), +0x60 nom,
//            +0x68 fabrique, +0x78 identifiant, +0x80 descripteur RTTI,
//            +0x88 taille, +0x90 alignement, +0x98 classe parente,
//            +0xA0 ancetres (nombre +0xA8) : [0] = la classe elle-meme,
//            puis les parents jusqu'a la racine ; +0xBC drapeaux (u16 ;
//            0x400 = non recursif, 0x200 = non serialisable).
//   Propriete (FUN_140e40e40) : vtable = type (RTTI), +0x08 nom,
//            +0x10 description, +0x18 drapeaux, +0x20 (4e argument du
//            constructeur), +0x28 decalage (u32), +0x2C version minimale
//            (u32), vtable+0x30 = identifiant de type (2 = categorie,
//            0x3A = ignore), vtable+0x88 = lecture "solide".
//   Filtre de serialisation (FUN_140e48960) : ni categorie ni 0x3A,
//            (drapeaux & 0x6000) != 0x6000, != 0x4000 (sauf mode bit 7),
//            et pas (mode bit 0 et drapeaux & 4).
//   Serialisation "solide" (FUN_140e3e440 / FUN_140e3edc0) : ancetres du
//            dernier (racine) au premier (classe finale), proprietes dans
//            l'ordre de +0x30, sans nom ni taille ; une classe versionnee
//            sans marqueur est lue en version 1.

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "mod_common.h"
#include "schema_reflexion.h"

// Release (AP_RELEASE) : le releve n'est pas appele (version.cpp) ; chemin
// relatif pour ne pas embarquer le dossier du projet dans la DLL publiee.
#ifdef AP_RELEASE
static const char* SCHEMA_FICHIER = "mods\\logs\\schema_reflexion.tsv";
#else
static const char* SCHEMA_FICHIER =
    "C:\\Users\\Simon\\Desktop\\Visual\\Frost\\DLL\\logs\\schema_reflexion.tsv";
#endif

static uintptr_t g_base = 0;

// Types de proprietes deja decrits par une ligne V (une par vtable).
static uintptr_t g_vtVues[2048];
static int g_nVtVues = 0;

static bool LireChaine(uintptr_t p, char* out, size_t taille)
{
    out[0] = 0;
    if (p <= 0x10000) return false;
    // Lecture octet par octet : une chaine peut finir pres d'une page non
    // lisible.
    size_t i = 0;
    for (; i + 1 < taille; ++i) {
        char c = 0;
        if (!SafeRead((void*)(p + i), &c, 1)) break;
        if (c == 0) break;
        if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E) { i = 0; break; }
        out[i] = c;
    }
    out[i] = 0;
    return out[0] != 0;
}

static bool EstClasse(uintptr_t p)
{
    if (p <= 0x10000) return false;
    char c[128] = { 0 };
    return GetClassNameFP(p, c, sizeof(c)) && strcmp(c, "PropertyManager") == 0;
}

static void NomClasse(uintptr_t cl, char* out, size_t taille)
{
    out[0] = 0;
    uintptr_t pn = 0;
    if (cl > 0x10000 && SafeRead((void*)(cl + 0x60), &pn, 8)) LireChaine(pn, out, taille);
}

static uintptr_t Rva(uintptr_t a) { return (a > g_base) ? a - g_base : 0; }

// Appel protege de vtable+0x30 (identifiant de type de la propriete).
static int TypeIdPropriete(uintptr_t prop)
{
    int id = -1;
    __try {
        uintptr_t vt = *(uintptr_t*)prop;
        typedef int (*TypeId_t)(uintptr_t);
        TypeId_t f = *(TypeId_t*)(vt + 0x30);
        id = f(prop);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { id = -2; }
    return id;
}

// Premiere rencontre d'un type de propriete : une ligne V avec la vtable,
// la lecture solide (vtable+0x88) et les 0x80 premiers octets de l'objet,
// pour retrouver ensuite dans Ghidra le format de chaque type.
static void DecrireType(FILE* f, uintptr_t prop, const char* type, int typeId)
{
    uintptr_t vt = 0, lecture = 0;
    if (!SafeRead((void*)prop, &vt, 8)) return;
    for (int i = 0; i < g_nVtVues; ++i) if (g_vtVues[i] == vt) return;
    if (g_nVtVues < (int)(sizeof(g_vtVues) / sizeof(g_vtVues[0]))) g_vtVues[g_nVtVues++] = vt;
    SafeRead((void*)(vt + 0x88), &lecture, 8);
    fprintf(f, "V\t%s\t%d\t0x%llX\t0x%llX\t", type, typeId,
        (unsigned long long)Rva(vt), (unsigned long long)Rva(lecture));
    for (int off = 0; off < 0x80; off += 8) {
        uint64_t q = 0;
        SafeRead((void*)(prop + off), &q, 8);
        fprintf(f, "%s%llX", off ? " " : "", (unsigned long long)q);
    }
    fprintf(f, "\n");
}

// Une propriete sur une ligne ; ses sous-proprietes (element d'un tableau,
// cle / valeur d'une table) sur les lignes suivantes, avec profondeur + 1.
static void EcrirePropriete(FILE* f, const char* classe, int index, uintptr_t prop, int profondeur)
{
    char type[256] = { 0 }, nom[160] = { 0 }, nomCl[160] = { 0 };
    GetClassNameFP(prop, type, sizeof(type));
    uintptr_t pNom = 0, vt = 0;
    uint64_t drapeaux = 0;
    uint32_t decalage = 0, versionMin = 0;
    SafeRead((void*)prop, &vt, 8);
    SafeRead((void*)(prop + 0x08), &pNom, 8);
    SafeRead((void*)(prop + 0x18), &drapeaux, 8);
    SafeRead((void*)(prop + 0x28), &decalage, 4);
    SafeRead((void*)(prop + 0x2C), &versionMin, 4);
    LireChaine(pNom, nom, sizeof(nom));
    int typeId = TypeIdPropriete(prop);

    // Classe referencee (structure, objet, element) : premier pointeur vers
    // une classe de la reflexion entre +0x20 et +0x80.
    int offCl = -1;
    for (int off = 0x20; off < 0x80; off += 8) {
        if (off == 0x28) continue;   // decalage + version
        uintptr_t v = 0;
        if (!SafeRead((void*)(prop + off), &v, 8)) break;
        if (EstClasse(v)) { NomClasse(v, nomCl, sizeof(nomCl)); offCl = off; break; }
    }

    fprintf(f, "P\t%s\t%d\t%d\t%s\t%s\t%d\t0x%llX\t0x%X\t%u\t%s\t0x%X\t0x%llX\n",
        classe, index, profondeur, nom[0] ? nom : "-", type[0] ? type : "?",
        typeId, (unsigned long long)drapeaux, decalage, versionMin,
        nomCl[0] ? nomCl : "-", offCl < 0 ? 0 : offCl, (unsigned long long)Rva(vt));
    DecrireType(f, prop, type[0] ? type : "?", typeId);

    if (profondeur >= 3) return;
    // Sous-proprietes : tout objet a vtable dont la classe RTTI contient
    // "Property", range dans la propriete (inline ou pointe) entre +0x30 et +0x100.
    for (int off = 0x30; off < 0x100; off += 8) {
        uintptr_t v = 0;
        if (!SafeRead((void*)(prop + off), &v, 8)) break;
        char c[256] = { 0 };
        // Inline : la vtable est a prop+off.
        if (GetClassNameFP(prop + off, c, sizeof(c)) && strstr(c, "Property") && !strstr(c, "PropertyManager")) {
            fprintf(f, "S\t%s\t%d\t%d\tinline+0x%X\n", classe, index, profondeur + 1, off);
            EcrirePropriete(f, classe, index, prop + off, profondeur + 1);
            continue;
        }
        c[0] = 0;
        if (v > 0x10000 && v != prop && GetClassNameFP(v, c, sizeof(c)) && strstr(c, "Property")
            && !strstr(c, "PropertyManager")) {
            fprintf(f, "S\t%s\t%d\t%d\tpointe+0x%X\n", classe, index, profondeur + 1, off);
            EcrirePropriete(f, classe, index, v, profondeur + 1);
        }
    }
}

static void EcrireClasse(FILE* f, uintptr_t cl, int i)
{
    char nom[192] = { 0 };
    NomClasse(cl, nom, sizeof(nom));
    if (!nom[0]) { fprintf(f, "# classe %d illisible (0x%llX)\n", i, (unsigned long long)cl); return; }

    int32_t versionnee = 0, nHier = 0, nProps = 0;
    uint64_t taille = 0, alignement = 0;
    uint16_t dr = 0;
    uintptr_t parent = 0, hier = 0, props = 0;
    SafeRead((void*)(cl + 0x28), &versionnee, 4);
    SafeRead((void*)(cl + 0x30), &props, 8);
    SafeRead((void*)(cl + 0x38), &nProps, 4);
    SafeRead((void*)(cl + 0x88), &taille, 8);
    SafeRead((void*)(cl + 0x90), &alignement, 8);
    SafeRead((void*)(cl + 0x98), &parent, 8);
    SafeRead((void*)(cl + 0xA0), &hier, 8);
    SafeRead((void*)(cl + 0xA8), &nHier, 4);
    SafeRead((void*)(cl + 0xBC), &dr, 2);

    char nomParent[192] = { 0 };
    NomClasse(parent, nomParent, sizeof(nomParent));

    fprintf(f, "C\t%s\t%d\t0x%llX\t%llu\t%d\t0x%X\t%s\t%d\t", nom, i, (unsigned long long)taille,
        (unsigned long long)alignement, versionnee, dr, nomParent[0] ? nomParent : "-", nProps);
    // Hierarchie dans l'ordre de serialisation : de la fin du tableau vers 0.
    if (hier > 0x10000 && nHier > 0 && nHier < 64) {
        for (int h = nHier - 1; h >= 0; --h) {
            uintptr_t c = 0; char n[192] = { 0 };
            SafeRead((void*)(hier + (uintptr_t)h * 8), &c, 8);
            NomClasse(c, n, sizeof(n));
            fprintf(f, "%s%s", n[0] ? n : "?", h ? ">" : "");
        }
    }
    fprintf(f, "\n");

    if (props > 0x10000 && nProps > 0 && nProps < 4096) {
        for (int p = 0; p < nProps; ++p) {
            uintptr_t prop = 0;
            if (!SafeRead((void*)(props + (uintptr_t)p * 8), &prop, 8) || prop <= 0x10000) continue;
            EcrirePropriete(f, nom, p, prop, 0);
        }
    }
}

static int EcrireSchema(uintptr_t tableau, int n)
{
    FILE* f = nullptr;
    if (fopen_s(&f, SCHEMA_FICHIER, "w") != 0 || !f) {
        printf("[SCHEMA] ecriture impossible : %s\n", SCHEMA_FICHIER);
        return -1;
    }
    fprintf(f, "# C classe index taille alignement versionnee drapeaux parent nb_props hierarchie(racine>fin)\n");
    fprintf(f, "# P classe index_prop profondeur nom type_rtti type_id drapeaux decalage version_min classe_ref off_classe_ref vtable_rva\n");
    fprintf(f, "# S classe index_prop profondeur emplacement (la ligne P suivante est la sous-propriete)\n");
    fprintf(f, "# V type_rtti type_id vtable_rva lecture_solide_rva octets(0..0x80, par 8)\n");

    g_nVtVues = 0;
    int ecrites = 0;
    for (int i = 0; i < n; ++i) {
        uintptr_t cl = 0;
        if (!SafeRead((void*)(tableau + (uintptr_t)i * 8), &cl, 8) || cl <= 0x10000) continue;
        EcrireClasse(f, cl, i);
        ecrites++;
    }
    fclose(f);
    return ecrites;
}

void SchemaReflexionTick()
{
    static bool fait = false;
    static DWORD debut = 0;
    if (fait) return;
    if (!debut) debut = GetTickCount();
    // Le registre se remplit au demarrage (initialiseurs statiques et
    // chargements) : on attend 20 s par prudence.
    if (GetTickCount() - debut < 20000) return;

    g_base = (uintptr_t)GetModuleHandleW(NULL);
    uintptr_t rva = Adr(0x2A12DC8);   // registre des classes (Epic seulement pour l'instant)
    if (!rva) { fait = true; printf("[SCHEMA] registre des classes non porte sur cette version\n"); return; }
    uintptr_t registre = 0, tableau = 0;
    int32_t n = 0;
    if (!SafeRead((void*)(g_base + rva), &registre, 8) || registre <= 0x10000) return;
    if (!SafeRead((void*)registre, &tableau, 8) || !SafeRead((void*)(registre + 8), &n, 4)) return;
    if (tableau <= 0x10000 || n <= 0 || n > 100000) return;

    fait = true;
    DWORD t0 = GetTickCount();
    int ecrites = EcrireSchema(tableau, n);
    printf("[SCHEMA] %d classe(s) sur %d ecrite(s) dans logs\\schema_reflexion.tsv (%lu ms)\n",
        ecrites, n, GetTickCount() - t0);
}
