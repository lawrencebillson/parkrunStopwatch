
#include <Wire.h>
#include <LiquidCrystal.h>
#include <HCRTC.h>
#include <AT24Cxx.h>
#include <Time.h>
#include <TimerOne.h>


/* parkrunStopwatch - an Arduino based stopwatch, suitible for use in parkrun events
 *  Lawrence Billson, 2016
 *  
 *  All public domain under Creative Commons
 *  Thanks to those that have provided awesome libraries - especially:
 *    HobbyComponents.com for the RTC library
 *    Manjunath CV for the AT24Cxx EEPROM library
 *    John Boxall and Tronixlabs for their BCD/RTC code
 *    FreeTronics for the LCD button code - nice work!
 */

// Version - to print on the display
char prsversion[] = "0.1";

/* The RTC and EEPROM have fixed addresses of 0x68 and 
    0x50 so define these in software */
#define I2CDS1307Add 0x68
#define i2c_address 0x50

// EEPROM memory map 
// Byte 0 - the current number of runners, it's important that this is early in the EEPROM so it's in the high-wear area
// Byte 1 - BCD Hour - first runner
// Byte 2 - BCD Minute - first runner
// Byte 3 - BCD Seconds - first runner
// Byte 4 - BCD Hour - second runner
// ... 

/* How big is our flash - kiloBYTES */
#define FLASHSIZE 4
#define BUTTON_ADC_PIN           A0  // A0 is the button ADC input


/* LCD Button Code */
#define RIGHT 0  // right
#define UP 100  // up
#define DOWN 255  // down
#define LEFT  409  // left
#define SELECT  640  // right
#define BERROR 10  // hysteresis for valid button sensing window



/* Create an instance of HCRTC library */
HCRTC HCRTC;

// Open the LCD
LiquidCrystal lcd(8,9,4,5,6,7);

// Initilaize using AT24CXX(i2c_address, size of eeprom in KB).
AT24Cxx eep(i2c_address, FLASHSIZE);

// Time for some global variables
int runners = 0;


void setup() {
  
  // initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
    }



  // Set the local clock
  setfromrtc();    

  // Initialise the display
  lcd.begin(16, 2);

  //button adc input
  pinMode( BUTTON_ADC_PIN, INPUT );         //ensure A0 is an input
  digitalWrite( BUTTON_ADC_PIN, LOW );      //ensure pullup is off on A0
  
  // Work out the current state
  runners = eep.read(0);

  // Setup a 1 second timer interupt to update my LCD screen
  Timer1.initialize(1000000);

  // Print a little startup splash screen
  lcd.setCursor( 0, 0 );   //top left
  lcd.print( "parkrunStopwatch" );
  lcd.setCursor( 0, 2 ); // Bottom Left
  lcd.print( "Version ");
  lcd.print(prsversion);
  delay(1000);
  lcd.clear();

    // are we in an event?
  if (runners != 0) {
    Timer1.attachInterrupt(screenpaint); // execute once per second
  } else {
    lcd.setCursor (0 , 0); // Bottom left
    lcd.print("0:00:00 - Ready!");
  }

  lcd.setCursor (0 , 2 ); // Bottom left
  lcd.print("Token - ");
  lcd.print(runners);
}

void loop() {
   
  int button; 
  button = readbutton();

  if (button == 1) {
      overline();
  }

  if (button == 5) {
      dumpdata();
  } 

  if (button == 2) {
    zerortc();
  }

  if (button == 4) {
    clearevent();
  }



  /* Button codes
   *  1 = select - runner over the ine
   *  2 = left - start the event
   *  3 = up 
   *  4 = down
   *  5 = right - dump the runners list
   */
}

int readbutton() {
  // Read the state of the button, figure out what it means
  int bval = analogRead(0);
  int cval;

  //10ms debounce
  delay(10);
  cval = analogRead(0);
  
  if (bval == cval) {
  
    if (bval == 1023) {
        return 0;
    }
    if (bval == RIGHT) {
      return 5;
    }
    
    if (((bval <= DOWN) && (!(bval <= (DOWN - BERROR)))) || ((bval >= DOWN) && (!(bval >= (DOWN + BERROR))))) {
      return 4;
    }
  
    if (((bval <= UP) && (!(bval <= (UP - BERROR)))) || ((bval >= UP) && (!(bval >= (UP + BERROR))))) {
      return 3;
    }
  
    if (((bval <= LEFT) && (!(bval <= (LEFT - BERROR)))) || ((bval >= LEFT) && (!(bval >= (LEFT + BERROR))))) {
      return 2;
    }
  
    if (((bval <= SELECT) && (!(bval <= (SELECT - BERROR)))) || ((bval >= SELECT) && (!(bval >= (SELECT + BERROR))))) {
      return 1;
    }
  }
}

void dumpdata() {
  int eepromin;

  lcd.setCursor(8,0);
  lcd.print("Snd:");
  // Some test code - try dumping all of the data from our EEPROM
  for (int i = 1 ; i != (runners + 1) ; i++) {
    lcd.setCursor(12,0);
    lcd.print(i);
    Serial.print("Runner\t");
    Serial.print(i);
    Serial.print("\t");
    Serial.print(bcdToDec(eep.read(i * 3)));
    printDigits(bcdToDec(eep.read(i * 3 + 1)));
    printDigits(bcdToDec(eep.read(i * 3 + 2)));
    Serial.println();
  }
  lcd.setCursor(8,0);
  lcd.print("        ");
}

void screenpaint() {
    lcd.setCursor (0 , 0); // Top Left
    lcd.print(hour());
    lcdDigits(minute());
    lcdDigits(second());
}

void overline() {
  // A runner has crossed the line - good for them
  runners++;
  
  // Store their time! Code goes here!
  eep.update((runners * 3), decToBcd(hour()));
  eep.update((runners * 3 + 1), decToBcd(minute()));
  eep.update((runners * 3 + 2), decToBcd(second()));

  // Store the runner data
  eep.update(0, runners); // Byte 0 is our magic 'runners' spot
  
  lcd.setCursor (0 , 1); // Bottom left
  lcd.print("Token - ");
  lcd.print(runners);
}

void clearevent() {
  // Stop screen updates
  Timer1.detachInterrupt();
  lcd.clear();
  lcd.setCursor (0 , 0); // Bottom left
  lcd.print("0:00:00 - Ready!");
 
  //  Set number of runners to zero
  runners = 0;
  eep.update(0, runners); // Byte 0 is our magic 'runners' spot
  // Update the display
  lcd.setCursor (0 , 1); // Bottom left
  lcd.print("Token - ");
  lcd.print(runners);
  
}

void zerortc() {
  // Use this at the start of an event - set a bogus date!
  // Parameters are: I2C address, year, month, date, hour, minute, second, day of week

  if (runners == 0) {
    lcd.clear();
    HCRTC.RTCWrite(I2CDS1307Add, 70, 1, 1, 0, 0, 0, 4);
    setfromrtc();
    lcd.setCursor (0 , 1); // Bottom left
    lcd.print("Token -        ");
    // Paint the screen once per second
    Timer1.attachInterrupt(screenpaint);
  }
  else {
    // Runners doesn't equal zero, we should reset our RTC
    lcd.setCursor (0 , 1); // Bottom left
    lcd.print("EVENT IN PROGRESS!");
    delay(500);
    lcd.clear();
    lcd.setCursor (0 , 1); // Bottom left
    lcd.print("Token - ");
    lcd.print(runners);
  }

}

void zeroeeprom() {
  // Lots of integer arithmetic mucking around - trying to avoid a floating point divison if we can avoid it
  int displayed = 0; // We only want to update the screen once
  unsigned int perc; // Percentage complete - could be handy to know
  unsigned int total; // Only grab the length once
  unsigned long int calc; // Required - we overflow with our divide if we don't do this
  
  lcd.setCursor( 0, 0 );   //top left
  lcd.print( "Erasing EEPROM:" );
  Serial.println("Erasing flash!");
  Serial.print("Size of EEPROM:");
  Serial.println(eep.length());
  total = eep.length();
  Serial.println("KB");
  for (int i = 0 ; i < eep.length() ; i++) {
    // Write a zero to the address
    eep.update(i, 0x00);    
    
    // Are we at a percentage break?
    calc = 100 * i;
    perc = calc / total;

    // Do we display it?
    if (displayed != perc) {
      lcd.setCursor( 0, 2 ); // Bottom left
      lcd.print(perc);
      lcd.print("% Complete");
      displayed = perc;
      }
    }

  // Print Msg When Completed.
  Serial.println("EEPROM Erase completed.");
  lcd.clear();
  lcd.setCursor( 0, 0); // Top left
  lcd.print("EEPROM erase");
  lcd.setCursor( 0, 2); // Bottom left
  lcd.print("complete");
  delay(500);
}

// Thanks Tronixlabs!
byte bcdToDec(byte val)  {
// BCD to Decimal conversion
  return ( (val/16*10) + (val%16) );
}

byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void lcdDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  lcd.print(":");
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}
void setfromrtc() {
  // Read data in from the RTC
  Serial.println("Reading RTC");
  HCRTC.RTCRead(I2CDS1307Add);
  Serial.print("RTC date is ");
  Serial.print(HCRTC.GetDateString());
  Serial.print("RTC time is ");
  Serial.println(HCRTC.GetTimeString());
  // Update Arduino's clock for the same
   // Format is setTime(hr,min,sec,day,month,yr);
  setTime(HCRTC.GetHour(),HCRTC.GetMinute(),HCRTC.GetSecond(),HCRTC.GetDay(),HCRTC.GetMonth(),HCRTC.GetYear());
  Serial.print("Set time on arduino to ");
    Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}



