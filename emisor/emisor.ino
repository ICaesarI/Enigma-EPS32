/* EMISOR_Enigma.ino
   - Cliente SPP
   - Cifra con Enigma y envía JSON por Bluetooth
*/

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include "esp_bt_device.h"

BluetoothSerial SerialBT;

// =========================================================
// !!! CONFIGURACIÓN BLUETOOTH Y CLAVE SECRETA !!!
// =========================================================
// Reemplaza esta MAC con la MAC REAL obtenida del módulo ESP32 que actúa como RECEPTOR.
// Ejemplo: {0x6C, 0xC8, 0x40, 0x8C, 0xB1, 0xE6}
uint8_t receptorMAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // <--- REEMPLAZAR
// La CLAVE debe ser idéntica a la configurada en el RECEPTOR.
const String CLAVE = "CesarClaveSecreta"; // <--- REEMPLAZAR (USAR LA CLAVE REAL)

// =========================================================
//  LÓGICA ENIGMA
// =========================================================
String ALFABETO = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
struct RotorData {
  String wiring;
  char notch;
};
std::map<String, RotorData> ROTORES = {
  {"I",   {"EKMFLGDQVZNTOWYHXUSPAIBRCJ", 'Q'}},
  {"II",  {"AJDKSIRUXBLHWTMCQGZNPYFVOE", 'E'}},
  {"III", {"BDFHJLCPRTXVZNYEIWGAKMUSQO", 'V'}},
  {"IV",  {"ESOVPZJAYQUIRHXLNFTGKDCMWB", 'J'}},
  {"V",   {"VZBRGITYUPSDNHLXAWMJQOFEKC", 'Z'}}
};
String REFLECTOR_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT";

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
void step_positions(int positions[], String rotors[], int numRotors) {
  bool doblePaso = false;
  positions[0] = (positions[0] + 1) % 26;
  for (int i = 0; i < numRotors - 1; i++) {
    char notch = ROTORES[rotors[i]].notch;
    if (positions[i] == ALFABETO.indexOf(notch) || doblePaso) {
      positions[i + 1] = (positions[i + 1] + 1) % 26;
      doblePaso = (positions[i + 1] == ALFABETO.indexOf(ROTORES[rotors[i + 1]].notch));
    } else {
      break;
    }
  }
}
String enigma_process(String msg, String rotors[], int positions[], int numRotors) {
  String out = "";
  msg = sanitize(msg);
  for (char c : msg) {
    step_positions(positions, rotors, numRotors);
    char signal = c;
    for (int i = 0; i < numRotors; i++) {
      signal = rotor_forward(signal, rotors[i], positions[i]);
    }
    signal = reflector(signal);
    for (int i = numRotors - 1; i >= 0; i--) {
      signal = rotor_backward(signal, rotors[i], positions[i]);
    }
    out += signal;
  }
  return out;
}

// =========================================================
//  FUNCIONES AUXILIARES
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

  if (!SerialBT.begin("ESP32_Emisor", true)) {
    Serial.println("Error iniciando Bluetooth");
    while (true) delay(1000);
  }
  delay(800);

  const uint8_t* mac = esp_bt_dev_get_address();
  Serial.printf("MAC REAL SPP Emisor = %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.println("\n-----------------------------");
  Serial.println(" PROCESO DE CONEXIÓN");
  Serial.println("-----------------------------");
  Serial.println("Conectando al receptor (por MAC)...");
  while (!SerialBT.connect(receptorMAC)) {
    Serial.println("Fallo al conectar - reintentando en 2s...");
    delay(2000);
  }
  Serial.println("✔ Conectado al receptor!");

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
    while (true) delay(1000);
  }

  Serial.println("\n-----------------------------");
  Serial.println(" Listo. Escribe mensajes para cifrar.");
  Serial.println("-----------------------------");
}

void loop() {
  if (!authenticated || !SerialBT.connected()) {
    if (!SerialBT.connected()) {
        Serial.println("\nCONEXIÓN PERDIDA. Intentando reconectar...");
        authenticated = false;
        
        Serial.println("Reconectando al receptor (por MAC)...");
        while (!SerialBT.connect(receptorMAC)) {
            Serial.println("Fallo al conectar - reintentando en 2s...");
            delay(2000);
        }
        Serial.println("✔ Reconectado. Re-autenticando...");
        SerialBT.println("AUTH:" + CLAVE);
        
        unsigned long t0 = millis();
        while (millis() - t0 < 3000) {
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
            Serial.println("Falló re-autenticación. Reinicia el Emisor.");
            return;
        }
    } else {
        delay(200);
        return;
    }
  }

  Serial.println("\n=================================");
  Serial.println("=== INGRESO DE NUEVO MENSAJE ===");
  Serial.println("=================================");
  Serial.println("Escribe el mensaje (solo A-Z, se convertirán a mayúsculas):");
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
  int positions[5] = {0, 0, 0, 0, 0}; 
  
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
  Serial.println(" CONFIGURACIÓN FINAL");
  Serial.println("-----------------------------");
  Serial.print("Rotores: ");
  for (int i = 0; i < numRotores; i++) {
    Serial.print(rotors[i]);
    if (i < numRotores - 1) Serial.print(" - ");
  }
  Serial.println();
  Serial.print("Posiciones: ");
  for (int i = 0; i < numRotores; i++) {
    Serial.print("A");
    if (i < numRotores - 1) Serial.print(" - ");
  }
  Serial.println();

  String cifrado = enigma_process(msg, rotors, positions, numRotores);
  Serial.println("Mensaje original: " + msg);
  Serial.println("Mensaje cifrado: " + cifrado);

  StaticJsonDocument<512> doc;
  doc["mensaje"] = cifrado;
  JsonArray rot = doc.createNestedArray("rotors");
  for (int i = 0; i < numRotores; i++) {
    rot.add(rotors[i]);
  }
  JsonArray pos = doc.createNestedArray("pos");
  for (int i = 0; i < numRotores; i++) {
    pos.add("A");
  }
  doc["numRotores"] = numRotores;

  String json;
  serializeJson(doc, json);

  SerialBT.println(json);
  Serial.println("\nJSON enviado. Esperando respuesta...");

  unsigned long t0 = millis();
  bool respuestaRecibida = false;
  
  while (millis() - t0 < 5000) {
    if (SerialBT.available()) {
      String respuesta = SerialBT.readStringUntil('\n');
      respuesta.trim();
      Serial.println("Respuesta descifrada: " + respuesta);
      respuestaRecibida = true;
      break;
    }
    delay(10);
  }
  
  if (!respuestaRecibida) {
    Serial.println("Timeout - No se recibió respuesta");
  }
  
  Serial.println("\nPresiona ENTER para enviar otro mensaje...");
  waitForSerialInput();
}