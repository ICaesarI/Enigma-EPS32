/* RECEPTOR - Lógica Enigma, JSON, y Autenticación */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h> 
#include "esp_bt_device.h" 

BluetoothSerial SerialBT;

// =========================================================
//  CONFIGURACIÓN GLOBAL
// =========================================================
// !!! La CLAVE debe ser idéntica a la configurada en el EMISOR.
const String CLAVE = "CLAVESECRETA"; 

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

void step_positions(int &p1, int &p2, int &p3, String r1, String r2, String r3) {
  char notch1 = ROTORES[r1].notch;
  char notch2 = ROTORES[r2].notch;
  bool doble = (p2 == ALFABETO.indexOf(notch2));
  p1 = (p1 + 1) % 26;
  if (p1 == ALFABETO.indexOf(notch1) || doble) {
    p2 = (p2 + 1) % 26;
    if (p2 == ALFABETO.indexOf(notch2)) p3 = (p3 + 1) % 26;
  }
}

String enigma_process(String msg, String r1, String r2, String r3, int p1, int p2, int p3) {
  String out = "";
  msg = sanitize(msg);
  for (char c : msg) {
    step_positions(p1, p2, p3, r1, r2, r3);
    char c1 = rotor_forward(c, r1, p1);
    char c2 = rotor_forward(c1, r2, p2);
    char c3 = rotor_forward(c2, r3, p3);
    char c4 = reflector(c3);
    char c5 = rotor_backward(c4, r3, p3);
    char c6 = rotor_backward(c5, r2, p2);
    char c7 = rotor_backward(c6, r1, p1);
    out += c7;
  }
  return out;
}

// =========================================================
//  SETUP Y LOOP
// =========================================================
bool authenticated = false;

String limpiar(String s) {
  s.trim();
  s.replace("\r", "");
  s.replace("\n", "");
  return s;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n###################################");
  Serial.println("########## RECEPTOR ENIGMA ##########");
  Serial.println("###################################");

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

  // --- Lógica de Autenticación ---
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

  // --- Lógica de Recepción y Descifrado (Solo si está autenticado) ---
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
    String r1 = doc["rotors"][0].as<String>();
    String r2 = doc["rotors"][1].as<String>();
    String r3 = doc["rotors"][2].as<String>();
    
    int p1 = 0, p2 = 0, p3 = 0;

    String descifrado = enigma_process(cifrado, r1, r2, r3, p1, p2, p3);

    Serial.println("\n-----------------------------");
    Serial.println(" MENSAJE RECIBIDO");
    Serial.println("-----------------------------");
    Serial.println("Cifrado: " + cifrado);
    Serial.println("Descifrado: " + descifrado);
    Serial.println("-----------------------------");

    SerialBT.println(descifrado);
  }
}