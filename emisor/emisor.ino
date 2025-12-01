#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

// MAC del receptor
uint8_t receptorMAC[] = {0x94, 0x51, 0xDC, 0x5B, 0xE0, 0xD2};

void setup() {
  Serial.begin(115200);
  Serial.println("=== EMISOR ENCENDIDO ===");

  if (!SerialBT.begin("ESP32_Emisor", true)) {
    Serial.println("Error iniciando Bluetooth en modo master");
    while (true);
  }

  Serial.println("Intentando conectar al receptor...");
  
  if (SerialBT.connect(receptorMAC)) {
    Serial.println("CONECTADO AL RECEPTOR!");
  } else {
    Serial.println("No se pudo conectar. Reinicia el emisor.");
  }
}

void loop() {
  if (SerialBT.connected()) {
    SerialBT.println("Mensaje desde el emisor ESP32");
    Serial.println("Mensaje enviado.");
    delay(2000);
  }
}
