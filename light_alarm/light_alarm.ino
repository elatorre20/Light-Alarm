#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimeLib.h>

//timer 
#define TIMER_INTERVAL 101
volatile uint16_t timer_ticks, display_count, seconds, minutes, time_update_flag; //to keep track of time
volatile uint32_t debounce = 0; //for debouncing the switch
volatile uint8_t timer_period; //for task scheduling, to ensure that tasks are not repeated within 1 period
uint16_t alarm_time = 420; //time for the alarm to be triggered, default 7:00
uint8_t snoozed = 1; //whether the user has hit the button after the alarm triggers
void timerSetup(){//period in MS with a 16MHz clock. 
  //Note that if you are not using a 32u4-based MCU, you will have to change the register names/setup appropriately
  TCCR1A = 0; //no OC
  TCCR1B = 0; //clear control register
  TCCR1B = (1<<CS12)|(0<<CS11)|(0<<CS10)|(0<<WGM13)|(1<<WGM12); //256 prescaler, counting up to OCR1A
  OCR1A = TIMER_INTERVAL; //16,000,000/256/(OCR1A-1) = 625Hz
  TIMSK1 |= (1<<OCIE1A);
}
ISR(TIMER1_COMPA_vect){ //timer interrupt handler
  timer_period = 1; //for task scheduling
  timer_ticks++;
  debounce++; //increment debounce counter
  display_count++; //increment display counter
  if(timer_ticks >= 625){ //every 625 ticks add a second
    timer_ticks = 0;
    time_update_flag = 1; //update times in main loop
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
uint8_t brightness = 0; //current brightness
uint8_t lamp_brightness = 0; //lamp mode brightness
uint8_t alarm_brightness = 150; //brightness of alarm

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

  //setup time
  timerSetup();
  setTime(0,0,0,1,1,2000); //set clock time to midnight

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
    if(time_update_flag){ //approximately once a second, update the number of minutes
      seconds = second();
      minutes = minute() + (60*hour()); //day as a number of minutes instead of hours/minutes
      time_update_flag = 0;
    }
    analogWrite(LAMP_PIN, brightness); //apply lamp brightness
    if(!digitalRead(BTN_PIN)){//cant use interrupt on button pin so we have to poll and debounce it here
      if(debounce > 200){
        if(!snoozed){
          Serial.println("snoozed");
          snoozed = 1;
          debounce = 0;
        }
        else{
          debounce = 0;
          mode++;
          mode = mode % 5;
        }
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
              alarm_brightness = 200;
            }
            else if(alarm_brightness){
              alarm_brightness-= 50;
            }
          }
          else if(mode == 3){ //time adjustment mode
            adjustTime((long)-60); //subtract one minute
          }
          else if(mode == 4){//lamp mode
            if(brightness == 255){
              brightness = 200;
            }
            else if(brightness){
              brightness-= 50;
            }
          }
        }
        else{ //encoder has turned one step clockwise
          //do nothing in clock mode
          if(mode == 1){ //alarm time set mode
            alarm_time++;
            if(alarm_time >= 1440){
              alarm_time = 0;
            }
          }
          else if(mode == 2){//alarm brightness set mode
            if(alarm_brightness < 200){
              alarm_brightness += 50;   
            }
            else{
              alarm_brightness = 255;
            }
          }
          else if(mode == 3){ //time adjustment mode
            adjustTime((long)60); //add one minute
          }
          else if(mode == 4){//lamp mode
            if(brightness < 200){
              brightness += 50;   
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
      // sprintf(print_buf, "%02d:%02d:%02d", (minutes/(uint16_t)60), (minutes%60), seconds);
      // Serial.println(print_buf);
      // Serial.println(alarm_time);
      // Serial.println(brightness);
      // Serial.println(snoozed);
      // Serial.println(" ");
      if(mode == 0){ //normal clock mode, display clock and wait for alarm
        if(minutes == alarm_time){ //handle the alarm
          if(seconds == 0){
            snoozed = 0; //reset snooze while the alarm starts
          }
          brightness = seconds * 4; //ramp up over the course of a minute
          if(brightness > alarm_brightness){
            brightness = alarm_brightness; //do not exceed alarm max level
          }
          if(seconds == 59){
            brightness = alarm_brightness; //full brightness at the end of the minute
          }
        }
        if(snoozed){
          brightness = 0; //clear brightness if not currently alarming
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
        sprintf(print_buf, "%2d", (alarm_brightness/50));
        display.print(print_buf);
        display.setCursor(0,25);
        display.setTextSize(1);
        display.print(F("(0 to turn alarm off)"));
        display.display();
        // Serial.println(F("Set alarm brightness:"));
      }
       else if(mode == 3){ //set current time
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
      else if(mode == 4){ //lamp mode with adjustable brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Lamp mode: "));
        display.setTextSize(2);
        display.setCursor(30,10);
        memset(print_buf, 0, 32);
        sprintf(print_buf, "%2d", (brightness/50));
        display.print(print_buf);
        display.display();
        // Serial.println(F("Lamp mode: "));
      }
    }
  }
  timer_period = 0;
}