/* EMISOR_Enigma.ino
 * Cliente SPP que cifra con Enigma y envía JSON por Bluetooth.
 */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include "esp_bt_device.h"
#include <map>

BluetoothSerial SerialBT;

// =========================================================
//  CONFIGURACIÓN GLOBAL
// =========================================================
// MAC REAL del dispositivo Receptor. ¡ACTUALIZAR!
uint8_t receptorMAC[] = {0x6C, 0xC8, 0x40, 0x8C, 0xB1, 0xE6};  
const String CLAVE = "CesarClaveSecreta"; // Debe coincidir con el Receptor

// =========================================================
//  LÓGICA ENIGMA
// =========================================================
String ALFABETO = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
struct RotorData {
    String wiring;
    char notch;
};
// Definición de rotores y sus muescas (notch)
std::map<String, RotorData> ROTORES = {
    {"I",   {"EKMFLGDQVZNTOWYHXUSPAIBRCJ", 'Q'}},
    {"II",  {"AJDKSIRUXBLHWTMCQGZNPYFVOE", 'E'}},
    {"III", {"BDFHJLCPRTXVZNYEIWGAKMUSQO", 'V'}},
    {"IV",  {"ESOVPZJAYQUIRHXLNFTGKDCMWB", 'J'}},
    {"V",   {"VZBRGITYUPSDNHLXAWMJQOFEKC", 'Z'}}
};
String REFLECTOR_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT"; // Reflector fijo tipo B

String sanitize(String msg) {
    String out = "";
    msg.toUpperCase();
    for (char c : msg) if (c >= 'A' && c <= 'Z') out += c;
    return out;
}

char rotor_forward(char c, String rotor, int offset) {
    String wiring = ROTORES[rotor].wiring;
    int idx = (ALFABETO.indexOf(c) + offset) % 26;
    return wiring[idx];
}
char rotor_backward(char c, String rotor, int offset) {
    String wiring = ROTORES[rotor].wiring;
    int idx = wiring.indexOf(c);
    return ALFABETO[(idx - offset + 26) % 26];
}
char reflector(char c) {
    return REFLECTOR_B[ALFABETO.indexOf(c)];
}

// Lógica de movimiento de rotores (stepping), incluye doble paso
void step_positions(int positions[], String rotors[], int numRotors) {
    if (numRotors < 1) return;

    // 1. El rotor más rápido (índice 0) siempre avanza
    positions[0] = (positions[0] + 1) % 26;

    // 2. Revisar los rotores intermedios por notch (doble paso)
    for (int i = 0; i < numRotors - 1; i++) {
        
        int notch_i_index = ALFABETO.indexOf(ROTORES[rotors[i]].notch);
        
        // La condición verifica si el rotor actual está en su notch, 
        // o si el rotor siguiente está en su notch (para el doble paso).
        if (positions[i] == notch_i_index || positions[i+1] == ALFABETO.indexOf(ROTORES[rotors[i+1]].notch)) {
            
            positions[i + 1] = (positions[i + 1] + 1) % 26;
        } else {
            break;
        }
    }
}

String enigma_process(String msg, String rotors[], int positions[], int numRotors) {
    String out = "";
    msg = sanitize(msg);
    for (char c : msg) {
        step_positions(positions, rotors, numRotors);  // Mover rotores antes de cifrar
        
        char signal = c;
        
        // 1. Rotores (Forward)
        for (int i = 0; i < numRotors; i++) {
            signal = rotor_forward(signal, rotors[i], positions[i]);
        }
        
        // 2. Reflector
        signal = reflector(signal);
        
        // 3. Rotores (Backward)
        for (int i = numRotors - 1; i >= 0; i--) {
            signal = rotor_backward(signal, rotors[i], positions[i]);
        }
        out += signal;
    }
    return out;
}

// =========================================================
//  FUNCIONES AUXILIARES (Entrada Serial)
// =========================================================
bool authenticated = false;

void waitForSerialInput() {
    while (!Serial.available()) { delay(10); }
}

int leerNumero() {
    while (!Serial.available()) { delay(10); }
    String input = Serial.readStringUntil('\n');
    input.trim();
    return input.toInt();
}

String leerTexto() {
    while (!Serial.available()) { delay(10); }
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    return input;
}

void mostrarRotoresDisponibles() {
    Serial.println("\n-----------------------------");
    Serial.println(" ROTORES DISPONIBLES");
    Serial.println("-----------------------------");
    Serial.println("I   - Notch: Q");
    Serial.println("II  - Notch: E");    
    Serial.println("III - Notch: V");
    Serial.println("IV  - Notch: J");
    Serial.println("V   - Notch: Z");
    Serial.println("-----------------------------");
}

// =========================================================
//  SETUP Y LOOP
// =========================================================

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n#################################");
    Serial.println("########## EMISOR ENIGMA ##########");
    Serial.println("#################################");

    // Iniciar Bluetooth como Cliente SPP
    if (!SerialBT.begin("ESP32_Emisor", true)) {
        Serial.println("Error iniciando Bluetooth");
        while (true) delay(1000);
    }
    delay(800);

    const uint8_t* mac = esp_bt_dev_get_address();
    Serial.printf("MAC REAL SPP Emisor = %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.println("\n-----------------------------");
    Serial.println(" PROCESO DE CONEXIÓN INICIAL");
    Serial.println("-----------------------------");
    
    // Bucle robusto para la conexión inicial al Receptor
    while (!SerialBT.connect(receptorMAC)) {
        Serial.println("Fallo al conectar - reintentando en 2s...");
        delay(2000);
    }
    Serial.println("✔ Conectado al receptor!");

    // Autenticación
    Serial.println("\n Enviando AUTH...");
    SerialBT.println("AUTH:" + CLAVE);

    unsigned long t0 = millis();
    while (millis() - t0 < 3000) {
        if (SerialBT.available()) {
            String r = SerialBT.readStringUntil('\n');
            r.trim();
            if (r == "AUTH_OK") {
                authenticated = true;
                Serial.println("Autenticación OK");
                break;
            }
        }
        delay(10);
    }

    if (!authenticated) {
        Serial.println("No se autenticó - desconectando");
        SerialBT.disconnect();
        while (true) delay(1000); // Bloquear si falla la autenticación inicial
    }

    Serial.println("\n-----------------------------");
    Serial.println(" Listo. Escribe mensajes para cifrar.");
    Serial.println("-----------------------------");
}

// Gestión de Reconexión, Re-Autenticación y Ciclo de Cifrado
void loop() {
    // 1. Manejo de Conexión Perdida (Reconexión y Re-Autenticación)
    if (!SerialBT.connected()) {
        Serial.println("\nCONEXIÓN PERDIDA. Intentando reconectar y re-autenticar...");
        authenticated = false;
        
        // Delay crucial para esperar que el Receptor reactive su servicio BT después de usar WiFi
        Serial.println("Esperando que el Receptor reactive BT (3s)...");
        delay(3000);  
        
        while (!SerialBT.connect(receptorMAC)) {
            Serial.println("Fallo al conectar - reintentando en 2s...");
            delay(2000);
        }
        Serial.println("✔ Reconectado. Iniciando Re-autenticación...");
        
        SerialBT.println("AUTH:" + CLAVE);
    }

    // 2. Proceso de Re-autenticación (si reconectó pero no está autenticado)
    if (SerialBT.connected() && !authenticated) {
        
        unsigned long t0 = millis();
        while (millis() - t0 < 4000) { // Timeout de 4 segundos para respuesta AUTH_OK
            if (SerialBT.available()) {
                String r = SerialBT.readStringUntil('\n'); 
                r.trim(); 
                
                if (r == "AUTH_OK") { 
                    authenticated = true;
                    Serial.println("✔ Re-Autenticación OK");
                    break; 
                }
            }
            delay(10);
        }
        
        if (!authenticated) {
            Serial.println("❌ Falló re-autenticación. Retornando.");
            return;
        }
    }
    
    // Si no está autenticado después de los intentos, esperar
    if (!authenticated) {
          delay(500);
          return;
    }


    // =========================================================
    // LÓGICA DE ENVÍO (SÓLO SI ESTÁ CONECTADO Y AUTENTICADO)
    // =========================================================

    Serial.println("\n=================================");
    Serial.println("=== INGRESO DE NUEVO MENSAJE ===");
    Serial.println("=================================");
    Serial.println("Escribe el mensaje:");
    String msg = leerTexto();

    Serial.println("\n¿Cuántos rotores quieres usar? (3-5):");
    int numRotores = 0;
    while (numRotores < 3 || numRotores > 5) {
        numRotores = leerNumero();
        if (numRotores < 3 || numRotores > 5) {
            Serial.println("Error: Debe ser entre 3 y 5. Intenta de nuevo:");
        }
    }

    mostrarRotoresDisponibles();

    String rotors[5]; 
    int positions[5] = {0, 0, 0, 0, 0}; // Posiciones iniciales fijas en 'A' (índice 0)
    
    Serial.println("\nSelecciona los rotores en orden (de izquierda a derecha):");
    
    for (int i = 0; i < numRotores; i++) {
        Serial.printf("Rotor %d/%d (I,II,III,IV,V): ", i + 1, numRotores);
        String rotor = leerTexto();
        
        while (ROTORES.find(rotor) == ROTORES.end()) {
            Serial.println("Rotor no válido. Usa I,II,III,IV,V:");
            rotor = leerTexto();
        }
        
        rotors[i] = rotor;
    }

    Serial.println("\n-----------------------------");
    Serial.println(" CONFIGURACIÓN FINAL (Posiciones iniciales: A)");
    Serial.println("-----------------------------");
    Serial.print("Rotores: ");
    for (int i = 0; i < numRotores; i++) {
        Serial.print(rotors[i]);
        if (i < numRotores - 1) Serial.print(" - ");
    }
    Serial.println();

    String cifrado = enigma_process(msg, rotors, positions, numRotores);
    Serial.println("Mensaje original: " + msg);
    Serial.println("Mensaje cifrado: " + cifrado);

    // Creación y Serialización del JSON
    StaticJsonDocument<512> doc;
    doc["mensaje"] = cifrado;
    JsonArray rot = doc.createNestedArray("rotors");
    for (int i = 0; i < numRotores; i++) {
        rot.add(rotors[i]);
    }
    JsonArray pos = doc.createNestedArray("pos");
    for (int i = 0; i < numRotores; i++) {
        pos.add("A"); // Se envía 'A' como indicador de posición inicial 0
    }
    doc["numRotores"] = numRotores;

    String json;
    serializeJson(doc, json);

    SerialBT.println(json);
    Serial.println("\nJSON enviado. Esperando respuesta descifrada...");

    unsigned long t0_resp = millis();
    bool respuestaRecibida = false;
    
    while (millis() - t0_resp < 5000) { // Esperar 5 segundos por respuesta
        if (SerialBT.available()) {
            String respuesta = SerialBT.readStringUntil('\n');
            respuesta.trim();
            Serial.println("Respuesta descifrada del Receptor: " + respuesta);
            respuestaRecibida = true;
            break;
        }
        delay(10);
    }
    
    if (!respuestaRecibida) {
        Serial.println("Timeout - No se recibió respuesta. El Receptor puede estar ocupado con WiFi.");
    }
    
    Serial.println("\nPresiona ENTER para enviar otro mensaje...");
    waitForSerialInput();
}
