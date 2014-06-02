/* 
 * SensorDrivenSwitch
 * 
 * Copyright (C) 2014 Almar van Merwijk
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>

#define DEBUG 1

#define SOFTWARE_VERSION  1
/* 
 * The location/address of the settings in eeprom memory 
 */
#define NOT_STORED             -1 /* Item is not stored in eeprom */
#define EEPROM_SW_VERSION       0 /* When not equal, dismiss old data */
#define EEPROM_THRESHOLD        1 /* If sensor below this value, then turn on the switch */
#define EEPROM_ON_TIME_SEC      2 /* Time relais is activated, in seconds */
#define EEPROM_REPEAT_AFTER_MIN 3 /* Time after when the sensor is evaluated, in minutes */
#define EEPROM_SWITCH           4 /* 0=off, 1=on, 2=auto */
#define UNDEFINED               0xFF

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


enum 
{
  OFF=0,
  ON=1,
  AUTO=2
};
const char* switch_text[] = {"Off ","On  ","Auto"};
/*
 * Setting Items
 */
enum 
{
  ID_SENSOR,
  ID_SWITCH,
  ID_THRESHOLD,
  ID_ON_TIME,
  ID_REPEAT_AFTER,
  ID_VERSION,
  ID_END
};
/*
 * 
 */
struct setting {
  const int id;
  const char* text; 
  const int line_offset;
  const int eeprom_offset;
  byte value;
  byte eeprom_value;
};
struct setting settings[] = { /* Fill settings array with data */
{ID_SENSOR,      "Sensor Read:    ",12, NOT_STORED,              0,                UNDEFINED}, /* 0-100% */
{ID_SWITCH,      "Switch:         ", 8, EEPROM_SWITCH,           OFF,              UNDEFINED}, /* Off-On-Auto */
{ID_THRESHOLD,   "Threshold:      ",10, EEPROM_THRESHOLD,        0,                UNDEFINED}, /* 0-100 */
{ID_ON_TIME,     "On Time(sec):   ",13, EEPROM_ON_TIME_SEC,      0,                UNDEFINED}, /* 0-240 (4 minutes) */
{ID_REPEAT_AFTER,"Repeat(min):    ",12, EEPROM_REPEAT_AFTER_MIN, 0,                UNDEFINED}, /* 0-240 (4 hours) */
{ID_VERSION,     "Version:        ", 9, EEPROM_SW_VERSION,       SOFTWARE_VERSION, UNDEFINED}, /* Read Only */                         
{ID_END,         0,                  0, 0,                       0,                0}          /* END */
};


/*
 * Global variables
 */
int menu_item = 0;

LiquidCrystal lcd(PIN_IO_RS, PIN_IO_ENABLE, PIN_IO_D4, PIN_IO_D5, PIN_IO_D6, PIN_IO_D7);


/* 
 * Function prototypes 
 */
boolean read_settings();
void write_settings();
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

#ifdef DEBUG
  lcd.print("Wait For Serial"); 
  Serial.begin(9600);
  while (!Serial) { /* leonardo board has no ft chip, this way we don't miss any data */
    if (read_lcd_buttons() == BTN_SELECT) break; /* But if a key is pressed we stop waiting for serial */
  }
#else
  lcd.print("Almigo SD Switch");
#endif

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
 * read settings from eeprom, return false when software version mismatches
 */
boolean read_settings()
{
  int i;
  boolean sw_ok = false;
  for (i=0; settings[i].id != ID_END; i++) {
    if (settings[i].eeprom_offset != NOT_STORED) {
      settings[i].eeprom_value = EEPROM.read(settings[i].eeprom_offset);
      if (settings[i].id == ID_VERSION) { /* Check software version */
        sw_ok = (settings[i].eeprom_value == SOFTWARE_VERSION);
      }
    }
  }
  return sw_ok;
}
/*
 *
 */
void write_settings()
{
  int i;
  for (i=0; settings[i].id != ID_END; i++) {
    if (settings[i].eeprom_offset != NOT_STORED) {
      if (settings[i].eeprom_value != settings[i].value) {
        EEPROM.write(settings[i].eeprom_offset, settings[i].value);
        settings[i].eeprom_value = settings[i].value;
      }
    }
  }
}

/* 
 * sync_settings syncs current settings with eeprom
 */
void sync_settings()
{
  int i;
  if (read_settings()) { /* software version matches, settings ok */
    for (i=0; settings[i].id != ID_END; i++) {
      if (settings[i].eeprom_offset != NOT_STORED) {
        settings[i].value = settings[i].eeprom_value; /* Copy from eeprom to current */
      }
    }
  }
  else { /* If software is different, we use default settings, write them to eeprom */
    write_settings();
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

  settings[0].value = analogRead(PIN_ADC_SENSOR);
#ifdef DEBUG
  /* Only display differences */
  if (abs(prev_sensor_val - settings[0].value) > 2) {
    Serial.print("Sensor:");
    Serial.println( settings[0].value );   
    prev_sensor_val = settings[0].value;
  }
#endif
  return settings[0].value;
}

int draw_screen()
{
  lcd.setCursor(settings[0].line_offset,0);
  lcd.print( settings[0].value );
  lcd.print("  ");

  lcd.setCursor(settings[menu_item].line_offset,1);
  /* All numeric, except for ID_SWITCH */
  if (settings[menu_item].id == ID_SWITCH) lcd.print(switch_text[settings[menu_item].value]);
  else lcd.print(settings[menu_item].value);
  
}

int handle_user_input()
{
  switch (read_lcd_buttons())  {
  case BTN_RIGHT: 
    {
      if (settings[menu_item].id == ID_SWITCH) {
        settings[menu_item].value = (settings[menu_item].value+1) % AUTO;
      }
      break;
    }
  case BTN_LEFT: 
    {
      break;
    }
  case BTN_UP: 
    {
      if (menu_item > 1) menu_item--;
      break;
    }
  case BTN_DOWN: 
    {
      if (settings[menu_item+1].id != ID_END) menu_item++;
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
  if (settings[ID_SWITCH].value == AUTO) {
    /* Check sensor/time if we need to update */
    ;
  }
  else { /* Manual */  
    if (prev_switch_val != settings[ID_SWITCH].value) {
      if (settings[ID_SWITCH].value == 1) digitalWrite(PIN_IO_SWITCH, HIGH);
      if (settings[ID_SWITCH].value == 0) digitalWrite(PIN_IO_SWITCH, LOW);
    }
    prev_switch_val = settings[ID_SWITCH].value;
  }
}



