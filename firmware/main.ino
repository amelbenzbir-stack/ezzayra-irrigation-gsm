#include <SoftwareSerial.h>
// ─────────────────────────────────────────
//  CONFIGURATION DES BROCHES (PINS)
// ─────────────────────────────────────────
// Modem GSM SIM800L — port série logiciel
SoftwareSerial gsm(10, 11);   // RX=pin10, TX=pin11

// Relais (LOW = activé sur la plupart des modules opto-isolés)
#define RELAY_V1   2    // Électrovanne 1
#define RELAY_V2   3    // Électrovanne 2
#define RELAY_V3   4    // Électrovanne 3
#define RELAY_V4   5    // Électrovanne 4
#define RELAY_PUMP 6    // Motopompe

// LED témoin de vie (clignote pour montrer que le système tourne)
#define LED_STATUS 13

// ─────────────────────────────────────────
//  LISTE BLANCHE — numéros autorisés
// ─────────────────────────────────────────
// Seuls ces numéros peuvent envoyer des commandes
const int NB_AUTORISES = 3;
String numerosAutorises[NB_AUTORISES] = {
  "+21620123456",   // Agriculteur principal
  "+21655987654",   // Technicien
  "+21698111222"    // Responsable exploitation
};
bool stopEnAttente   = false;
unsigned long heureStop = 0;
int nbTentativesNonAutorisees = 0;
unsigned long totalSecondsPompe = 0;
// ─────────────────────────────────────────
//  ÉTAT DU SYSTÈME
// ─────────────────────────────────────────
bool etatVanne[5]  = {false, false, false, false, false}; // V1..V4 + Pompe
bool etatPompe     = false;

// ─────────────────────────────────────────
//  MACHINE À ÉTATS
// ─────────────────────────────────────────
enum Etat {
  VEILLE,        // En attente d'un SMS
  SMS_RECU,      // Un SMS vient d'arriver
  VERIFICATION,  // Vérification du numéro
  EXECUTION,     // Exécution de la commande
  REPONSE,       // Envoi du SMS de confirmation
  ALERTE         // Envoi d'un SMS d'alerte
};

Etat etatActuel = VEILLE;

// ─────────────────────────────────────────
//  VARIABLES POUR STOCKER LE SMS REÇU
// ─────────────────────────────────────────
String numeroExpediteur = "";
String contenuSMS       = "";
String smsBrut          = "";   // Tout le message +CMT brut

// ─────────────────────────────────────────
//  WATCHDOG LOGICIEL
// ─────────────────────────────────────────
unsigned long dernierHeartbeat = 0;
const unsigned long TIMEOUT_WATCHDOG = 30000; // 30 secondes

// ─────────────────────────────────────────
//  DURÉE MAXIMALE DE FONCTIONNEMENT POMPE
// ─────────────────────────────────────────
unsigned long heureDemarragePompe = 0;
const unsigned long DUREE_MAX_POMPE = 3600000; // 1 heure en ms

// ============================================================
//  SETUP — s'exécute une seule fois au démarrage
// ============================================================
void setup() {
  // --- Port série pour le débogage (Serial Monitor Arduino IDE)
  Serial.begin(9600);
  Serial.println("=== EZZAYRA Irrigation GSM - Demarrage ===");

  // --- Port série pour le modem GSM
  gsm.begin(9600);

  // --- Configuration des relais
  pinMode(RELAY_V1,   OUTPUT);
  pinMode(RELAY_V2,   OUTPUT);
  pinMode(RELAY_V3,   OUTPUT);
  pinMode(RELAY_V4,   OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);

  // SÉCURITÉ : tout fermer au démarrage
  toutFermer();
  Serial.println("Securite: toutes les sorties fermees");

  // --- Initialisation du modem GSM
  delay(3000); // Attendre que le modem démarre
  initialiserModem();

  Serial.println("Systeme pret. En attente de commandes SMS...");
  dernierHeartbeat = millis();
}

// ============================================================
//  LOOP — s'exécute en boucle en permanence
// ============================================================
void loop() {
  verifierWatchdog();
  cligneterLED();
  verifierDureePompe();

  if (Serial.available()) {
    String cmdTest = Serial.readStringUntil('\n');
    cmdTest.trim();
    cmdTest.toUpperCase();

    if (cmdTest.length() == 0) return;

    Serial.println(">>> COMMANDE : " + cmdTest);
    numeroExpediteur = "+21620123456";
    //numeroExpediteur = "+21699999999"; // non autorisé
    contenuSMS = cmdTest;
    etatActuel = VERIFICATION; // ← clé du fix !
  }

  // Machine à états
  switch (etatActuel) {
    case VEILLE:
      break;

    case VERIFICATION:
      if (numeroAutorise(numeroExpediteur)) {
        Serial.println("Numero autorise -> execution");
        etatActuel = EXECUTION;
      } else {
        journaliser(numeroExpediteur, contenuSMS);
        etatActuel = VEILLE;
      }
      break;

    case EXECUTION:
      executerCommande(contenuSMS);
      etatActuel = VEILLE;
      break;
  }

  dernierHeartbeat = millis();
}
/*void loop() {
  // ── Watchdog logiciel ──
  verifierWatchdog();

  // ── Clignotement LED témoin ──
  cligneterLED();

  // ── Vérifier durée max pompe ──
  verifierDureePompe();

  // ── Machine à états principale ──
  switch (etatActuel) {

    case VEILLE:
      // Écouter le port série du modem
      if (gsm.available()) {
        smsBrut = lireReponseGSM();
        // Si le modem signale un nouveau SMS
        if (smsBrut.indexOf("+CMT:") >= 0) {
          Serial.println("SMS recu !");
          Serial.println(smsBrut);
          etatActuel = SMS_RECU;
        }
      }
      break;

    case SMS_RECU:
      // Extraire le numéro et le contenu
      extraireSMS(smsBrut);
      Serial.println("Expediteur : " + numeroExpediteur);
      Serial.println("Contenu    : " + contenuSMS);
      etatActuel = VERIFICATION;
      break;

    case VERIFICATION:
    if (numeroAutorise(numeroExpediteur)) {
    Serial.println("Numero autorise -> execution");
    etatActuel = EXECUTION;
    } else {
    journaliser(numeroExpediteur, contenuSMS);
    etatActuel = VEILLE;
    }
    break;

    case EXECUTION:
      executerCommande(contenuSMS);
      etatActuel = REPONSE;
      break;

    case REPONSE:
      // La réponse est envoyée dans executerCommande()
      // Supprimer le SMS traité
      envoyerAT("AT+CMGD=1,4", 1000);
      etatActuel = VEILLE;
      break;

    case ALERTE:
      // Géré par envoyerAlerte()
      etatActuel = VEILLE;
      break;
  }

  dernierHeartbeat = millis();
}*/

// ============================================================
//  INITIALISATION DU MODEM GSM
// ============================================================
void initialiserModem() {
  Serial.println("Initialisation du modem...");

  // 1. Tester la connexion
  int essais = 0;
  while (essais < 10) {
    String rep = envoyerAT("AT", 1000);
    if (rep.indexOf("OK") >= 0) {
      Serial.println("Modem repond : OK");
      break;
    }
    essais++;
    delay(1000);
  }

  // 2. Mode texte SMS (obligatoire)
  envoyerAT("AT+CMGF=1", 1000);
  Serial.println("Mode texte active");

  // 3. Encodage GSM standard
  envoyerAT("AT+CSCS=\"GSM\"", 1000);

  // 4. Notifications SMS automatiques
  envoyerAT("AT+CNMI=2,2,0,0,0", 1000);
  Serial.println("Notifications SMS activees");

  // 5. Vider les anciens SMS
  envoyerAT("AT+CMGD=1,4", 2000);
  Serial.println("Anciens SMS supprimes");

  // 6. Vérifier connexion réseau
  String creg = envoyerAT("AT+CREG?", 1000);
  Serial.println("Reseau: " + creg);

  // 7. Envoyer SMS de démarrage aux numéros autorisés
  envoyerSMS(numerosAutorises[0], "Systeme irrigation demarre - Pret a recevoir commandes");
}

// ============================================================
//  ENVOYER UNE COMMANDE AT AU MODEM
// ============================================================
String envoyerAT(String commande, int attente) {
  gsm.println(commande);          // Envoyer la commande
  delay(attente);                 // Attendre la réponse
  String reponse = "";
  while (gsm.available()) {
    reponse += (char)gsm.read();  // Lire la réponse
  }
  Serial.println("AT >> " + commande);
  Serial.println("AT << " + reponse);
  return reponse;
}

// ============================================================
//  LIRE LA RÉPONSE DU MODEM (SMS entrant)
// ============================================================
String lireReponseGSM() {
  String reponse = "";
  unsigned long debut = millis();
  while (millis() - debut < 2000) {   // Lire pendant 2 secondes max
    while (gsm.available()) {
      char c = gsm.read();
      reponse += c;
    }
  }
  return reponse;
}

// ============================================================
//  EXTRAIRE NUMÉRO ET CONTENU DU SMS BRUT
// ============================================================
// Format reçu :
// +CMT: "+21620123456","","25/06/08,10:30:00+04"
// #OUVRIR V1
void extraireSMS(String smsComplet) {
  // Extraire le numéro entre les premiers guillemets après +CMT:
  int posCMT = smsComplet.indexOf("+CMT:");
  int debut  = smsComplet.indexOf("\"", posCMT) + 1;
  int fin    = smsComplet.indexOf("\"", debut);
  if (debut > 6 && fin > debut) {
    numeroExpediteur = smsComplet.substring(debut, fin);
  }

  // Extraire le contenu : dernière ligne du message
  int dernierRetour = smsComplet.lastIndexOf('\n');
  if (dernierRetour >= 0) {
    contenuSMS = smsComplet.substring(dernierRetour + 1);
    contenuSMS.trim();  // Supprimer espaces et \r
    contenuSMS.toUpperCase(); // Mettre en majuscules pour la comparaison
  }
}

// ============================================================
//  VÉRIFIER SI LE NUMÉRO EST AUTORISÉ
// ============================================================
bool numeroAutorise(String numero) {
  for (int i = 0; i < NB_AUTORISES; i++) {
    if (numero == numerosAutorises[i]) {
      return true;
    }
  }
  return false;
}

// ============================================================
//  EXÉCUTER LA COMMANDE SMS
// ============================================================
void executerCommande(String cmd) {
  Serial.println("Execution: " + cmd);

  // ── #OUVRIR Vx ──
  if (cmd.startsWith("#OUVRIR V")) {
    int numVanne = cmd.charAt(9) - '0';   // Extraire le chiffre
    if (numVanne >= 1 && numVanne <= 4) {
      ouvrirVanne(numVanne);
      String heure = lireHeure();
      envoyerSMS(numeroExpediteur, "Vanne " + String(numVanne) + " ouverte - " + heure);
    } else {
      envoyerSMS(numeroExpediteur, "ERREUR: numero de vanne invalide (1-4)");
    }
  }

  // ── #FERMER Vx ──
  else if (cmd.startsWith("#FERMER V")) {
    int numVanne = cmd.charAt(9) - '0';
    if (numVanne >= 1 && numVanne <= 4) {
      fermerVanne(numVanne);
      String heure = lireHeure();
      envoyerSMS(numeroExpediteur, "Vanne " + String(numVanne) + " fermee - " + heure);
    } else {
      envoyerSMS(numeroExpediteur, "ERREUR: numero de vanne invalide (1-4)");
    }
  }

  // ── #POMPE ON ──
  else if (cmd == "#POMPE ON") {
    demarrerPompe();
    String heure = lireHeure();
    envoyerSMS(numeroExpediteur, "Pompe demarree - " + heure);
  }

  // ── #POMPE OFF ──
  else if (cmd == "#POMPE OFF") {
    arreterPompe();
    String heure = lireHeure();
    envoyerSMS(numeroExpediteur, "Pompe arretee - " + heure);
  }

  // ── #ETAT ──
  else if (cmd == "#ETAT") {
    envoyerEtat();
  }

  // ── #STOP — demande confirmation ──
  else if (cmd == "#STOP") {
  stopEnAttente = true;
  heureStop = millis();
  envoyerSMS(numeroExpediteur,
    "ATTENTION: tapez #STOP CONFIRM dans 60s pour confirmer l'arret");
  }

  // ── #STOP CONFIRM — exécution réelle ──
  else if (cmd == "#STOP CONFIRM") {
    if (stopEnAttente && millis() - heureStop < 60000) {
    toutFermer();
    stopEnAttente = false;
    String heure = lireHeure();
    envoyerSMS(numeroExpediteur,
      "ARRET URGENCE execute - " + heure + " - Toutes sorties fermees");
    } else {
    envoyerSMS(numeroExpediteur,
      "ERREUR: delai depasse. Renvoyez #STOP pour recommencer");
    }
  }
  // ── #PROG Vx hh:mm durée ──
  else if (cmd.startsWith("#PROG V")) {
    // Format attendu : #PROG V1 06:30 20
    int numV    = cmd.charAt(7) - '0';       // chiffre de la vanne
    String hhmm = cmd.substring(9, 14);      // ex: "06:30"
    int duree   = cmd.substring(15).toInt(); // durée en minutes

    if (numV >= 1 && numV <= 4 && duree > 0) {
    Serial.println("Prog V" + String(numV) + " a " + hhmm + " duree " + String(duree) + "min");
    envoyerSMS(numeroExpediteur,
      "Prog V" + String(numV) +
      " planifiee a " + hhmm +
      " duree " + String(duree) + "min - OK");
    } else {
    envoyerSMS(numeroExpediteur,
      "ERREUR: format incorrect. Exemple: #PROG V1 06:30 20");
    }
  }
  // ── #AIDE ──
  else if (cmd == "#AIDE") {
    Serial.println("Envoi liste commandes...");
    envoyerSMS(numeroExpediteur,
    "COMMANDES DISPONIBLES:\n"
    "#OUVRIR V1..V4\n"
    "#FERMER V1..V4\n"
    "#POMPE ON/OFF\n"
    "#PROG Vx hh:mm min\n"
    "#ETAT\n"
    "#STOP\n"
    "#AIDE");
  }

  // ── Commande inconnue ──
  else {
    Serial.println("Commande inconnue: " + cmd);
    envoyerSMS(numeroExpediteur, "Commande non reconnue: " + cmd);
  }
}

// ============================================================
//  ENVOYER UN SMS
// ============================================================
void envoyerSMS(String numero, String message) {
Serial.println("Envoi SMS a " + numero + " : " + message);
gsm.print("AT+CMGS=\"");
gsm.print(numero);
gsm.println("\"");
// Attendre le vrai ">" du modem (max 5 secondes)
unsigned long t = millis();
bool gotPrompt = false;
while (millis() - t < 5000) {
  if (gsm.available()) {
    char c = gsm.read();
    if (c == '>') {
      gotPrompt = true;
      break;
    }
  }
}
// Si le ">" n'arrive pas → annuler
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
//  ENVOYER L'ÉTAT COMPLET DU SYSTÈME
// ============================================================
void envoyerEtat() {
  // Lire le signal GSM
  String signal = envoyerAT("AT+CSQ", 1000);
  int niveauSignal = 0;
  int posCSQ = signal.indexOf("+CSQ: ");
  if (posCSQ >= 0) {
    niveauSignal = signal.substring(posCSQ + 6, posCSQ + 8).toInt();
  }

  // Construire le message d'état
  String msg = "ETAT SYSTEME:\n";
  msg += "V1:" + String(etatVanne[1] ? "OUVERTE" : "FERMEE") + " ";
  msg += "V2:" + String(etatVanne[2] ? "OUVERTE" : "FERMEE") + "\n";
  msg += "V3:" + String(etatVanne[3] ? "OUVERTE" : "FERMEE") + " ";
  msg += "V4:" + String(etatVanne[4] ? "OUVERTE" : "FERMEE") + "\n";
  msg += "POMPE:" + String(etatPompe ? "ON" : "OFF") + "\n";
  msg += "Signal:" + String(niveauSignal) + "/31\n";
  msg += "Pompe total: " + String(totalSecondsPompe/3600) + "h " + String((totalSecondsPompe%3600)/60) + "min\n";
  msg += lireHeure();

  envoyerSMS(numeroExpediteur, msg);
}

// ============================================================
//  ENVOYER UNE ALERTE AUTOMATIQUE
// ============================================================
void envoyerAlerte(String message) {
  Serial.println("ALERTE: " + message);
  // Envoyer à tous les numéros autorisés
  for (int i = 0; i < NB_AUTORISES; i++) {
    envoyerSMS(numerosAutorises[i], "ALERTE: " + message);
    delay(2000);
  }
}

// ============================================================
//  PILOTAGE DES RELAIS
// ============================================================
void ouvrirVanne(int num) {
  int pin = num + 1;  // V1=pin2, V2=pin3, V3=pin4, V4=pin5
  digitalWrite(pin, LOW);  // LOW = relais activé (opto-isolé)
  etatVanne[num] = true;
  Serial.println("Vanne " + String(num) + " OUVERTE");
}

void fermerVanne(int num) {
  int pin = num + 1;
  digitalWrite(pin, HIGH);  // HIGH = relais désactivé
  etatVanne[num] = false;
  Serial.println("Vanne " + String(num) + " FERMEE");
}

void demarrerPompe() {
  digitalWrite(RELAY_PUMP, LOW);
  etatPompe = true;
  heureDemarragePompe = millis();
  Serial.println("Pompe DEMARREE");
}

void arreterPompe() {
  digitalWrite(RELAY_PUMP, HIGH);
  etatPompe = false;
  // Ajouter le temps de fonctionnement
  totalSecondsPompe += (millis() - heureDemarragePompe) / 1000;
  Serial.println("Pompe ARRETEE");
  Serial.println("Total pompe: " + String(totalSecondsPompe/3600) + "h " + String((totalSecondsPompe%3600)/60) + "min");
}

//  JOURNALISATION DES TENTATIVES NON AUTORISÉES
void journaliser(String numero, String cmd) {
  nbTentativesNonAutorisees++;
  Serial.println("=== TENTATIVE NON AUTORISEE #" + 
                 String(nbTentativesNonAutorisees) + " ===");
  Serial.println("  Numero  : " + numero);
  Serial.println("  Commande: " + cmd);
  Serial.println("  Heure   : " + lireHeure());
  
  // Alerter l'admin si plus de 3 tentatives
  if (nbTentativesNonAutorisees >= 3) {
    envoyerSMS(numerosAutorises[0],
      "ALERTE SECURITE: " + 
      String(nbTentativesNonAutorisees) + 
      " tentatives non autorisees depuis " + numero);
    nbTentativesNonAutorisees = 0; // reset compteur
  }
}
void toutFermer() {
  // Tout mettre à HIGH = tout désactiver (relais opto-isolés)
  digitalWrite(RELAY_V1,   HIGH);
  digitalWrite(RELAY_V2,   HIGH);
  digitalWrite(RELAY_V3,   HIGH);
  digitalWrite(RELAY_V4,   HIGH);
  digitalWrite(RELAY_PUMP, HIGH);
  for (int i = 1; i <= 4; i++) etatVanne[i] = false;
  etatPompe = false;
  Serial.println("TOUTES SORTIES FERMEES");
}

// ============================================================
//  LIRE L'HEURE VIA LE MODEM GSM
// ============================================================
String lireHeure() {
  String rep = envoyerAT("AT+CCLK?", 1000);
  // Extraire la partie heure de : +CCLK: "25/06/08,10:30:45+04"
  int debut = rep.indexOf("\"") + 1;
  int fin   = rep.indexOf("\"", debut);
  if (debut > 0 && fin > debut) {
    return rep.substring(debut, fin);
  }
  return "heure inconnue";
}

// ============================================================
//  WATCHDOG LOGICIEL
// ============================================================
void verifierWatchdog() {
  if (millis() - dernierHeartbeat > TIMEOUT_WATCHDOG) {
    Serial.println("WATCHDOG: systeme bloque, reinitialisation...");
    // Sur Arduino, on peut forcer un reset logiciel
    // ou simplement réinitialiser le modem
    initialiserModem();
    dernierHeartbeat = millis();
  }
}

// ============================================================
//  VÉRIFIER DURÉE MAXIMALE DE LA POMPE
// ============================================================
void verifierDureePompe() {
  if (etatPompe && (millis() - heureDemarragePompe > DUREE_MAX_POMPE)) {
    Serial.println("SECURITE: duree max pompe depassee, arret automatique");
    arreterPompe();
    envoyerAlerte("Pompe arretee automatiquement - duree maximale 1h depassee");
  }
}

// ============================================================
//  CLIGNOTEMENT LED TÉMOIN DE VIE
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
