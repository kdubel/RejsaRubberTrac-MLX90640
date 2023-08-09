#include "ble.h"

BLDevice::BLDevice() { // We initialize a couple things in constructor
  datapackOne.distance = 0;
  datapackOne.protocol = PROTOCOL;
  datapackTwo.protocol = PROTOCOL;
  datapackThr.distance = 0;
  datapackThr.protocol = PROTOCOL;
  mainService = BLEService(0x1ff7);
  GATTone = BLECharacteristic(0x01);
  GATTtwo = BLECharacteristic(0x02);
  GATTthr = BLECharacteristic(0x03);
}

void BLDevice::setupDevice(char bleName[]) {
  uint8_t macaddr[6];

  Bluefruit.autoConnLed(false); // DISABLE BLUE BLINK ON CONNECT STATUS
  Bluefruit.begin(); 
  Bluefruit.getAddr(macaddr);
  sprintf(bleName, "%s%02x%02x%02x\0",bleName, macaddr[2], macaddr[1], macaddr[0]); // Extend bleName[] with the last 4 octets of the mac address
  Bluefruit.setName(bleName);
  Serial.print("Starting bluetooth with MAC address ");
//  Serial.printBufferReverse(macaddr, 6, ':');
  Serial.println();
  Serial.printf("Device name: %s\n", bleName);

  setupMainService();
  startAdvertising();
}

void BLDevice::setupMainService(void) {
  mainService.begin();

  GATTone.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);  // Options: CHR_PROPS_BROADCAST, CHR_PROPS_NOTIFY, CHR_PROPS_INDICATE, CHR_PROPS_READ, CHR_PROPS_WRITE_WO_RESP, CHR_PROPS_WRITE
  GATTone.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTone.setFixedLen(20);
  GATTone.begin();

  GATTtwo.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
  GATTtwo.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTtwo.setFixedLen(20);
  GATTtwo.begin();

  GATTthr.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_READ);
  GATTthr.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  GATTthr.setFixedLen(20);
  GATTthr.begin();
}


void BLDevice::startAdvertising(void) {
  Bluefruit.setTxPower(4);
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(mainService);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0); 
}

void BLDevice::renderPacketTemperature(int16_t measurements[], uint8_t mirrorTire, one_t &FirstPacket, two_t &SecondPacket, thr_t &ThirdPacket) {
  for(uint8_t x=0;x<8;x++){
    uint8_t _x = x;
    if (mirrorTire == 1) {
      _x = 7-x;
    }
    FirstPacket.temps[_x]=measurements[x*2];
    SecondPacket.temps[_x]=measurements[x*2 + 1];
    ThirdPacket.temps[_x]=max(FirstPacket.temps[_x], SecondPacket.temps[_x]);
  }
}

void BLDevice::renderPacketBattery(int vbattery, int percentage, two_t &SecondPacket) {
  SecondPacket.voltage = vbattery;
  SecondPacket.charge = percentage;
}

void BLDevice::transmit(int16_t tempMeasurements[], uint8_t mirrorTire, int16_t distance, int vBattery, int lipoPercentage) {
  renderPacketTemperature(tempMeasurements, mirrorTire, datapackOne, datapackTwo, datapackThr);
  renderPacketBattery(vBattery, lipoPercentage, datapackTwo);
  datapackOne.distance = datapackThr.distance = distance;
  GATTone.notify(&datapackOne, sizeof(datapackOne));
  GATTtwo.notify(&datapackTwo, sizeof(datapackTwo));
  GATTthr.notify(&datapackThr, sizeof(datapackThr));
}

boolean BLDevice::isConnected() {
  return Bluefruit.connected();
}
