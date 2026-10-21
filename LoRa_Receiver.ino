#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---------- OLED SETTINGS ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ---------- SX1276 SETTINGS ----------
#define LORA_SS   10
#define LORA_RST   9
#define LORA_DIO0  2

const long LORA_FREQUENCY = 433E6;

// ---------- RECEIVED VALUES ----------
unsigned long packetNumber = 0;
unsigned long lastPacketTime = 0;
unsigned long lastDisplayTime = 0;

float voltage = 0;
float current = 0;
float power = 0;
float energy = 0;
float frequency = 0;
float pf = 0;

bool dataReceived = false;
bool staleWarning = false;

byte screen = 0;

// ---------- OLED DISPLAY ----------
void showDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (!dataReceived) {
    display.println(F("PZEM LoRa Receiver"));
    display.println(F("---------------------"));
    display.println();
    display.println(F("Waiting for data..."));
    display.println();
    display.println(F("Check transmitter"));
    display.display();
    return;
  }

  bool stale = millis() - lastPacketTime > 5000;

  if (stale) {
    display.println(F("NO RECENT DATA!"));
  } else {
    display.println(F("PZEM LIVE READINGS"));
  }

  display.println(F("---------------------"));

  if (screen == 0) {
    display.print(F("Voltage: "));
    display.print(voltage, 1);
    display.println(F(" V"));

    display.print(F("Current: "));
    display.print(current, 3);
    display.println(F(" A"));

    display.print(F("Power:   "));
    display.print(power, 1);
    display.println(F(" W"));
  } else {
    display.print(F("Energy: "));
    display.print(energy, 3);
    display.println(F(" kWh"));

    display.print(F("Freq:   "));
    display.print(frequency, 1);
    display.println(F(" Hz"));

    display.print(F("PF:     "));
    display.println(pf, 2);
  }

  display.println();
  display.print(F("Packet #: "));
  display.println(packetNumber);

  display.display();
}

// ---------- RECEIVE LORA PACKET ----------
void receiveLoRa() {
  int packetSize = LoRa.parsePacket();

  if (packetSize <= 0) {
    return;
  }

  char packet[100];
  int index = 0;

  while (LoRa.available()) {
    char c = (char)LoRa.read();

    if (index < (int)sizeof(packet) - 1) {
      packet[index++] = c;
    }
  }

  packet[index] = '\0';

  Serial.print(F("Received: "));
  Serial.println(packet);

  // Expected format:
  // PZEM,number,voltage,current,power,energy,frequency,pf

  char *fields[8];
  byte count = 0;

  char *token = strtok(packet, ",");

  while (token != NULL && count < 8) {
    fields[count++] = token;
    token = strtok(NULL, ",");
  }

  if (count != 8 || strcmp(fields[0], "PZEM") != 0) {
    Serial.println(F("Invalid packet format"));
    return;
  }

  // Parse the fields
  unsigned long newPacketNumber =
    strtoul(fields[1], NULL, 10);

  float newVoltage   = atof(fields[2]);
  float newCurrent   = atof(fields[3]);
  float newPower     = atof(fields[4]);
  float newEnergy    = atof(fields[5]);
  float newFrequency = atof(fields[6]);
  float newPF        = atof(fields[7]);

  // Validate numeric values
  if (!isfinite(newVoltage) ||
      !isfinite(newCurrent) ||
      !isfinite(newPower) ||
      !isfinite(newEnergy) ||
      !isfinite(newFrequency) ||
      !isfinite(newPF) ||
      newVoltage < 0 || newCurrent < 0 ||
      newPower < 0 || newEnergy < 0 ||
      newFrequency < 0 || newFrequency > 100 ||
      newPF < 0 || newPF > 1.1) {

    Serial.println(F("Invalid measurement values"));
    return;
  }

  // Save a complete valid reading
  packetNumber = newPacketNumber;
  voltage = newVoltage;
  current = newCurrent;
  power = newPower;
  energy = newEnergy;
  frequency = newFrequency;
  pf = newPF;

  lastPacketTime = millis();
  dataReceived = true;

  Serial.println(F("Valid PZEM data accepted"));

  Serial.print(F("Voltage: "));
  Serial.println(voltage, 1);

  Serial.print(F("Current: "));
  Serial.println(current, 3);

  Serial.print(F("Power: "));
  Serial.println(power, 1);

  Serial.print(F("Energy: "));
  Serial.println(energy, 3);

  Serial.print(F("Frequency: "));
  Serial.println(frequency, 1);

  Serial.print(F("Power factor: "));
  Serial.println(pf, 2);

  Serial.println();

  showDisplay();
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);

  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000);

  // Initialize OLED
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS)) {

    Serial.println(F("SSD1306 initialization failed!"));
    while (true) {
      // Stop if OLED initialization fails
    }
  }

  // OLED startup screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("OLED Initialized!"));
  display.println();
  display.println(F("Starting LoRa..."));
  display.display();

  delay(1000);

  // Initialize LoRa
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println(F("LoRa initialization failed!"));

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("LoRa Init Failed"));
    display.display();

    while (true) {
      // Stop if LoRa initialization fails
    }
  }

  // Must match the transmitter settings
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  Serial.println(F("LoRa receiver ready"));

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("LoRa Ready"));
  display.println();
  display.println(F("Waiting for PZEM"));
  display.display();

  delay(1000);

  showDisplay();
}

// ---------- MAIN LOOP ----------
void loop() {
  // Receive packets continuously
  receiveLoRa();

  unsigned long now = millis();

  // Change OLED screen every 3 seconds
  if (now - lastDisplayTime >= 3000) {
    lastDisplayTime = now;
    screen = !screen;
    showDisplay();
  }

  // Refresh the stale-data warning when needed
  bool currentlyStale =
    dataReceived && (now - lastPacketTime > 5000);

  if (currentlyStale != staleWarning) {
    staleWarning = currentlyStale;
    showDisplay();
  }
}