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
int maxSpeed = 1000;
int calibrationSpeed = 500;
int acceleration = 2000;
int immediateStopAcceleration = 100000;
long startPosition = 0;
long endPosition = 0;
long targetPosition = 0;
int current_position = -1;  //0- open, 1 - closed, -1-between or unk

enum PositionState {
  OPEN,
  CLOSE,
  BETWEEN,
  UNKNOWN
};

PositionState currentPosition = UNKNOWN;


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
  currentPosition = UNKNOWN;
  Serial.println("Opening...");

  while (true) {
    stepper.runSpeed(); // Ensure continuous motion
    
    if (digitalRead(STOP_INPUT) == LOW) {
      stepper.stop();
      Serial.println("Stopping motor.");
      return;
    }

    if (digitalRead(LIMIT_OPEN) == LOW && opening) {
      stepper.stop();

      Serial.println("Reached open limit");
      moveFromLimit(-calibrationSpeed, LIMIT_OPEN);
      stepper.setCurrentPosition(startPosition);
      stepper.setSpeed(-calibrationSpeed); // Reverse direction for closing
      Serial.println("Closing...");
      opening = false;
    }

    if (digitalRead(LIMIT_CLOSE) == LOW && !opening) {
      stepper.stop();

      Serial.println("Reached close limit");      
      Serial.println("Moving away from the close limit");
      moveFromLimit(calibrationSpeed, LIMIT_CLOSE);
      delay(100); 

      endPosition = stepper.currentPosition();
      Serial.print("Setting end position to ");
      Serial.println(endPosition);
      break;
    }
  }
  currentPosition = CLOSE;
  moving = false;
  Serial.println("Calibration finished.");
  delay(1000);  // in case of rebouce from the limit switch
}


void loop() {
    long position = stepper.currentPosition();    

    if (digitalRead(OPEN_INPUT) == LOW && !moving && currentPosition != OPEN) {
        Serial.println("Opening...");
        stepper.moveTo(startPosition);
        targetPosition = startPosition;
        moving = true;
        currentPosition = UNKNOWN;
    }
    if (digitalRead(CLOSE_INPUT) == LOW && !moving && currentPosition != CLOSE) {
        Serial.println("Closing...");
        stepper.moveTo(endPosition); 
        targetPosition = endPosition;
        moving = true;
        currentPosition = UNKNOWN;
    }
    if (digitalRead(STOP_INPUT) == LOW && moving) {
        Serial.println("Stopping motor.");
        stepper.stop();      
        moving = false;
        currentPosition = UNKNOWN;
    }
    if (digitalRead(LIMIT_OPEN) == LOW && moving) {
        Serial.println("Reached open limit.");
        stepper.setAcceleration(immediateStopAcceleration);
        stepper.stop();
        currentPosition = UNKNOWN;
        calibrate();
    }
    if (digitalRead(LIMIT_CLOSE) == LOW && moving) {
        Serial.println("Reached close limit.");
        stepper.setAcceleration(immediateStopAcceleration);
        stepper.stop();
        currentPosition = UNKNOWN;
        calibrate();
    }
    stepper.run();
    
    if ((abs(position - targetPosition) < 1) && moving){
        Serial.println("Target reached.");
        stepper.stop();
        stepper.disableOutputs();
        moving = false;
        if (targetPosition == startPosition)
            currentPosition = OPEN;
        else
            currentPosition = CLOSE;
    }
}