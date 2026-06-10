# ============================================================
#  EZZAYRA SOLUTIONS — Simulateur modem GSM SIM800L
#  Fichier  : sim_modem.py
#  Auteur   : Amel Ben Zbir
#  Rôle     : Simule le modem SIM800L pour tester le firmware
#             Arduino sans matériel physique
# ============================================================

import serial
import time
import datetime

# ─────────────────────────────────────────
#  CONFIGURATION
# ─────────────────────────────────────────
PORT_MODEM   = "COM6"   # Port que le simulateur utilise
                        # Arduino utilise COM6 (l'autre bout)
BAUD_RATE    = 9600
NUMERO_TEST  = "+21620123456"  # Numéro qui envoie les SMS simulés

# ─────────────────────────────────────────
#  COULEURS pour le terminal
# ─────────────────────────────────────────
RESET  = "\033[0m"
VERT   = "\033[92m"
ROUGE  = "\033[91m"
BLEU   = "\033[94m"
JAUNE  = "\033[93m"
CYAN   = "\033[96m"

def log_arduino(msg):
    print(f"{BLEU}[ARDUINO →]{RESET} {msg}")

def log_modem(msg):
    print(f"{VERT}[MODEM  ←]{RESET} {msg}")

def log_info(msg):
    print(f"{JAUNE}[INFO]{RESET} {msg}")

def log_sms(msg):
    print(f"{CYAN}[SMS]{RESET} {msg}")

# ─────────────────────────────────────────
#  RÉPONSES AUX COMMANDES AT
# ─────────────────────────────────────────
def repondre_AT(commande, ser):
    """Analyse la commande AT reçue et envoie la bonne réponse"""
    cmd = commande.strip()
    log_arduino(cmd)

    # ── Test de connexion ──
    if cmd == "AT":
        envoyer(ser, "OK")

    # ── Mode texte ──
    elif cmd == "AT+CMGF=1":
        envoyer(ser, "OK")

    # ── Encodage ──
    elif cmd == 'AT+CSCS="GSM"':
        envoyer(ser, "OK")

    # ── Notifications SMS ──
    elif cmd == "AT+CNMI=2,2,0,0,0":
        envoyer(ser, "OK")

    # ── Supprimer SMS ──
    elif cmd.startswith("AT+CMGD"):
        envoyer(ser, "OK")

    # ── Vérifier réseau ──
    elif cmd == "AT+CREG?":
        envoyer(ser, "+CREG: 0,1\r\nOK")

    # ── Signal GSM ──
    elif cmd == "AT+CSQ":
        envoyer(ser, "+CSQ: 18,0\r\nOK")

    # ── Heure réseau ──
    elif cmd == "AT+CCLK?":
        now = datetime.datetime.now()
        heure = now.strftime('%y/%m/%d,%H:%M:%S+04')
        envoyer(ser, f'+CCLK: "{heure}"\r\nOK')

    # ── Infos modem ──
    elif cmd == "ATI":
        envoyer(ser, "SIM800 R14.18\r\nOK")

    # ── Numéro SIM ──
    elif cmd == "AT+CIMI":
        envoyer(ser, "605020123456789\r\nOK")

    # ── Envoyer SMS — étape 1 ──
    elif cmd.startswith("AT+CMGS"):
        # Extraire le numéro destinataire
        debut = cmd.find('"') + 1
        fin   = cmd.find('"', debut)
        numero_dest = cmd[debut:fin]
        log_info(f"SMS vers : {numero_dest}")
        # Envoyer le prompt ">"
        ser.write(b"\r\n> ")
        log_modem("> (en attente du texte...)")
        # Lire le texte du SMS (jusqu'au char 26 = Ctrl+Z)
        texte_sms = ""
        timeout = time.time() + 10
        while time.time() < timeout:
            if ser.in_waiting:
                octet = ser.read(1)
                if octet == b'\x1a':  # char(26) = fin du SMS
                    break
                texte_sms += octet.decode('utf-8', errors='ignore')
        # Afficher le SMS envoyé
        log_sms(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        log_sms(f"De      : Arduino (carte)")
        log_sms(f"Vers    : {numero_dest}")
        log_sms(f"Message : {texte_sms.strip()}")
        log_sms(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        # Confirmer l'envoi
        envoyer(ser, "+CMGS: 5\r\nOK")

    # ── Commande inconnue ──
    else:
        if cmd:  # ignorer les lignes vides
            log_info(f"Commande non reconnue: {cmd}")
            envoyer(ser, "OK")

def envoyer(ser, reponse):
    """Envoie une réponse au port série"""
    message = f"\r\n{reponse}\r\n"
    ser.write(message.encode('utf-8'))
    log_modem(reponse.replace('\r\n', ' | '))

# ─────────────────────────────────────────
#  ENVOYER UN SMS SIMULÉ VERS L'ARDUINO
# ─────────────────────────────────────────
def envoyer_sms_simule(ser, numero, texte):
    """Simule la réception d'un SMS par l'Arduino"""
    now = datetime.datetime.now()
    date = now.strftime('%y/%m/%d,%H:%M:%S+04')
    trame = f'\r\n+CMT: "{numero}","","{date}"\r\n{texte}\r\n'
    ser.write(trame.encode('utf-8'))
    log_sms(f"SMS simulé envoyé : [{numero}] → {texte}")

# ─────────────────────────────────────────
#  MENU INTERACTIF
# ─────────────────────────────────────────
def afficher_menu():
    print(f"\n{CYAN}{'═'*50}{RESET}")
    print(f"{CYAN}  SIMULATEUR MODEM GSM — Ezzayra Irrigation{RESET}")
    print(f"{CYAN}{'═'*50}{RESET}")
    print(f"  {VERT}1{RESET} → Envoyer #OUVRIR V1")
    print(f"  {VERT}2{RESET} → Envoyer #OUVRIR V2")
    print(f"  {VERT}3{RESET} → Envoyer #FERMER V1")
    print(f"  {VERT}4{RESET} → Envoyer #POMPE ON")
    print(f"  {VERT}5{RESET} → Envoyer #POMPE OFF")
    print(f"  {VERT}6{RESET} → Envoyer #ETAT")
    print(f"  {VERT}7{RESET} → Envoyer #STOP")
    print(f"  {VERT}8{RESET} → Envoyer #STOP CONFIRM")
    print(f"  {VERT}9{RESET} → Envoyer #AIDE")
    print(f"  {VERT}0{RESET} → SMS depuis numéro NON AUTORISÉ")
    print(f"  {VERT}c{RESET} → SMS personnalisé")
    print(f"  {ROUGE}q{RESET} → Quitter")
    print(f"{CYAN}{'═'*50}{RESET}")

# ─────────────────────────────────────────
#  PROGRAMME PRINCIPAL
# ─────────────────────────────────────────
def main():
    print(f"\n{VERT}=== EZZAYRA — Simulateur SIM800L ==={RESET}")
    print(f"Connexion sur {PORT_MODEM} à {BAUD_RATE} baud...")

    try:
        ser = serial.Serial(PORT_MODEM, BAUD_RATE, timeout=0.1)
        print(f"{VERT}Port {PORT_MODEM} ouvert avec succès !{RESET}")
        print(f"En attente de l'Arduino...\n")
    except Exception as e:
        print(f"{ROUGE}Erreur ouverture port : {e}{RESET}")
        print("Vérifie que VSPE est actif et que COM4 est disponible.")
        return

    buffer = ""

    try:
        while True:
            # ── Lire les données de l'Arduino ──
            while ser.in_waiting:
                octet = ser.read(1)
                try:
                    char = octet.decode('utf-8')
                    if char == '\n':
                        if buffer.strip():
                            repondre_AT(buffer.strip(), ser)
                        buffer = ""
                    else:
                        buffer += char
                except:
                    pass

            # ── Menu interactif (non bloquant) ──
            import msvcrt
            if msvcrt.kbhit():
                touche = msvcrt.getch().decode('utf-8', errors='ignore')

                if touche == '1':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#OUVRIR V1")
                elif touche == '2':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#OUVRIR V2")
                elif touche == '3':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#FERMER V1")
                elif touche == '4':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#POMPE ON")
                elif touche == '5':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#POMPE OFF")
                elif touche == '6':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#ETAT")
                elif touche == '7':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#STOP")
                elif touche == '8':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#STOP CONFIRM")
                elif touche == '9':
                    envoyer_sms_simule(ser, NUMERO_TEST, "#AIDE")
                elif touche == '0':
                    envoyer_sms_simule(ser, "+21699999999", "#OUVRIR V1")
                elif touche == 'c':
                    print("Entrez le SMS à envoyer : ", end='')
                    texte = input()
                    envoyer_sms_simule(ser, NUMERO_TEST, texte)
                elif touche == 'm':
                    afficher_menu()
                elif touche == 'q':
                    print(f"\n{JAUNE}Fermeture du simulateur...{RESET}")
                    break

    except KeyboardInterrupt:
        print(f"\n{JAUNE}Arrêt par Ctrl+C{RESET}")
    finally:
        ser.close()
        print(f"{VERT}Port fermé.{RESET}")

if __name__ == "__main__":
    afficher_menu()
    main()