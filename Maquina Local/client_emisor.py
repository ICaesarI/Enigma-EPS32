import socket
import json
import re

# ============================================================
#   MISMO SISTEMA ENIGMA COMPLETO
# ============================================================

ROTORES = {
    "I":  ("EKMFLGDQVZNTOWYHXUSPAIBRCJ", "Q"),
    "II": ("AJDKSIRUXBLHWTMCQGZNPYFVOE", "E"),
    "III":("BDFHJLCPRTXVZNYEIWGAKMUSQO", "V"),
    "IV": ("ESOVPZJAYQUIRHXLNFTGKDCMWB", "J"),
    "V":  ("VZBRGITYUPSDNHLXAWMJQOFEKC", "Z")
}

REFLECTOR_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT"
ALFABETO = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def sanitize(msg):
    return re.sub(r'[^A-Z]', '', msg.upper())

def rotor_forward(c, rotor, offset):
    wiring, _ = ROTORES[rotor]
    i = (ALFABETO.index(c) + offset) % 26
    return wiring[i]

def rotor_backward(c, rotor, offset):
    wiring, _ = ROTORES[rotor]
    i = wiring.index(c)
    return ALFABETO[(i - offset) % 26]

def reflector(c):
    return REFLECTOR_B[ALFABETO.index(c)]

def step_positions(positions, notch1, notch2, notch3):
    p1, p2, p3 = positions
    doble_paso = (p2 == ALFABETO.index(notch2))

    p1 = (p1 + 1) % 26

    if p1 == ALFABETO.index(notch1) or doble_paso:
        p2 = (p2 + 1) % 26
        if p2 == ALFABETO.index(notch2):
            p3 = (p3 + 1) % 26

    return (p1, p2, p3)

def enigma_process(msg, rotors, pos_ini):
    r1, r2, r3 = rotors
    p1, p2, p3 = pos_ini

    _, notch1 = ROTORES[r1]
    _, notch2 = ROTORES[r2]
    _, notch3 = ROTORES[r3]

    out = ""

    for c in msg:
        p1, p2, p3 = step_positions((p1, p2, p3), notch1, notch2, notch3)

        c1 = rotor_forward(c, r1, p1)
        c2 = rotor_forward(c1, r2, p2)
        c3 = rotor_forward(c2, r3, p3)

        c4 = reflector(c3)

        c5 = rotor_backward(c4, r3, p3)
        c6 = rotor_backward(c5, r2, p2)
        c7 = rotor_backward(c6, r1, p1)

        out += c7

    return out

# ============================================================
#   CLIENTE SOCKET (EMISOR)
# ============================================================

HOST = "192.168.x.x"  # Cambia por la IP de tu PC si el servidor está en otra máquina
PORT = 5000

print("=== EMISOR ENIGMA ===")

mensaje = input("Mensaje a enviar: ")
mensaje = sanitize(mensaje)

print("\nElige los 3 rotores (I–V) en orden:")
r1 = input("Rotor 1: ").upper()
r2 = input("Rotor 2: ").upper()
r3 = input("Rotor 3: ").upper()
rotors = [r1, r2, r3]

# Posiciones iniciales fijas en 'A'
p1, p2, p3 = 'A', 'A', 'A'
pos_ini = [ALFABETO.index(p1), ALFABETO.index(p2), ALFABETO.index(p3)]

cifrado = enigma_process(mensaje, rotors, pos_ini)
print(f"\nMensaje cifrado: {cifrado}")

# Enviar al receptor
paquete = json.dumps({
    "mensaje": cifrado,
    "rotors": rotors,
    "pos": [p1, p2, p3]
})

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, PORT))
client.send(paquete.encode())

respuesta = client.recv(4096).decode()
print(f"\nRespuesta (descifrada): {respuesta}")

client.close()