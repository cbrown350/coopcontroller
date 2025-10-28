
#include <Arduino.h>
// #include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <OneWire.h>
#include <DallasTemperature.h>

/*
  U8g2lib Example Overview:
    Frame Buffer Examples: clearBuffer/sendBuffer. Fast, but may not work with all Arduino boards because of RAM consumption
    Page Buffer Examples: firstPage/nextPage. Less RAM usage, should work with all Arduino boards.
    U8x8 Text Only Example: No RAM usage, direct communication with display controller. No graphics, 8x8 Text only.
    
*/

#include "HeaterController.h"

// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 22, /* data=*/ 21, /* reset=*/ U8X8_PIN_NONE);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(DS18B20_TEMP_SENSOR_PIN);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature tempSensors(&oneWire);

// void setup1(void) {
//   Serial.begin(115200);
//   // u8g2.begin();
//   tempSensors.begin();
//   Serial.println("Dallas Temperature IC Control Library Demo");

//   // Configure encoder pins as inputs with pullups
//   pinMode(ROTARY_ENCODER_A_PIN, INPUT_PULLUP);
//   pinMode(ROTARY_ENCODER_B_PIN, INPUT_PULLUP); 
//   pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);

//   pinMode(FILAMENT_RUNOUT_PIN, INPUT_PULLUP);
//   pinMode(MOVEMENT_SENSOR_PIN, INPUT_PULLUP);
  
//   pinMode(BLUE_LED_PIN, OUTPUT);

//   digitalWrite(RELAY_PIN, HIGH);
//   pinMode(RELAY_PIN, OUTPUT);

//   pinMode(BUZZER_PIN, OUTPUT);
//   digitalWrite(BUZZER_PIN, HIGH);
  
//   // call sensors.requestTemperatures() to issue a global temperature
//   // request to all devices on the bus
//   Serial.print("Requesting temperatures...");
//   tempSensors.requestTemperatures(); // Send the command to get temperatures
//   Serial.println("DONE");

//   Serial.print("Temperature for the device 1 (index 0) is: ");
//   Serial.println(tempSensors.getTempCByIndex(0));

//   // count++;
//   // tempSensors.setUserDataByIndex(0, count);
//   // int x = tempSensors.getUserDataByIndex(0);
//   // Serial.println(count);
// }

// #include <ESP32Encoder.h>
// // #include <Adafruit_GFX.h>
// // #include <Adafruit_SSD1306.h>
// #include "esp_task_wdt.h"

// static IRAM_ATTR void enc_cb(void* arg) {
//   ESP32Encoder* enc = (ESP32Encoder*) arg;
//   if(enc->getCount() > 55) {
//     enc->setCount(30);
//   }
//   if(enc->getCount() < 30) {
//     enc->setCount(55);
//   }
//   Serial.printf("enc cb: count: %d\n", enc->getCount());
//   static bool leds = false;
//   // digitalWrite(LED_BUILTIN, (int)leds);
//   leds = !leds;
// }

// extern bool loopTaskWDTEnabled;
// extern TaskHandle_t loopTaskHandle;

// ESP32Encoder encoder(true, enc_cb);
// // Adafruit_SSD1306 display(128, 32, &Wire);

// static const char* LOG_TAG = "main";

// void setup(){
//   loopTaskWDTEnabled = true;
//   // pinMode(LED_BUILTIN, OUTPUT);
//   Serial.begin(115200);
//   // Encoder A and B pins connected with 1K series resistor to pins 4 and 5, common pin to ground.
//   //         |- A   --- 1K --- pin 4
//   //  >=[enc]|- GND
//   //         |- B   --- 1K --- pin 5

//   ESP32Encoder::useInternalWeakPullResistors = puType::up;
//   encoder.attachSingleEdge(ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_A_PIN);
//   // encoder.clearCount();
//   encoder.setCount(30);
//   encoder.setFilter(1023);

//   // if (! display.begin(SSD1306_SWITCHCAPVCC, 0x3c))
//   // {
//   //   for (;;) {
//   //     Serial.println("display init failed");
//   //   }
//   // }
//   // display.cp437(true);
//   // display.clearDisplay();
//   // display.setTextSize(2);
//   // display.setTextColor(WHITE);
//   // display.setCursor(0,0);
//   // display.display();
//   esp_log_level_set("*", ESP_LOG_DEBUG);
//   esp_log_level_set("main", ESP_LOG_DEBUG);
//   esp_log_level_set("ESP32Encoder", ESP_LOG_DEBUG);
//   esp_task_wdt_add(loopTaskHandle);
// }

// void loop(){

//   // display.clearDisplay();
//   // display.setCursor(0,0);
//   // display.printf("E: %lld\n", encoder.getCount());
//   // display.display();
//   // Serial.printf("Enc count: %d\n", encoder.getCount());
//   delay(500);
  
// }

// // void loop(void) {
// //   u8g2.clearBuffer();					// clear the internal memory
// //   u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
// //   u8g2.drawStr(0,16,"Hello World!");	// write something to the internal memory
// //   u8g2.sendBuffer();					// transfer internal memory to the display
// //   delay(500);  

// //   // // Set up ESP32 PWM to output 2kHz signal on BUZZER_PIN
// //   // ledcSetup(0, 2000, 8);
// //   // ledcAttachPin(BUZZER_PIN, 0);
// //   // ledcWrite(0, 128);  // Set duty cycle to 50%

// //   // Starting at 0, display a number and increment it when the encoder is rotated one way and decrement when rotated the other. The pushbutton should reset to 0
// //   int counter = 0;
// //   static int lastEncA = 0;
// //   static int lastEncB = 0;

// //   while(1) {
// //     int encA = digitalRead(ROTARY_ENCODER_A_PIN);
// //     int encB = digitalRead(ROTARY_ENCODER_B_PIN);
// //     int sw = digitalRead(ROTARY_ENCODER_PUSHBUTTON_SW_PIN);

// //     // Detect encoder rotation
// //     if(encA != lastEncA) {
// //       if(encB != encA) {
// //         counter++;
// //       } else {
// //         counter--;
// //       }
// //     }
// //     lastEncA = encA;
// //     lastEncB = encB;

// //     // Reset counter on button press
// //     if(sw == LOW) {
// //       counter = 0;
// //     }

// //     // Update display
// //     u8g2.clearBuffer();
// //     u8g2.setFont(u8g2_font_ncenB14_tr);
// //     u8g2.drawStr(0, 32, ("Count: " + String(counter)).c_str());

// //     if(digitalRead(FILAMENT_RUNOUT_B_PIN) == LOW) {
// //       u8g2.setFont(u8g2_font_ncenB14_tr);
// //       u8g2.drawStr(0, 15, "Runout");
// //     }
// //     if(digitalRead(FILAMENT_MOVEMENT_SENSE_B_PIN) == LOW) {
// //       u8g2.setFont(u8g2_font_ncenB14_tr);
// //       u8g2.drawStr(0, 15, "Movement");
// //     }
    
// //     delay(5); // Debounce delay

    
// //   //   digitalWrite(BLUE_LED_PIN, HIGH);
// //   //   // digitalWrite(27, LOW);
// //   //   delay(500);
// //   //   digitalWrite(BLUE_LED_PIN, LOW);
// //   //   // digitalWrite(27, HIGH);
// //   //   delay(500);

// //     // Read ADC pin and display max and min values of sin wave
// //     int adcValue = analogRead(CURRENT_SENSE_PIN);
// //     static int maxAdcValue = 0;
// //     static int minAdcValue = 1023;

// //     if (adcValue > maxAdcValue) {
// //       maxAdcValue = adcValue;
// //     }
// //     if (adcValue < minAdcValue) {
// //       minAdcValue = adcValue;
// //     }

// //     u8g2.setFont(u8g2_font_ncenB14_tr);
// //     u8g2.drawStr(0, 48, ("Max: " + String(maxAdcValue)).c_str());
// //     u8g2.drawStr(0, 64, ("Min: " + String(minAdcValue)).c_str());
      
// //     u8g2.sendBuffer();
    
// //     Serial.print("Requesting temperatures...");
// //     tempSensors.requestTemperatures(); // Send the command to get temperatures
// //     Serial.println("DONE");

// //     Serial.print("Temperature for the device 1 (index 0) is: ");
// //     Serial.println(tempSensors.getTempFByIndex(0));
// //   }


// //   // // Blink LEDs on GPIO15 and GPIO2 1Hz
// //   // pinMode(2, OUTPUT);
// //   // pinMode(27, OUTPUT);
// //   // while (true) {
// //   //   digitalWrite(2, HIGH);
// //   //   // digitalWrite(27, LOW);
// //   //   delay(500);
// //   //   digitalWrite(2, LOW);
// //   //   // digitalWrite(27, HIGH);
// //   //   delay(500);
// //   // }
// // }


#include <ESP32RotaryEncoder.h>

RotaryEncoder rotaryEncoder( ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN);

void knobCallback( long value )
{
	Serial.printf( "Value: %ld\r", value );
}

void buttonCallback( unsigned long duration )
{
	Serial.printf( "boop! button was down for %lu ms\n", duration );
}

void setup()
{
	Serial.begin( 115200 );

	// This tells the library that the encoder has its own pull-up resistors
	rotaryEncoder.setEncoderType( EncoderType::FLOATING );

	// Range of values to be returned by the encoder: minimum is 1, maximum is 10
	// The third argument specifies whether turning past the minimum/maximum will
	// wrap around to the other side:
	//  - true  = turn past 10, wrap to 1; turn past 1, wrap to 10
	//  - false = turn past 10, stay on 10; turn past 1, stay on 1
	rotaryEncoder.setBoundaries( 30, 55, true );

	// The function specified here will be called every time the knob is turned
	// and the current value will be passed to it
	rotaryEncoder.onTurned( &knobCallback );

	// The function specified here will be called every time the button is pushed and
	// the duration (in milliseconds) that the button was down will be passed to it
	rotaryEncoder.onPressed( &buttonCallback );

	// This is where the inputs are configured and the interrupts get attached
	rotaryEncoder.begin();
}

void loop()
{
	// Your stuff here
}