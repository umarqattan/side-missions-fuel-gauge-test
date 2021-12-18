#include <Wire.h>
#include <LiFuelGauge.h>
#include "ble.h"
#include "name.h"
#include <lwipopts.h>
#include "SparkFun_BNO080_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_BNO080


// LiFuelGauge constructor parameters
// 1. IC type, MAX17043 or MAX17044
// 2. Number of interrupt to which the alert pin is associated (Optional) 
// 3. ISR to call when an alert interrupt is generated (Optional)

// Creates a LiFuelGauge instance for the MAX17043 IC
// and attaches lowPower to INT0 (PIN2 on most boards, PIN3 on Leonardo)

// A flag to indicate a generated alert interrupt
volatile boolean alert = false;

void lowPower() { 
    Serial.print("You should lower the 'low power' threshold by 10% now.");
    alert = true; 
}
LiFuelGauge gauge(MAX17043, 25, lowPower);
BNO080 IMU;


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


    // Start i2c and BNO080
    Wire.flush();   // Reset I2C
    IMU.begin(BNO080_DEFAULT_ADDRESS, Wire);
    Wire.begin(SDA, SCL); 
    //Wire.setClockStretchLimit(4000);

    if (!IMU.begin())
    {
        Serial.println(F("BNO080 not detected at default I2C address. Check your jumpers and the hookup guide. Freezing..."));
        while (1);
    }

    Wire.setClock(400000); //Increase I2C data rate to 400kHz
    IMU.enableRotationVector(50); //Send data update every 50ms

    Serial.println(F("Rotation vector enabled"));
    Serial.println(F("Output in form roll, pitch, yaw"));
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

        // Note: the following code reads the VBAT Voltage level but not the current charge.
        float level = (float(analogRead(A13) * 2 * 3.3 / 4095));
        Serial.print("Battery Level: ");
        Serial.println(level);

        lastTime = currentTime;
        updateStateOfCharge();
        Serial.print("SOC: ");
        Serial.print(stateOfCharge);
        Serial.println();
        MEMCPY(&batteryLevelCharacteristicValue, &stateOfCharge, 4);
        pBatteryLevelCharacteristic->setValue((uint8_t *) batteryLevelCharacteristicValue, 4);
        pBatteryLevelCharacteristic->notify();

        //Look for reports from the IMU
        if (IMU.dataAvailable() == true)
        {
            unsigned long timeStamp = IMU.getTimeStamp();
            float quatI = IMU.getQuatI();
            float quatJ = IMU.getQuatJ();
            float quatK = IMU.getQuatK();
            float quatReal = IMU.getQuatReal();
            float quatRadianAccuracy = IMU.getQuatRadianAccuracy();

            Serial.print(timeStamp);
            Serial.print(F(","));
            Serial.print(quatI, 2);
            Serial.print(F(","));
            Serial.print(quatJ, 2);
            Serial.print(F(","));
            Serial.print(quatK, 2);
            Serial.print(F(","));
            Serial.print(quatReal, 2);
            Serial.print(F(","));
            Serial.print(quatRadianAccuracy, 2);
            Serial.print(F(","));

            Serial.println();
        }
    }
}

