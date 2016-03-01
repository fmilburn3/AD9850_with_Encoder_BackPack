/*
   Energia sketch for an encoder with push button to control a AD9850 frequency generator.  
   Displays to the FR6989 LCD if available.
   
   The button is coded to increment/decrement the change associated with turning the encoder. For
   example, the encoder can be set to increase or decrease the value by 100 with each turn of the
   knob. When the button is pushed it can be made to reduce the increment or decrement to 10 
   (instead of 100). This allows the user to rapidly change values or achieve fine control as
   desired.
   
   The sketch uses the FR6989 LCD if available and will automatically switch from Hz to kHz to MHz
   as the frequency increases.  The increment amount from the encoder is displayed via the battery
   symbol. No bars means it is incrementing or decrementing one Hz at a time, one bar means 10 Hz,
   2 bars means 100 Hz, and so on.
   
   Includes code and ideas from the following:
      - Encoder sketch by m3tr0g33k at playground.arduino.cc/Main/RotaryEncoders
   
   Encoder Connections
   There are 5 pins on the encoder.  On one side are 3 pins denoted here as 1, 2, and 3 that
   control the rotary encoder.  The two on the other side (pins 4 and 5) are the push button.
   Encoder      MSP430 Pin      Comments   
   ------------------------------------------------------------
   Pin 1         19             outside encoder pin (Channel A)
   Pin 2        GND             middle encoder pin (Common)
   Pin 3         18             outside encoder pin (Channel B)
   Pin 4         13             push button
   Pin 5        GND             push button
   
   Placing a 0.1 uF capacitor between the push button and ground; and 1 nF capacitors
   between the encoder pins and ground reduced bounce on some of the circuits tried.
   
   Notes:  Connect encoder pins 1, 3, and 4 to microcontroller pins that accept external interrupts.
              (Ports 1 and 2 generally have interrupt capability on the MSP series)
           If the encoder is turning the wrong way, swap pins or swap the pin definition below.
           
   AD9850 Connections
   AD9850       MSP430 Pin      Comments   
   -------------------------------------------------------
   VCC          3V3              
   W_CLK          5    
   FQ_UD          6
   DATA          11
   RESET         12
   GND          GND
   QOUT1                        Square wave out
   QOUT2                        Square wave out
   ZOUT1                        Sine wave out
   ZOUT2                        Sine wave out
   
   Notes:  Only one of the outputs can be active at a time; i.e. not possible to generate a sine
           and square wave at the same time.  The small pot on the board may need adjustment to
           get a good square wave and has limited frequency.

   Created by Frank Milburn    January 2015
   Tested on MSP-EXP430FR6989 and Energia V17
             EK-TM4123GXL and Energia V17
             MSP-EXP432P401R and Energia V17
             
*/

// libraries
#include "energia.h"
#if defined (__MSP430FR6989) 
  #include "LCD_Launchpad.h"
  LCD_LAUNCHPAD lcd;
#endif

// simplify pulsing the AD9850
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }

// pin assignments
const int buttonPin     = 13;               // connect the pushbutton to this LP pin
const int encoderPin1   = 19;               // connect one encoder pin to this LP pin
const int encoderPin2   = 18;               // connect the other encoder pin to this LP pin
const int W_CLK         = 5;                // connect to AD9850 module word load clock pin (CLK)
const int FQ_UD         = 6;                // connect to AD9850 freq update pin (FQ)
const int DATA          = 11;               // connect to AD9850 serial data load pin (DATA)
const int RESET         = 12;               // connect to AD9850 reset pin (RST)

// variables used to control program behavior
const long startVal     = 1000;             // set this to whatever starting value is desired
const long minVal       = 0;                // minimum value that will be allowed as input
const long maxVal       = 32000000;         // maximum value that will be allowed as input
                                            // limiting to 32000000 in order to use integers
const long startInc     = 1000;             // values increase/decrease by this value to start
const long minInc       = 1;                // minimum increment/decrement allowed when turning
const long maxInc       = 1000000;          // maximum increment/decrement allowed when turning
const long divInc       = 10;               // factor for decreasing the increment for improved
                                            // control when the pushbutton is pushed
// variables used in ISRs
volatile long encoderVal = startVal;
volatile long encoderInc = startInc;
volatile boolean encoderLH1 = false;
volatile boolean encoderLH2 = false;

void setup() {
  Serial.begin (115200);
  // FR6989 LCD
  #if defined (__MSP430FR6989)
    lcd.init();
  #endif
  // Encoder
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(encoderPin1, INPUT_PULLUP); 
  pinMode(encoderPin2, INPUT_PULLUP); 
  attachInterrupt(encoderPin1, ISR_Encoder1, CHANGE);  // interrupt for encoderPin1
  attachInterrupt(encoderPin2, ISR_Encoder2, CHANGE);  // interrupt for encoderPin2
  attachInterrupt(buttonPin, ISR_Button, FALLING);     // interrupt for encoder button
  // AD9850
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);
  analogReadResolution(12); // MCU dependent
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);         // this pulse enables serial mode - Datasheet page 12 figure 10
}

void loop(){ 
  // variables used to track whether or not a change has occurred
  static long lastEncoderVal = 1;                         
  static long lastEncoderInc = 0;
  
  //check for change in encoder
  if (lastEncoderVal != encoderVal) {
    if (encoderVal > maxVal) encoderVal = maxVal ;     // do not exceed max input value
    if (encoderVal < minVal) encoderVal = minVal;      // do not drop beneath min input value  
    sendFrequency(encoderVal);   
    printFreq(encoderVal);
    lastEncoderVal = encoderVal;
  }
  // check for change in button
  if (lastEncoderInc != encoderInc) {
    if (encoderInc < minInc) encoderInc = maxInc;      // if below min increment, set to max
    printInc(encoderInc);
    lastEncoderInc = encoderInc;
  } 
}

void ISR_Button() {                                    
  encoderInc /= divInc;                                // change the increment amount
}

void ISR_Encoder1(){                                   
  // Low to High transition?
  if (digitalRead(encoderPin1) == HIGH) { 
    encoderLH1 = true;
    if (!encoderLH2) {
      encoderVal += encoderInc;                        // increase the value+
    }        
  }
  // High-to-low transition?
  if (digitalRead(encoderPin1) == LOW) {
    encoderLH1 = false;
  }
}

void ISR_Encoder2(){
  // Low-to-high transition?
  if (digitalRead(encoderPin2) == HIGH) {  
    encoderLH2 = true;
    if (!encoderLH1) {
      encoderVal -= encoderInc;                       // decrease the value
    }
  }
  // High-to-low transition?
  if (digitalRead(encoderPin2) == LOW) {
    encoderLH2 = false;
  }
}

// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }
}
 
 // frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
void sendFrequency(double frequency) {
  int32_t freq = frequency * 4294967295/125000000;  // note 125 MHz clock on 9850
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
  pulseHigh(FQ_UD);  // Done!  Should see output
}
 
// FR6989 LCD frequency print function
// uses the battery segment indicator to show range of output
void printFreq(long freq) {
  Serial.print("Frequency: "); Serial.print(freq, DEC); Serial.println(" Hz");
  #if defined (__MSP430FR6989)      
    // note that the lcd library will only print 16 bit integers (i.e. smaller than 65,536)
    // We need to go up to 40,000 so using unsigned integer 
    lcd.clear();   
    if (freq > 9999999) {
      lcd.println(freq/1000);              // Displays MHz if above 10 MHz
      lcd.showSymbol(LCD_SEG_DOT1, 0);
      lcd.showSymbol(LCD_SEG_DOT2, 1);
      lcd.showSymbol(LCD_SEG_DOT3, 0);
      lcd.showSymbol(LCD_SEG_DOT4, 0);
      lcd.showSymbol(LCD_SEG_DOT5, 0);
      lcd.showChar('M', 5);
      printInc(encoderInc);
    }
    else if (freq > 999999) {
      lcd.println(int(freq / 1000));       // Displays MHz if 1 MHz to 10 MHz
      lcd.showSymbol(LCD_SEG_DOT1, 1);
      lcd.showSymbol(LCD_SEG_DOT2, 0);
      lcd.showSymbol(LCD_SEG_DOT3, 0);
      lcd.showSymbol(LCD_SEG_DOT4, 0);
      lcd.showSymbol(LCD_SEG_DOT5, 0);
      lcd.showChar('M', 5);
      printInc(encoderInc);
    }
    else if (freq > 99999) {
      lcd.println(int(freq / 100));       // Displays kHz if 100,000 Hz to 1 MHz
      lcd.showSymbol(LCD_SEG_DOT1, 0);
      lcd.showSymbol(LCD_SEG_DOT2, 0);
      lcd.showSymbol(LCD_SEG_DOT3, 1);
      lcd.showSymbol(LCD_SEG_DOT4, 0);
      lcd.showSymbol(LCD_SEG_DOT5, 0);
      lcd.showChar('k', 5);
      printInc(encoderInc);
    }    
    else if (freq > 9999) {
      lcd.println(int(freq / 10));        // Displays kHz if between 10,000 Hz and 100,000 Hz
      lcd.showSymbol(LCD_SEG_DOT1, 0);
      lcd.showSymbol(LCD_SEG_DOT2, 1);
      lcd.showSymbol(LCD_SEG_DOT3, 0);
      lcd.showSymbol(LCD_SEG_DOT4, 0);
      lcd.showSymbol(LCD_SEG_DOT5, 0);
      lcd.showChar('k', 5);
      printInc(encoderInc);
    }
    else {                                  
      lcd.println(int(freq));             // Displays Hz if below 10,000
      lcd.showSymbol(LCD_SEG_DOT1, 0);
      lcd.showSymbol(LCD_SEG_DOT2, 0);
      lcd.showSymbol(LCD_SEG_DOT3, 0);
      lcd.showSymbol(LCD_SEG_DOT4, 0);
      lcd.showSymbol(LCD_SEG_DOT5, 0);
      printInc(encoderInc);
    }        
  #endif
 }
 
// FR6989 LCD increment indication function
// uses the battery indicator to show increments from encoder
void printInc(long increment) {
  Serial.print("Inc/Dec: "); Serial.print(increment, DEC); Serial.println(" Hz");
  #if defined (__MSP430FR6989)        // allows display on the LCD if using a FR6989 LaunchPad   
    if (encoderInc == 1000000) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 1);
      lcd.showSymbol(LCD_SEG_BAT2, 1);
      lcd.showSymbol(LCD_SEG_BAT3, 1); 
      lcd.showSymbol(LCD_SEG_BAT4, 1);
      lcd.showSymbol(LCD_SEG_BAT5, 1);
    }
    else if (encoderInc == 100000) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 1);
      lcd.showSymbol(LCD_SEG_BAT2, 1);
      lcd.showSymbol(LCD_SEG_BAT3, 1); 
      lcd.showSymbol(LCD_SEG_BAT4, 1);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }    
    else if (encoderInc == 10000) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 1);
      lcd.showSymbol(LCD_SEG_BAT2, 1);
      lcd.showSymbol(LCD_SEG_BAT3, 1); 
      lcd.showSymbol(LCD_SEG_BAT4, 0);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }
    else if (encoderInc == 1000) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 1);
      lcd.showSymbol(LCD_SEG_BAT2, 1);
      lcd.showSymbol(LCD_SEG_BAT3, 0); 
      lcd.showSymbol(LCD_SEG_BAT4, 0);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }
    else if (encoderInc == 100) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 1);
      lcd.showSymbol(LCD_SEG_BAT2, 0);
      lcd.showSymbol(LCD_SEG_BAT3, 0); 
      lcd.showSymbol(LCD_SEG_BAT4, 0);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }
    else if (encoderInc == 10) {
      lcd.showSymbol(LCD_SEG_BAT0, 1);      
      lcd.showSymbol(LCD_SEG_BAT1, 0);
      lcd.showSymbol(LCD_SEG_BAT2, 0);
      lcd.showSymbol(LCD_SEG_BAT3, 0); 
      lcd.showSymbol(LCD_SEG_BAT4, 0);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }
    else {                                  
      lcd.showSymbol(LCD_SEG_BAT0, 0);      
      lcd.showSymbol(LCD_SEG_BAT1, 0);
      lcd.showSymbol(LCD_SEG_BAT2, 0);
      lcd.showSymbol(LCD_SEG_BAT3, 0);
      lcd.showSymbol(LCD_SEG_BAT4, 0);
      lcd.showSymbol(LCD_SEG_BAT5, 0);
    }     
  #endif
 }
