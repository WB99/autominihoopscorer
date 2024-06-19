#include <Arduino_LSM9DS1.h>

constexpr int red_led = 22; 
constexpr int green_led = 23; 

const float triggerThreshold = 1.1; // Example threshold value in G's
const int captureDuration = 1500; // Capture for 1500 milliseconds after triggering
const int numSamples = 176; // Only capture 176 samples per shot

// const int captureDuration = 1000;
// const int numSamples = 115;

unsigned long lastTriggerTime = 0;
int shotCount = 0; // Initialize shot counter

void setup() {
  // Shot ready/making inference LED signal setup
  pinMode(green_led, OUTPUT);
  pinMode(red_led, OUTPUT);
  digitalWrite(green_led, LOW);
  digitalWrite(red_led, HIGH);

  Serial.begin(9600);
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.println("ShotID, Time, aX, aY, aZ, gX, gY, gZ");
}

void loop() {
  float aX, aY, aZ, gX, gY, gZ;
  unsigned long currentTime = millis();
  int samplesCount = 0; // Counter for samples captured per shot

  if (currentTime - lastTriggerTime > captureDuration && IMU.accelerationAvailable()) {
    IMU.readAcceleration(aX, aY, aZ); // Read initial acceleration to check for trigger
    float accelerationMagnitude = sqrt(aX * aX + aY * aY + aZ * aZ);

    // Check if acceleration exceeds threshold
    if (accelerationMagnitude > triggerThreshold) {
      // Set LED to show busy
      digitalWrite(green_led, HIGH);
      digitalWrite(red_led, LOW);

      lastTriggerTime = currentTime; // Update last trigger time
      shotCount++; // Increment shot counter
      Serial.println("SHOT START"); // Indicate Start of shot capture

      while (samplesCount < numSamples) {
        if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
          IMU.readAcceleration(aX, aY, aZ);
          IMU.readGyroscope(gX, gY, gZ);

          // Format the output without spaces after commas
          Serial.print(shotCount); Serial.print(",");
          Serial.print(samplesCount++); Serial.print(",");
          Serial.print(aX, 3); Serial.print(",");
          Serial.print(aY, 3); Serial.print(",");
          Serial.print(aZ, 3); Serial.print(",");
          Serial.print(gX, 3); Serial.print(",");
          Serial.print(gY, 3); Serial.print(",");
          Serial.print(gZ, 3); Serial.println();
        }
      }

      // Indicate the end of shot data capture
      Serial.println("SHOT END");
      // Reset green & red LEDs to show ready
      delay(200);  // Cooldown to prevent retriggering IMU from same shot
      digitalWrite(green_led, LOW);
      digitalWrite(red_led, HIGH);
    }
  }
}
