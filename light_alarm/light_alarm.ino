#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//timer 
#define TIMER_INTERVAL 100
volatile uint16_t timer_ticks, display_count, seconds, minutes, hours; //to keep track of time
volatile uint32_t debounce = 0; //for debouncing the switch
volatile uint8_t timer_period; //for task scheduling, to ensure that tasks are not repeated within 1 period
uint16_t alarm_time; //time for the alarm to be triggered
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
  if(timer_ticks == 625){ //every 625 ticks add a second
    timer_ticks = 0;
    seconds++;
  }
  if(seconds == 60){ //every 60 seconds add a minute
    seconds = 0;
    minutes++;
  }
  if(minutes == 60){ //every 60 minutes add an hour
    minutes = 0;
    hours = (hours++)%24; //24 hours in a day
  }
  return true;
}

//controls
#define ENCODER_PIN_0 10 //rotary encoder pins
#define ENCODER_PIN_1 11
#define BTN_PIN       12 //mode button pin
uint8_t mode, encoder_0_last, encoder_1_last, btn_count = 0;   //control mode

//light
#define LAMP_PIN 3 //lamp FET pin
uint16_t brightnes; //lamp brightness

//display 
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 32 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
#define SSD1306_NO_SPLASH
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
char printBuf[32];

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
    if(!digitalRead(BTN_PIN)){//cant use interrupt on button pin so we have to poll and debounce it here
      if(debounce > 200){
        debounce = 0;
        mode++;
        mode = mode % 5;
        btn_count++;
      }
    }
    if(display_count > 315){//write to screen
      display_count = 0;
      Serial.println(mode);
      if(mode == 0){ //normal mode, display clock and wait for alarm
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(15,10);
        sprintf(printBuf, "%02d:%02d:%02d", hours, minutes, seconds);
        display.print(printBuf);
        display.display();
        Serial.println(printBuf);
      }
      else if(mode == 1){ //set alarm time
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm time:"));
        display.display();
        Serial.println(F("Set alarm time:"));
      }
      else if(mode == 2){ //set alarm brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm brightness:"));
        display.display();
        Serial.println(F("Set alarm brightness:"));
      }
      else if(mode == 3){ //set alarm brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Set alarm speed:"));
        display.display();
        Serial.println(F("Set alarm speed:"));
      }
      else if(mode == 4){ //lamp mode with adjustable brightness
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0,0);
        display.print(F("Lamp mode: "));
        display.display();
        Serial.println(F("Lamp mode: "));
      }
    }
  }
  timer_period = 0;
}