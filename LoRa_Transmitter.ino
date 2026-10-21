#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <LoRa.h>
#include <math.h>

// ---------- PZEM ----------
#define PZEM_RX_PIN 4
#define PZEM_TX_PIN 5

SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSWSerial);

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- SX1276 ----------
#define LORA_SS   10
#define LORA_RST   9
#define LORA_DIO0  2

const long LORA_FREQUENCY = 433E6;

// ---------- Measurements ----------
float voltage, current, power, energy, frequency, pf;

unsigned long packetNumber = 0;
unsigned long lastSend = 0;
unsigned long lastScreen = 0;

byte screen = 0;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("PZEM LoRa Meter");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1500);

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    lcd.clear();
    lcd.print("LoRa Init Failed");
    Serial.println("LoRa initialization failed!");
    while (true) {}
  }

  LoRa.setTxPower(17);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  lcd.clear();
  lcd.print("LoRa Ready");
  Serial.println("PZEM LoRa transmitter ready");
  delay(1000);
}

void loop() {
  unsigned long now = millis();

  // Read and transmit once per second
  if (now - lastSend >= 1000) {
    lastSend = now;

    voltage   = pzem.voltage();
    current   = pzem.current();
    power     = pzem.power();
    energy    = pzem.energy();
    frequency = pzem.frequency();
    pf        = pzem.pf();

    // Reject invalid sensor readings
    if (isnan(voltage) || isnan(current) ||
        isnan(power) || isnan(energy) ||
        isnan(frequency) || isnan(pf)) {

      Serial.println("PZEM reading error");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PZEM Read Error");
      lcd.setCursor(0, 1);
      lcd.print("Check Wiring");
      return;
    }

    packetNumber++;

    // Packet:
    // PZEM,number,voltage,current,power,energy,frequency,pf
    String packet = "PZEM,";
    packet += String(packetNumber);
    packet += ",";
    packet += String(voltage, 1);
    packet += ",";
    packet += String(current, 3);
    packet += ",";
    packet += String(power, 1);
    packet += ",";
    packet += String(energy, 3);
    packet += ",";
    packet += String(frequency, 1);
    packet += ",";
    packet += String(pf, 2);

    LoRa.beginPacket();
    LoRa.print(packet);
    int result = LoRa.endPacket();

    Serial.print("Packet: ");
    Serial.println(packet);

    if (result == 1) {
      Serial.println("LoRa transmission successful");
    } else {
      Serial.println("LoRa transmission failed");
    }

    Serial.println();
  }

  // Switch LCD screens every 4 seconds
  if (now - lastScreen >= 4000) {
    lastScreen = now;
    screen = !screen;
    lcd.clear();
  }

  if (screen == 0) {
    lcd.setCursor(0, 0);
    lcd.print("V:");
    lcd.print(voltage, 1);
    lcd.print("V I:");
    lcd.print(current, 2);

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(power, 1);
    lcd.print("W PF:");
    lcd.print(pf, 2);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("E:");
    lcd.print(energy, 3);
    lcd.print(" kWh");

    lcd.setCursor(0, 1);
    lcd.print("Freq:");
    lcd.print(frequency, 1);
    lcd.print(" Hz");
  }
}