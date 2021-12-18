#include <Wire.h>
#include <LiFuelGauge.h>
#include "ble.h"
#include "name.h"
#include <lwipopts.h>


// LiFuelGauge constructor parameters
// 1. IC type, MAX17043 or MAX17044
// 2. Number of interrupt to which the alert pin is associated (Optional) 
// 3. ISR to call when an alert interrupt is generated (Optional)

// Creates a LiFuelGauge instance for the MAX17043 IC
// and attaches lowPower to INT0 (PIN2 on most boards, PIN3 on Leonardo)

// A flag to indicate a generated alert interrupt
volatile boolean alert = false;

void lowPower() { alert = true; }
LiFuelGauge gauge(MAX17043, 0, lowPower);

const uint16_t battery_level_delay_ms = 1000;

BLECharacteristic *pBatteryLevelCharacteristic;
BLEService *pBatteryService;
uint8_t batteryLevelCharacteristicValue[4];
float stateOfCharge;

unsigned long lastTime = 0;
unsigned long currentTime;

void updateStateOfCharge() { 
    stateOfCharge = gauge.getVoltage();
}

void setup()
{
    Serial.begin(115200); // Initializes serial port

    ble::setup();
    // NOTE: make sure to add BLE service and characteristic prior to advertising the service
    pBatteryService = ble::pServer->createService(BLEUUID((uint16_t)0x180F));
    pBatteryLevelCharacteristic = ble::createCharacteristic(BLEUUID((uint16_t)0x2A19), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, "Battery Level", pBatteryService);
    pBatteryService->start();
    ble::start();

    gauge.reset();  // Resets MAX17043
    delay(200);  // Waits for the initial measurements to be made
    
    // Sets the Alert Threshold to 10% of full capacity
    gauge.setAlertThreshold(10);
    Serial.println(String("Alert Threshold is set to ") + 
                   gauge.getAlertThreshold() + '%');

}

void loop()
{
    currentTime = millis();
    if (ble::isServerConnected && currentTime >= lastTime + battery_level_delay_ms) {
        if (alert)
        {
            Serial.println("Beware, Low Power!");
            Serial.println("Finalizing operations...");
            gauge.clearAlertInterrupt();  // Resets the ALRT pin
            alert = false;
            Serial.println("Storing data...");
            Serial.println("Sending notification...");
            Serial.println("System operations are halted...");
            gauge.sleep();  // Forces the MAX17043 into sleep mode
        }

        lastTime = currentTime;
        updateStateOfCharge();
        Serial.print("SOC: ");
        Serial.print(stateOfCharge);
        Serial.println();
        MEMCPY(&batteryLevelCharacteristicValue, &stateOfCharge, 4);
        pBatteryLevelCharacteristic->setValue((uint8_t *) batteryLevelCharacteristicValue, 4);
        pBatteryLevelCharacteristic->notify();
    }
}

