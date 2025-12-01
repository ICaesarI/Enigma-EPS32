import socket
import json
import re

# ============================================================
#   ENIGMA: ROTORES, REFLECTOR, ALFABETO
# ============================================================

ROTORES = {
    "I":  ("EKMFLGDQVZNTOWYHXUSPAIBRCJ", "Q"),  # Notch
    "II": ("AJDKSIRUXBLHWTMCQGZNPYFVOE", "E"),
    "III":("BDFHJLCPRTXVZNYEIWGAKMUSQO", "V"),
    "IV": ("ESOVPZJAYQUIRHXLNFTGKDCMWB", "J"),
    "V":  ("VZBRGITYUPSDNHLXAWMJQOFEKC", "Z")
}

REFLECTOR_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT"
ALFABETO = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

# ============================================================
#   FUNCIONES ENIGMA
# ============================================================

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

        # Adelante
        c1 = rotor_forward(c, r1, p1)
        c2 = rotor_forward(c1, r2, p2)
        c3 = rotor_forward(c2, r3, p3)

        # Reflector
        c4 = reflector(c3)

        # Atras
        c5 = rotor_backward(c4, r3, p3)
        c6 = rotor_backward(c5, r2, p2)
        c7 = rotor_backward(c6, r1, p1)

        out += c7

    return out

# ============================================================
#   SERVIDOR SOCKET (RECEPTOR)
# ============================================================

HOST = "0.0.0.0"
PORT = 5000

print("=== RECEPTOR ENIGMA (Servidor) ===")

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(1)
print(f"Esperando conexión en puerto {PORT}...")

while True:
    conn, addr = server.accept()
    print(f"Conexión de {addr}")

    data = conn.recv(4096)
    if not data:
        conn.close()
        continue

    paquete = json.loads(data.decode())

    mensaje_cifrado = paquete["mensaje"]
    rotors = paquete["rotors"]
    p1, p2, p3 = paquete["pos"]
    pos_ini = [ALFABETO.index(p1), ALFABETO.index(p2), ALFABETO.index(p3)]

    descifrado = enigma_process(mensaje_cifrado, rotors, pos_ini)

    print(f"Mensaje recibido: {mensaje_cifrado}")
    print(f"Descifrado: {descifrado}")

    conn.send(descifrado.encode())
    conn.close()
