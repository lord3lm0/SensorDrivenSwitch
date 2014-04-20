/* -------------------------------------------------------------------------------------------------
 * Sensor driven switch 
 * -------------------------------------------------------------------------------------------------
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>

/* ---------------------------------------------------------------------------------------------- */
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

/* ---------------------------------------------------------------------------------------------- */
/* Settings */
struct Settings {
  byte sw_version;       /* When not equal, dismiss old data */
  byte threshold;        /* If sensor below this value, then turn on relais */
  byte on_time_sec;      /* Time relais is activated, in seconds */
  byte repeat_after_min; /* Time after when the sensor is evaluated, in minutes */
};
/* The location of the settings in eeprom memory */
#define EEPROM_SETTINGS_SW_VERSION       0
#define EEPROM_SETTINGS_THRESHOLD        1
#define EEPROM_SETTINGS_ON_TIME_SEC      2
#define EEPROM_SETTINGS_REPEAT_AFTER_MIN 3

/* ---------------------------------------------------------------------------------------------- */
/* Function prototypes */
void readSettings();
void writeSettings();
void fillDefaultSettings();
void syncSettings();
int read_LCD_buttons();
int read_sensor();

/* ---------------------------------------------------------------------------------------------- */
/* LCD object, select the pins used on the LCD panel */
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* One copy of the settings as seen in EEprom, the other as last altered by user. */
Settings settings_eeprom, settings_current;

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
int adc_moist_in = 0;
int prev_adc_key_in  = 0;
int prev_adc_moist_in = 0;
int relais = 2;

/* ---------------------------------------------------------------------------------------------- */
void setup()
{
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);
  pinMode(relais, OUTPUT); 
  lcd.print("Wait For Serial"); // print a simple message
  Serial.begin(9600);
  while (!Serial) { //leonardo board
    if (read_LCD_buttons() == btnSELECT) break;
  }
  // lcd.setCursor(0,0);
  lcd.clear();
  lcd.print("GardenTech 2000 ");


  syncSettings();
}
/* ---------------------------------------------------------------------------------------------- */

void loop()
{
  lcd.setCursor(9,1);            // move cursor to second line "1" and 9 spaces over
  //lcd.print(millis()/1000);      // display seconds elapsed since power-up
  lcd.print( read_sensor() );
  lcd.print("  ");

  lcd.setCursor(0,1);            // move to the begining of the second line
  lcd_key = read_LCD_buttons();  // read the buttons

    switch (lcd_key)               // depending on which button was pushed, we perform an action
  {
  case btnRIGHT:
    {
      lcd.print("RIGHT  ");
      break;
    }
  case btnLEFT:
    {
      lcd.print("LEFT    ");
      break;
    }
  case btnUP:
    {
      lcd.print("UP     ");
      digitalWrite(relais, HIGH);
      break;
    }
  case btnDOWN:
    {
      lcd.print("DOWN   ");
      digitalWrite(relais, LOW);
      break;
    }
  case btnSELECT:
    {
      lcd.print("SELECT ");
      break;
    }
  case btnNONE:
    {
      lcd.print("Sensor:");
      break;
    }
  }

}

/* ---------------------------------------------------------------------------------------------- */
void readSettings()
{
  settings_eeprom.sw_version = EEPROM.read(EEPROM_SETTINGS_SW_VERSION);
  settings_eeprom.threshold = EEPROM.read(EEPROM_SETTINGS_THRESHOLD);
  settings_eeprom.on_time_sec = EEPROM.read(EEPROM_SETTINGS_ON_TIME_SEC);
  settings_eeprom.repeat_after_min = EEPROM.read(EEPROM_SETTINGS_REPEAT_AFTER_MIN);
}
/* ---------------------------------------------------------------------------------------------- */
void writeSettings()
{
  if (settings_eeprom.sw_version != settings_current.sw_version) {
    EEPROM.write(EEPROM_SETTINGS_SW_VERSION, settings_current.sw_version);
    settings_eeprom.sw_version = settings_current.sw_version;
  }
  if (settings_eeprom.threshold != settings_current.threshold) {
    EEPROM.write(EEPROM_SETTINGS_THRESHOLD, settings_current.threshold);
    settings_eeprom.threshold = settings_current.threshold;
  }
  if (settings_eeprom.on_time_sec != settings_current.on_time_sec) {
    EEPROM.write(EEPROM_SETTINGS_ON_TIME_SEC, settings_current.on_time_sec);
    settings_eeprom.on_time_sec = settings_current.on_time_sec;
  }
  if (settings_eeprom.repeat_after_min != settings_current.repeat_after_min) {
    EEPROM.write(EEPROM_SETTINGS_REPEAT_AFTER_MIN, settings_current.repeat_after_min);
    settings_eeprom.repeat_after_min = settings_current.repeat_after_min;
  }
  
}
/* ---------------------------------------------------------------------------------------------- */
void fillDefaultSettings()
{
  settings_current.sw_version = 1;
  settings_current.threshold = 0; /* Never On */
  settings_current.on_time_sec = 0; /* Never On */
  settings_current.repeat_after_min = 60; /* 1 Hour */
}
/* ---------------------------------------------------------------------------------------------- */
void syncSettings()
{
  fillDefaultSettings();
  readSettings();
  /* If software is newer or when the eeprom is empty, we use default settings */
  if (settings_current.sw_version != settings_eeprom.sw_version) {
    writeSettings();
  }
  else { /* Copy settings from eeprom to current */
    settings_current = settings_eeprom;
    settings_current.threshold != settings_eeprom.threshold;
    settings_current.on_time_sec != settings_eeprom.on_time_sec;
    settings_current.repeat_after_min != settings_eeprom.repeat_after_min;
  }
}
/* ---------------------------------------------------------------------------------------------- */
// read the buttons
int read_LCD_buttons()
{

  adc_key_in = analogRead(0);      // read the value from the sensor 
  if (abs(prev_adc_key_in - adc_key_in) > 2) {
    Serial.println( adc_key_in );   
    prev_adc_key_in = adc_key_in;
  }

  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
  // we add approx 50 to those values and check to see if we are close
  if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
  // For My board this threshold
  if (adc_key_in < 50)   return btnRIGHT;  // 0
  if (adc_key_in < 200)  return btnUP;     // 100
  if (adc_key_in < 400)  return btnDOWN;   // 257
  if (adc_key_in < 600)  return btnLEFT;   // 410
  if (adc_key_in < 800)  return btnSELECT; // 642 

  return btnNONE;  // when all others fail, return this...
}
/* ---------------------------------------------------------------------------------------------- */
int read_sensor()
{
  adc_moist_in = analogRead(1);
  if (abs(prev_adc_moist_in - adc_moist_in) > 2) {
    Serial.print("Soil:");
    Serial.println( adc_moist_in );   
    prev_adc_moist_in = adc_moist_in;
  }
  return adc_moist_in;
}
/* ---------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */

