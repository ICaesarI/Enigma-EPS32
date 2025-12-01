/* RECEPTOR_Enigma.ino
   - Servidor SPP (Bluetooth)
   - Recibe JSON, descifra y envía a Telegram
*/
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_bt_device.h"

BluetoothSerial SerialBT;

// =========================================================
// !!! CONFIGURACIÓN WI-FI Y TELEGRAM !!!
// =========================================================
// Reemplaza con tu Bot Token real de Telegram
#define BOT_TOKEN "0000000000:AABBCcDdEeFfGgHhIiJjKkLlMmNnOoPpQq" // <--- REEMPLAZAR
// Reemplaza con tu Chat ID real de Telegram
#define CHAT_ID "0000000000" // <--- REEMPLAZAR
// Reemplaza con el nombre de tu red Wi-Fi
#define WIFI_SSID "TU_RED_WIFI_AQUI" // <--- REEMPLAZAR
// Reemplaza con la contraseña de tu red Wi-Fi
#define WIFI_PASS "TU_CONTRASENA_AQUI" // <--- REEMPLAZAR

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
//  FUNCIONES WIFI Y TELEGRAM (Estabilidad BT/WiFi)
// =========================================================
bool wifiConnected = false;

void setCustomDNS() {
    IPAddress dns(8, 8, 8, 8);
    WiFi.setDNS(dns); 
}

bool connectWiFi() {
    SerialBT.end(); 
    Serial.print("Conectando WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS); 
    
    for(int i = 0; i < 60; i++) { 
        if(WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            setCustomDNS(); 
            Serial.println(" ✔");
            return true;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.println(" ❌");
    wifiConnected = false;
    return false;
}

void syncTime() { 
    Serial.print("Sincronizando hora (NTP)...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov"); 
    time_t now = time(nullptr);
    while (now < 100000) { delay(500); now = time(nullptr); Serial.print("."); }
    Serial.println(" ✔");
}

void reactivarBluetooth() {
    WiFi.disconnect(true); 
    WiFi.mode(WIFI_OFF);
    SerialBT.begin("ESP32_Receptor"); 
    Serial.println("✔ BT Reactivado.");
}

void enviarTelegram(const String& mensaje) {
    for (int attempt = 0; attempt < 2; attempt++) { 
        Serial.print("\nEnviando a Telegram (Intento " + String(attempt + 1) + ")... ");
        
        if(!wifiConnected || WiFi.status() != WL_CONNECTED) {
            if(!connectWiFi()) { 
                Serial.println("No hay conexión WiFi.");
                if (attempt == 0) continue; 
                reactivarBluetooth();
                return;
            }
        }
        
        HTTPClient http;
        String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
        
        String encodedMsg = mensaje;
        encodedMsg.replace(" ", "%20");
        encodedMsg.replace("\n", "%0A");
        
        String payload = "chat_id=" + String(CHAT_ID) + "&text=" + encodedMsg;
        
        http.begin(url);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.setTimeout(30000); 
        
        int httpCode = http.POST(payload);
        
        Serial.print("HTTP "); Serial.println(httpCode);
        
        if(httpCode == 200) {
            Serial.println("✔ ¡ENVIADO A TELEGRAM!");
            http.end(); 
            reactivarBluetooth(); 
            return; 
        } else {
            Serial.println("❌ Falló el envío.");
            http.end(); 
            if (attempt == 0) {
                WiFi.disconnect(true); 
                wifiConnected = false;
            }
        }
    }
    reactivarBluetooth();
}
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
  
  // 1. Apagar WiFi para inicializar BT sin conflictos
  WiFi.mode(WIFI_OFF);

  // 2. Iniciar BT como servidor SPP
  if (!SerialBT.begin("ESP32_Receptor")) {
    Serial.println("❌ Error iniciando Bluetooth");
    while (true) delay(1000);
  }
  delay(800);

  const uint8_t* mac = esp_bt_dev_get_address();
  Serial.printf("MAC REAL SPP Receptor = %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("Esperando conexión...");

  // Test INICIAL
  Serial.println("\n-----------------------------");
  Serial.println(" TEST DE CONECTIVIDAD INICIAL");
  Serial.println("-----------------------------");
  SerialBT.end(); 
  if(connectWiFi()) { 
      syncTime();
      enviarTelegram("ENIGMA+RECEPTOR+INICIADO+✅");
  } else {
      Serial.println("❌ Test inicial fallido (No se pudo enviar a Telegram).");
      reactivarBluetooth(); 
  }
}

void loop() {
  if (!SerialBT.hasClient()) return;

  // Lógica de Autenticación
  if (!authenticated) {
    if (SerialBT.available()) {
      String raw = SerialBT.readStringUntil('\n');
      raw = limpiar(raw);

      if (raw.startsWith("AUTH:")) {
        String clave = raw.substring(5);
        clave = limpiar(clave);
        if (clave == CLAVE) {
          authenticated = true;
          Serial.println("\nAUTENTICACIÓN CORRECTA");
          SerialBT.println("AUTH_OK");
        } else {
          Serial.println("\n❌ Clave incorrecta -> desconectando");
          SerialBT.disconnect();
        }
      }
    }
    return;
  }

  // Lógica de Recepción y Descifrado
  if (SerialBT.available()) {
    String json = SerialBT.readStringUntil('\n');
    json.trim();

    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
      Serial.println("\n❌ JSON inválido: " + String(err.c_str()));
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

    Serial.println("\n-----------------------------");
    Serial.println(" MENSAJE RECIBIDO");
    Serial.println("-----------------------------");
    Serial.println("Cifrado: " + cifrado);
    Serial.println("Descifrado: " + descifrado);
    Serial.println("-----------------------------");

    // 1. Responder por BT
    SerialBT.println(descifrado);
    
    // 2. Enviar a Telegram
    String msgTelegram = "ENIGMA+MENSAJE%0ACIFRADO:+" + cifrado + "%0ADESCIFRADO:+" + descifrado;
    enviarTelegram(msgTelegram); 
  }
}