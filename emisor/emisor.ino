/* EMISOR - Lógica Enigma, JSON, y Autenticación */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h> 
#include "esp_bt_device.h" 

BluetoothSerial SerialBT;

// =========================================================
//  CONFIGURACIÓN GLOBAL
// =========================================================
// !!! Reemplaza esta MAC con la MAC REAL obtenida del módulo ESP32 que actúa como RECEPTOR.
//Ejemplo = "0x94, 0x51, 0xDC, 0x5B, 0xE0, 0xD2"
uint8_t receptorMAC[] = {}; 
// La CLAVE debe ser idéntica a la configurada en el RECEPTOR.
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

void waitForSerialInput() {
  while (!Serial.available()) { delay(10); }
}

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

  Serial.println("-----------------------------");
  Serial.println(" PROCESO DE CONEXIÓN Y AUTH");
  Serial.println("-----------------------------");
  Serial.println("Conectando al receptor (por MAC)...");
  
  while (!SerialBT.connect(receptorMAC)) {
    Serial.println("Fallo al conectar - reintentando en 2s...");
    delay(2000);
  }
  Serial.println("✔ Conectado al receptor!");

  Serial.println("Enviando AUTH...");
  SerialBT.println("AUTH:" + CLAVE);

  unsigned long t0 = millis();
  while (millis() - t0 < 3000) { 
    if (SerialBT.available()) {
      String r = SerialBT.readStringUntil('\n');
      r.trim();
      if (r == "AUTH_OK") {
        authenticated = true;
        Serial.println("✔ Autenticación OK");
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

  Serial.println("\nListo. Escribe mensajes para cifrar en el monitor.");
}

void loop() {
  if (!authenticated || !SerialBT.connected()) {
    delay(200);
    return;
  }

  Serial.println("\n=================================");
  Serial.println("=== INGRESO DE NUEVO MENSAJE ===");
  Serial.println("=================================");
  Serial.println("Escribe el mensaje (Enter):");
  waitForSerialInput();
  String msg = Serial.readStringUntil('\n');
  msg.trim();

  Serial.println("Rotor 1 (I,II,III,IV,V):");
  waitForSerialInput(); String r1 = Serial.readStringUntil('\n'); r1.trim();
  Serial.println("Rotor 2 (I,II,III,IV,V):");
  waitForSerialInput(); String r2 = Serial.readStringUntil('\n'); r2.trim();
  Serial.println("Rotor 3 (I,II,III,IV,V):");
  waitForSerialInput(); String r3 = Serial.readStringUntil('\n'); r3.trim();

  int p1 = 0, p2 = 0, p3 = 0;

  String cifrado = enigma_process(msg, r1, r2, r3, p1, p2, p3);
  Serial.println("Mensaje cifrado: " + cifrado);

  StaticJsonDocument<512> doc;
  doc["mensaje"] = cifrado;
  JsonArray rot = doc.createNestedArray("rotors");
  rot.add(r1); rot.add(r2); rot.add(r3);
  JsonArray pos = doc.createNestedArray("pos");
  pos.add("A"); pos.add("A"); pos.add("A");

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
      Serial.println("✔ Respuesta descifrada: " + respuesta);
      respuestaRecibida = true;
      break;
    }
    delay(10);
  }
  if (!respuestaRecibida) {
    Serial.println("Timeout - No se recibió respuesta");
  }
}