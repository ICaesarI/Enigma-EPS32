#include "BluetoothSerial.h"
BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  Serial.println("=== RECEPTOR ENCENDIDO ===");

  if (!SerialBT.begin("ESP32_Receptor")) {
    Serial.println("Error iniciando Bluetooth");
    while (true);
  }

  Serial.println("Receptor listo. Esperando mensajes...");
}

void loop() {
  if (SerialBT.available()) {
    String msg = SerialBT.readStringUntil('\n');
    Serial.print("Mensaje recibido: ");
    Serial.println(msg);
  }
}
