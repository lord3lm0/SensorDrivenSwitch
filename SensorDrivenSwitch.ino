/* 
 * SensorDrivenSwitch
 * 
 * Copyright (C) 2014 Almar van Merwijk
 */

#include <LiquidCrystal.h>
#include <EEPROM.h>

#define DEBUG 1

#define SW_VERSION  1
/* 
 * The location/address of the settings in eeprom memory 
 */
#define NOT_STORED              -1 /* Item is not stored in eeprom */
#define EEPROM_SW_VERSION        0 /* When not equal, dismiss old data */
#define EEPROM_THRESHOLD         1 /* If sensor below this value, then turn on the switch */
#define EEPROM_ON_TIME_SEC       2 /* Time relais is activated, in seconds */
#define EEPROM_REPEAT_AFTER_MIN  3 /* Time after when the sensor is evaluated, in minutes */
#define EEPROM_SWITCH            4 /* 0=off, 1=on, 2=auto */
#define EEPROM_HYSTERESIS        5 /* Threshold + or - hysteresis */


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
 * Draw flags, what parts of the screen do need to be drawn
 */
#define DRAW_NONE          0
#define DRAW_SENSOR        1
#define DRAW_MENU          2
#define DRAW_ALL           3
#define DRAW_SAVE_NEEDED   4

/*
 * The following values define how fast/slow the menu responds on button presses
 */
#define BUTTON_START_DELAY 1600
#define BUTTON_REPEAT_DELAY 300

/* 
 * button names for buttons on lcd panel
 */
enum {
  BTN_RIGHT,
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_SELECT,
  BTN_NONE
};

enum 
{
  OFF=0,
  AUTO=1,
  ON=2
};
const char* switch_text[] = {"Off ","Auto", "On  "};
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
  ID_HYSTERESIS,
  ID_MAX_SENSOR,
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
  int max_value;
  int read_only;
};

/*
 * Global variables
 */
struct setting settings[] = { /* Fill settings array with data */
/* id            text      line_offset  eeprom_offset          value,eeprom_value,max_value,read_only */
{ID_SENSOR,      "Sensor:         ", 7, NOT_STORED,              0,          0, 255, 1}, /* 0-255 */
{ID_SWITCH,      "Switch:         ", 8, EEPROM_SWITCH,           OFF,        0,   2, 0}, /* Off-On-Auto */
{ID_THRESHOLD,   "Threshold:      ",10, EEPROM_THRESHOLD,        0,          0, 255, 0}, /* 0-255 */
{ID_ON_TIME,     "On Time(sec):   ",13, EEPROM_ON_TIME_SEC,      0,          0, 240, 0}, /* 0-240 (4 minutes) */
{ID_REPEAT_AFTER,"Repeat(min):    ",12, EEPROM_REPEAT_AFTER_MIN, 0,          0, 240, 0}, /* 0-240 (4 hours) */
{ID_VERSION,     "Version:        ", 9, EEPROM_SW_VERSION,       SW_VERSION, 0, 100, 1}, /* Read Only */                         
{ID_HYSTERESIS,  "Hysteresis:     ",11, EEPROM_HYSTERESIS,       0,          0, 255, 0}, /* Threshold +/- value */
{ID_MAX_SENSOR,  "Sensor Max:     ",11, NOT_STORED,              0,          0, 255, 1}, /* Raw Sensor Value */
{ID_END,         0,                  0, 0,                       0,          0,   0, 0}  /* END */
};

#define LINE_OFFSET_SWITCH 13

int menu_item = ID_SWITCH;
int switch_status = OFF; /* ON or OFF */

LiquidCrystal lcd(PIN_IO_RS, PIN_IO_ENABLE, PIN_IO_D4, PIN_IO_D5, PIN_IO_D6, PIN_IO_D7);


/* 
 * Function prototypes 
 */
boolean read_settings();
void write_settings();
void sync_settings();
int read_lcd_buttons();
int read_sensor();
int draw_screen(int draw);
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
  draw_screen(DRAW_ALL);
}

/*
 * loop is the main loop which is executed repeatedly
 */
void loop()
{
  int draw = DRAW_NONE;
  draw |= read_sensor();
  draw |= handle_user_input();
  draw |= update_switch();
  
  draw_screen(draw);  
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
        sw_ok = (settings[i].eeprom_value == SW_VERSION);
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
  static int count = 0;
  static int button_delay = BUTTON_START_DELAY;
  int keys = analogRead(PIN_ADC_LCD_BUTTONS);
  if (abs(prev_keys - keys) > 2) {
    prev_keys = keys;
    count = 0;
  }
  if (keys > 1000) {
    button_delay = BUTTON_START_DELAY;
    return BTN_NONE;   /* most likely result */
  }
  count++;

  if (count >= 100) { /* Filter noise */
    if (count == button_delay) {
      /* Button held, simulate repeated button presses */
      count = 0;
      button_delay = BUTTON_REPEAT_DELAY; /* Faster then original delay */
      return BTN_NONE; /* Auto repeat */
    }
    if (keys < 50)   return BTN_RIGHT;  /* My value: 0 */
    if (keys < 200)  return BTN_UP;     /* My value: 100 */
    if (keys < 400)  return BTN_DOWN;   /* My value: 257 */
    if (keys < 600)  return BTN_LEFT;   /* My value: 410 */
    if (keys < 800)  return BTN_SELECT; /* My value: 642 */
  }

  return BTN_NONE;
}

/* 
 * Read sensor just reads an analog value, checks for change
 */
int read_sensor()
{
  static int prev_sensor_val = 0;
  int draw_status = DRAW_NONE;
  
  settings[0].value = analogRead(PIN_ADC_SENSOR);
  
  if (abs(prev_sensor_val - settings[0].value) > 2) {
    prev_sensor_val = settings[0].value;
    draw_status = DRAW_SENSOR;
  }
  if (settings[ID_MAX_SENSOR].value < settings[0].value) {
    settings[ID_MAX_SENSOR].value = settings[0].value;
    if (menu_item == ID_MAX_SENSOR) draw_status = DRAW_ALL;
  }

  return draw_status;
}

/*
 *
 */
int draw_screen(int draw)
{
  if (draw == DRAW_ALL) {
    lcd.clear();
  }
  
  if ((draw & DRAW_SENSOR) != 0) { /* Check DRAW_SENSOR bit */
    /* First line, always sensor value */
    lcd.setCursor(0,0);
    lcd.print(settings[0].text);
    lcd.setCursor(settings[0].line_offset, 0);
    lcd.print("    ");
    lcd.setCursor(settings[0].line_offset, 0);
    lcd.print(settings[0].value);
    lcd.setCursor(LINE_OFFSET_SWITCH,0);
    lcd.print(switch_text[switch_status]);
  }
  
  if ((draw & DRAW_MENU) != 0) { /* Check DRAW_MENU bit */
    lcd.setCursor(0,1);
    lcd.print(settings[menu_item].text);

    lcd.setCursor(settings[menu_item].line_offset, 1);
    lcd.print("    ");
    lcd.setCursor(settings[menu_item].line_offset, 1);
    /* All numeric, except for ID_SWITCH */
    if (settings[menu_item].id == ID_SWITCH) {
      lcd.print(switch_text[settings[menu_item].value]);
    }
    else {
      lcd.print(settings[menu_item].value);
    }
    
    if ((draw & DRAW_SAVE_NEEDED) != 0) {
      lcd.setCursor(15,1);
      lcd.print("*");
    }
  }
  
}

int handle_user_input()
{
  static int prev_button_pressed = BTN_NONE;
  int draw_status = DRAW_NONE;
  
  int button_pressed = read_lcd_buttons();
  if (prev_button_pressed == button_pressed) return draw_status;
  
  prev_button_pressed = button_pressed;
  switch (button_pressed)  {
    case BTN_RIGHT: {
      if (settings[menu_item].read_only == 1) break; /* Read Only */
      if (settings[menu_item].value < settings[menu_item].max_value) {
        settings[menu_item].value++;
        draw_status = DRAW_MENU | DRAW_SAVE_NEEDED;
      }            
      break;
    }
    case BTN_LEFT: {
      if (settings[menu_item].read_only == 1) break; /* Read Only */
      if (settings[menu_item].value > 0) { 
        settings[menu_item].value--;
        draw_status = DRAW_MENU | DRAW_SAVE_NEEDED;
      }
      break;
    }
  case BTN_UP: {
      if (menu_item > 1) {
        menu_item--;
        draw_status = DRAW_MENU;
      }
      write_settings();
      break;
    }
  case BTN_DOWN: {
      if (settings[menu_item+1].id != ID_END) {
        menu_item++;
        draw_status = DRAW_MENU;
      }
      write_settings();
      break;
    }
  case BTN_SELECT: {
      break;
    }
  case BTN_NONE: {
      break;
    }
  }
  return draw_status;
}

int update_switch()
{
  int draw_status = DRAW_NONE;
  static int prev_switch_val = -1; /* Start with unknown */
  int new_switch_value = settings[ID_SWITCH].value;
  int threshold;
  
  if (settings[ID_SWITCH].value == AUTO) {
    /* Check sensor/time if we need to update */
    threshold = settings[ID_THRESHOLD].value;
    if (switch_status == OFF) threshold -= settings[ID_HYSTERESIS].value;
    else threshold += settings[ID_HYSTERESIS].value;
    
    if (settings[ID_SENSOR].value < threshold) {
      new_switch_value = ON;
    }
    else {
      new_switch_value = OFF;
    }
  }
  /* Update the physical switch if needed */
  if (prev_switch_val != new_switch_value) {
    if (new_switch_value == ON) {
      digitalWrite(PIN_IO_SWITCH, HIGH);
      switch_status = ON;
    }
    else {
      digitalWrite(PIN_IO_SWITCH, LOW);
      switch_status = OFF;
    }

    draw_status |= DRAW_SENSOR;
    prev_switch_val = new_switch_value;
  }
  
  

  return draw_status;
}



