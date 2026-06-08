# Ezzayra — Télégestion d'irrigation par SMS/GSM

> Stage d'été — Informatique embarquée  
> **Entreprise :** EZZAYRA SOLUTIONS, Tunis  
> **Étudiant :** Amel Ben Zbir  
> **Durée :** 6 à 8 semaines

---

## Contexte

EZZAYRA SOLUTIONS est une entreprise tunisienne spécialisée dans la digitalisation des chaînes de valeur agricoles. Sa plateforme **AgriManager** couvre la gestion parcellaire, le suivi satellitaire et le pilotage de l'irrigation pour plus de 60 000 hectares en Tunisie, Côte d'Ivoire et Espagne.

Sur le terrain, de nombreuses exploitations sont éloignées, sans couverture Internet fiable, mais couvertes par le réseau **GSM 2G**. Ce projet vise à concevoir un système embarqué autonome permettant de commander à distance, par simple SMS, les équipements d'irrigation d'une exploitation agricole.

---

## Objectif du projet

Concevoir, développer et tester un prototype fonctionnel de **carte de commande à distance** comprenant :

- Un microcontrôleur **Arduino Mega** (prototype V1)
- Une carte de **relais opto-isolée 4 canaux** pilotant 2 à 4 électrovannes (12/24V) et une motopompe
- Un **modem GSM SIM800L** pour la réception et l'envoi de SMS
- Un **firmware robuste** implémentant un protocole de commandes SMS sécurisé avec alertes automatiques

---

## Architecture du système

```
Agriculteur (SMS)
      │
      ▼
Réseau GSM 2G
      │
      ▼
Modem SIM800L ──(UART)──► Arduino Mega
                                │
                           GPIO (5V logique)
                                │
                     Carte relais opto-isolée
                      /        |        \
                 Vanne 1   Vanne 2   Motopompe
                 (12/24V)  (12/24V)   (220V via contacteur)
```

---

## Protocole de commandes SMS

| Commande SMS | Action | Réponse |
|---|---|---|
| `#OUVRIR Vx` | Ouvrir l'électrovanne n°x | `Vanne x ouverte [horodatage]` |
| `#FERMER Vx` | Fermer l'électrovanne n°x | `Vanne x fermée [horodatage]` |
| `#POMPE ON` | Démarrer la motopompe | `Pompe démarrée` |
| `#POMPE OFF` | Arrêter la motopompe | `Pompe arrêtée` |
| `#ETAT` | État du système | État vannes + pompe + signal GSM |
| `#PROG Vx hh:mm durée` | Programmation horaire | Confirmation |
| `#STOP` | Arrêt d'urgence général | `Arrêt d'urgence exécuté` |

---

## Fonctionnalités de sécurité

- **Liste blanche** : seuls les numéros autorisés peuvent commander le système
- **Comportement par défaut sécurisé** : au redémarrage, toutes les sorties sont mises à l'état fermé/arrêt
- **Watchdog** logiciel : reset automatique si le système se bloque
- **Journalisation** : toute commande non autorisée est ignorée et enregistrée
- **Alertes automatiques** : coupure d'alimentation, défaut pompe, durée maximale dépassée

---

## Stack technique

| Composant | Technologie |
|---|---|
| Microcontrôleur | Arduino Mega (C/C++) |
| Communication GSM | SIM800L — commandes AT |
| Simulation hardware | [Wokwi.com](https://wokwi.com) |
| Simulateur modem | Python 3 + pyserial |
| Schémas électriques | KiCad / EasyEDA |
| Versioning | Git / GitHub |

---

## Structure du dépôt

```
ezzayra-irrigation-gsm/
├── firmware/          ← Code source C++ Arduino
│   ├── main/
│   │   └── main.ino
│   └── lib/
│       ├── gsm/       ← Module communication SIM800L
│       ├── relay/     ← Module pilotage relais
│       ├── sms_parser/← Module parsing commandes SMS
│       └── watchdog/  ← Module watchdog
├── simulation/        ← Simulateur Python du modem GSM
│   ├── sim_modem.py
│   └── test_commands.py
├── schemas/           ← Schémas électriques KiCad/EasyEDA
│   ├── schematic.kicad_sch
│   └── BOM.csv
├── docs/              ← Documentation
│   ├── rapport_stage.pdf
│   ├── manuel_utilisateur.md
│   └── etat_art.md
└── README.md
```

---

## Planning du stage

| Phase | Contenu | Durée |
|---|---|---|
| Phase 1 | Étude bibliographique, commandes AT, état de l'art | Semaine 1 |
| Phase 2 | Conception schéma électrique, protocole SMS, BOM | Semaines 2-3 |
| Phase 3 | Développement firmware C++ + simulateur Python | Semaines 3-5 |
| Phase 4 | Tests robustesse et validation | Semaine 6 |
| Phase 5 | Rapport, documentation, soutenance | Semaine 7 |

---

## Livrables attendus

- [ ] Cahier des charges et étude comparative des plateformes
- [ ] Schéma électrique complet + BOM avec coûts
- [ ] Code source C++ commenté et versionné
- [ ] Simulateur Python du modem GSM
- [ ] Rapport de tests (commandes, robustesse, sécurité)
- [ ] Manuel d'installation destiné à l'agriculteur
- [ ] Rapport de stage + démonstration vidéo

---

## Lancer la simulation

### Prérequis
```bash
pip install pyserial
```

### Démarrer le simulateur modem
```bash
cd simulation
python sim_modem.py
```

### Tester le firmware sur Wokwi
1. Ouvrir [wokwi.com](https://wokwi.com) → New Project → Arduino Mega
2. Copier le contenu de `firmware/main/main.ino`
3. Ouvrir le Serial Monitor et envoyer une commande SMS simulée

---

## Encadrement

- **Entreprise :** EZZAYRA SOLUTIONS — [www.ezzayra.com](https://www.ezzayra.com)
- **Encadrant technique :** M. Yasser BOUOUD (CEO Ezzayra)
- **Contact :** yasser.bououd@ezzayra.com

---

*Stage réalisé dans le cadre de l'écosystème AgriManager — EZZAYRA SOLUTIONS, Tunis, 2025*
