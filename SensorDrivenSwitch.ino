/* 
 * SensorDrivenSwitch
 * 
 * Copyright (C) 2014 Almar van Merwijk
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>

#define DEBUG 1

/* 
 * settings as stored in eeprom 
 */
struct settings {
  byte sw_version;       /* When not equal, dismiss old data */
  byte threshold;        /* If sensor below this value, then turn on the switch */
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
 * button names for buttons on lcd panel
 */
#define BTN_RIGHT  0
#define BTN_UP     1
#define BTN_DOWN   2
#define BTN_LEFT   3
#define BTN_SELECT 4
#define BTN_NONE   5

/* 
 * Pin layout
 */
#define PIN_ADC_LCD_BUTTONS 0
#define PIN_ADC_SENSOR      1
#define PIN_IO_SWITCH       2

/*
 * The following is for a DFRobot lcd panel connected to an leonardo board.
 */
#define PIN_IO_RS           8
#define PIN_IO_ENABLE       9
#define PIN_IO_D4           4
#define PIN_IO_D5           5
#define PIN_IO_D6           6
#define PIN_IO_D7           7

#define LCD_WIDTH           16
#define LCD_HEIGHT          2

/*
 * Menu item strings. Also define offset for values.
 *                         "0123456789ABCDEF"
 */
const char* SENSOR_VALUE = "Sensor Read:    ";
int SENSOR_VALUE_OFFSET = 0xC;
const char* SWITCH_STATE = "Switch State:   ";
int SWITCH_STATE_OFFSET = 0xD;

#define NUM_MENU_ITEMS = 1;

/*
 * Global variables
 */
int sensor_val = 0;
int switch_val = 0;
int menu_item = 0;
struct settings settings_eeprom;  /* copy of setting in eeprom */
struct settings settings_current; /* settings as currently used */

LiquidCrystal lcd(PIN_IO_RS, PIN_IO_ENABLE, PIN_IO_D4, PIN_IO_D5, PIN_IO_D6, PIN_IO_D7);


/* 
 * Function prototypes 
 */
void read_settings();
void write_settings();
void fill_default_settings();
void sync_settings();
int read_lcd_buttons();
int read_sensor();
int draw_screen();
int handle_user_input();
int update_switch();

/*
 * Every arduino program starts with the setup.
 */
void setup()
{
  pinMode(PIN_IO_SWITCH, OUTPUT); 

  lcd.begin(LCD_WIDTH, LCD_HEIGHT);
  lcd.clear();
  lcd.print("Wait For Serial"); 

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial) { /* leonardo board has no ft chip, this way we don't miss any data */
    if (read_lcd_buttons() == BTN_SELECT) break; /* But if a key is pressed we stop waiting for serial */
  }
#endif

  lcd.clear();
  lcd.print(SENSOR_VALUE);
  lcd.setCursor(0,1);
  lcd.print(SWITCH_STATE);

  /* Setting are read from eeprom when available */
  sync_settings();
}

/*
 * loop is the main loop which is executed repeatedly
 */
void loop()
{
  read_sensor();
  draw_screen();
  handle_user_input();
  update_switch();
}

/*
 * read settings from eeprom
 */
void read_settings()
{
  settings_eeprom.sw_version = EEPROM.read(EEPROM_SETTINGS_SW_VERSION);
  settings_eeprom.threshold = EEPROM.read(EEPROM_SETTINGS_THRESHOLD);
  settings_eeprom.on_time_sec = EEPROM.read(EEPROM_SETTINGS_ON_TIME_SEC);
  settings_eeprom.repeat_after_min = EEPROM.read(EEPROM_SETTINGS_REPEAT_AFTER_MIN);
}
/*
 *
 */
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
/* 
 * fill_default_settings
 */
void fill_default_settings()
{
  settings_current.sw_version = 1;
  settings_current.threshold = 0; /* Never On */
  settings_current.on_time_sec = 0; /* Never On */
  settings_current.repeat_after_min = 60; /* 1 Hour */
}
/* 
 * sync_settings syncs current settings with eeprom
 */
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
  }
}

/*
 * The buttons from the lcd are connected via a adc pin, different analog values for different pins.
 */
int read_lcd_buttons()
{
  static int prev_keys = 0;
  int keys = analogRead(PIN_ADC_LCD_BUTTONS);
#ifdef DEBUG  
  if (abs(prev_keys - keys) > 2) {
    Serial.println( keys );   
    prev_keys = keys;
#endif    
  }

  if (keys > 1000) return BTN_NONE;   /* most likely result */
  if (keys < 50)   return BTN_RIGHT;  /* My value: 0 */
  if (keys < 200)  return BTN_UP;     /* My value: 100 */
  if (keys < 400)  return BTN_DOWN;   /* My value: 257 */
  if (keys < 600)  return BTN_LEFT;   /* My value: 410 */
  if (keys < 800)  return BTN_SELECT; /* My value: 642 */

  return BTN_NONE;
}

/* 
 * Read sensor just reads an analog value, only in debug mode it spits out more data
 */
int read_sensor()
{
  static int prev_sensor_val = 0;

  sensor_val = analogRead(PIN_ADC_SENSOR);
#ifdef DEBUG
  /* Only display differences */
  if (abs(prev_sensor_val - sensor_val) > 2) {
    Serial.print("Sensor:");
    Serial.println( sensor_val );   
    prev_sensor_val = sensor_val;
  }
#endif
  return sensor_val;
}

int draw_screen()
{
  lcd.setCursor(SENSOR_VALUE_OFFSET,0);
  lcd.print( sensor_val );
  lcd.print("  ");

  lcd.setCursor(SWITCH_STATE_OFFSET,1);
  if (switch_val == 0) lcd.print("Off");
  else lcd.print("On ");
}

int handle_user_input()
{
  switch (read_lcd_buttons())  {
  case BTN_RIGHT: 
    {
      break;
    }
  case BTN_LEFT: 
    {
      break;
    }
  case BTN_UP: 
    {
      switch_val = 1;
      break;
    }
  case BTN_DOWN: 
    {
      switch_val = 0;
      break;
    }
  case BTN_SELECT: 
    {
      break;
    }
  case BTN_NONE: 
    {
      break;
    }
  }
}

int update_switch()
{
  static int prev_switch_val = -1;
  if (prev_switch_val != switch_val) {
    if (switch_val == 1) digitalWrite(PIN_IO_SWITCH, HIGH);
    if (switch_val == 0) digitalWrite(PIN_IO_SWITCH, LOW);
    prev_switch_val = switch_val;
  }
}



