#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>

#define LED_PIN 2         // Onboard LED
#define TOUCH_PIN T0      // GPIO 4 (Touch Pin 0)

bool modeBLE = false;
int scanTime = 3;         // BLE Scan duration in seconds
BLEScan* pBLEScan;

// Wi-Fi Promiscuous Callback
void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
  int rssi = pkt->rx_ctrl.rssi;
  
  // Detect Deauth Frame Types (0x00C0 = Deauth, 0x00A0 = Disassoc)
  uint8_t frameType = pkt->payload[0];
  if (frameType == 0xC0 || frameType == 0xA0) {
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("{\"alert\":\"DEAUTH_ATTACK_DETECTED\", \"rssi\":%d, \"channel\":%d}\n", rssi, pkt->rx_ctrl.channel);
    delay(50);
    digitalWrite(LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Initialize Wi-Fi in Promiscuous Mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);

  // Initialize BLE Scanner
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);

  Serial.println("==================================================");
  Serial.println("[+] ESP32 Security Sensor Running");
  Serial.println("[+] Touch GPIO 4 to toggle Wi-Fi Sniffer / BLE Tracker");
  Serial.println("==================================================");
}

void loop() {
  // Check capacitive touch on GPIO 4 to toggle modes
  if (touchRead(TOUCH_PIN) < 30) { 
    modeBLE = !modeBLE;
    digitalWrite(LED_PIN, HIGH);
    delay(300); // Debounce
    digitalWrite(LED_PIN, LOW);
    
    if (modeBLE) {
      esp_wifi_set_promiscuous(false);
      Serial.println("\n[>>> Switched to BLE Proximity Tracking Mode <<<]");
    } else {
      esp_wifi_set_promiscuous(true);
      Serial.println("\n[>>> Switched to Wi-Fi Airspace Sniffer Mode <<<]");
    }
  }

  if (modeBLE) {
    // Perform BLE Scanning (Updated for ESP32 v3.x core API)
    BLEScanResults* foundDevices = pBLEScan->start(scanTime, false);
    if (foundDevices != nullptr) {
      for (int i = 0; i < foundDevices->getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices->getDevice(i);
        Serial.printf("{\"type\":\"BLE\", \"mac\":\"%s\", \"rssi\":%d}\n", 
                      device.getAddress().toString().c_str(), 
                      device.getRSSI());
      }
    }
    pBLEScan->clearResults();
  } else {
    // Wi-Fi Promiscuous Hopping across Channels 1 to 13
    for (int ch = 1; ch <= 13; ch++) {
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      delay(100);
      if (touchRead(TOUCH_PIN) < 30) break; // Early breakout on touch
    }
  }
}