
#include "Arduino.h"
#include "OXRS_LCD.h"
#include <Ethernet.h>


OXRS_LCD::OXRS_LCD ()
{
  _last_lcd_trigger = 0L;
  _last_event_display = 0L;  
  _last_rx_trigger = 0L;
  _last_tx_trigger = 0L;
  _ethernet_link_status = 0;
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
  
}


void OXRS_LCD::draw_header(char * fw_maker_code, char * fw_name, char * fw_version,  char * fw_platform )
{
  char buffer[30];
  
  int logo_h = 0;
  int logo_w = 0;
  const unsigned char * logo_bm = NULL;
  uint16_t logo_fg = TFT_BLACK;
  uint16_t logo_bg = TFT_WHITE;
  if (strcmp("SHA", fw_maker_code) == 0)
  {
    logo_h = sha40Height;
    logo_w = sha40Width;
    logo_bm = sha40;
    logo_fg = tft.color565(0, 165, 179);
    logo_bg = TFT_WHITE;
  }
  if (strcmp("AC", fw_maker_code) == 0)
  {
    logo_h = ac40Height;
    logo_w = ac40Width;
    logo_bm = ac40;
    logo_fg = TFT_WHITE;
    logo_bg = TFT_BROWN;
  }
  if (strcmp("BMD", fw_maker_code) == 0)
  {
    logo_h = bmd40Height;
    logo_w = bmd40Width;
    logo_bm = bmd40;
    logo_fg = TFT_WHITE;
    logo_bg = TFT_BLUE;
  }
 
  tft.drawBitmap(0, 0, logo_bm, logo_w, logo_h, logo_fg, logo_bg);
  tft.fillRect(42, 0, 240, 40,  TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&Roboto_Light_13);
  tft.drawString(fw_name, 46, 0);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "Maker: %s", fw_maker_code);
  tft.drawString(buffer, 46, 13);
  sprintf(buffer, "Vers.: %s / %s", fw_version, fw_platform); 
  tft.drawString(buffer, 46, 26); 
}

void OXRS_LCD::show_ethernet()
{
  byte mac[6];
  
  _show_IP(Ethernet.localIP(), Ethernet.linkStatus() == LinkON ? 1 : 0);
  Ethernet.MACAddress(mac);
  _show_MAC(mac);
}

void OXRS_LCD::_show_IP (IPAddress ip, int link_status)
{
  char buffer[30];
  
  tft.fillRect(0, 50, 240, 13,  TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "IP  : %03d.%03d.%03d.%03d", ip[0],ip[1], ip[2], ip[3]);
  tft.drawString(buffer, 12, 50);

  _set_ip_link_led(link_status);
  
}

void OXRS_LCD::_show_MAC (byte mac[])
{
  char buffer[30];
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "MAC : %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  tft.drawString(buffer, 12, 65);
}

void OXRS_LCD::show_MQTT_topic (char * topic)
{
  char buffer[30];

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "MQTT: %s",topic);
  tft.drawString(buffer, 12, 80);

  _set_mqtt_rx_led(0);
  _set_mqtt_tx_led(0); 
}


void OXRS_LCD::show_temp (float temperature)
{
  char buffer[30];

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum( TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "TEMP: %2.2f C", temperature);
  tft.drawString(buffer, 12, 95);
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
 * control mqtt rx/tx virtual leds 
 */
void OXRS_LCD::trigger_mqtt_rx_led (void)
{
  _set_mqtt_rx_led(1);
  _last_rx_trigger = millis(); 
}

void OXRS_LCD::trigger_mqtt_tx_led (void)
{
  _set_mqtt_tx_led(1);
  _last_tx_trigger = millis(); 
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
 *  rx and tx led timed out
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
      _set_mqtt_rx_led(0);
      _last_rx_trigger = 0L;
    }
  }
 
  // turn off tx LED if timed out
  if (_last_tx_trigger)
  {
    if ((millis() - _last_tx_trigger) > RX_TX_LED_ON)
    {
      _set_mqtt_tx_led(0);
      _last_tx_trigger = 0L;
    }
  }
 
  // check if ethernet link status has changed
  uint8_t tmp = Ethernet.linkStatus();
  if (tmp != _ethernet_link_status)
  {
    if (tmp != LinkON)
    {
      _show_IP(IPAddress(0,0,0,0), 0);
    }
    else
    {
      _show_IP(Ethernet.localIP(), 1);
    }
    _ethernet_link_status = tmp;
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

void OXRS_LCD::_set_mqtt_rx_led(int active)
{
  uint16_t color;
 
  color = active ? TFT_YELLOW : TFT_DARKGREY;
  tft.fillRoundRect(2, 80, 8, 5, 2,  color);
}

void OXRS_LCD::_set_mqtt_tx_led(int active)
{
  uint16_t color;
 
  color = active ? TFT_ORANGE : TFT_DARKGREY;
  tft.fillRoundRect(2, 88, 8, 5, 2,  color);
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
