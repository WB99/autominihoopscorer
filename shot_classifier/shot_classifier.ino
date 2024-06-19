#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include "tensorflow/lite/schema/schema_generated.h"
#include "tflite_model.h"  // Include the model header

BLEDevice central;
BLEService batteryService("180F");                                              
BLEUnsignedCharCharacteristic batteryLevelChar("2A19", BLERead | BLENotify);    

constexpr int red_led = 22; 
constexpr int green_led = 23; 
constexpr int blue_led = 24;
const float triggerThreshold = 1.1;

const int captureDuration = 1500;
const int numSamples = 176;

// new data
// const int captureDuration = 1000;
// const int numSamples = 115;

unsigned long lastTriggerTime = 0;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

constexpr int kTensorArenaSize = 128 * 1024;
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

void setup() {
  // Shot ready/making inference LED signal setup
  pinMode(green_led, OUTPUT);
  pinMode(red_led, OUTPUT);
  digitalWrite(green_led, LOW);
  digitalWrite(red_led, HIGH);

  // IMU setup
  Serial.begin(9600);
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // BLE Setup
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  pinMode(blue_led, OUTPUT); // initialise blue LED to indicate when a central is connected
  BLE.setLocalName("Minihoops");
  BLE.setAdvertisedService(batteryService); // add the service UUID
  batteryService.addCharacteristic(batteryLevelChar); // add the battery level characteristic
  BLE.addService(batteryService); // Add the battery service
  BLE.advertise();
  Serial.println("Bluetooth device active");

  // TFLite Model setup
  tflModel = tflite::GetModel(tflite_model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }

  // Create TFLite interpreter
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      tflModel, resolver, tensor_arena, kTensorArenaSize);
  tflInterpreter = &static_interpreter;

  // Allocate memory
  // if (tflInterpreter->AllocateTensors() != kTfLiteOk) {
  //   Serial.println("AllocateTensors() failed");
  //   return;
  // }

  TfLiteStatus allocate_status = tflInterpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
      Serial.println("AllocateTensors() failed");
      return;
  }

  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}

void loop() {
  // Check BLE central connection once
  central = BLE.central();

  // If no connection, loop to wait for connection
  if (!central) {
    while (1){
      central = BLE.central();
      if (central){
        Serial.print("Connected to central: ");
        Serial.println(central.address());
        digitalWrite(blue_led, HIGH); // Turn off blue LED - connected
        break;
      }
      else {
        BLE.advertise();
        digitalWrite(blue_led, LOW);  // Turn on blue LED - waiting for connection
        Serial.println("Waiting for a connection...");
        delay(1000);  // Add some delay to not flood the serial output
      }
    }
  }

  
  float aX, aY, aZ, gX, gY, gZ;
  unsigned long currentTime = millis();
  int samplesCount = 0;

  if (currentTime - lastTriggerTime > captureDuration && IMU.accelerationAvailable()) {
    IMU.readAcceleration(aX, aY, aZ);
    float accelerationMagnitude = sqrt(aX * aX + aY * aY + aZ * aZ);

    // Start IMU capture when trigger threshold exceeded (shot detected)
    if (accelerationMagnitude > triggerThreshold) {
      digitalWrite(green_led, HIGH);
      digitalWrite(red_led, LOW);
      
      lastTriggerTime = currentTime;
      Serial.println();
      Serial.println("SHOT START");

      while (samplesCount < numSamples) {
        if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
          IMU.readAcceleration(aX, aY, aZ);
          IMU.readGyroscope(gX, gY, gZ);

          // Normalize the IMU data between 0 to 1 and store in the model's input tensor
          tflInputTensor->data.f[samplesCount * 6 + 0] = (aX + 4.0) / 8.0;
          tflInputTensor->data.f[samplesCount * 6 + 1] = (aY + 4.0) / 8.0;
          tflInputTensor->data.f[samplesCount * 6 + 2] = (aZ + 4.0) / 8.0;
          tflInputTensor->data.f[samplesCount * 6 + 3] = (gX + 2000.0) / 4000.0;
          tflInputTensor->data.f[samplesCount * 6 + 4] = (gY + 2000.0) / 4000.0;
          tflInputTensor->data.f[samplesCount * 6 + 5] = (gZ + 2000.0) / 4000.0;

          samplesCount++;
        }
      }

      Serial.println("SHOT END");

      if (tflInterpreter->Invoke() != kTfLiteOk) {
        Serial.println("Invoke failed!");
        return;
      } else {
        // Print predicitons on serial monitor
        float output_missed = tflOutputTensor->data.f[0];
        float output_made = tflOutputTensor->data.f[1];
        Serial.print("MISSED: ");
        Serial.println(output_missed, 3);
        Serial.print("MADE: ");
        Serial.println(output_made, 3);
        
        // Send prediciton (0/1) via bluetooth
        int result = output_missed > 0.5 ? 0 : 1;
        batteryLevelChar.writeValue(result);

        // Reset green & red LEDs
        digitalWrite(green_led, LOW);
        digitalWrite(red_led, HIGH);
      }
    }
  }
}
