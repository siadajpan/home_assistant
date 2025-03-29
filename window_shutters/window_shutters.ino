#include <AccelStepper.h>

#define EN_PIN 7
#define DIR_PIN  8
#define STEP_PIN 9
#define LIMIT_OPEN 2
#define LIMIT_CLOSE 3
#define OPEN_INPUT 4
#define CLOSE_INPUT 5
#define STOP_INPUT 6

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
bool moving = false;
int maxSpeed = 500;
int calibrationSpeed = 200;
int acceleration = 2500;
int immediateStopAcceleration = 100000;
long startPosition = 0;
long endPosition = 0;

void setup() {
    Serial.begin(115200);
    pinMode(EN_PIN, OUTPUT);
    pinMode(LIMIT_OPEN, INPUT_PULLUP);
    pinMode(LIMIT_CLOSE, INPUT_PULLUP);
    pinMode(OPEN_INPUT, INPUT_PULLUP);
    pinMode(CLOSE_INPUT, INPUT_PULLUP);
    pinMode(STOP_INPUT, INPUT_PULLUP);

    digitalWrite(EN_PIN, LOW);
    Serial.println("Stepper motor controller initialized.");
    stepper.setMaxSpeed(maxSpeed);
    stepper.setAcceleration(acceleration);
    calibrate();
}

void moveFromLimit(int speed, int limit){
  stepper.setSpeed(speed); // Reverse direction for closing
  
  while (true) {
    stepper.runSpeed();
    int limit_count = 0;
    // Limit is clear 3 time in the row. This is to reduce re-bouncing effect
    for (int i=0; i<3; i++){
      if(digitalRead(limit) == HIGH){
        limit_count ++;
      }
    }
    if (limit_count == 3)
      break;
  }
  Serial.println("Away from limit.");
}

void calibrate() {
  Serial.println("Calibrating...");
  stepper.setSpeed(calibrationSpeed); // Start moving in opening direction
  bool opening = true;

  while (true) {
    stepper.runSpeed(); // Ensure continuous motion

    if (digitalRead(STOP_INPUT) == LOW) {
      stepper.stop();
      Serial.println("Stopping motor.");
      return;
    }

    if (digitalRead(LIMIT_OPEN) == LOW && opening) {
      stepper.stop();
      Serial.println("Reached open limit. Moving away");
      moveFromLimit(-calibrationSpeed, LIMIT_OPEN);
      stepper.setCurrentPosition(startPosition);
      stepper.setSpeed(-calibrationSpeed); // Reverse direction for closing

      opening = false;
    }

    if (digitalRead(LIMIT_CLOSE) == LOW && !opening) {
      stepper.stop();
      Serial.println("Reached close limit. Moving away");
      moveFromLimit(calibrationSpeed, LIMIT_CLOSE);
      endPosition = stepper.currentPosition();

      break;
    }
  }
  Serial.println("Calibration finished.");
  stepper.moveTo(endPosition); 
}


void loop() {
    bool moving = stepper.isRunning();
    if (digitalRead(OPEN_INPUT) == LOW && !moving) {
        Serial.println("Opening...");
        stepper.moveTo(startPosition); 
    }
    if (digitalRead(CLOSE_INPUT) == LOW && !moving) {
        Serial.println("Closing...");
        stepper.moveTo(endPosition); 
    }
    if (digitalRead(STOP_INPUT) == LOW && moving) {
        Serial.println("Stopping motor.");
        stepper.stop();
    }
    if (digitalRead(LIMIT_OPEN) == LOW && moving) {
        Serial.println("Reached open limit.");
        stepper.setAcceleration(immediateStopAcceleration);
        stepper.stop();
        calibrate();
    }
    if (digitalRead(LIMIT_CLOSE) == LOW && moving) {
        Serial.println("Reached close limit.");
        stepper.setAcceleration(immediateStopAcceleration);
        stepper.stop();
        calibrate();
    }
    stepper.run();
}
