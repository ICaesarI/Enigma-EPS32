/* HERRAMIENTA: Obtener MAC Address del ESP32 */
#include <Arduino.h>
#include "BluetoothSerial.h"
#include "esp_bt_device.h"

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n#################################");
  Serial.println("## HERRAMIENTA DE OBTENCIÓN MAC ##");
  Serial.println("#################################");

  if (!SerialBT.begin("MAC_FINDER")) {
    Serial.println("FATAL: Error iniciando Bluetooth.");
    while (true) delay(1000);
  }
  
  delay(500);
  
  const uint8_t* mac = esp_bt_dev_get_address();
  
  Serial.println("\n----------------------------------");
  Serial.println("DIRECCIÓN MAC SPP (Bluetooth Classic):");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("----------------------------------");
  
  Serial.println("Guarda esta MAC para usarla en el Emisor!");
}

void loop() {
  delay(10000);
}