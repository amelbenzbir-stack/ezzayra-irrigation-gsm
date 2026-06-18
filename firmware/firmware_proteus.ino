#include <Arduino.h>
#include <LiquidCrystal.h>
#define MODE_DEMO false

// ─────────────────────────────────────────
//  CONFIGURATION LCD 16x2
//  RS=22, E=23, D4=24, D5=25, D6=26, D7=27
// ─────────────────────────────────────────
LiquidCrystal lcd(22, 23, 24, 25, 26, 27);

// ─────────────────────────────────────────
//  MODEM GSM — Serial1 (TX1=18, RX1=19)
//  Connecté au COMPIM dans Proteus (mode reel uniquement)
// ─────────────────────────────────────────
#define gsm Serial1

// ─────────────────────────────────────────
//  RELAIS — pins adaptées pour Proteus
// ─────────────────────────────────────────
#define RELAY_V1    28
#define RELAY_V2    29
#define RELAY_V3    37
#define RELAY_V4    36
#define RELAY_PUMP  35

#define LED_STATUS  13

// ─────────────────────────────────────────
//  LISTE BLANCHE
// ─────────────────────────────────────────
const int NB_AUTORISES = 3;
String numerosAutorises[NB_AUTORISES] = {
  "+21620123456",
  "+21655987654",
  "+21698111222"
};

// ─────────────────────────────────────────
//  VARIABLES GLOBALES
// ─────────────────────────────────────────
bool stopEnAttente            = false;
unsigned long heureStop       = 0;
int nbTentativesNonAutorisees = 0;
unsigned long totalSecondsPompe = 0;
bool etatVanne[5]  = {false, false, false, false, false};
bool etatPompe     = false;
unsigned long heureDemarragePompe = 0;
unsigned long dernierHeartbeat    = 0;
unsigned long dernierCheckSignal  = 0;
unsigned long dernierCheckProg    = 0;

const unsigned long TIMEOUT_WATCHDOG  = 30000;
const unsigned long DUREE_MAX_POMPE   = 3600000;
const unsigned long INTERVALLE_SIGNAL = 600000;
const unsigned long INTERVALLE_PROG   = 60000;   // mode reel : 60s reelles

// ─────────────────────────────────────────
//  HORLOGE SIMULEE ACCELEREE (MODE_DEMO uniquement)
//  Permet de demontrer #PROG sans attendre une vraie journee.
//  1 minute simulee s'ecoule toutes les VITESSE_DEMO_MS ms reelles.
//  Avec 3000ms -> 1 heure simulee s'ecoule en 3 minutes reelles.
// ─────────────────────────────────────────
const unsigned long VITESSE_DEMO_MS   = 3000;  // ajuster pour filmer plus/moins vite
const unsigned long INTERVALLE_PROG_DEMO = 1000; // verification chaque seconde reelle en demo

int demoHeure   = 6;   // heure simulee de depart (modifiable via #HEURE)
int demoMinute  = 0;
int demoJour    = 1;
unsigned long dernierTickDemo = 0;

// ─────────────────────────────────────────
//  PROGRAMMATION HORAIRE (#PROG)
// ─────────────────────────────────────────
struct Programmation {
  bool active;
  int heure;
  int minute;
  int dureeMin;
  bool enCours;
  unsigned long heureOuverture;
  int dernierJourDeclenche;
};
Programmation prog[5];

// ─────────────────────────────────────────
//  MACHINE À ÉTATS
// ─────────────────────────────────────────
enum Etat { VEILLE, SMS_RECU, VERIFICATION, EXECUTION, REPONSE, ALERTE };
Etat etatActuel = VEILLE;

String numeroExpediteur = "";
String contenuSMS       = "";
String smsBrut          = "";

// ─────────────────────────────────────────
//  DÉCLARATIONS FONCTIONS
// ─────────────────────────────────────────
void toutFermer();
void initialiserModem();
void verifierWatchdog();
void cligneterLED();
void verifierDureePompe();
void verifierSignalGSM();
void verifierProgrammations();
void avancerHorlogeDemo();
void extraireSMS(String smsComplet);
bool numeroAutorise(String numero);
void executerCommande(String cmd);
void journaliser(String numero, String cmd);
void ouvrirVanne(int num);
void fermerVanne(int num);
void demarrerPompe();
void arreterPompe();
void envoyerSMS(String numero, String message);
void envoyerEtat();
void envoyerAlerte(String message);
String envoyerAT(String commande, int attente);
String lireReponseGSM();
String lireHeure();
bool lireHeureActuelle(int &heure, int &minute, int &jour);
void afficherLCD(String ligne1, String ligne2);
void mettreAJourLCD();

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  Serial.println("=== EZZAYRA Irrigation GSM - Demarrage ===");

  if (!MODE_DEMO) {
    Serial1.begin(9600);
  }

  pinMode(RELAY_V1,   OUTPUT);
  pinMode(RELAY_V2,   OUTPUT);
  pinMode(RELAY_V3,   OUTPUT);
  pinMode(RELAY_V4,   OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EZZAYRA SOLUTIONS");
  lcd.setCursor(0, 1);
  lcd.print("Demarrage...");
  delay(2000);

  toutFermer();
  Serial.println("Securite: toutes les sorties fermees");

  for (int i = 1; i <= 4; i++) {
    prog[i].active = false;
    prog[i].enCours = false;
    prog[i].dernierJourDeclenche = -1;
  }

  afficherLCD("Systeme pret", "Attente SMS...");

  if (!MODE_DEMO) {
    delay(3000);
    initialiserModem();
  } else {
    Serial.println("[DEMO] Mode demonstration actif.");
    Serial.println("[DEMO] Tapez votre commande dans la barre ci-dessus (Ex: #OUVRIR V1)");
    Serial.println("[DEMO] Horloge simulee : 1 min simulee toutes les " +
      String(VITESSE_DEMO_MS) + "ms reelles. Heure de depart : 06:00");
    Serial.println("[DEMO] Utilisez #HEURE hh:mm pour forcer l'heure simulee.");
    numeroExpediteur = "+21620123456"; // Simule que c'est vous l'expediteur autorise
    dernierTickDemo = millis();
  }
  Serial.println("Systeme pret. En attente de commandes SMS...");
  dernierHeartbeat    = millis();
  dernierCheckSignal  = millis();
  dernierCheckProg    = millis();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  if (!MODE_DEMO) {
    verifierWatchdog();
    verifierSignalGSM();
  } else {
    avancerHorlogeDemo();
  }

  // verifierProgrammations() tourne dans les 2 modes : en mode reel
  // elle interroge AT+CCLK?, en mode demo elle lit l'horloge simulee.
  verifierProgrammations();

  cligneterLED();
  verifierDureePompe();

  // --- INTERCEPTION DE LA SAISIE CLAVIER (MONITEUR SERIE) EN MODE DEMO ---
  if (MODE_DEMO && Serial.available() > 0) {
    String saisieClavier = Serial.readStringUntil('\n');
    saisieClavier.trim();
    saisieClavier.toUpperCase();

    if (saisieClavier.length() > 0) {
      Serial.println("\n--- [SMS SIMULE RECU] ---");
      Serial.println("Contenu tape : " + saisieClavier);

      // Commande speciale demo : forcer l'heure simulee
      // Format : #HEURE hh:mm  (ex: #HEURE 06:29)
      if (saisieClavier.startsWith("#HEURE ")) {
        String hhmm = saisieClavier.substring(7);
        int posDeuxPt = hhmm.indexOf(':');
        if (posDeuxPt > 0) {
          demoHeure  = hhmm.substring(0, posDeuxPt).toInt();
          demoMinute = hhmm.substring(posDeuxPt + 1).toInt();
          Serial.println("[DEMO] Horloge simulee forcee a " +
            String(demoHeure) + ":" + String(demoMinute));
        }
      } else {
        contenuSMS = saisieClavier;
        afficherLCD("SMS recu:", contenuSMS.substring(0, 16));
        etatActuel = EXECUTION;
      }
    }
  }

  // --- LOGIQUE REELLE DE RECEPTION VIA MODEM GSM ---
  if (!MODE_DEMO && gsm.available()) {
    smsBrut = lireReponseGSM();
    if (smsBrut.indexOf("+CMT:") >= 0) {
      Serial.println("SMS recu !");
      Serial.println(smsBrut);
      etatActuel = SMS_RECU;
    }
  }

  switch (etatActuel) {
    case VEILLE:
      break;

    case SMS_RECU:
      extraireSMS(smsBrut);
      Serial.println("Expediteur : " + numeroExpediteur);
      Serial.println("Contenu    : " + contenuSMS);
      afficherLCD("SMS recu:", contenuSMS.substring(0, 16));
      etatActuel = VERIFICATION;
      break;

    case VERIFICATION:
      if (numeroAutorise(numeroExpediteur)) {
        Serial.println("Numero autorise -> execution");
        etatActuel = EXECUTION;
      } else {
        journaliser(numeroExpediteur, contenuSMS);
        afficherLCD("ACCES REFUSE", numeroExpediteur.substring(0, 16));
        etatActuel = VEILLE;
      }
      break;

    case EXECUTION:
      executerCommande(contenuSMS);
      mettreAJourLCD();
      etatActuel = MODE_DEMO ? VEILLE : REPONSE;
      break;

    case REPONSE:
      envoyerAT("AT+CMGD=1,4", 1000);
      etatActuel = VEILLE;
      break;

    case ALERTE:
      etatActuel = VEILLE;
      break;
  }

  dernierHeartbeat = millis();
}

// ============================================================
//  HORLOGE SIMULEE — avance 1 minute toutes les VITESSE_DEMO_MS
// ============================================================
void avancerHorlogeDemo() {
  if (millis() - dernierTickDemo >= VITESSE_DEMO_MS) {
    dernierTickDemo = millis();
    demoMinute++;
    if (demoMinute >= 60) {
      demoMinute = 0;
      demoHeure++;
      if (demoHeure >= 24) {
        demoHeure = 0;
        demoJour++;
      }
    }
  }
}

// ============================================================
//  LCD — AFFICHAGE
// ============================================================
void afficherLCD(String ligne1, String ligne2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ligne1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(ligne2.substring(0, 16));
}

void mettreAJourLCD() {
  // Laisse le message d'action visible 2s avant de revenir a l'etat general
  // (utile pour la vid eo de demonstration)
  delay(2000);
  String ligne1 = "V1:";
  ligne1 += etatVanne[1] ? "O " : "F ";
  ligne1 += "V2:";
  ligne1 += etatVanne[2] ? "O " : "F ";
  ligne1 += "V3:";
  ligne1 += etatVanne[3] ? "O " : "F ";
  ligne1 += "V4:";
  ligne1 += etatVanne[4] ? "O" : "F";

  String ligne2;
  if (MODE_DEMO) {
    // En demo, affiche l'heure simulee a la place de l'etat pompe seul
    ligne2 = "POMPE:" + String(etatPompe ? "ON " : "OFF") + " ";
    if (demoHeure < 10) ligne2 += "0";
    ligne2 += String(demoHeure) + ":";
    if (demoMinute < 10) ligne2 += "0";
    ligne2 += String(demoMinute);
  } else {
    ligne2 = "POMPE:";
    ligne2 += etatPompe ? "ON  " : "OFF ";
    ligne2 += etatPompe ? ">>>" : "---";
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(ligne1);
  lcd.setCursor(0, 1);
  lcd.print(ligne2);
}

// ============================================================
//  INITIALISATION DU MODEM GSM (mode reel uniquement)
// ============================================================
void initialiserModem() {
  Serial.println("Initialisation du modem...");
  afficherLCD("Init modem...", "Attente GSM...");

  int essais = 0;
  while (essais < 10) {
    String rep = envoyerAT("AT", 1000);
    if (rep.indexOf("OK") >= 0) {
      Serial.println("Modem repond : OK");
      afficherLCD("Modem OK", "GSM connecte");
      break;
    }
    essais++;
    delay(1000);
  }

  envoyerAT("AT+CMGF=1", 1000);
  Serial.println("Mode texte active");

  envoyerAT("AT+CSCS=\"GSM\"", 1000);
  envoyerAT("AT+CNMI=2,2,0,0,0", 1000);
  Serial.println("Notifications SMS activees");

  envoyerAT("AT+CMGD=1,4", 2000);
  Serial.println("Anciens SMS supprimes");

  String creg = envoyerAT("AT+CREG?", 1000);
  Serial.println("Reseau: " + creg);

  envoyerSMS(numerosAutorises[0],
    "Systeme irrigation demarre - Pret a recevoir commandes");

  afficherLCD("Systeme pret", "Attente SMS...");
}

// ============================================================
//  COMMANDES AT (mode reel uniquement)
// ============================================================
String envoyerAT(String commande, int attente) {
  if (MODE_DEMO) return "OK";
  gsm.println(commande);
  delay(attente);
  String reponse = "";
  while (gsm.available()) {
    reponse += (char)gsm.read();
  }
  Serial.println("AT >> " + commande);
  Serial.println("AT << " + reponse);
  return reponse;
}

String lireReponseGSM() {
  String reponse = "";
  unsigned long debut = millis();
  while (millis() - debut < 2000) {
    while (gsm.available()) {
      reponse += (char)gsm.read();
    }
  }
  return reponse;
}

// ============================================================
//  EXTRAIRE SMS (mode reel uniquement)
// ============================================================
void extraireSMS(String smsComplet) {
  int posCMT = smsComplet.indexOf("+CMT:");
  int debut  = smsComplet.indexOf("\"", posCMT) + 1;
  int fin    = smsComplet.indexOf("\"", debut);
  if (debut > 6 && fin > debut) {
    numeroExpediteur = smsComplet.substring(debut, fin);
  }

  int rechercheDebutContenu = (fin > 0) ? fin : posCMT;
  int posLF = smsComplet.indexOf('\n', rechercheDebutContenu);
  int posCR = smsComplet.indexOf('\r', rechercheDebutContenu);

  int posFinEntete = -1;
  if (posLF >= 0 && posCR >= 0) {
    posFinEntete = min(posLF, posCR);
  } else if (posLF >= 0) {
    posFinEntete = posLF;
  } else if (posCR >= 0) {
    posFinEntete = posCR;
  }

  if (posFinEntete >= 0) {
    contenuSMS = smsComplet.substring(posFinEntete + 1);
  } else {
    contenuSMS = "";
  }

  contenuSMS.replace("\r", "");
  contenuSMS.replace("\n", "");
  contenuSMS.trim();
  contenuSMS.toUpperCase();
}

// ============================================================
//  LISTE BLANCHE
// ============================================================
bool numeroAutorise(String numero) {
  for (int i = 0; i < NB_AUTORISES; i++) {
    if (numero == numerosAutorises[i]) return true;
  }
  return false;
}

// ============================================================
//  EXÉCUTER COMMANDE
// ============================================================
void executerCommande(String cmd) {
  Serial.println("Execution: " + cmd);

  if (cmd.startsWith("#OUVRIR V")) {
    int numVanne = cmd.charAt(9) - '0';
    if (numVanne >= 1 && numVanne <= 4) {
      ouvrirVanne(numVanne);
      String heure = lireHeure();
      envoyerSMS(numeroExpediteur,
        "Vanne " + String(numVanne) + " ouverte - " + heure);
    } else {
      envoyerSMS(numeroExpediteur, "ERREUR: vanne invalide (1-4)");
    }
  }

  else if (cmd.startsWith("#FERMER V")) {
    int numVanne = cmd.charAt(9) - '0';
    if (numVanne >= 1 && numVanne <= 4) {
      fermerVanne(numVanne);
      String heure = lireHeure();
      envoyerSMS(numeroExpediteur,
        "Vanne " + String(numVanne) + " fermee - " + heure);
    } else {
      envoyerSMS(numeroExpediteur, "ERREUR: vanne invalide (1-4)");
    }
  }

  else if (cmd == "#POMPE ON") {
    demarrerPompe();
    envoyerSMS(numeroExpediteur, "Pompe demarree - " + lireHeure());
  }

  else if (cmd == "#POMPE OFF") {
    arreterPompe();
    envoyerSMS(numeroExpediteur, "Pompe arretee - " + lireHeure());
  }

  else if (cmd == "#ETAT") {
    envoyerEtat();
  }

  else if (cmd == "#STOP") {
    stopEnAttente = true;
    heureStop = millis();
    afficherLCD("STOP demande", "Confirmez...");
    envoyerSMS(numeroExpediteur,
      "ATTENTION: tapez #STOP CONFIRM dans 60s");
  }

  else if (cmd == "#STOP CONFIRM") {
    if (stopEnAttente && millis() - heureStop < 60000) {
      toutFermer();
      stopEnAttente = false;
      afficherLCD("ARRET URGENCE", "Tout ferme!");
      envoyerSMS(numeroExpediteur,
        "ARRET URGENCE execute - " + lireHeure());
    } else {
      envoyerSMS(numeroExpediteur,
        "ERREUR: delai depasse. Renvoyez #STOP");
    }
  }

  // ── #PROG Vx hh:mm duree — programmation horaire reelle ──
  // ── #PROG Vx OFF — annule une programmation existante ──
  else if (cmd.startsWith("#PROG V")) {
    int numV = cmd.charAt(7) - '0';

    if (numV < 1 || numV > 4) {
      envoyerSMS(numeroExpediteur, "ERREUR: vanne invalide (1-4)");
      return;
    }

    String reste = cmd.substring(9);
    reste.trim();

    if (reste == "OFF") {
      prog[numV].active = false;
      prog[numV].enCours = false;
      Serial.println("Prog V" + String(numV) + " annulee");
      envoyerSMS(numeroExpediteur,
        "Programmation V" + String(numV) + " annulee");
      return;
    }

    int posEspace = reste.indexOf(' ');
    if (posEspace < 0) {
      envoyerSMS(numeroExpediteur,
        "ERREUR: format. Exemple: #PROG V1 06:30 20");
      return;
    }

    String hhmm   = reste.substring(0, posEspace);
    int duree     = reste.substring(posEspace + 1).toInt();
    int posDeuxPt = hhmm.indexOf(':');

    if (posDeuxPt < 0 || duree <= 0) {
      envoyerSMS(numeroExpediteur,
        "ERREUR: format. Exemple: #PROG V1 06:30 20");
      return;
    }

    int h = hhmm.substring(0, posDeuxPt).toInt();
    int m = hhmm.substring(posDeuxPt + 1).toInt();

    if (h < 0 || h > 23 || m < 0 || m > 59) {
      envoyerSMS(numeroExpediteur,
        "ERREUR: heure invalide. Exemple: #PROG V1 06:30 20");
      return;
    }

    prog[numV].active = true;
    prog[numV].heure = h;
    prog[numV].minute = m;
    prog[numV].dureeMin = duree;
    prog[numV].enCours = false;
    prog[numV].dernierJourDeclenche = -1;

    Serial.println("Prog V" + String(numV) + " enregistree a " + hhmm +
      " duree " + String(duree) + "min");
    afficherLCD("PROG V" + String(numV) + " OK", hhmm + " " + String(duree) + "min");

    envoyerSMS(numeroExpediteur,
      "Prog V" + String(numV) +
      " enregistree a " + hhmm +
      " duree " + String(duree) +
      "min - sera executee chaque jour. Pour annuler: #PROG V" +
      String(numV) + " OFF");
  }

  else if (cmd == "#AIDE") {
    envoyerSMS(numeroExpediteur,
      "COMMANDES:\n"
      "#OUVRIR V1..V4\n"
      "#FERMER V1..V4\n"
      "#POMPE ON/OFF\n"
      "#PROG Vx hh:mm min\n"
      "#PROG Vx OFF\n"
      "#ETAT\n"
      "#STOP\n"
      "#AIDE");
  }

  else {
    Serial.println("Commande inconnue: " + cmd);
    envoyerSMS(numeroExpediteur, "Commande non reconnue: " + cmd);
  }
}

// ============================================================
//  ENVOYER SMS
// ============================================================
void envoyerSMS(String numero, String message) {
  Serial.println("Envoi SMS a " + numero + " : " + message);
  Serial.println(">>> [SMS ENVOYE A " + numero + "] : " + message);
  if (MODE_DEMO) return;
  gsm.print("AT+CMGS=\"");
  gsm.print(numero);
  gsm.println("\"");

  unsigned long t = millis();
  bool gotPrompt = false;
  while (millis() - t < 5000) {
    if (gsm.available()) {
      char c = gsm.read();
      if (c == '>') { gotPrompt = true; break; }
    }
  }
  if (!gotPrompt) {
    Serial.println("ERREUR: modem ne repond pas au CMGS");
    return;
  }
  gsm.print(message);
  gsm.write(26);
  delay(3000);
  Serial.println("SMS envoye.");
}

// ============================================================
//  ENVOYER ÉTAT
// ============================================================
void envoyerEtat() {
  int niveauSignal = 31;
  if (!MODE_DEMO) {
    String signal = envoyerAT("AT+CSQ", 1000);
    int posCSQ = signal.indexOf("+CSQ: ");
    if (posCSQ >= 0) {
      niveauSignal = signal.substring(posCSQ + 6, posCSQ + 8).toInt();
    }
  }

  String msg = "ETAT SYSTEME:\n";
  msg += "V1:" + String(etatVanne[1] ? "OUVERTE" : "FERMEE") + " ";
  msg += "V2:" + String(etatVanne[2] ? "OUVERTE" : "FERMEE") + "\n";
  msg += "V3:" + String(etatVanne[3] ? "OUVERTE" : "FERMEE") + " ";
  msg += "V4:" + String(etatVanne[4] ? "OUVERTE" : "FERMEE") + "\n";
  msg += "POMPE:" + String(etatPompe ? "ON" : "OFF") + "\n";
  msg += "Signal:" + String(niveauSignal) + "/31\n";
  msg += "Pompe total: " + String(totalSecondsPompe/3600) + "h ";
  msg += String((totalSecondsPompe%3600)/60) + "min\n";

  bool auMoinsUne = false;
  String progMsg = "Prog actives: ";
  for (int i = 1; i <= 4; i++) {
    if (prog[i].active) {
      auMoinsUne = true;
      progMsg += "V" + String(i) + "@";
      if (prog[i].heure < 10) progMsg += "0";
      progMsg += String(prog[i].heure) + ":";
      if (prog[i].minute < 10) progMsg += "0";
      progMsg += String(prog[i].minute) + "(" + String(prog[i].dureeMin) + "min) ";
    }
  }
  msg += auMoinsUne ? progMsg + "\n" : "Prog actives: aucune\n";
  msg += lireHeure();

  if (MODE_DEMO) {
    String heureLCD = (demoHeure < 10 ? "0" : "") + String(demoHeure) + ":" +
                       (demoMinute < 10 ? "0" : "") + String(demoMinute);
    afficherLCD("Signal:" + String(niveauSignal) + "/31", "Heure sim:" + heureLCD);
  } else {
    afficherLCD("Signal:" + String(niveauSignal) + "/31",
                "Pompe:" + String(etatPompe ? "ON" : "OFF"));
  }
  envoyerSMS(numeroExpediteur, msg);
}

// ============================================================
//  ENVOYER ALERTE
// ============================================================
void envoyerAlerte(String message) {
  Serial.println("ALERTE: " + message);
  afficherLCD("ALERTE!", message.substring(0, 16));
  for (int i = 0; i < NB_AUTORISES; i++) {
    envoyerSMS(numerosAutorises[i], "ALERTE: " + message);
    delay(2000);
  }
}

// ============================================================
//  PILOTAGE RELAIS
// ============================================================
void ouvrirVanne(int num) {
  int pins[] = {0, RELAY_V1, RELAY_V2, RELAY_V3, RELAY_V4};
  digitalWrite(pins[num], LOW);
  etatVanne[num] = true;
  Serial.println("Vanne " + String(num) + " OUVERTE");
  afficherLCD("Vanne " + String(num) + " OUVERTE", lireHeure());
}

void fermerVanne(int num) {
  int pins[] = {0, RELAY_V1, RELAY_V2, RELAY_V3, RELAY_V4};
  digitalWrite(pins[num], HIGH);
  etatVanne[num] = false;
  Serial.println("Vanne " + String(num) + " FERMEE");
  afficherLCD("Vanne " + String(num) + " FERMEE", lireHeure());
}

void demarrerPompe() {
  digitalWrite(RELAY_PUMP, LOW);
  etatPompe = true;
  heureDemarragePompe = millis();
  Serial.println("Pompe DEMARREE");
  afficherLCD("Pompe DEMARREE", lireHeure());
}

void arreterPompe() {
  digitalWrite(RELAY_PUMP, HIGH);
  etatPompe = false;
  totalSecondsPompe += (millis() - heureDemarragePompe) / 1000;
  Serial.println("Pompe ARRETEE");
  Serial.println("Total pompe: " + String(totalSecondsPompe/3600) + "h "
    + String((totalSecondsPompe%3600)/60) + "min");
  afficherLCD("Pompe ARRETEE", lireHeure());
}

void toutFermer() {
  digitalWrite(RELAY_V1,   HIGH);
  digitalWrite(RELAY_V2,   HIGH);
  digitalWrite(RELAY_V3,   HIGH);
  digitalWrite(RELAY_V4,   HIGH);
  digitalWrite(RELAY_PUMP, HIGH);
  for (int i = 1; i <= 4; i++) etatVanne[i] = false;
  etatPompe = false;
  Serial.println("TOUTES SORTIES FERMEES");
}

void journaliser(String numero, String cmd) {
  nbTentativesNonAutorisees++;
  Serial.println("=== TENTATIVE NON AUTORISEE #" +
    String(nbTentativesNonAutorisees) + " ===");
  Serial.println("  Numero  : " + numero);
  Serial.println("  Commande: " + cmd);
  if (nbTentativesNonAutorisees >= 3) {
    envoyerSMS(numerosAutorises[0],
      "ALERTE SECURITE: " + String(nbTentativesNonAutorisees) +
      " tentatives depuis " + numero);
    nbTentativesNonAutorisees = 0;
  }
}

// ============================================================
//  HORODATAGE — mode reel via AT+CCLK?, mode demo via horloge simulee
// ============================================================
String lireHeure() {
  if (MODE_DEMO) {
    String h = (demoHeure < 10 ? "0" : "") + String(demoHeure);
    String m = (demoMinute < 10 ? "0" : "") + String(demoMinute);
    return h + ":" + m + " (heure simulee, jour " + String(demoJour) + ")";
  }
  String rep = envoyerAT("AT+CCLK?", 1000);
  int debut = rep.indexOf("\"") + 1;
  int fin   = rep.indexOf("\"", debut);
  if (debut > 0 && fin > debut) {
    return rep.substring(debut, fin);
  }
  return "heure inconnue";
}

// ============================================================
//  LIRE HEURE/MINUTE/JOUR ACTUELS (pour #PROG)
//  Mode reel : parse AT+CCLK?  |  Mode demo : horloge simulee
// ============================================================
bool lireHeureActuelle(int &heure, int &minute, int &jour) {
  if (MODE_DEMO) {
    heure = demoHeure;
    minute = demoMinute;
    jour = demoJour;
    return true;
  }
  String rep = envoyerAT("AT+CCLK?", 1000);
  int debut = rep.indexOf("\"") + 1;
  int fin   = rep.indexOf("\"", debut);
  if (debut <= 0 || fin <= debut) return false;

  String horodatage = rep.substring(debut, fin);

  int posVirgule = horodatage.indexOf(',');
  if (posVirgule < 0) return false;

  String partieDate  = horodatage.substring(0, posVirgule);
  String partieHeure = horodatage.substring(posVirgule + 1);

  int posSlash1 = partieDate.indexOf('/');
  int posSlash2 = partieDate.indexOf('/', posSlash1 + 1);
  if (posSlash1 < 0 || posSlash2 < 0) return false;
  jour = partieDate.substring(posSlash2 + 1).toInt();

  int posDeuxPt1 = partieHeure.indexOf(':');
  int posDeuxPt2 = partieHeure.indexOf(':', posDeuxPt1 + 1);
  if (posDeuxPt1 < 0 || posDeuxPt2 < 0) return false;
  heure  = partieHeure.substring(0, posDeuxPt1).toInt();
  minute = partieHeure.substring(posDeuxPt1 + 1, posDeuxPt2).toInt();

  return true;
}

// ============================================================
//  WATCHDOG (mode reel uniquement)
// ============================================================
void verifierWatchdog() {
  if (millis() - dernierHeartbeat > TIMEOUT_WATCHDOG) {
    Serial.println("WATCHDOG: reinitialisation...");
    initialiserModem();
    dernierHeartbeat = millis();
  }
}

// ============================================================
//  DURÉE MAXIMALE POMPE
// ============================================================
void verifierDureePompe() {
  if (etatPompe && (millis() - heureDemarragePompe > DUREE_MAX_POMPE)) {
    Serial.println("SECURITE: duree max pompe depassee");
    arreterPompe();
    envoyerAlerte("Pompe arretee auto - 1h depassee");
  }
}

// ============================================================
//  VÉRIFICATION SIGNAL GSM (mode reel uniquement, toutes les 10 min)
// ============================================================
void verifierSignalGSM() {
  if (millis() - dernierCheckSignal > INTERVALLE_SIGNAL) {
    String rep = envoyerAT("AT+CSQ", 1000);
    int posCSQ = rep.indexOf("+CSQ: ");
    if (posCSQ >= 0) {
      int csq = rep.substring(posCSQ + 6, posCSQ + 8).toInt();
      if (csq < 10 && csq != 99) {
        Serial.println("ALERTE: Signal GSM faible: " + String(csq) + "/31");
        envoyerAlerte("Signal GSM faible: " + String(csq) + "/31");
        afficherLCD("Signal faible!", String(csq) + "/31");
      }
    }
    dernierCheckSignal = millis();
  }
}

// ============================================================
//  VÉRIFICATION DES PROGRAMMATIONS HORAIRES (#PROG)
//  Mode reel  : verifie toutes les 60s reelles via AT+CCLK?
//  Mode demo  : verifie chaque seconde reelle, contre l'horloge
//               simulee qui avance plus vite (voir avancerHorlogeDemo)
// ============================================================
void verifierProgrammations() {
  unsigned long intervalle = MODE_DEMO ? INTERVALLE_PROG_DEMO : INTERVALLE_PROG;
  if (millis() - dernierCheckProg < intervalle) return;
  dernierCheckProg = millis();

  // 1. Fermer automatiquement les vannes dont la duree est ecoulee.
  //    En mode demo, la duree programmee (en "minutes simulees") est
  //    convertie en duree reelle equivalente via VITESSE_DEMO_MS,
  //    pour que la fermeture survienne au bon moment filme.
  for (int i = 1; i <= 4; i++) {
    if (prog[i].enCours) {
      unsigned long dureeReelleMs;
      if (MODE_DEMO) {
        dureeReelleMs = (unsigned long)prog[i].dureeMin * VITESSE_DEMO_MS;
      } else {
        dureeReelleMs = (unsigned long)prog[i].dureeMin * 60000UL;
      }
      if (millis() - prog[i].heureOuverture >= dureeReelleMs) {
        fermerVanne(i);
        prog[i].enCours = false;
        Serial.println("Prog V" + String(i) + " : fermeture automatique (duree ecoulee)");
        envoyerSMS(numerosAutorises[0],
          "Vanne " + String(i) + " fermee automatiquement (fin programmation) - " + lireHeure());
      }
    }
  }

  // 2. Verifier s'il existe au moins une programmation active
  bool auMoinsUneActive = false;
  for (int i = 1; i <= 4; i++) {
    if (prog[i].active) auMoinsUneActive = true;
  }
  if (!auMoinsUneActive) return;

  int heureActuelle, minuteActuelle, jourActuel;
  if (!lireHeureActuelle(heureActuelle, minuteActuelle, jourActuel)) {
    return;
  }

  // 3. Declencher les programmations dont l'heure correspond
  for (int i = 1; i <= 4; i++) {
    if (!prog[i].active || prog[i].enCours) continue;

    bool memeJourDejaDeclenche = (prog[i].dernierJourDeclenche == jourActuel);

    if (!memeJourDejaDeclenche &&
        heureActuelle == prog[i].heure &&
        minuteActuelle == prog[i].minute) {

      ouvrirVanne(i);
      prog[i].enCours = true;
      prog[i].heureOuverture = millis();
      prog[i].dernierJourDeclenche = jourActuel;

      Serial.println("Prog V" + String(i) + " : declenchement automatique");
      envoyerSMS(numerosAutorises[0],
        "Vanne " + String(i) + " ouverte automatiquement (programmation) - " + lireHeure());
    }
  }
}

// ============================================================
//  CLIGNOTEMENT LED TÉMOIN
// ============================================================
void cligneterLED() {
  static unsigned long dernierCligno = 0;
  static bool etatLED = false;
  if (millis() - dernierCligno > 1000) {
    etatLED = !etatLED;
    digitalWrite(LED_STATUS, etatLED);
    dernierCligno = millis();
  }
}
