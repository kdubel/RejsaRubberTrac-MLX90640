#include <Arduino.h>
#include <Wire.h>
#include <Tasker.h>
#include "Configuration.h"
#include "temp_sensor.h"
#include "dist_sensor.h"
#include "display.h"
#include "ble.h"
#include "algo.h"
  
TempSensor tempSensor;
DistSensor distSensor;
uint8_t mirrorTire = 0;
char wheelPos[] = "  ";  // Wheel position for Tire A
char deviceNameSuffix[] = "  ";

#if BOARD == BOARD_ESP32_LOLIND32
  #if FIS_SENSOR2_PRESENT == 1
    TempSensor tempSensor2;
    uint8_t mirrorTire2 = 0;
    char wheelPos2[] = "  ";  // Wheel position for Tire B
  #endif
  #if DIST_SENSOR2 != DIST_NONE
    DistSensor distSensor2;
  #endif
#endif

BLDevice bleDevice;
Display display;
Tasker tasker;

int vBattery = 0;          // Current battery voltage in mV
int lipoPercentage = 0;    // Current battery percentage
float updateRate = 0.0;    // Reflects the actual update rate
int measurementCycles = 0; // Counts how many measurement cycles were completed. printStatus() uses it to roughly calculate refresh rate.

// Function declarations
void updateWheelPos(void);
void printStatus(void);
void blinkOnTempChange(int16_t);
void blinkOnDistChange(uint16_t);
int getVbat(void);
void updateBattery(void);
void updateRefreshRate(void);

#ifdef DUMMYDATA
  #include "dummydata.h"
#endif


// ----------------------------------------

void setup(){
  Serial.begin(115200);
  while (!Serial); // Wait for Serial
  Serial.printf("\nBegin startup. Arduino version: %d\n", ARDUINO);

#ifdef DUMMYDATA
  debug("=======  DUMMYDATA  ========\n");
#endif

  if (GPIOLEDDIST > 0) pinMode(GPIOLEDDIST, OUTPUT);
  if (GPIOLEDTEMP > 0) pinMode(GPIOLEDTEMP, OUTPUT);
  pinMode(GPIODISTSENSORXSHUT, OUTPUT);
  pinMode(GPIOLEFT, INPUT_PULLUP);
  pinMode(GPIOFRONT, INPUT_PULLUP);
  pinMode(GPIOCAR, INPUT_PULLUP);
  pinMode(GPIOMIRR, INPUT_PULLUP);

  updateBattery();
  updateWheelPos();
  char bleName[32] = "RejsaRubber";
  sprintf(bleName, "%s%s\0",bleName, deviceNameSuffix); // Extend bleName[] with the suffix


// TIRE 1 MIRRORED?
  if ((MIRRORTIRE == 1 || digitalRead(GPIOMIRR) == 0)) {
    mirrorTire = 1;
    debug("Temperature sensor orientation for %s is mirrored.\n", wheelPos);
  }

// I2C channel 1
  Wire.begin();

  #if DIST_SENSOR != DIST_NONE
    debug("Starting distance sensor for %s...\n", wheelPos);
    if (distSensor.initialise(&Wire, wheelPos)) {
      debug("Distance sensor for %s present.\n", wheelPos);
    }
    else {
      debug("ERROR: Distance sensor for %s not present.\n", wheelPos);
    }
  #endif

  debug("Starting temperature sensor for %s...\n", wheelPos);
  if (!tempSensor.initialise(FIS_REFRESHRATE, &Wire)) {
    // perform automatic system reboot to retry temp sensor initialization
    debug("Rebooting the MCU now...\n");
    NVIC_SystemReset();
  }

// display
#if DISP_DEVICE != DISP_NONE
  display.setup();
  tasker.setInterval(updateDisplay,200);
#endif

// BLE
  debug("Starting BLE device: %s\n", bleName);
  bleDevice.setupDevice(bleName);

// Set up periodic functions
#ifdef _DEBUG
  tasker.setInterval(printStatus, SERIAL_UPDATERATE*1000); // Print status every second
#endif
  tasker.setInterval(updateBattery, BATTERY_UPDATERATE*1000); // Update battery status every minute
  tasker.setInterval(updateRefreshRate, 2000);

  debug("Running!\n");

#ifdef DUMMYDATA
  // 2do: make DUMMYDATA compatible with 2x I2C
  dummyloop();
#endif
}                                                           

void loop() {
// I2C channel 1
  #if DIST_SENSOR != DIST_NONE
    distSensor.measure();
  #endif
  tempSensor.measure();

// I2C channel 2
#if FIS_SENSOR2_PRESENT == 1
  #if DIST_SENSOR2 != DIST_NONE
    distSensor2.measure();
  #endif
  tempSensor2.measure();
#endif

  if (bleDevice.isConnected()) {
    bleDevice.transmit(tempSensor.measurement_16, mirrorTire, distSensor.distance, vBattery, lipoPercentage);
  }
  
  #if DISP_DEVICE == DISP_NONE // Only use the LEDs w/o display
    blinkOnTempChange(tempSensor.measurement_16[8]/20);    // Use one single temp in the middle of the array
    blinkOnDistChange(distSensor.distance/20);    // value/nn -> Ignore smaller changes to prevent noise triggering blinks
  #endif

// 2do: integrate tempSensor2 & distSensor2 into BLE transmission
// 2do: integrate Wheelpost into BLE transmission

  measurementCycles++;

  tasker.loop();
}

void updateDisplay(void) {
  display.refreshDisplay(tempSensor.measurement, tempSensor.outerTireEdgePositionSmoothed, tempSensor.innerTireEdgePositionSmoothed, tempSensor.validAutozoomFrame, updateRate, distSensor.distance, lipoPercentage, bleDevice.isConnected());

// 2do: integrate tempSensor2 & distSensor2 for display
}

// Figure out wheel position coding
void updateWheelPos(void) {
#if FIS_SENSOR2_PRESENT == 1
  if (digitalRead(GPIOLEFT)) {
    // GPIOLEFT  = 0 => axis: both sensors are mounted on the front or rear axle
    wheelPos[1]  = 'L';
    wheelPos2[1] = 'R';

    if (digitalRead(GPIOFRONT)) {
      // GPIOFRONT = 0 => axis 1 (front)
      wheelPos[0]  = 'F';
      wheelPos2[0] = 'F';
      deviceNameSuffix[0] = 'F';
    } else {
      // GPIOFRONT = 1 => axis 2 (rear)
      wheelPos[0]  = 'R';
      wheelPos2[0] = 'R';
      deviceNameSuffix[0] = 'R';
    }
  }
  else {
    // GPIOLEFT  = 1 => axis: axis: both sensors are mounted on the left or right vehicle side
    wheelPos[0]  = 'F';
    wheelPos2[0] = 'R';

    if (digitalRead(GPIOFRONT)) {
      // GPIOFRONT = 0 => axis 1 (left)
      wheelPos[1]  = 'L';
      wheelPos2[1] = 'L';
      deviceNameSuffix[0] = 'L';
    } else {
      // GPIOFRONT = 1 => axis 2 (right)
      wheelPos[1]  = 'R';
      wheelPos2[1] = 'R';
      deviceNameSuffix[0] = 'R';
    }
  }

  // overwrite left/right if we happen to be a motorcycle
  if (!digitalRead(GPIOCAR))  wheelPos[1]  = ' ';
  if (!digitalRead(GPIOCAR))  wheelPos2[1] = ' ';

#else
  uint8_t wheelPosCode = digitalRead(GPIOLEFT) + (digitalRead(GPIOFRONT) << 1) + (digitalRead(GPIOCAR) << 2);
  if (wheelPosCode >= 7) wheelPosCode = DEVICENAMECODE; // set from configuration
  
  switch (wheelPosCode) {
    case 0: sprintf(wheelPos, "FL"); break;
    case 1: sprintf(wheelPos, "FR"); break;
    case 2: sprintf(wheelPos, "RL"); break;
    case 3: sprintf(wheelPos, "RR"); break;
    case 4: sprintf(wheelPos, "F "); break;
    case 5: sprintf(wheelPos, "F "); break;
    case 6: sprintf(wheelPos, "R "); break;
    case 7: sprintf(wheelPos, "  "); break;
    default: sprintf(wheelPos, "??"); break;
  }
  strncpy(deviceNameSuffix, wheelPos, 3);
#endif
}

void printStatus(void) {
#if DIST_SENSOR != DIST_NONE
  char distSensor_str[6];
  sprintf(distSensor_str, "%imm", distSensor.distance);
#else
  const char* distSensor_str = "N/A  ";
#endif
#if DIST_SENSOR2 != DIST_NONE
  char distSensor2_str[6];
  sprintf(distSensor2_str, "%imm", distSensor2.distance);
#else
  const char* distSensor2_str = "N/A  ";
#endif


  debug("Rate: %.1fHz\tV: %dmV (%d%%) \tWheel: %s\tD: %s\t", (float)updateRate, vBattery, lipoPercentage, wheelPos, distSensor_str);
  debug("Zoomrate: %.2f%% \tOutliers: %.2f%%\tMaxRowDelta: %.1f\tAvgTemp: %.1f\tAvgStdDev: %.1f\t", tempSensor.runningAvgZoomedFramesRate*100, tempSensor.runningAvgOutlierRate*100, tempSensor.maxRowDeltaTmp/10, tempSensor.movingAvgFrameTmp/10, tempSensor.movingAvgStdDevFrameTmp/10);
  for (uint8_t i=0; i<FIS_X; i++) {
    debug("T: %.1f\t",(float)tempSensor.measurement[i]/10);
  }
  debug("\n");
#if FIS_SENSOR2_PRESENT == 1
  debug("\t\t\t\t\tWheel: %s\tD: %s\t", wheelPos2, distSensor2_str);
  debug("Zoomrate: %.2f%% \tOutliers: %.2f%%\tMaxRowDelta: %.1f\tAvgTemp: %.1f\tAvgStdDev: %.1f\t", tempSensor2.runningAvgZoomedFramesRate*100, tempSensor2.runningAvgOutlierRate*100, tempSensor2.maxRowDeltaTmp/10, tempSensor2.movingAvgFrameTmp/10, tempSensor2.movingAvgStdDevFrameTmp/10);
  for (uint8_t i=0; i<FIS_X; i++) {
    debug("T: %.1f\t",(float)tempSensor2.measurement[i]/10);
  }
  debug("\n");
#endif
}

void updateRefreshRate(void) {
  static long lastUpdate = 0;
  updateRate = (float)measurementCycles / (millis()-lastUpdate) * 1000;
  lastUpdate = millis();
  measurementCycles = 0;
}

#if DISP_DEVICE == DISP_NONE
void blinkOnDistChange(uint16_t distnew) {
  if (GPIOLEDDIST > 0) {
    static uint16_t distold = 0;

/* DEPRECATED?
    if (!Bluefruit.connected()) {
      digitalWrite(ledDist, HIGH);
      return;
    }
*/
    
    if (distold != distnew) {
      digitalWrite(GPIOLEDDIST, HIGH);
      delay(3);
      digitalWrite(GPIOLEDDIST, LOW);
    }
    distold = distnew;
  }
}

void blinkOnTempChange(int16_t tempnew) {
  if (GPIOLEDTEMP > 0) {
    static int16_t tempold = 0;
    if (tempold != tempnew) {
      digitalWrite(GPIOLEDTEMP, HIGH);
      delay(3);
      digitalWrite(GPIOLEDTEMP, LOW);
    }
    tempold = tempnew;
  }
}
#endif

int getVbat(void) {
  double adcRead=0;
  adcRead = analogRead(VBAT_PIN);
  return adcRead * MILLIVOLTFULLSCALE * BATRESISTORCOMP / STEPSFULLSCALE;
}

void updateBattery(void) {
  vBattery = getVbat();
  lipoPercentage = lipoPercent(vBattery);
}
