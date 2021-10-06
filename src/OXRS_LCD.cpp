
#include "Arduino.h"
#include "OXRS_LCD.h"
#include <TFT_eSPI.h>               // Hardware-specific library
#include "bitmaps.h"                // images bitmaps
#include "Free_Fonts.h"             // Include the header file attached to this sketch
#include "roboto_fonts.h"

TFT_eSPI tft = TFT_eSPI();          // Invoke library

// for ethernet
OXRS_LCD::OXRS_LCD (EthernetClass& ethernet)
{
  _wifi = NULL;
  _ethernet = &ethernet;
  _oxrs_lcd();
}
// for wifi
OXRS_LCD::OXRS_LCD (WiFiClass& wifi)
{
  _wifi = &wifi;
  _ethernet = NULL;
  _oxrs_lcd();
}
// common
void OXRS_LCD::_oxrs_lcd (void)
{
  _last_lcd_trigger = 0L;
  _last_event_display = 0L;  
  _last_rx_trigger = 0L;
  _last_tx_trigger = 0L;
  _ethernet_link_status = Unknown;
  _wifi_connection_status = WL_IDLE_STATUS;
  memset(_io_values, 0, sizeof(_io_values));
}

void OXRS_LCD::begin (uint32_t ontime_event, uint32_t ontime_display)
{
  tft.begin();               // Initialise the display
  tft.setRotation(1);
  tft.fillRect(0, 0, 240, 240,  TFT_BLACK);

  // set up for backlight dimming (PWM)
  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
  _set_backlight(LCD_BL_ON);
  
  _ontime_display = ontime_display;
  _ontime_event = ontime_event;

  Serial.print(F("[lcd ] mounting SPIFFS..."));
  if (!SPIFFS.begin())
  { 
    Serial.println(F("failed, might need formatting?"));
    return; 
  }
  Serial.println(F("done"));
}


void OXRS_LCD::draw_header(const char * fwShortName, const char * fwMaker, const char * fwVersion, const char * fwPlatform)
{
  char buffer[30];

  // default logo is OXRS
  int logo_h = oxrs40Height;
  int logo_w = oxrs40Width;
  const unsigned char * logo_bm = oxrs40;
  uint16_t logo_fg = tft.color565(167, 239, 225);
  uint16_t logo_bg = tft.color565(45, 74, 146);

  // try to draw maker supplied logo.bmp from SPIFFS
  // if no valid file found, draw default OXRS logo stored in PROGMEM
  if (!_drawBmp("/logo.bmp", 0, 0))
  {
    tft.drawBitmap(0, 0, logo_bm, logo_w, logo_h, logo_fg, logo_bg);
  }

  tft.fillRect(42, 0, 240, 40,  TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&Roboto_Light_13);
  
  tft.drawString(fwShortName, 46, 0);

  tft.drawString(fwMaker, 46, 13);
 
  tft.drawString("Version", 46, 26); 
  sprintf(buffer, ": %s / %s", fwVersion, fwPlatform); 
  tft.drawString(buffer, 46+50, 26); 
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TC_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  tft.drawString("connecting...", 240/2 , 50); 
}


void OXRS_LCD::show_MQTT_topic (const char * topic)
{
  char buffer[30];

  tft.fillRect(0, 80, 240, 13,  TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "MQTT: %s", topic);
  tft.drawString(buffer, 12, 80);

  // show idle activity until we get an explicit connection 
  // status update via show_mqtt_connection_status()
  _set_mqtt_tx_led(MQTT_STATE_IDLE);
  _set_mqtt_rx_led(MQTT_STATE_IDLE);
}


void OXRS_LCD::show_temp (float temperature)
{
  char buffer[30];
  
  tft.fillRect(0, 95, 240, 13,  TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "TEMP: %2.1f C", temperature);
  tft.drawString(buffer, 12, 95);
}

/*
 * draw ports with inactive inputs on screen 
 */
void OXRS_LCD::draw_ports (int port_layout, uint8_t mcps_found)
{ 
  _port_layout = port_layout;
  if (_port_layout == PORT_LAYOUT_INPUT_96)
  {
    for (int index = 1; index <= 96; index += 16)
    {
      int active = (bitRead(mcps_found, 0)) ? 1 : 0;
      for (int i = 0; i < 16; i++)
      {
        if ((i % 4) == 0)
        {
          _update_input_96(TYPE_FRAME, index+i, active);
        }
        _update_input_96(TYPE_STATE, index+i, 0);
      }
      mcps_found >>= 1;
    }  
  }

  if (_port_layout == PORT_LAYOUT_INPUT_128)
  {
    
    for (int index = 1; index <= 128; index += 16)
    {
      int active = (bitRead(mcps_found, 0)) ? 1 : 0;
      for (int i = 0; i < 16; i++)
      {
        if ((i % 4) == 0)
        {
          _update_input_128(TYPE_FRAME, index+i, active);
        }
        _update_input_128(TYPE_STATE, index+i, 0);
      }
      mcps_found >>= 1;
    }  
  }
  
  if (_port_layout == PORT_LAYOUT_OUTPUT_128)
  {    
    tft.fillRect(0, 134, 240, 87,  TFT_WHITE);

    for (int index = 1; index <= 128; index += 16)
    {
      int active = (bitRead(mcps_found, 0)) ? 1 : 0;
      for (int i = 0; i < 16; i++)
      {
        _update_output_128(TYPE_FRAME, index+i, active);
        _update_output_128(TYPE_STATE, index+i, active ? 0 : -1);
      }
      mcps_found >>= 1;
    }  
  }
  
  if (_port_layout == PORT_LAYOUT_IO_48)
  {    

    for (int index = 1; index <= 48; index += 16)
    {
      for (int i = 0; i < 16; i++)
      {
        if (index <= 16)
        {
          _update_io_48(TYPE_FRAME, index+i, 1);
        }
        _update_io_48(TYPE_STATE, index+i, 0);
      }
      mcps_found >>= 1;
    }  
  }

}

/*
 * draw event on bottom line of screen
 */
void OXRS_LCD::show_event (const char * s_event)
{
  // Show last input event on bottom line
  tft.fillRect(0, 225, 240, 240,  TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
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
 * control mqtt rx/tx virtual leds 
 */
void OXRS_LCD::trigger_mqtt_rx_led (void)
{
  _set_mqtt_rx_led(MQTT_STATE_ACTIVE);
  _last_rx_trigger = millis(); 
}

void OXRS_LCD::trigger_mqtt_tx_led (void)
{
  _set_mqtt_tx_led(MQTT_STATE_ACTIVE);
  _last_tx_trigger = millis(); 
}

void OXRS_LCD::show_mqtt_connection_status (bool state)
{
  if (!state)
  {
    _set_mqtt_tx_led(MQTT_STATE_DOWN);
    _set_mqtt_rx_led(MQTT_STATE_DOWN);
    _last_tx_trigger = 0L;
    _last_rx_trigger = 0L;
  }
  else
  {
    _set_mqtt_tx_led(MQTT_STATE_IDLE);
    _set_mqtt_rx_led(MQTT_STATE_IDLE);
  }
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
        switch (_port_layout) {
          case PORT_LAYOUT_INPUT_96:
            _update_input_96(TYPE_STATE, index+i+1, !bitRead(io_value, i)); break;
          case PORT_LAYOUT_INPUT_128:
            _update_input_128(TYPE_STATE, index+i+1, !bitRead(io_value, i)); break;
          case PORT_LAYOUT_OUTPUT_128:
            _update_output_128(TYPE_STATE, index+i+1, bitRead(io_value, i)); break;
          case PORT_LAYOUT_IO_48:
            if (index < 16)
            {
              _update_io_48(TYPE_STATE, index+i+1, !bitRead(io_value, i));
            } else
            {
              _update_io_48(TYPE_STATE, index+i+1, bitRead(io_value, i));
            }
            break;
        }
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
 *  rx and tx led timed out
 *  link status has changed
 */
void OXRS_LCD::loop(void)
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
  
  // turn off rx LED if timed out
  if (_last_rx_trigger)
  {
    if ((millis() - _last_rx_trigger) > RX_TX_LED_ON)
    {
      _set_mqtt_rx_led(MQTT_STATE_IDLE);
      _last_rx_trigger = 0L;
    }
  }
 
  // turn off tx LED if timed out
  if (_last_tx_trigger)
  {
    if ((millis() - _last_tx_trigger) > RX_TX_LED_ON)
    {
      _set_mqtt_tx_led(MQTT_STATE_IDLE);
      _last_tx_trigger = 0L;
    }
  }
 
  // check if ethernet link status has changed
  if (_ethernet) _show_ethernet();

  // check if wifi connection status has changed
  if (_wifi) _show_wifi();
}

// Ethernet
void OXRS_LCD::_show_ethernet()
{
  int current_link_status = _ethernet->linkStatus();
  if (current_link_status != _ethernet_link_status)
  {
    _ethernet_link_status = current_link_status;

    // refresh IP on link status change
    if (_ethernet_link_status == LinkON)
    {
      if (_ethernet->localIP()[0] == 0) {_ethernet_link_status = Unknown;}
      _show_IP(_ethernet->localIP(), 1);
    }
    else
    {
      _show_IP(IPAddress(0,0,0,0), 0);
    }

    // refresh MAC on link status changes
    byte mac[6];
    _ethernet->MACAddress(mac);
    _show_MAC(mac);
  }
}

// WiFi    TODO : needs to be tested
void OXRS_LCD::_show_wifi()
{
  int current_connection_status = _wifi->status();
  if (current_connection_status != _wifi_connection_status)
  {
    _wifi_connection_status = current_connection_status;

    // refresh IP on connection status change
    if (_wifi_connection_status == WL_CONNECTED)
    {
      _show_IP(_wifi->localIP(), 1);
    }
    else
    {
      _show_IP(IPAddress(0,0,0,0), 0);
    }

    // refresh MAC on connection status changes
    byte mac[6];
    _wifi->macAddress(mac);
    _show_MAC(mac);
  }
}

// common
void OXRS_LCD::_show_IP (IPAddress ip, int link_status)
{
  char buffer[30];
  
  tft.fillRect(0, 50, 240, 13,  TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  if (ip[0] == 0)
  {
    sprintf(buffer, "IP  : ---.---.---.---");
  }
  else
  {
    sprintf(buffer, "IP  : %03d.%03d.%03d.%03d", ip[0],ip[1], ip[2], ip[3]);
  }
  tft.drawString(buffer, 12, 50);

  _set_ip_link_led(link_status);
}

void OXRS_LCD::_show_MAC (byte mac[])
{
  char buffer[30];
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "MAC : %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  tft.drawString(buffer, 12, 65);
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
void OXRS_LCD::_update_input_96 (uint8_t type, uint8_t index, int active)
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
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
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
void OXRS_LCD::_update_input_128 (uint8_t type, uint8_t index, int active)
{  
  int bw = 23;
  int bh = 19;
  int xo = 25;
  int x = 0;
  int y = 136;
  int port;
  uint16_t color;

  index -= 1;
  port = index / 4;
  y = y + (port % 2) * bh;
  if (port > 15) {y = y + 2 * bh + 3;}
  xo = xo + ((port % 16) / 8) * 3;
  x = xo + ((port % 16) / 2) * bw;

  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = active ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
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

/**
  animation of out state in ports view
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
  index:   | 1| 2| 3| 4| 5| 6| 7| 8|| 9|10|11|12|13|14|15|16| |17|18|19|20|21|22|23|24||25|26|27|28|29|30|31|32|     
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           |33|  |  |  |  |  |  |  ||  |  |  |  |  |  |  |  | |  |  |  |  |  |  |  |  ||  |  |  |  |  |  |  |64|     
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           |65|  |  |  |  |  |  |  ||  |  |  |  |  |  |  |  | |  |  |  |  |  |  |  |  ||  |  |  |  |  |  |  |96|     
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+
           |97|  |  |  |  |  |  |  ||  |  |  |  |  |  |  |  | |  |  |  |  |  |  |  |  ||  |  |  |  |  |  |  |128|     
           +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+ +--+--+--+--+--+--+--+--++--+--+--+--+--+--+--+--+

   frame : x = 0; y = 134 ; w = 240; h = 4*21+2 = 86
 */
void OXRS_LCD::_update_output_128 (uint8_t type, uint8_t index, int active)
{  
  int bw = 8;
  int bh = 19;
  int xo = 4;
  int x = 0;
  int y = 136;
  uint16_t color;
  int index_mod;

  index -= 1;
  index_mod = index % 32;
  
  y = y + ((index / 32) * (bh + 2));
  xo = xo + (((index_mod) / 8) * 2) + (((index_mod) / 16) * 2);
  x = xo + ((index_mod) * (bw - 1));

  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = active ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
  }
  else
  // draw virtual led in port
  {
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
    switch (active) {
      case -1:
        tft.drawRect(x+2, y+bh/2+2, bw-4, bh/2-4,  TFT_DARKGREY);
        break;
      case 0:
        tft.fillRect(x+2, y+bh/2+2, bw-4, bh/2-4,  TFT_LIGHTGREY);
        break;
      case 1:
        tft.fillRect(x+1, y+1,      bw-2, bh-2,  TFT_RED);
        break;
    }
  }     
}

/**
  animation of input and outputstate in ports view
  Ports:    | 1 | 3 | 5 | 7 |     Index:      | 1 : 2 | 7 : 8 |    function:  | O : O |
            +---+---+---+---+....             |.......|.......|               |.......|
            | 2 | 4 | 6 | 8 |                 | 3 :   | 9 :   |               | I :   |
                                              +-------+-------+......         +-------+
                                              | 4 : 5 | 10: 11|
                                              |.......|.......|
                                              | 6 :   | 12:   |
  index 00..15 Input 00..15     ; 0 -> 2;         1 -> 5;         2 -> 8
  index 16..47 Output 00..31   ;  0 -> 0; 1 -> 1; 2 -> 3; 3 -> 4; 4 ->6; 5 -> 7
  0 .. 15 (inputs)
  index = i * 3 + 2
  16 .. 31 , 32 .. 47 (outputs)
  i -= 16
  index = (i / 2) * 3 + i % 2
*/
void OXRS_LCD::_update_io_48 (uint8_t type, uint8_t index, int active)
{  

  int bw = 27;
  int bh = 33;
  int bht = bh-bw/2;
  int xo = 10;
  int x = 0;
  int y = 150;
  int port;
  int i;
  uint16_t color;

  index -= 1;
  i = index;
  if (i < 16)
  {
    index = i * 3 + 2;
  }
  else
  {
    i -= 16;
    index = (i / 2) * 3 + i % 2; 
  }
  port = index / 3;
  y = y + (port % 2) * bh;
  xo = xo + (port / 8) * 3;
  x = xo + (port / 2) * bw;

  if (type == TYPE_FRAME)
  // draw port fame
  {
    color = active ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
    tft.drawRect(x, y, bw/2+1, bht, color);
    tft.drawRect(x+bw/2, y, bw/2+1, bht, color);
  }
  else
  // draw virtual led in port
  {
    switch (index % 3){
      case 0:
        color = active ? TFT_RED : TFT_DARKGREY; 
        tft.fillRect(x+1     , y+1      , bw/2-1, bht-2, color);
        break;
      case 1:
        color = active ? TFT_RED : TFT_DARKGREY; 
        tft.fillRect(x+1+bw/2, y+1      , bw/2-1, bht-2, color);
        break;
      case 2:
        color = active ? TFT_YELLOW : TFT_DARKGREY;
        tft.fillRoundRect(x+2     , y+bht+1 , bw/2-3, bh-bht-3, 3, color);
        break;
    }
  }     
}


/*
 * animated "leds"
 */
void OXRS_LCD::_set_mqtt_rx_led(int state)
{
  uint16_t color[3] = {TFT_DARKGREY, TFT_YELLOW, TFT_RED};
  
  if (state < 3) tft.fillRoundRect(2, 80, 8, 5, 2,  color[state]);
}

void OXRS_LCD::_set_mqtt_tx_led(int state)
{
   uint16_t color[3] = {TFT_DARKGREY, TFT_ORANGE, TFT_RED};

  if (state < 3) tft.fillRoundRect(2, 88, 8, 5, 2,  color[state]);
}

void OXRS_LCD::_set_ip_link_led(int active)
{
  uint16_t color;
 
  color = active ? TFT_GREEN : TFT_RED;
  tft.fillRoundRect(2, 54, 8, 5, 2,  color);
}

/*
 * set backlight of LCD val in % [0..100]
 */
void OXRS_LCD::_set_backlight(int val)
{
  ledcWrite(BL_PWM_CHANNEL, 255*val/100); 
}




// Bodmers BMP image rendering function

bool OXRS_LCD::_drawBmp(const char *filename, int16_t x, int16_t y) 
{
  uint32_t  seekOffset;
  uint16_t  w, h, row, col;
  uint8_t   r, g, b;
  bool      drawn = true;

  Serial.print(F("[lcd ] reading "));  
  Serial.print(filename);
  Serial.print(F("..."));

  File file = SPIFFS.open(filename, "r");
  if (!file) 
  {
    Serial.println(F("failed to open file"));
    return false;
  }

  if (_read16(file) == 0x4D42)
  {
    _read32(file);
    _read32(file);
    seekOffset = _read32(file);
    _read32(file);
    w = _read32(file);
    h = _read32(file);
    if ((w != 40) || (h != 40))
    {
      Serial.println(F("[lcd ] warning! logo size not 40x40"));
    }

    if ((_read16(file) == 1) && (_read16(file) == 24) && (_read32(file) == 0))
    {
      // crop to h = 40
      y += 40 - 1;

      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      file.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++)
      {
        
        file.read(lineBuffer, sizeof(lineBuffer));
        uint8_t*  bptr = lineBuffer;
        uint16_t* tptr = (uint16_t*)lineBuffer;
        // Convert 24 to 16 bit colours
        for (uint16_t col = 0; col < w; col++)
        {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        // crop to w = 40
        tft.pushImage(x, y--, 40, 1, (uint16_t*)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);

      file.close();
      Serial.println(F("done"));
      return true;
    }
  }
  file.close();
  Serial.println(F("done"));
  Serial.println(F("[lcd ] bmp format not recognized."));
  return false;
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t OXRS_LCD::_read16(File &f) 
{
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t OXRS_LCD::_read32(File &f) 
{
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
