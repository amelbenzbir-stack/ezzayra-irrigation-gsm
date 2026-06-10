# ============================================================
#  EZZAYRA SOLUTIONS — Simulateur modem GSM SIM800L v2
#  Mode : communication via Serial Monitor Wokwi (COM5)
# ============================================================

import serial
import time
import datetime
import threading

# ─────────────────────────────────────────
#  CONFIGURATION
# ─────────────────────────────────────────
PORT_WOKWI  = "COM6"   # Port Serial Monitor de Wokwi
BAUD_RATE   = 9600
NUMERO_TEST = "+21620123456"  # Numéro autorisé
NUMERO_HACK = "+21699999999"  # Numéro non autorisé

# ─────────────────────────────────────────
#  COULEURS terminal
# ─────────────────────────────────────────
RESET = "\033[0m"
VERT  = "\033[92m"
ROUGE = "\033[91m"
BLEU  = "\033[94m"
JAUNE = "\033[93m"
CYAN  = "\033[96m"

def log_send(msg):
    print(f"{VERT}[ENVOI →]{RESET} {msg}")

def log_recv(msg):
    print(f"{BLEU}[RECU  ←]{RESET} {msg}")

def log_info(msg):
    print(f"{JAUNE}[INFO]{RESET} {msg}")

def log_sms(msg):
    print(f"{CYAN}[SMS]{RESET} {msg}")

# ─────────────────────────────────────────
#  ENVOYER UNE COMMANDE AU SERIAL MONITOR
# ─────────────────────────────────────────
def envoyer_commande(ser, commande):
    """Envoie une commande SMS simulée au Serial Monitor de Wokwi"""
    msg = commande + "\n"
    ser.write(msg.encode('utf-8'))
    log_send(commande)
    time.sleep(0.5)

# ─────────────────────────────────────────
#  LIRE LES RÉPONSES DE WOKWI
# ─────────────────────────────────────────
def lire_reponses(ser, stop_event):
    """Thread qui lit en permanence les réponses de Wokwi"""
    buffer = ""
    while not stop_event.is_set():
        try:
            if ser.in_waiting:
                char = ser.read(1).decode('utf-8', errors='ignore')
                if char == '\n':
                    ligne = buffer.strip()
                    if ligne:
                        log_recv(ligne)
                        # Détecter les SMS envoyés par le système
                        if "Envoi SMS" in ligne:
                            log_sms("━━━━━━━━━━━━━━━━━━━━━━━━")
                        if "OUVERTE" in ligne or "FERMEE" in ligne:
                            log_sms(f"État vanne: {ligne}")
                        if "DEMARREE" in ligne or "ARRETEE" in ligne:
                            log_sms(f"État pompe: {ligne}")
                    buffer = ""
                else:
                    buffer += char
        except:
            pass
        time.sleep(0.01)

# ─────────────────────────────────────────
#  MENU INTERACTIF
# ─────────────────────────────────────────
def afficher_menu():
    print(f"\n{CYAN}{'═'*50}{RESET}")
    print(f"{CYAN}  SIMULATEUR SMS — Ezzayra Irrigation v2{RESET}")
    print(f"{CYAN}{'═'*50}{RESET}")
    print(f"  {VERT}1{RESET} → #OUVRIR V1")
    print(f"  {VERT}2{RESET} → #OUVRIR V2")
    print(f"  {VERT}3{RESET} → #FERMER V1")
    print(f"  {VERT}4{RESET} → #POMPE ON")
    print(f"  {VERT}5{RESET} → #POMPE OFF")
    print(f"  {VERT}6{RESET} → #ETAT")
    print(f"  {VERT}7{RESET} → #STOP")
    print(f"  {VERT}8{RESET} → #STOP CONFIRM")
    print(f"  {VERT}9{RESET} → #AIDE")
    print(f"  {VERT}0{RESET} → #PROG V1 06:30 20")
    print(f"  {ROUGE}x{RESET} → Numéro NON AUTORISÉ (#OUVRIR V1)")
    print(f"  {VERT}c{RESET} → Commande personnalisée")
    print(f"  {VERT}m{RESET} → Afficher ce menu")
    print(f"  {ROUGE}q{RESET} → Quitter")
    print(f"{CYAN}{'═'*50}{RESET}\n")

# ─────────────────────────────────────────
#  PROGRAMME PRINCIPAL
# ─────────────────────────────────────────
def main():
    print(f"\n{VERT}=== EZZAYRA — Simulateur SMS v2 ==={RESET}")
    print(f"Connexion sur {PORT_WOKWI} à {BAUD_RATE} baud...")

    try:
        ser = serial.Serial(PORT_WOKWI, BAUD_RATE, timeout=0.1)
        print(f"{VERT}Port {PORT_WOKWI} ouvert !{RESET}")
    except Exception as e:
        print(f"{ROUGE}Erreur : {e}{RESET}")
        print("Vérifie que Wokwi tourne et que VSPE est actif.")
        return

    # Démarrer le thread de lecture
    stop_event = threading.Event()
    thread_lecture = threading.Thread(
        target=lire_reponses, 
        args=(ser, stop_event), 
        daemon=True
    )
    thread_lecture.start()
    log_info("Lecture des réponses Wokwi activée")
    log_info("Lance la simulation Wokwi puis appuie sur une touche\n")

    try:
        import msvcrt
        while True:
            if msvcrt.kbhit():
                touche = msvcrt.getch().decode('utf-8', errors='ignore')

                if touche == '1':
                    envoyer_commande(ser, "#OUVRIR V1")
                elif touche == '2':
                    envoyer_commande(ser, "#OUVRIR V2")
                elif touche == '3':
                    envoyer_commande(ser, "#FERMER V1")
                elif touche == '4':
                    envoyer_commande(ser, "#POMPE ON")
                elif touche == '5':
                    envoyer_commande(ser, "#POMPE OFF")
                elif touche == '6':
                    envoyer_commande(ser, "#ETAT")
                elif touche == '7':
                    envoyer_commande(ser, "#STOP")
                elif touche == '8':
                    envoyer_commande(ser, "#STOP CONFIRM")
                elif touche == '9':
                    envoyer_commande(ser, "#AIDE")
                elif touche == '0':
                    envoyer_commande(ser, "#PROG V1 06:30 20")
                elif touche == 'x':
                    log_info(f"Test numéro non autorisé : {NUMERO_HACK}")
                    envoyer_commande(ser, "#OUVRIR V1")
                elif touche == 'c':
                    print("Entrez la commande : ", end='', flush=True)
                    cmd = input()
                    envoyer_commande(ser, cmd.upper())
                elif touche == 'm':
                    afficher_menu()
                elif touche == 'q':
                    print(f"\n{JAUNE}Fermeture...{RESET}")
                    break

            time.sleep(0.05)

    except KeyboardInterrupt:
        print(f"\n{JAUNE}Arrêt Ctrl+C{RESET}")
    finally:
        stop_event.set()
        ser.close()
        print(f"{VERT}Port fermé.{RESET}")

if __name__ == "__main__":
    afficher_menu()
    main()