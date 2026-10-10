
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

// ESP32 pins
const int VOLTAGE_PIN = 34;
const int CURRENT_PIN = 35;
const int RELAY_PIN = 23;
const int GREEN_LED = 21;
const int RED_LED = 22;

// OLED SDA = GPIO 19, SCL = GPIO 18

// Protection limits
float minVoltage = 220.0;
float maxVoltage = 240.0;
float minCurrent = 1.0;
float maxCurrent = 3.0;

// Timing
const unsigned long SAMPLE_MS = 100;
const unsigned long REPORT_MS = 1000;
const unsigned long RECOVERY_MS = 3000;
const unsigned long DISPLAY_MS = 250;

unsigned long lastSample = 0;
unsigned long lastReport = 0;
unsigned long lastDisplay = 0;
unsigned long safeSince = 0;

// Simulated readings
float voltage = 0.0;
float current = 0.0;

// Protection state
bool relayConnected = false;
bool recoveryTiming = false;
bool voltageFault = true;
bool currentFault = true;
bool previousVoltageFault = false;
bool previousCurrentFault = false;

String fault = "STARTUP";

// Fault event counters
unsigned long voltageFaultCount = 0;
unsigned long currentFaultCount = 0;

// Relay: HIGH = disconnected, LOW = connected
void disconnectLoad() {
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  relayConnected = false;
  recoveryTiming = false;
  safeSince = 0;
}

void connectLoad() {
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  relayConnected = true;
  recoveryTiming = false;

  Serial.println("[EVENT] LOAD CONNECTED");
}

void updateFaultText() {
  if (voltageFault && currentFault) {
    fault = "VOLTAGE_AND_CURRENT_FAULT";
  } else if (voltageFault) {
    fault = "VOLTAGE_FAULT";
  } else if (currentFault) {
    fault = "CURRENT_FAULT";
  } else {
    fault = "NONE";
  }
}

// Log fault transitions, not every sample
void logFaultEvents() {
  if (voltageFault && !previousVoltageFault) {
    voltageFaultCount++;
    Serial.printf("[FAULT] Voltage #%lu: %.1f V\n",
                  voltageFaultCount, voltage);
  }

  if (!voltageFault && previousVoltageFault) {
    Serial.printf("[RECOVERY] Voltage safe: %.1f V\n", voltage);
  }

  if (currentFault && !previousCurrentFault) {
    currentFaultCount++;
    Serial.printf("[FAULT] Current #%lu: %.2f A\n",
                  currentFaultCount, current);
  }

  if (!currentFault && previousCurrentFault) {
    Serial.printf("[RECOVERY] Current safe: %.2f A\n", current);
  }

  previousVoltageFault = voltageFault;
  previousCurrentFault = currentFault;
}

void updateDisplay() {
  if (!oledAvailable) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("VOLTAGE PROTECTOR");
  display.println("---------------------");

  display.printf("Voltage: %6.1f V\n", voltage);
  display.printf("Current: %6.2f A\n", current);

  display.print("Relay: ");
  display.println(relayConnected ? "CONNECTED" : "DISCONNECTED");

  display.printf("V faults: %lu\n", voltageFaultCount);
  display.printf("I faults: %lu\n", currentFaultCount);

  display.display();
}

// Serial commands
void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  float value;

  if (command == "HELP") {
    Serial.println("Commands: STATUS");
    Serial.println("SET VMIN 220");
    Serial.println("SET VMAX 240");
    Serial.println("SET IMIN 1");
    Serial.println("SET IMAX 3");
    return;
  }

  if (command == "STATUS") {
    Serial.printf("Voltage: %.1f V | Current: %.2f A\n",
                  voltage, current);
    Serial.printf("Voltage limits: %.1f-%.1f V\n",
                  minVoltage, maxVoltage);
    Serial.printf("Current limits: %.1f-%.1f A\n",
                  minCurrent, maxCurrent);
    Serial.printf("Fault: %s | Relay: %s\n",
                  fault.c_str(),
                  relayConnected ? "CONNECTED" : "DISCONNECTED");
    Serial.printf("Voltage fault events: %lu | Current fault events: %lu\n",
                  voltageFaultCount, currentFaultCount);
    return;
  }

  if (sscanf(command.c_str(), "SET VMIN %f", &value) == 1) {
    if (value >= 0 && value < maxVoltage && value <= 300) {
      minVoltage = value;
      Serial.println("Minimum voltage updated.");
    } else {
      Serial.println("Invalid VMIN.");
    }
  } else if (sscanf(command.c_str(), "SET VMAX %f", &value) == 1) {
    if (value > minVoltage && value <= 300) {
      maxVoltage = value;
      Serial.println("Maximum voltage updated.");
    } else {
      Serial.println("Invalid VMAX.");
    }
  } else if (sscanf(command.c_str(), "SET IMIN %f", &value) == 1) {
    if (value >= 0 && value < maxCurrent && value <= 10) {
      minCurrent = value;
      Serial.println("Minimum current updated.");
    } else {
      Serial.println("Invalid IMIN.");
    }
  } else if (sscanf(command.c_str(), "SET IMAX %f", &value) == 1) {
    if (value > minCurrent && value <= 10) {
      maxCurrent = value;
      Serial.println("Maximum current updated.");
    } else {
      Serial.println("Invalid IMAX.");
    }
  } else {
    Serial.println("Unknown command. Type HELP.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  analogReadResolution(12);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Safe startup: keep load disconnected
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  // Initialize I2C: SDA = 19, SCL = 18
  Wire.begin(19, 18);

  oledAvailable = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);

  if (oledAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("VOLTAGE PROTECTOR");
    display.println("Starting...");
    display.display();
  } else {
    Serial.println("[ERROR] OLED initialization failed.");
  }

  Serial.println();
  Serial.println("VOLTAGE AND CURRENT PROTECTOR");
  Serial.println("Safe voltage: 220-240 V");
  Serial.println("Safe current: 1-3 A");
  Serial.println("Recovery delay: 3 seconds");
  Serial.println("Simulation only. Type HELP for commands.");
}

void loop() {
  if (Serial.available()) {
    processCommand(Serial.readStringUntil('\n'));
  }

  unsigned long now = millis();

  if (now - lastSample < SAMPLE_MS) return;
  lastSample = now;

  int rawV = analogRead(VOLTAGE_PIN);
  int rawI = analogRead(CURRENT_PIN);

  // Potentiometer simulation, not calibrated sensor measurements
  voltage = rawV * 300.0 / 4095.0;
  current = rawI * 10.0 / 4095.0;

  voltageFault = (voltage < minVoltage || voltage > maxVoltage);
  currentFault = (current < minCurrent || current > maxCurrent);

  logFaultEvents();
  updateFaultText();

  if (voltageFault || currentFault) {
    disconnectLoad();
  } else {
    if (!relayConnected) {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, LOW);

      if (!recoveryTiming) {
        safeSince = now;
        recoveryTiming = true;
        Serial.println("[EVENT] Safe readings; recovery timer started.");
      }

      if (now - safeSince >= RECOVERY_MS) {
        connectLoad();
      }
    } else {
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
    }
  }

  if (now - lastDisplay >= DISPLAY_MS) {
    lastDisplay = now;
    updateDisplay();
  }

  if (now - lastReport >= REPORT_MS) {
    lastReport = now;

    Serial.printf(
      "Voltage: %.1f V | Current: %.2f A | "
      "V_fault: %s | I_fault: %s | Fault: %s | Relay: %s\n",
      voltage, current,
      voltageFault ? "YES" : "NO",
      currentFault ? "YES" : "NO",
      fault.c_str(),
      relayConnected ? "CONNECTED" : "DISCONNECTED"
    );
  }
}
