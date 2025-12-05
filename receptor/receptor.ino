/* RECEPTOR_Enigma.ino
 * Servidor SPP (Bluetooth) que recibe JSON, descifra y envía a Telegram.
 * Implementa Lógica de Alternancia de Radio (BT <-> WiFi).
 */
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include "esp_bt_device.h"
#include <map>

BluetoothSerial SerialBT;

// =========================================================
//  CONFIGURACIÓN GLOBAL
// =========================================================
const String CLAVE = "CesarClaveSecreta"; 
// ACTUALIZA CON TUS DATOS REALES
#define BOT_TOKEN "YOUR_REAL_BOT_TOKEN_HERE" 
#define CHAT_ID "YOUR_REAL_CHAT_ID_HERE"
#define WIFI_SSID "YOUR_WIFI_SSID_HERE"
#define WIFI_PASS "YOUR_WIFI_PASSWORD_HERE"

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

// FUNCIÓN: Lógica de Avance (Stepping), incluye doble paso
void step_positions(int positions[], String rotors[], int numRotors) {
    if (numRotors < 1) return;

    // 1. El rotor más rápido (índice 0) siempre avanza
    positions[0] = (positions[0] + 1) % 26;

    // 2. Revisar rotores intermedios (doble paso por notch)
    for (int i = 0; i < numRotors - 1; i++) {
        
        int notch_i_index = ALFABETO.indexOf(ROTORES[rotors[i]].notch);
        int notch_i_plus_1_index = ALFABETO.indexOf(ROTORES[rotors[i+1]].notch);

        if (positions[i] == notch_i_index || positions[i+1] == notch_i_plus_1_index) {
            
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
        step_positions(positions, rotors, numRotors); 
        
        char signal = c;
        
        // 1. Rotores (Forward)
        for (int i = 0; i < numRotors; i++) {
            signal = rotor_forward(signal, rotors[i], positions[i]);
        }
        
        // 2. Reflector
        signal = reflector(signal);
        
        // 3. Rotores (Backward)
        for (int i = numRotors - 1; i >= 0; i--){
            signal = rotor_backward(signal, rotors[i], positions[i]);
        }
        out += signal;
    }
    return out;
}

// =========================================================
//  GESTIÓN DE RADIO (BT <-> WIFI)
// =========================================================
bool wifiConnected = false;
bool authenticated = false; 

void setCustomDNS() {
    IPAddress dns(8, 8, 8, 8);
    WiFi.setDNS(dns); 
}

// Desactiva WiFi y reinicia BT en modo Servidor. Resetea la autenticación.
void reactivarBluetooth() {
    WiFi.disconnect(true); 
    WiFi.mode(WIFI_OFF);
    SerialBT.begin("ESP32_Receptor"); 
    authenticated = false; 
    Serial.println("✔ BT Reactivado. Esperando reconexión...");
}

bool connectWiFi() {
    SerialBT.end(); // Desactiva BT para liberar el módulo de radio
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
    // Si falla, reactivamos BT inmediatamente
    reactivarBluetooth(); 
    return false;
}

void syncTime() {
    // Código de sincronización de tiempo.
}

void enviarTelegram(const String& mensaje) {
    for (int attempt = 0; attempt < 2; attempt++) { 
        Serial.print("\nEnviando a Telegram (Intento " + String(attempt + 1) + ")... ");
        
        if(!wifiConnected || WiFi.status() != WL_CONNECTED) {
            if(!connectWiFi()) { 
                Serial.println("No hay conexión WiFi.");
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
        http.setTimeout(45000); 
        
        int httpCode = http.POST(payload);
        
        Serial.print("HTTP "); Serial.println(httpCode);
        
        if(httpCode == 200) {
            Serial.println("✔ ¡ENVIADO A TELEGRAM!");
            http.end(); 
            reactivarBluetooth(); // Revertir a modo BT después de la transferencia WiFi
            return; 
        } else {
            Serial.println("❌ Falló el envío.");
            http.end(); 
            if (attempt == 0) {
                // Si falla, reinicia WiFi y vuelve a intentar
                WiFi.disconnect(true); 
                wifiConnected = false;
            }
        }
    }
    // Si falla en ambos intentos, reactiva Bluetooth
    reactivarBluetooth();
}

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
    Serial.println("\n###################################");
    Serial.println("########## RECEPTOR ENIGMA ##########");
    Serial.println("###################################");
    
    // Inicializar BT como servidor SPP
    WiFi.mode(WIFI_OFF);
    if (!SerialBT.begin("ESP32_Receptor")) {
        Serial.println("❌ Error iniciando Bluetooth");
        while (true) delay(1000);
    }
    delay(800);

    const uint8_t* mac = esp_bt_dev_get_address();
    Serial.printf("MAC REAL SPP Receptor = %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.println("Esperando conexión...");

    // Test inicial de conectividad. connectWiFi desactiva BT. enviarTelegram lo reactiva al final.
    Serial.println("\n-----------------------------");
    Serial.println(" TEST DE CONECTIVIDAD INICIAL");
    Serial.println("-----------------------------");
    if(connectWiFi()) { 
        syncTime();
        enviarTelegram("ENIGMA RECEPTOR INICIADO ✔");
    } else {
        Serial.println("❌ Test inicial fallido.");
    }
}

void loop() {
    // 1. Detección de desconexión:
    if (authenticated && !SerialBT.hasClient()) {
        Serial.println("CLIENTE DESCONECTADO. Esperando nueva conexión...");
        authenticated = false; // Forzar re-AUTH
        return; 
    }
    
    // 2. Si no hay cliente conectado, salir.
    if (!SerialBT.hasClient()) return; 

    // 3. Lógica de Autenticación
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

    // 4. Lógica de Recepción y Descifrado
    if (SerialBT.available()) {
        String json = SerialBT.readStringUntil('\n');
        json.trim();

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err) {
            Serial.println("\n❌ JSON inválido: " + String(err.c_str()));
            SerialBT.println("ERROR: JSON_INVALIDO"); 
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

        // 1. Responder por BT antes de desactivar BT
        SerialBT.println(descifrado);
        
        // 2. Enviar a Telegram (transición a WiFi)
        String msgTelegram = "ENIGMA MENSAJE\nCIFRADO: " + cifrado + "\nDESCIFRADO: " + descifrado;
        enviarTelegram(msgTelegram); 
    }
}
