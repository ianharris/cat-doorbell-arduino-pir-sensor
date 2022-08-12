// include libraries
#include <ArduinoBLE.h>
#include "SparkFunBQ27441.h"

// const integers
const int LED_STATUS_PIN = 3;
const int PIR_PIN = 2;
const int GPOUT_PIN = 12;
const int PERCENTAGE_INTERVAL = 1;

// const unsigned integers
const unsigned int BATTERY_CAPACITY = 2200;

// volatile bool for hardware interrupt
volatile bool readBatteryRequired = false;

// error codes
int BLE_ERROR = 1;
int LIPO_ERROR = 2;

// custom BLE service for relaying PIR reading and battery service
BLEService pirService("349cf6c9-83b7-4b52-8b75-e3cb3313d7bf");
BLEService batteryService("180F");

// custom service characteristics and battery characteristic
BLEBooleanCharacteristic pirCharacteristic("cdc2c683-437c-46c9-8d5a-f92cea301abe", BLERead | BLENotify);
BLEUnsignedCharCharacteristic batteryCharacteristic("2A19", BLERead | BLENotify);

// interrupt service routine for GPOUT_PIN - allows battery babysitter to indicate 
// a change in battery level via GPOUT_PIN
void gpout_interrupt_handler() {
    readBatteryRequired = true;
}

// function to allow status led blink to indicate detected error
void error_indicator(int error) {

  // set the status pin to low
  digitalWrite(LED_STATUS_PIN, LOW);

  // create an infinite loop
  while(1) {

    // iterate 0 to error to create the LED visual indication of error
    for(int i=0; i<error; ++i) {
      digitalWrite(LED_STATUS_PIN, HIGH);
      delay(500);
      digitalWrite(LED_STATUS_PIN, LOW);
      delay(500);
    }

    // create an additional delay to demarcate the error flashes
    delay(1500);
  }
}

// read pir state - update characteristic as required
void managePIR() {

  static int previousPirState = LOW;

  // declare the pir state
  int pirState;
  
  // read the state of the PIR input pin
  pirState = digitalRead(PIR_PIN);

  if(pirState != previousPirState) {

    // update the characteristic
    pirCharacteristic.writeValue(pirState == HIGH);

    // reset the previousPirState
    previousPirState = pirState;
  }
}

// set the battery characteristic to the value returned by lipo.soc()
void setBatteryCharacteristicValue() {
  // get the state of charge
  unsigned int soc = lipo.soc();

  // set the characteristic
  batteryCharacteristic.writeValue((char)(0xFF & soc));
}

// manage the battery - read when readBatteryrequired is true
void manageBattery() {

  // read the GPOUT value
  if (readBatteryRequired) {

    // set the characteristic value
    setBatteryCharacteristicValue();

    // reset the read battery required indicator
    readBatteryRequired = false;
  }
}

// setup ble
void setup_ble() {
  
  // initialise BLE
  if (!BLE.begin()) {
    error_indicator(BLE_ERROR);
  }

  // set the local name for advertising packets
  BLE.setLocalName("Doorbell");
  BLE.setDeviceName("Doorbell");

  // add the pir service to the list of advertised services
  BLE.setAdvertisedService(pirService);

  // add the characteristic to the PIR service
  pirService.addCharacteristic(pirCharacteristic);
  // add the pir service
  BLE.addService(pirService);
  // initialise a value for the pirCharacteristic
  pirCharacteristic.writeValue(false);

  // add the battery characteristic to the batter service
  batteryService.addCharacteristic(batteryCharacteristic);
  // add the batter service
  BLE.addService(batteryService);
  // initialize the value for the pirCharacteristic
  setBatteryCharacteristicValue();
}

void setup_battery_monitoring() {

  // Set the GPOUT pin as an input w/ pullup
  pinMode(GPOUT_PIN, INPUT_PULLUP);
  
  // Use lipo.begin() to initialize the BQ27441-G1A and confirm that it's
  // connected and communicating.
  if (!lipo.begin()) // begin() will return true if communication is successful
  {
    // If communication fails, print an error message and loop forever.
    error_indicator(LIPO_ERROR);
  }

  // To configure the values below, you must be in config mode
  lipo.enterConfig();

  // Set the battery capacity
  lipo.setCapacity(BATTERY_CAPACITY);

  // Set GPOUT to active-low
  lipo.setGPOUTPolarity(LOW);

  // Set GPOUT to SOC_INT mode
  lipo.setGPOUTFunction(SOC_INT);

  // Set percentage change integer
  lipo.setSOCIDelta(PERCENTAGE_INTERVAL);

  // Exit config mode to save changes
  lipo.exitConfig();
}

void setup() {
  
  // set the digital pin types
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  // initialise the LED_STATUS_PIN
  digitalWrite(LED_STATUS_PIN, HIGH);

  // set up the battery monitoring
  setup_battery_monitoring();

  // set up ble
  setup_ble();

  // attach the hardware interrupt
  attachInterrupt(digitalPinToInterrupt(GPOUT_PIN), gpout_interrupt_handler, LOW);

  // start advertising
  BLE.advertise();

  digitalWrite(LED_STATUS_PIN, LOW);
}

void loop() {

  // wait for a BLE central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {

    // stop advertising
    BLE.stopAdvertise();

    // read the value of the PIR and update every 200ms
    // while the central is connected
    while (central.connected()) {

      // call the PIR management function
      managePIR();

      // call the battery manage function
      manageBattery();

      // delay for 200ms
      delay(200);
    }

    // disconnected should start advertising again
    BLE.advertise();
  }

  // call manage PIR - still want to see detections via LED when not connected
  managePIR();

  // call the manage battery function
  manageBattery();

  // if no central is connected wait for 200ms and check again
  delay(200);
}
