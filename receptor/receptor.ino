/* RECEPTOR - Lógica Enigma, JSON, y Autenticación con Rotores Variables */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include "esp_bt_device.h"

BluetoothSerial SerialBT;

// =========================================================
//  CONFIGURACIÓN GLOBAL
// =========================================================
// !!! CONFIGURACIÓN REQUERIDA !!!
// La CLAVE debe ser idéntica a la configurada en el EMISOR.
const String CLAVE = "CLAVESECRETA123"; // <--- REEMPLAZAR

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

String limpiar(String s) {
  s.trim();
  s.replace("\r", "");
  s.replace("\n", "");
  return s;
}

// =========================================================
//  SETUP Y LOOP
// =========================================================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== RECEPTOR ENIGMA (Rotores Variables) ===");

  if (!SerialBT.begin("ESP32_Receptor")) {
    Serial.println("Error iniciando Bluetooth");
    while (true) delay(1000);
  }
  delay(800);

  const uint8_t* mac = esp_bt_dev_get_address();
  Serial.printf("MAC REAL SPP Receptor = %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.println("Esperando conexión...");
}

void loop() {
  if (!SerialBT.hasClient()) return;

  if (!authenticated) {
    if (SerialBT.available()) {
      String raw = SerialBT.readStringUntil('\n');
      raw = limpiar(raw);

      if (raw.startsWith("AUTH:")) {
        String clave = raw.substring(5);
        clave = limpiar(clave);
        if (clave == CLAVE) {
          authenticated = true;
          Serial.println("AUTENTICACIÓN CORRECTA");
          SerialBT.println("AUTH_OK");
        } else {
          Serial.println("Clave incorrecta -> desconectando");
          SerialBT.disconnect();
        }
      }
    }
    return;
  }

  if (SerialBT.available()) {
    String json = SerialBT.readStringUntil('\n');
    json.trim();

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
      Serial.println("JSON inválido");
      return;
    }

    String cifrado = doc["mensaje"].as<String>();
    
    int numRotores = doc["numRotores"] | 3; 
    
    String rotors[5];
    int positions[5] = {0, 0, 0, 0, 0}; 
    
    JsonArray rotorsArray = doc["rotors"];
    for (int i = 0; i < numRotores && i < rotorsArray.size(); i++) {
      rotors[i] = rotorsArray[i].as<String>();
    }

    String descifrado = enigma_process(cifrado, rotors, positions, numRotores);

    Serial.println("Recibido cifrado: " + cifrado);
    Serial.println("Descifrado: " + descifrado);

    SerialBT.println(descifrado);
  }
}