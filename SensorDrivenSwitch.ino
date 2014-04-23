/* 
 * SensorDrivenSwitch
 * 
 * Copyright (C) 2014 Almar van Merwijk
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>

/* 
 * buttons from lcd panel
 */
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

/* 
 * settings as stored in eeprom 
 */
struct settings {
  byte sw_version;       /* When not equal, dismiss old data */
  byte threshold;        /* If sensor below this value, then turn on relais */
  byte on_time_sec;      /* Time relais is activated, in seconds */
  byte repeat_after_min; /* Time after when the sensor is evaluated, in minutes */
};

/* 
 * The location/address of the settings in eeprom memory 
 */
#define EEPROM_SETTINGS_SW_VERSION       0
#define EEPROM_SETTINGS_THRESHOLD        1
#define EEPROM_SETTINGS_ON_TIME_SEC      2
#define EEPROM_SETTINGS_REPEAT_AFTER_MIN 3

/* 
 * Function prototypes 
 */
void read_settings();
void write_settings();
void fill_default_settings();
void sync_settings();
int read_lcd_buttons();
int read_sensor();

/* 
 * LCD object, select the pins used on the LCD panel.
 * The following is for a DFRobot lcd panel connected to an leonardo board.
 */
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

/* 
 * One copy of the settings as seen in eeprom, the other as last altered by user. 
 */
struct settings settings_eeprom, settings_current;

/* 
 * 
 */
int lcd_key     = 0;
int adc_key_in  = 0;
int adc_sensor_in = 0;
int prev_adc_key_in  = 0;
int prev_adc_sensor_in = 0;
int relais = 2;

/*
 * Menu item strings. Also define offset for values, so translations will be easier.
 *                         "0123456789ABCDEF"
 */
const char* SENSOR_VALUE = "Sensor Read:    ";
int SENSOR_VALUE_OFFSET = 0xC;
const char* SWITCH_STATE = "Switch State:   ";
int SWITCH_STATE_OFFSET = 0xD;

/*
 * Each arduino program starts with the setup.
 */
void setup()
{
  pinMode(relais, OUTPUT); 

  lcd.begin(16, 2);      /* start the library, 16 width, 2 high */
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Wait For Serial"); 

  Serial.begin(9600);
  while (!Serial) { /* leonardo board has no ft chip, this way we don't miss any data */
    if (read_lcd_buttons() == btnSELECT) break; /* But if a key is pressed we stop waiting for serial */
  }

  lcd.clear();
  lcd.print(SENSOR_VALUE);
  lcd.setCursor(0,1);
  lcd.print(SWITCH_STATE);

  /* Setting are read from eeprom when available */
  sync_settings();
}
/* ---------------------------------------------------------------------------------------------- */

void loop()
{
  lcd.setCursor(SENSOR_VALUE_OFFSET,1);
  lcd.print( read_sensor() );
  
  lcd.print("  ");

  lcd.setCursor(0,1);            // move to the begining of the second line
  lcd_key = read_lcd_buttons();  // read the buttons

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
void read_settings()
{
  settings_eeprom.sw_version = EEPROM.read(EEPROM_SETTINGS_SW_VERSION);
  settings_eeprom.threshold = EEPROM.read(EEPROM_SETTINGS_THRESHOLD);
  settings_eeprom.on_time_sec = EEPROM.read(EEPROM_SETTINGS_ON_TIME_SEC);
  settings_eeprom.repeat_after_min = EEPROM.read(EEPROM_SETTINGS_REPEAT_AFTER_MIN);
}
/* ---------------------------------------------------------------------------------------------- */
void write_settings()
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
void fill_default_settings()
{
  settings_current.sw_version = 1;
  settings_current.threshold = 0; /* Never On */
  settings_current.on_time_sec = 0; /* Never On */
  settings_current.repeat_after_min = 60; /* 1 Hour */
}
/* ---------------------------------------------------------------------------------------------- */
void sync_settings()
{
  fill_default_settings();
  read_settings();
  /* If software is newer or when the eeprom is empty, we use default settings */
  if (settings_current.sw_version != settings_eeprom.sw_version) {
    write_settings();
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
int read_lcd_buttons()
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
  adc_sensor_in = analogRead(1);
  if (abs(prev_adc_sensor_in - adc_sensor_in) > 2) {
    Serial.print("Soil:");
    Serial.println( adc_sensor_in );   
    prev_adc_sensor_in = adc_sensor_in;
  }
  return adc_sensor_in;
}
/* ---------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------- */


