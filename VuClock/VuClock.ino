#include <EEPROM.h>
#include <Wire.h>

// Definitions
#define DS3231_I2C_ADDRESS 0x68

// modeLed - ML
#define ML_BEAT        0
#define ML_FADERN8     1
#define ML_ALTERN8     2
#define ML_FADERHALF   3
#define ML_ALTERHALF   4
#define ML_ON          5
#define ML_OFF         6
#define ML_FIRST       ML_BEAT
#define ML_LAST        ML_OFF

// modeClock - MC
#define MC_TIME        0
#define MC_SET         1

// EEPROM addresses
#define ADDR_MODELED   0

// Pin IDs
const int pinSet     = 7;
const int pinAdjust  = 4;

const int pinHours   = 6;
const int pinLedHour = 9;

const int pinMinutes = 5;
const int pinLedMin  = 3;

// Globals
// Current time
int curSecond;
int curMinute;
int curHour;
// Current beat
int curBeat;
int curBeatDt;
// LED mode
int modeLed;

//----------------
//----------------
// Function: setup
//----------------
//----------------
void setup() 
{ 
  // Serial port
  Serial.begin(9600);
  
  // Setup I2C  
  Wire.begin();

  // Set input mode
  pinMode(pinSet, INPUT_PULLUP);
  pinMode(pinAdjust, INPUT_PULLUP);

  // Get the time
  GetCurTime();
  
  // Get the stored LED mode
  modeLed = EEPROM.read(ADDR_MODELED);
  
  // Is the Set button pressed?
  if(digitalRead(pinSet) == HIGH)
  {
    // Set the needles to max to allow manual adjustment
    analogWrite(pinHours, 0xFF);
    analogWrite(pinMinutes, 0xFF);

    // Wait until the button is released
    while(digitalRead(pinSet) == HIGH);
    // Quick pause
    delay(200);
    
    // Wait until the button is pressed again
    while(digitalRead(pinSet) == LOW);
    // Quick pause
    delay(200);
    
    // Wait until the button is released
    while(digitalRead(pinSet) == HIGH);    
  } 
} 

//---------------
//---------------
// Function: loop
//---------------
//---------------
void loop()
{
  static unsigned long timeLastTick = millis();
  static unsigned long timeLastBeat = millis();
  static unsigned long timeLastSet;
  static unsigned long timeLastAdjust;
  static int wasSet = LOW;
  static int wasAdjust = LOW;
  static boolean beatDirection = false;
  static int modeClock = MC_TIME;

  unsigned long timeNow = millis();
  boolean adjustPress = false;
  boolean setPress = false;
  
  //-----------
  // Set button
  //-----------
  if(digitalRead(pinSet) == HIGH)
  {
    if(wasSet == LOW)
    {
      timeLastSet = millis();
    }

    wasSet = HIGH;
  }
  else
  {
    if(wasSet == HIGH && (timeNow - timeLastSet) > 100)
    {
      // Set button pressed
      setPress = true;
      Serial.println("Set pressed");
    }
    
    wasSet = LOW;
  }

  //--------------
  // Adjust button
  //--------------
  if(digitalRead(pinAdjust) == HIGH)
  {
    if(wasAdjust == LOW)
    {
      timeLastAdjust = millis();
    }

    wasAdjust = HIGH;
  }
  else
  {
    if(wasAdjust == HIGH && (timeNow - timeLastAdjust) > 100)
    {
      // Set button pressed
      adjustPress = true;
      Serial.println("Adjust pressed");
    }
    
    wasAdjust = LOW;
  }
  
  //-----------
  // What Next?
  //-----------
  if(modeClock == MC_SET)
  {
    //---------
    // Set Mode
    //---------
    if(!ClockAdjust(setPress, adjustPress))
    {
      modeClock = MC_TIME;
      Serial.println("Exiting set mode");
    }
  }
  else
  {
    //----------
    // Time Mode
    //----------
    if((timeNow - timeLastTick) > 1000)
    {
      int lastHour = curHour ;
      
      // Move on a second
      Tick();
      
      // Every hour resync time
      if(curHour != lastHour)
      {
        Serial.println("Resync");
        GetCurTime();
      }
      
      // Alternate the beat direction
      if(beatDirection)
      {
        curBeat = 0xFF;
        beatDirection = false ;
      }
      else
      {
        curBeat = 0;
        beatDirection = true ;
      }
      
      // Reset the tick time
      timeLastTick = timeNow ;
      timeLastBeat = timeNow ;
    }
    else
    {
      if((timeNow - timeLastBeat) > (1000 / 0xFF))
      {
        // Increment / decrement the beat
        curBeat += beatDirection ? +1 : -1 ;
        
        // Reset the beat time
        timeLastBeat = timeNow;
      }
    }  
    
    // Adjust pressed?
    if(adjustPress)
    {
      // Cycle through the LED modes on adjust button pressed
      modeLed++;
      if(modeLed > ML_LAST)
      {
          modeLed = ML_FIRST;
      }
      
      // Store the mode in EEPROM
      EEPROM.write(ADDR_MODELED, modeLed);
      
      Serial.print("modeLed = ");
      Serial.println(modeLed);
    }
    
    // Set pressed?
    if(setPress)
    {
      // Enter SET clock mode
      modeClock = MC_SET;
      Serial.println("Entering set mode");
    }
    
    //-------
    // Output
    //-------
    // Set the time output
    analogWrite(pinHours, (curHour * 0xFF) / 12);
    analogWrite(pinMinutes, (curMinute * 0xFF) / 60);
    
    // Set the Led output
    switch(modeLed)
    {
      case ML_BEAT: // Beating
        analogWrite(pinLedHour, curBeat);
        analogWrite(pinLedMin, curBeat);
        break ;
        
      case ML_FADERN8: // Alternating        
        analogWrite(pinLedHour, curBeat);
        analogWrite(pinLedMin, 0xFF - curBeat);
        break ;

      case ML_ALTERN8: // Alternating        
        analogWrite(pinLedHour, beatDirection ? 0x00 : 0xFF);
        analogWrite(pinLedMin, beatDirection ? 0xFF : 0x00);
        break ;
      
      case ML_FADERHALF:
        analogWrite(pinLedHour, curSecond < 30 ? curBeat : 0x00);
        analogWrite(pinLedMin, curSecond >=30 ? curBeat : 0x00);
        break ;
        
      case ML_ALTERHALF:
        analogWrite(pinLedHour, curSecond < 30 ? (beatDirection ? 0xFF : 0x00) : 0x00);
        analogWrite(pinLedMin, curSecond >=30 ? (beatDirection ? 0xFF : 0x00) : 0x00);
        break ;
        
      case ML_ON: // On
        analogWrite(pinLedHour, 0xFF);
        analogWrite(pinLedMin, 0xFF);
        break ;
        
      case ML_OFF: // Off
      default:
        analogWrite(pinLedHour, 0);
        analogWrite(pinLedMin, 0);
        break ;
    }
  }
}

//---------------
//---------------
// Function: Tick
//---------------
//---------------
void Tick()
{
  curSecond++;
  if(curSecond >= 60)
  {
    curSecond = 0;
    curMinute++;
    if(curMinute >= 60)
    {
      curMinute = 0;
      curHour++;
      if(curHour >= 12)
      {
        curHour = 0;
      }
    }
  }
}

//-----------------------
//-----------------------
// Function: Clock Adjust
//-----------------------
//-----------------------
boolean ClockAdjust(boolean setPress, boolean adjustPress)
{
  static boolean adjustUpdate = false;
  static int curAdjust = 0;
  static unsigned long timeFlash = 0;
  static boolean onFlash = false;
  
  int pinLedOn ;
  int pinLedOff ;
  unsigned long timeNow = millis();
  boolean clockAdjustRc = true;
  
  if(setPress)
  {
    curAdjust++;
    Serial.print("Set pressed ");
    Serial.println(curAdjust);
    if(curAdjust == 2)
    {
      curSecond = 0;
      clockAdjustRc = false;
    }
  }
  
  if(adjustPress)
  {
    switch(curAdjust)
    {
      case 0: // Adjusting the hour
        adjustUpdate = true;
        curHour++;
        if(curHour >= 12)
        {
          curHour = 0;
        }
        break ;
        
      case 1: // Adjusting minute
        adjustUpdate = true;
        curMinute++;
        if(curMinute >= 60)
        {
          curMinute = 0;
        }
        break ;
                
      default:
        break ;
    }
  }

  // Quit adjust mode?
  if(!clockAdjustRc)
  {
    // Set the time
    if(adjustUpdate)
    {
      SetCurTime();
      adjustUpdate = false;
    }
    
    curAdjust = 0;
  }
  else
  {
    // Set the outputs
    // Time
    analogWrite(pinHours, (curHour * 0xFF) / 12);
    analogWrite(pinMinutes, (curMinute * 0xFF) / 60);

    // LEDs
    switch(curAdjust)
    {
      case 0:
        pinLedOn = pinLedHour;
        pinLedOff = pinLedMin;
        break ;
        
      case 1:
        pinLedOn = pinLedMin;
        pinLedOff = pinLedHour;
        break ;
        
      default:
        break ;
    }
    
    if((timeNow - timeFlash) > 500)
    {
      onFlash = !onFlash;
      timeFlash = timeNow;
    }
    
    analogWrite(pinLedOn, onFlash ? 0xFF : 0x00);
    analogWrite(pinLedOff, 0x00);
  }          
    
  return clockAdjustRc ;
}

//---------------------
//---------------------
// Function: GetCurTime
//---------------------
//---------------------
void GetCurTime()
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 3);
  // request seven bytes of data from DS3231 starting from register 00h
  curSecond = bcdToDec(Wire.read() & 0x7f);
  curMinute = bcdToDec(Wire.read());
  curHour = bcdToDec(Wire.read() & 0x3f);
  if(curHour > 11)
  {
     curHour -= 12;
  }
  
//  *dayOfWeek = bcdToDec(Wire.read());
//  *dayOfMonth = bcdToDec(Wire.read());
//  *month = bcdToDec(Wire.read());
//  *year = bcdToDec(Wire.read());
}

//---------------------
//---------------------
// Function: SetCurTime
//---------------------
//---------------------
void SetCurTime()
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(curSecond)); // set seconds
  Wire.write(decToBcd(curMinute)); // set minutes
  Wire.write(decToBcd(curHour)); // set hours
//  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
//  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
//  Wire.write(decToBcd(month)); // set month
//  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}

// End of file
