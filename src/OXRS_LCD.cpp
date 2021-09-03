
#include "Arduino.h"
#include "OXRS_LCD.h"


OXRS_LCD::OXRS_LCD ()
{
  _last_lcd_trigger = 0L;
  _last_event_display = 0L;  
  memset(_io_values, 0, sizeof(_io_values));
}

void OXRS_LCD::begin (uint32_t ontime_event, uint32_t ontime_display)
{
  tft.begin();               // Initialise the display
  tft.fillRect(0, 0, 240, 240,  TFT_BLACK);

  // set up for backlight dimming (PWM)
  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
  _set_backlight(LCD_BL_ON);
  
  _ontime_display = ontime_display;
  _ontime_event = ontime_event;
  
}


void OXRS_LCD::draw_logo(char * firmware_version)
{
  char buffer[20];
  
  // logo
  tft.fillRect(0, 0, 240, 32,  TFT_WHITE);
  int x = (240 - logoWidth - superWidth - houseWidth) / 2;
  tft.drawBitmap(x, 0, logo, logoWidth, logoHeight, tft.color565(0, 165, 179), TFT_WHITE);
  tft.drawBitmap(x+logoWidth, 0, super, superWidth, superHeight, tft.color565(22, 111, 193), TFT_WHITE);
  tft.drawBitmap(x+logoWidth+superWidth, 0, house, houseWidth, houseHeight, tft.color565(0, 165, 179), TFT_WHITE);
  // version
  tft.setTextDatum( TC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(FMB9);       // Select Free Mono Bold 9
  strcpy(buffer, "URC v");
  strcat(buffer, firmware_version);
  tft.drawString(buffer, 240/2, 40);
}

void OXRS_LCD::show_IP (IPAddress ip)
{
  char buffer[30];
  
  // IP
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "IP : %03d.%03d.%03d.%03d", ip[0],ip[1], ip[2], ip[3]);
  tft.drawString(buffer, 10, 60);
}

void OXRS_LCD::show_MAC (byte mac[])
{
  char buffer[30];
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  tft.drawString(buffer, 10, 75);
}

void OXRS_LCD::show_MQTT_topic (char * topic)
{
  // MQTT topic
  tft.drawString("urc/conf/URC-33C0AA/...", 10, 90);
  // Temperature
  tft.drawString("Rack Temp.: 12.34 C", 10, 105);
}

/*
 * draw ports with inactive inputs on screen 
 */
void OXRS_LCD::draw_ports (uint8_t mcps_found)
{ 
  for (int index = 1; index <= 96; index += 16)
  {
    int active = (bitRead(mcps_found, 0)) ? 1 : 0;
    for (int i = 0; i < 16; i++)
    {
      if ((i % 4) == 0)
      {
        _update_input(TYPE_FRAME, index+i, active);
      }
      _update_input(TYPE_STATE, index+i, 0);
    }
    mcps_found >>= 1;
  }  
}

/*
 * draw event on bottom line of screen
 */
void OXRS_LCD::show_event (char s_event[])
{
  // Show last input event on bottom line
  tft.fillRect(0, 225, 240, 240,  TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(FMB9);       // Select Free Mono Bold 9
  tft.drawString(s_event, 10, 225);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _last_event_display = millis(); 
}

/*
 * clear event 
 */
void OXRS_LCD::_clear_event ()
{
  tft.fillRect(0, 225, 240, 240,  TFT_BLACK);
}


/*
 * process io_value :
 * check for changes vs last stored value
 * animate port display if change detected
 */
void OXRS_LCD::process (int mcp, uint16_t io_value)
{
  int i, index;
  uint16_t changed;
  
  // Compare with last stored value
  changed = io_value ^ _io_values[mcp];
  // change detected
  if (changed)
  {
    index = mcp * 16;
    for (i=0; i<16; i++)
    {
      if (bitRead(changed, i))
      {
        _set_backlight(LCD_BL_ON);
        _last_lcd_trigger = millis(); 
        
        _update_input(TYPE_STATE, index+i+1, !bitRead(io_value, i));       
      }
    }
    // Need to store so we can detect changes for port animation
    _io_values[mcp] = io_value;
  }
}

/*
 * update LCD if
 *  show_event timed out
 *  LCD_on timed out
 */
void OXRS_LCD::update(void)
{
  // Clear event display if timed out
  if (_ontime_event && _last_event_display)
  {
    if ((millis() - _last_event_display) > _ontime_event)
    {
      _clear_event();      
      _last_event_display = 0L;
    }
  }

  // Dim LCD if timed out
  if (_ontime_display && _last_lcd_trigger)
  {
    if ((millis() - _last_lcd_trigger) > _ontime_display)
    {
      _set_backlight(LCD_BL_DIM);
      _last_lcd_trigger = 0L;
    }
  }
} 

/**
  animation of input state in ports view
  Ports:    | 1 | 3 | 5 | 7 |     Index:      | 1 : 3 | 9 : 11|
            +---+---+---+---+....             |.......|.......|
            | 2 | 4 | 6 | 8 |                 | 2 : 4 | 10: 12|
                                              +-------+-------+......
                                              | 5 : 7 | 13: 15|
                                              |.......|.......|
                                              | 6 : 8 | 14: 16|                                             
*/
void OXRS_LCD::_update_input (uint8_t type, uint8_t index, uint8_t active)
{  
  int bw = 19;
  int bh = 19;
  int xo = 2;
  int x = 0;
  int y = 180;
  int port;
  uint16_t color;

  index -= 1;
  port = index / 4;
  y = y + (port % 2) * bh;
  xo = xo + (port / 8) * 3;
  x = xo + (port / 2) * bw;

  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = active ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
  }
  else
  // draw virtual led in port
  {
    color = active ? TFT_YELLOW : TFT_DARKGREY;
    switch (index % 4){
      case 0:
        tft.fillRoundRect(x+2     , y+2      , bw/2-2, bh/2-2, 2, color);
        break;
      case 1:
        tft.fillRoundRect(x+2     , y+bh/2+1 , bw/2-2, bh/2-2, 2, color);
        break;
      case 2:
        tft.fillRoundRect(x+1+bw/2, y+2      , bw/2-2, bh/2-2, 2, color);
        break;
      case 3:
        tft.fillRoundRect(x+1+bw/2, y+bh/2+1 , bw/2-2, bh/2-2, 2, color);
        break;
    }
  }     
}

/*
 * set backlight of LCD val in % [0..100]
 */
void OXRS_LCD::_set_backlight(int val)
{
  ledcWrite(BL_PWM_CHANNEL, 255*val/100); 
}
