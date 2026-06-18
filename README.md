# Ezzayra — Télégestion d'irrigation par SMS/GSM

> Stage d'ingénieur — Informatique Industrielle et Automatique (3e année, INSAT)
> **Entreprise :** EZZAYRA SOLUTIONS, Tunis
> **Étudiante :** Amal Ben Zbir
> **Durée :** 7 semaines — 2025-2026

---

## Contexte

EZZAYRA SOLUTIONS est une entreprise tunisienne spécialisée dans la digitalisation des chaînes de valeur agricoles. Sa plateforme **AgriManager** couvre la gestion parcellaire, le suivi satellitaire et le pilotage de l'irrigation pour plus de 60 000 hectares en Tunisie, Côte d'Ivoire et Espagne.

Sur le terrain, de nombreuses exploitations sont éloignées, sans couverture Internet fiable, mais couvertes par le réseau **GSM 2G**. Ce projet vise à concevoir un système embarqué autonome permettant de commander à distance, par simple SMS, les équipements d'irrigation d'une exploitation agricole.

---

## Objectif du projet

Concevoir, développer et tester un prototype fonctionnel de **carte de commande à distance** comprenant :

- Un microcontrôleur **Arduino Mega 2560** (prototype V1)
- Un module relais (4 canaux pour les électrovannes + 1 canal dédié à la motopompe)
- Un **modem GSM SIM800L** pour la réception et l'envoi de SMS
- Un afficheur **LCD 16x2** pour le diagnostic local sur site
- Un **firmware robuste** implémentant un protocole de commandes SMS sécurisé, avec programmation horaire automatique et alertes

---

## Architecture du système

```
Agriculteur (SMS)
      │
      ▼
Réseau GSM 2G
      │
      ▼
Modem SIM800L ──(UART)──► Arduino Mega 2560 ──► LCD 16x2 (diagnostic local)
                                │
                           GPIO (5V logique)
                                │
                      Modules relais (4 + 1 canaux)
                      /        |        \         \
                 Vanne 1   Vanne 2..4   Motopompe
                 (12/24V)   (12/24V)    (220V via contacteur)
```

---

## Protocole de commandes SMS

| Commande SMS | Action | Réponse |
|---|---|---|
| `#OUVRIR Vx` | Ouvrir l'électrovanne n°x (1 à 4) | `Vanne x ouverte - hh:mm:ss` |
| `#FERMER Vx` | Fermer l'électrovanne n°x | `Vanne x fermee - hh:mm:ss` |
| `#POMPE ON` | Démarrer la motopompe | `Pompe demarree - hh:mm:ss` |
| `#POMPE OFF` | Arrêter la motopompe | `Pompe arretee - hh:mm:ss` |
| `#PROG Vx hh:mm durée` | Planifier l'ouverture automatique d'une vanne | Confirmation + déclenchement/fermeture automatiques à l'heure prévue |
| `#PROG Vx OFF` | Annuler une programmation existante | `Programmation Vx annulee` |
| `#ETAT` | Rapport d'état complet | Vannes, pompe, signal GSM, programmations actives, horodatage |
| `#STOP` puis `#STOP CONFIRM` | Arrêt d'urgence à double confirmation (60s) | `ARRET URGENCE execute` |
| `#AIDE` | Liste des commandes disponibles | Liste complète |

---

## Fonctionnalités de sécurité

- **Liste blanche** : seuls les numéros pré-enregistrés peuvent commander le système ; toute autre tentative est ignorée et journalisée
- **État sûr au démarrage** : au redémarrage, toutes les sorties sont mises à l'état fermé
- **Watchdog logiciel** : réinitialisation automatique en cas de blocage supérieur à 30 secondes
- **Arrêt d'urgence à double confirmation** : la commande `#STOP` exige une confirmation explicite dans les 60 secondes
- **Protection pompe** : arrêt automatique après 1 heure de fonctionnement continu, avec alerte
- **Alerte signal GSM faible** : vérification périodique (10 min) du niveau de signal réseau
- **Isolation galvanique** : relais opto-isolés séparant la logique de commande des circuits de puissance

---

## Stack technique

| Composant | Technologie |
|---|---|
| Microcontrôleur | Arduino Mega 2560 (C/C++) |
| Communication GSM | SIM800L — commandes AT (`AT+CMGF`, `AT+CCLK?`, `AT+CSQ`...) |
| Affichage local | LCD 16x2 (bus 4 bits) |
| Simulation matérielle | Proteus 8 Professional |
| Schémas électriques | KiCad |
| Versioning | Git / GitHub |

---

## Structure du dépôt

```
ezzayra-irrigation-gsm/
├── firmware/
│   └── firmware_proteus.ino   ← Firmware complet (FSM, #PROG, LCD, sécurité)
│                                  Bascule production/démo via la constante
│                                  MODE_DEMO en tête de fichier :
│                                    - false → mode réel (Serial1 + modem SIM800L)
│                                    - true  → mode démonstration (lecture clavier
│                                              directe sur Serial + horloge simulée
│                                              accélérée, utilisé pour la vidéo)
├── simulation/
│   ├── simulation_ezzayra.pdsprj   ← Projet de simulation Proteus complet
│   └── firmware_proteus.ino.hex    ← Firmware compilé chargé dans la simulation
├── schemas/                    ← Schémas électriques KiCad
├── docs/
│   ├── Cahier_des_charges_v2.pdf
│   └── BOM_Ezzayra.pdf
└── README.md
```

---

## Mode de démonstration (simulation Proteus)

Le firmware intègre un **mode démonstration** activable par une simple constante (`MODE_DEMO`), pensé pour permettre une démonstration fluide et fiable du système sans dépendre d'un pont logiciel externe entre un modem GSM simulé et Proteus :

- Les commandes sont saisies directement dans le moniteur série Arduino (`Serial`), comme si elles étaient déjà des SMS décodés (ex. taper `#OUVRIR V1` directement)
- Une **horloge simulée accélérée** (1 minute simulée toutes les 3 secondes réelles) permet de démontrer la programmation horaire automatique (`#PROG`) sans attendre une vraie journée
- La commande `#HEURE hh:mm` permet de forcer l'horloge simulée à une valeur précise, utile pour caler une démonstration filmée

Cette approche a été retenue après plusieurs tentatives de pont série entre un simulateur externe et Proteus (COMPIM, ports série virtuels, automatisation de saisie), qui n'ont pas abouti à une solution suffisamment fiable pour une démonstration filmée.

---

## Planning du stage

| Phase | Contenu | Durée |
|---|---|---|
| P1 | Étude bibliographique, commandes AT, état de l'art | Semaine 1 |
| P2 | Conception schéma électrique, protocole SMS, BOM | Semaines 2-3 |
| P3 | Développement firmware C++ (FSM, relais, watchdog, #PROG) | Semaines 3-5 |
| P4 | Simulation Proteus, tests unitaires et de robustesse | Semaine 6 |
| P5 | Rapport, documentation, démonstration vidéo | Semaine 7 |

---

## Livrables

- [x] Cahier des charges et étude comparative des plateformes
- [x] Schéma électrique complet (KiCad) + BOM avec coûts
- [x] Firmware C++ commenté et versionné (commandes complètes + `#PROG` fonctionnel)
- [x] Simulation Proteus complète (relais, LCD, mode démonstration)
- [ ] Rapport de tests formalisé (scénario validé, mise en fichier en cours)
- [ ] Manuel d'installation destiné à l'agriculteur
- [ ] Rapport de stage complet
- [ ] Vidéo de démonstration

---

## Tester la simulation

### Prérequis
- Proteus 8 Professional
- Bibliothèque de modules relais Arduino installée (LIB + MODELS)

### Lancer la simulation
1. Ouvrir `simulation/simulation_ezzayra.pdsprj` dans Proteus
2. Lancer la simulation (▶)
3. Ouvrir le Virtual Terminal connecté sur Serial0
4. Taper directement une commande, par exemple `#OUVRIR V1`, puis Entrée

### Tester la programmation horaire automatique
```
#HEURE 06:25
#PROG V1 06:30 2
```
Puis observer le LCD : la vanne s'ouvre automatiquement à l'heure simulée programmée, et se referme après la durée prévue, sans intervention supplémentaire.

---

## Encadrement

- **Entreprise :** EZZAYRA SOLUTIONS — Tunis
- **Encadrant technique :** M. Yasser Bououd (CEO)
- **Encadrant académique :** INSAT — Informatique Industrielle et Automatique

---

*Stage réalisé dans le cadre de l'écosystème AgriManager — EZZAYRA SOLUTIONS, Tunis, 2025-2026*
