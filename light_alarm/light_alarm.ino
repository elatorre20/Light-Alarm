#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//timer 
#define TIMER_INTERVAL 100
volatile uint16_t timer_ticks, display_count, seconds, minutes; //to keep track of time
volatile uint32_t debounce = 0; //for debouncing the switch
volatile uint8_t timer_period; //for task scheduling, to ensure that tasks are not repeated within 1 period
uint16_t alarm_time = 6; //time for the alarm to be triggered, default 7:00
uint16_t alarm_start_time = 2; //time to start ramping up alarm
uint8_t alarm_speed = 120; //number of seconds over which to ramp up the brightness
uint8_t snoozed = 0;
void timerSetup(){//period roughly in MS with a 16MHz clock. 
  //Note that if you are not using a 32u4-based MCU, you will have to change the register names/setup appropriately
  TCCR1A = 0; //no OC
  TCCR1B = 0; //clear control register
  TCCR1B = (1<<CS12)|(0<<CS11)|(0<<CS10)|(0<<WGM13)|(1<<WGM12); //256 prescaler, counting up to OCR1A
  OCR1A = TIMER_INTERVAL; //16,000,000/256/OCR1A = 625Hz
  TIMSK1 |= (1<<OCIE1A);
}
ISR(TIMER1_COMPA_vect){ //timer interrupt handler
  timer_period = 1; //for task scheduling
  timer_ticks++;
  debounce++; //increment debounce counter
  display_count++; //increment display counter
  if(timer_ticks >= 625){ //every 625 ticks add a second
    timer_ticks = 0;
    seconds++;
  }
  if(seconds >= 60){ //every 60 seconds add a minute
    seconds = 0;
    minutes++;
  }
  if(minutes >= 1440){ //1440 minutes in a day
    minutes = 0;
  }
  return true;
}

//controls
#define ENCODER_PIN_0 10 //rotary encoder pins
#define ENCODER_PIN_1 11
#define BTN_PIN       12 //mode button pin
uint8_t mode = 0;
uint8_t encoder_0_last = 1;   //control mode

//light
#define LAMP_PIN 5 //lamp FET pin
uint8_t levels[9] = {0,1,3,7,15,31,63,127,255};
uint8_t brightness, alarm_brightness = 0; //lamp brightness

//display 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
#define SSD1306_NO_SPLASH
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
char print_buf[32];

void setup() {


  //debug serial
  Serial.begin(9600);

  //begin scheduling timer
  timerSetup();

  //pin setups
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(ENCODER_PIN_0, INPUT_PULLUP);
  pinMode(ENCODER_PIN_1, INPUT_PULLUP);
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(6, OUTPUT);
  digitalWrite(6, HIGH); //pullup for the I2C

  //display setup
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);           
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  display.clearDisplay();
  display.display();
  display.setTextSize(2);

}

void loop() {
  if(timer_period){//only do max once per tick
    analogWrite(LAMP_PIN, brightness); //apply lamp brightness
    if(!digitalRead(BTN_PIN)){//cant use interrupt on button pin so we have to poll and debounce it here
      if(debounce > 200){
        if(mode == 0){
          snoozed = 1;
        }
        debounce = 0;
        mode++;
        mode = mode % 6;
      }
    }
    if(!digitalRead(ENCODER_PIN_0)){ //poll and debounce the encoder
      if(debounce > 50){
        if(digitalRead(ENCODER_PIN_1)){ //encoder has turned one step counterclockwise
          //do nothing in clock mode
          if(mode == 1){ //alarm time set mode
            if(alarm_time == 0){
              alarm_time = 1439; //wraparound from 00:00 back to 23:59
            }
            else{
              alarm_time--;
            }
          }
          else if(mode == 2){//alarm brightness set mode
            if(alarm_brightness == 255){
              alarm_brightness = 225;
            }
            else if(brightness){
              alarm_brightness-= 25;
            }
          }
          else if(mode == 3){ //alarm speed set mode
            if(alarm_speed > 1){
              alarm_speed--;
            }
          }
          else if(mode == 4){ //time adjustment mode
            if(minutes == 0){
              minutes = 1439; //wraparound from 00:00 back to 23:59
            }
            else{
              minutes--;
            } 
          }
          else if(mode == 5){//lamp mode
            if(brightness == 255){
              brightness = 225;
            }
            else if(brightness){
              brightness-= 25;
            }
          }
        }
        else{ //encoder has turned one step clockwise
          //do nothing in clock mode
          if(mode == 1){ //alarm time set mode
            alarm_time++;
            alarm_start_time = alarm_time - 5;
            if(alarm_time <= 1440){
              alarm_time -= 1440;
              alarm_start_time -= 1440;
            }
          }
          else if(mode == 2){//alarm brightness set mode
            if(alarm_brightness < 225){
              alarm_brightness += 25;   
            }
            else{
              alarm_brightness = 255;
            }
          }
          else if(mode == 3){ //alarm speed set mode
            if(alarm_speed > 1){
              alarm_speed--;
            }
          }
          else if(mode == 4){ //time adjustment mode
            minutes++;
          }
          else if(mode == 5){//lamp mode
            if(brightness < 225){
              brightness += 25;   
            }
            else{
              brightness = 255;
            }
          }
        }
        debounce = 0;
      }
    }
    if(display_count > 315){//write to screen at 2FPS
      display_count = 0;
      Serial.println(alarm_start_time);
      Serial.println(minutes);
      Serial.println(snoozed);
      if(mode == 0){ //normal clock mode, display clock and wait for alarm
        brightness = 0;
        if(minutes == (alarm_start_time)){ //reset snooze before the alarm starts
          snoozed = 0; 
        }
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(15,10);
        sprintf(print_buf, "%02d:%02d:%02d", (minutes/(uint16_t)60), (minutes%60), seconds);
        display.print(print_buf);
        display.display();
        // Serial.println(print_buf);
      }
      else if(mode == 1){ //set alarm time
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm time:"));
        display.setTextSize(2);
        display.setCursor(15,10);
        sprintf(print_buf, "%02d:%02d:%02d", (alarm_time/(uint16_t)60), (alarm_time%60), 0);
        display.print(print_buf);
        display.display();
        // Serial.println(F("Set alarm time:"));
      }
      else if(mode == 2){ //set alarm brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm brightness:"));
        display.setTextSize(2);
        display.setCursor(30,10);
        memset(print_buf, 0, 32);
        sprintf(print_buf, "%2d", (alarm_brightness/25));
        display.print(print_buf);
        display.setCursor(0,25);
        display.setTextSize(1);
        display.print(F("(0 to turn alarm off)"));
        display.display();
        // Serial.println(F("Set alarm brightness:"));
      }
      else if(mode == 3){ //set alarm speed
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm speed:"));
        display.setTextSize(2);
        display.setCursor(30,10);
        memset(print_buf, 0, 32);
        sprintf(print_buf, "%3d", alarm_speed);
        display.print(print_buf);
        display.setCursor(0,25);
        display.setTextSize(1);
        display.print(F("(0 to turn on sharply)"));
        display.display();
        // Serial.println(F("Set alarm speed:"));
      }
       else if(mode == 4){ //set current time
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set current time:"));
        display.display();
        display.setTextSize(2);
        display.setCursor(15,10);
        sprintf(print_buf, "%02d:%02d:%02d", (minutes/(uint16_t)60), (minutes%60), seconds);
        display.print(print_buf);
        display.display();
        // Serial.println(F("Set current time:"));
      }
      else if(mode == 5){ //lamp mode with adjustable brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Lamp mode: "));
        display.setTextSize(2);
        display.setCursor(30,10);
        memset(print_buf, 0, 32);
        sprintf(print_buf, "%2d", (brightness/25));
        display.print(print_buf);
        display.display();
        // Serial.println(F("Lamp mode: "));
      }
    }
  }
  timer_period = 0;
}