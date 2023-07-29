// DIY IV18 VFD Real time digital clock by M.Angelini
// User can set the time with a single button. 
// A short push selects which digits to set (hours, minutes, etc)
// A Long push increments the selection
// Another short push returns to displaying the time as normal

// Code taken from https://www.valvewizard.co.uk/iv18clock.html and sightly  modified to operate with two ULN2803 instesad of ILQ615


#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;


// Definitions 

#define BUTTON_PIN  14      // pushbutton pin (this is also analog pin 0, but INTERNAL_PULLUP only works when you refer to this pin as '14')
#define SER_DATA_PIN  5     // serial data pin for VFD-grids shift register
#define SER_LAT_PIN  3      // latch pin for VFD-grids shift register
#define SER_CLK_PIN  2      // clock pin for VFD-grids shift register
#define LONG_PUSH_TIME  60  // Time to register a long button press
#define DATE_DELAY 4000     // Delay before date is displayed (loop exectutions)
//#define SERIAL            // uncomment for debug


/*******************************************************************************
 * * The following button-polling routine is written as a macro rather than a
 * function in order to avoid the difficulty of passing a pin address and bit 
 * variables to a function argument.
 * Button is momentary, active low.
 * It recognises a short push or long push depending on how long the button has been
 * held down. This also takes care of debouncing.
 ******************************************************************************/
#define POLL_LONG_BUTTON(button,push_n_hold,short_flag,long_flag)   \
                            if(button){                             \
                                if(push_n_hold < LONG_PUSH_TIME){   \
                                    push_n_hold ++;                 \
                                    if(push_n_hold == LONG_PUSH_TIME){\
                                        long_flag = true;              \
                                        push_n_hold = 0;            \
                                    }                               \
                                }                                   \
                            }                                       \
                            else if(push_n_hold >2 && push_n_hold < LONG_PUSH_TIME){\
                                    short_flag = true;                 \
                                    push_n_hold = 0;                \
                            }                                       \
                            else {                                  \
                                push_n_hold = 0;                    \
                            }                                       \


                            
byte user_selection = 0; //Cycle through user selection for setting the time (0 = set hours, 1 = set minutes, 2 = set seconds, 3 = set date, 4 = set month, 5 = set year)
byte push_n_hold; //A counter to time the length of a button push
boolean long_flag = false; //A long-push button flag
boolean short_flag = false; //A short-push button flag
word loopCounter; //A counter to set the display update rate


/******************************************************************************* 
* Digits are lit by shiting out one byte where each bit corresponds to a grid. 
* 1 = on; 0 = off;
* msb = leftmost digit grid;
* lsb = rightmost digit grid.
*******************************************************************************/

/******************************************************************************* 
* Segments are lit by shiting out one byte where each bit corresponds to a segment:  G-B-F-A-E-C-D-dp 
* --A--
* F   B
* --G--
* E   C
* --D----DP (decimal point)
* 
* 1 = on; 0 = off;
*******************************************************************************/
 byte sevenSeg[12] = {
  B01111110, //'0'
  B01000100, //'1'
  B11011010, //'2'
  B11010110, //'3'
  B11100100, //'4'
  B10110110, //'5'
  B10111110, //'6'
  B01010100, //'7'
  B11111110, //'8'
  B11110110, //'9'
  B11110000, //degrees symbol
  B00111010, //'C'
 };

/******************************************************************************* 
Funtion prototypes
*******************************************************************************/
void updateVFD(int pos, byte num, boolean decPoint);

void clearVFD(void);
void displayTime();
void displayDate();
void pollButton();


void setup(){
  #ifdef SERIAL
  Serial.begin(9600);   // Setup Arduino serial connection
  #endif 
 
  pinMode(SER_DATA_PIN,OUTPUT);
  pinMode(SER_CLK_PIN,OUTPUT);
  pinMode(SER_LAT_PIN,OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); //Button Input pin with internal pull-up

  
  if (!rtc.begin()) {
    #ifdef SERIAL
    Serial.println("Controlla le connessioni");
    #endif
    while(true);
  }

  //uncomment to set RTC time
   /*
    if (rtc.lostPower()) {
    #ifdef SERIAL
    Serial.println("Imposto data/ora");
    #endif
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));    
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
   }
   */
 
}

void loop(){
     loopCounter++;
     displayTime();
     pollButton();
                  
     if(loopCounter == DATE_DELAY){
        displayDate();
		loopCounter = 0;
     }
  
}   //End of main program loop


void updateVFD(int pos, byte num, boolean decPoint){ //This shifts 16 bits out LSB first on the rising edge of the clock, clock idles low. Leftmost byte is position 7-0, rightmost byte is 7-seg digit
      
     if(pos >= 0 && pos < 9){               //Only accept valid positons on the display
      digitalWrite(SER_CLK_PIN, LOW);
      digitalWrite(SER_DATA_PIN, LOW);
      digitalWrite(SER_LAT_PIN, LOW);
      num = num + decPoint;                // sets the LSB to turn on the decimal point
      word wordOut = (1 << (pos+8)) + num; // concatenate bytes into a 16-bit word to shift out
      boolean pinState;

        for (byte i=0; i<16; i++){        // once for each bit of data
          digitalWrite(SER_CLK_PIN, LOW);
          if (wordOut & (1<<i)){          // mask the data to see if the bit we want is 1 or 0
            pinState = 0;                 // set to 1 if true
          }
          else{
            pinState = 1; 
          }
                 
          digitalWrite(SER_DATA_PIN, pinState); //Send bit to register
          digitalWrite(SER_CLK_PIN, HIGH);      //register shifts bits on rising edge of clock
          digitalWrite(SER_DATA_PIN, LOW);      //reset the data pin to prevent bleed through
        }
      
        digitalWrite(SER_CLK_PIN, LOW);
        digitalWrite(SER_LAT_PIN, HIGH); //Latch the word to light the VFD
        delayMicroseconds(500); //This delay slows down the multiplexing to get get the brightest display (but too long causes flickering)
      
      clearVFD();
    }
} 

void clearVFD(void){
    digitalWrite(SER_DATA_PIN, HIGH);          //clear data and latch pins
    digitalWrite(SER_LAT_PIN, LOW);
        for (byte i=0; i<16; i++){            //once for each bit of data
            digitalWrite(SER_CLK_PIN, LOW);
            digitalWrite(SER_CLK_PIN, HIGH);  //register shifts bits on rising edge of clock
            }
    digitalWrite(SER_CLK_PIN, LOW);
    digitalWrite(SER_LAT_PIN, HIGH);          //Latch the word to update the VFD
}



void displayTime(){
	
	  DateTime t = rtc.now();   // Get time from the DS3231
	
	  int hh = t.hour();
    int mm = t.minute();
    #ifdef SERIAL
    Serial.print(hh); Serial.print(":"); Serial.println(mm);
    #endif
   
    byte tensHour = t.hour() / 10; //Extract the individual digits
    byte unitsHour = t.hour() % 10;
    byte tensMin = t.minute() / 10;
    byte unitsMin = t.minute() % 10;
    byte tensSec = t.second() / 10;
    byte unitsSec = t.second() % 10;
	
	
    updateVFD(7, sevenSeg[tensHour], false);  
    updateVFD(6, sevenSeg[unitsHour], false);
    updateVFD(5, 128, false);                //dash
    updateVFD(4, sevenSeg[tensMin], false);  
    updateVFD(3, sevenSeg[unitsMin], false);
    updateVFD(2, 128, false);                //dash
    updateVFD(1, sevenSeg[tensSec], false);  
    updateVFD(0, sevenSeg[unitsSec], false);
	
}

void displayDate(){
  
	  DateTime t = rtc.now();
	
    byte tensDay = t.day() / 10; //Extract the individual digits
    byte unitsDay = t.day() % 10;
    byte tensMon = t.month() / 10;
    byte unitsMon = t.month() % 10;
    byte thousandsYear = t.year() / 1000;
    byte hundredsYear = (t.year() /100) % 10;
    byte tensYear = (t.year() / 10) % 10;
    byte unitsYear = t.year() % 10;

    #ifdef SERIAL
	  Serial.print(t.year(), DEC);
    Serial.print('/');
    Serial.print(t.month(),DEC);
    Serial.print('/');
    Serial.println(t.day(),DEC);
    #endif
	
	

    for (int i = 0; i<11; i++){                       //scroll the display across the VFD
      for(word j = 0; j<14; j++){                      //j count determines the speed of scrolling
        updateVFD(i-3, sevenSeg[tensDay], false); 
        updateVFD(i-4, sevenSeg[unitsDay], true);   //with decimal point 
        updateVFD(i-5, sevenSeg[tensMon], false);  
        updateVFD(i-6, sevenSeg[unitsMon], true);    //with decimal point
        updateVFD(i-7, sevenSeg[thousandsYear], false); 
        updateVFD(i-8, sevenSeg[hundredsYear], false); 
        updateVFD(i-9, sevenSeg[tensYear], false);  
        updateVFD(i-10, sevenSeg[unitsYear], false);
      }
    }
    
    for(word j = 0; j<300; j++){   //Hold the display for a short time
        updateVFD(7, sevenSeg[tensDay], false); 
        updateVFD(6, sevenSeg[unitsDay], true);   //with decimal point 
        updateVFD(5, sevenSeg[tensMon], false); 
        updateVFD(4, sevenSeg[unitsMon], true);    //with decimal point
        updateVFD(3, sevenSeg[thousandsYear], false); 
        updateVFD(2, sevenSeg[hundredsYear], false); 
        updateVFD(1, sevenSeg[tensYear], false);  
        updateVFD(0, sevenSeg[unitsYear], false);
    }
	
}


void pollButton(){
  POLL_LONG_BUTTON(!digitalRead(BUTTON_PIN), push_n_hold, short_flag, long_flag) 
  //The following big if-statement contains the user time-setting stuff.
  //Press the button once to access time-setting mode; the current selection will be displayed.
  //Press and hold the button to increment the display.
  //Press once more to exit time-setting mode.
  //The display cycles through hr, min, sec, date, month, year, each time the mode is accessed.
  if(short_flag){ //if button has been pressed
    short_flag = false; //reset flag
    boolean button_reset = true;//Flag to say button must be released before a short push will again be recognised
    boolean set_new_time = true; //we have entered time-setting mode
    void clearVFD();
    int i[6]; //An array to store new user-set time values
         
    while(set_new_time){

      #ifdef SERIAL
      Serial.println(" ___");
      Serial.print(i[user_selection]); //Print stuff for debugging
      Serial.print("  "); //Print stuff for debugging
      #endif
	  
	      DateTime t = rtc.now();
        
        i[0] = t.hour(); //Values to be stored in the array
        i[1] = t.minute();
        i[2] = t.second();
        i[3] = t.day(); 
        i[4] = t.month();
        i[5] = t.year();
        byte tens = i[user_selection] / 10; //Extract the individual digits
        byte units = i[user_selection] % 10;
        byte thousandsYear = (i[user_selection] / 1000);
        byte hundredsYear = (i[user_selection] /100) % 10;
        byte tensYear = (i[user_selection] / 10) % 10;
        byte unitsYear = (i[user_selection] % 100) % 10;

        switch (user_selection) {
           case 0: //Display hours
              updateVFD(7, sevenSeg[tens], false);  
              updateVFD(6, sevenSeg[units], false);
              break;
           case 1: //Display mins
              updateVFD(4, sevenSeg[tens], false);  
              updateVFD(3, sevenSeg[units], false);
              break;
           case 2: //Display seconds
              updateVFD(1, sevenSeg[tens], false);  
              updateVFD(0, sevenSeg[units], false);
              break;
           case 3:  //Display date
              updateVFD(7, sevenSeg[tens], false);  
              updateVFD(6, sevenSeg[units], true);
              break;
           case 4: //Display month
              updateVFD(5, sevenSeg[tens], false);  
              updateVFD(4, sevenSeg[units], true);
              break;
           case 5:  //Display year
              updateVFD(3, sevenSeg[thousandsYear], false); 
              updateVFD(2, sevenSeg[hundredsYear], false); 
              updateVFD(1, sevenSeg[tensYear], false);  
              updateVFD(0, sevenSeg[unitsYear], false);
              break;
       }
            
        POLL_LONG_BUTTON(!digitalRead(BUTTON_PIN),push_n_hold,short_flag,long_flag)
          if(long_flag){ //A long push will increment the time
               
               i[user_selection]++; //Increment the time element
               if(i[0] > 23){ //hours cannot exceed 23
                  i[0] = 0;
               }  
               if(i[1] > 59){ //minutes cannot exceed 59
                  i[1] = 0;
               }
               if(i[2] > 59){ //seconds cannot exceed 59
                  i[2] = 0;
               }
               if(i[3] > 31){ //date cannot exceed 31
                  i[3] = 1;
               }  
               if(i[4] > 12){ //months cannot exceed 12
                  i[4] = 1;
               }                                                     
               if(i[5] > 3000){ //years cannot exceed 3000
                  i[5] = 2018;
               }

               rtc.adjust(DateTime(i[5], i[4], i[3], i[2], i[1], i[0]));               
               
               long_flag = false; //Long button push has been duly serviced
               button_reset = true;//Don't accept a short push until the button has actually been released
          }
  
          if(short_flag){
              if(button_reset){//If button has been pressed for a long time previously and has just now been released, 
                  button_reset = false; //any short push glitch will be ignored.
              }
              else{//Otherwise it is a genuine short push
                  set_new_time = false;//exit time-setting mode
              }
            short_flag = false; //Short button push has been duly serviced
          }
      }
    //User has finished setting the time      
    user_selection ++; //Decrement the user selection by 2 ready for next request
      if(user_selection > 5){//if user has cycled through all options,
        user_selection = 0; //reset user selection
      }
  }
}
