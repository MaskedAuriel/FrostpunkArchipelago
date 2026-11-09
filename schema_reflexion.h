#pragma once
// Releve du systeme de reflexion du moteur (PropertyManager) : pour chaque
// classe, ses proprietes DANS L'ORDRE de serialisation, avec leur type.
// Sert a decoder les scenes binaires hors du jeu (outils\scenes.py).

// Appele chaque seconde par la boucle principale ; ecrit une seule fois
// logs\schema_reflexion.tsv quand le PropertyManager est rempli.
void SchemaReflexionTick();
