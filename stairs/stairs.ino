/*
 * This code controls the light for the stairs.
 * 
 * There are 2 motion switches on the floor, 
 * one checks for the present on the bottom of the stairs
 * the other checks for the whole stairs.
 * 
 * If the first one is on and the state is off the leds lights up bottom-up
 * The lights stay lighted until the second control is off.
 * 
 * If the second one is on but the first one is off and the state is off leds lights up top-down
 * The lights stay lighted until the second control is off.
 *
 * Additionally there is a light sensor that is measuring the brightness of the scene.
 * The darker it is, the more dark-reddish the color of the leds is.
 * For bright scenes, the light is warm white.
 */
#include <Adafruit_NeoPixel.h>

#define LED_PIN 2
#define LIGHT_UP_DELAY 7  // time between each led on
#define NUMPIXELS 98

#define LOCAL_MOTION 3
#define SCENE_MOTION 4
#define LIGHT_SENSOR 7
#define MIN_TIME_ON 10000  // min time of lights being on after the first motion
#define MIN_TIME_ON_AFTER_MOTION 5000  // min time of lights being on after any motion

Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
bool lightsOn = false;
double timeSwitchOn = 0;
double lastMotionTime = 0;

struct Color {
  int red;
  int green;
  int blue;
};


void setup() {
  pixels.begin();
  pinMode(LOCAL_MOTION, INPUT);
  pinMode(SCENE_MOTION, INPUT);
  Serial.begin(9600);
}

Color getColor() {
  Color color;

  int brightRed = 255, brightGreen = 123, brightBlue = 51;
  int darkRed = 32, darkGreen = 12, darkBlue = 6;

  int brightThreshold = 100;
  int darkThreshold = 900;

  int brightness = analogRead(LIGHT_SENSOR);

  if (brightness < brightThreshold) {  // Very bright condition
    color.red = brightRed;
    color.green = brightGreen;
    color.blue = brightBlue;
  } 
  else if (brightness > darkThreshold) {  // Very dark condition
    color.red = darkRed;
    color.green = darkGreen;
    color.blue = darkBlue;
  } 
  else {
    // Intermediate brightness; interpolate between bright and dark values
    color.red = map(brightness, brightThreshold, darkThreshold, brightRed, darkRed);
    color.green = map(brightness, brightThreshold, darkThreshold, brightGreen, darkGreen);
    color.blue = map(brightness, brightThreshold, darkThreshold, brightBlue, darkBlue);
  }

  return color;
}

void lightOn(Color color, bool bottomUp){
  pixels.clear(); 

  for(int i=0; i<NUMPIXELS; i++) { 
    int pixel_num = i;
    if (bottomUp)  // Top-down case
      pixel_num = NUMPIXELS - 1 - i;
      
    pixels.setPixelColor(pixel_num, pixels.Color(color.red, color.green, color.blue));
    pixels.show();
    delay(LIGHT_UP_DELAY);
  }
}

void lightUp(Color color){
  lightOn(color, true);
}

void lightDown(Color color){
  lightOn(color, false);
}

void lightOff() {
  for (int brightness = 255; brightness >= 0; brightness -= 5) { // Gradually decrease brightness
    for (int i = 0; i < NUMPIXELS; i++) {
      uint32_t currentColor = pixels.getPixelColor(i);  // Get current color of the pixel
      int red = (currentColor >> 16) & 0xFF;   // Extract the red component
      int green = (currentColor >> 8) & 0xFF;  // Extract the green component
      int blue = currentColor & 0xFF;          // Extract the blue component

      // Scale each color component by the current brightness level
      pixels.setPixelColor(i, pixels.Color((red * brightness) / 255, (green * brightness) / 255, (blue * brightness) / 255));
    }
    pixels.show();
    delay(20);  // Small delay to create a smooth dimming effect
  }
  pixels.clear();  // Ensure all LEDs are fully off at the end
  pixels.show();
}

bool checkLocalMotion() {
  int sensorValue = digitalRead(LOCAL_MOTION);
  if (sensorValue == 1){
    Serial.print("local motion ");
    Serial.println(millis());
    return true;
  }
  return false;
}

bool checkSceneMotion() {
  int sensorValue = digitalRead(SCENE_MOTION);
  if (sensorValue == 1){
    Serial.print("scene motion ");
    Serial.println(millis());
    return true;
  }
  return false;
}

void loop() {
  bool localMotion = checkLocalMotion();
  bool sceneMotion = checkSceneMotion();
  if (localMotion || sceneMotion){
    lastMotionTime = millis();
  }
  Color color = getColor(); 

  if (lightsOn) {
    double timeOn = millis() - timeSwitchOn;
    double timeAfterLastMotion = millis() - lastMotionTime;

    if (!sceneMotion && !localMotion && (timeOn > MIN_TIME_ON) && (timeAfterLastMotion > MIN_TIME_ON_AFTER_MOTION)){
      lightOff();
      lightsOn = false;
    }
  }
  else {
    if (localMotion) {
      timeSwitchOn = millis();
      lightUp(color);
      lightsOn = true;
    }
    else if (sceneMotion){
      timeSwitchOn = millis();
      lightDown(color);
      lightsOn = true;
    }
  }
  delay(50);
}
