/*
 * OXRS_LCD.cpp
 * 
 */
 
#include "Arduino.h"
#include "OXRS_LCD.h"

#include <TFT_eSPI.h>               // Hardware-specific library
#include "OXRS_logo.h"              // default logo bitmap (24-bit-bitmap)
#include "Free_Fonts.h"             // GFX Free Fonts supplied with TFT_eSPI
#include "roboto_fonts.h"           // roboto_fonts Created by http://oleddisplay.squix.ch/
#include "icons.h"                  // resource file for icons
#include <pgmspace.h>

TFT_eSPI tft = TFT_eSPI();          // Invoke library

// for ethernet
OXRS_LCD::OXRS_LCD(EthernetClass& ethernet, OXRS_MQTT& mqtt)
{
  _wifi = NULL;
  _ethernet = &ethernet;
  _mqtt = &mqtt;

  memset(_io_values, 0, sizeof(_io_values));
}

// for wifi
OXRS_LCD::OXRS_LCD(WiFiClass& wifi, OXRS_MQTT& mqtt)
{
  _wifi = &wifi;
  _ethernet = NULL;
  _mqtt = &mqtt;

  memset(_io_values, 0, sizeof(_io_values));
}

void OXRS_LCD::begin()
{
  // initialise the display
  tft.begin();
  tft.setRotation(1);
  tft.fillRect(0, 0, 240, 240,  TFT_BLACK);

  // set up for backlight dimming (PWM)
  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
  _set_backlight(_brightness_on);
}

// ontime_display : display on after event occured    (default: 10 seconds)
// ontime_event   : time to show event on bottom line (default: 3 seconds)
// value range: 
//    0          : ever (no timer)
//    1 .. 600   : time in seconds (10 minutes max) range can be defined by the UI, not checked here
void OXRS_LCD::setOnTimeDisplay (int ontime_display)
{
  _ontime_display_ms = ontime_display * 1000;
}

void OXRS_LCD::setOnTimeEvent (int ontime_event)
{
  _ontime_event_ms = ontime_event * 1000;
}

// brightness_on  : brightness when on        (default: 100 %)
// brightness_dim : brightness when dimmed    (default:  10 %)
// value range    : 0 .. 100  : brightness in %  range can be defined by the UI, not checked here
void OXRS_LCD::setBrightnessOn (int brightness_on)
{
  _brightness_on = brightness_on;
}

void OXRS_LCD::setBrightnessDim (int brightness_dim)
{
  _brightness_dim = brightness_dim;
}

void OXRS_LCD::setPortConfig(uint8_t port, int config)
{
  // TODO: config should be an int so we can handle other config types later (i.e. no more bit math)
  if (config)
  {
    _update_security(TYPE_FRAME, port*4+1, PORT_STATE_OFF);
  }
  else
  {
    _update_input(TYPE_FRAME, port*4+1, PORT_STATE_OFF);
    bitWrite(_ports_to_flash, port, 0);
  }

  // update our port config global
  bitWrite(_port_config, port, config);

  // force content to be updated (reset MCP initialised flag)
  bitWrite(_io_values_initialised, port / 4, 0);
}

int OXRS_LCD::draw_header(const char * fwShortName, const char * fwMaker, const char * fwVersion, const char * fwPlatform, const uint8_t * fwLogo)
{
  char buffer[30];
  int return_code;

  int logo_w = 40;
  int logo_h = 40;
  int logo_x = 0;
  int logo_y = 0;

  // 1. try to draw maker supplied /logo.bmp from SPIFFS
  // 2, if not successful try to draw maker supplied logo via fwLogo (fwLogo from PROGMEM)
  // 3. if not successful draw embedded OXRS logo from PROGMEM
  return_code = LCD_INFO_LOGO_FROM_SPIFFS;
  if (!_drawBmp("/logo.bmp", logo_x, logo_y, logo_w, logo_h))
  {
    return_code = LCD_INFO_LOGO_FROM_PROGMEM;
    if (!fwLogo || !_drawBmp_P(fwLogo, logo_x, logo_y, logo_w, logo_h))
    {  
      return_code = LCD_INFO_LOGO_DEFAULT;
      if (!_drawBmp_P(OXRS_logo, logo_x, logo_y, logo_w, logo_h))
      {
        return_code = LCD_ERR_NO_LOGO;
      }
    }
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
  tft.drawString("Starting up...", 240/2 , 50); 
  
  return return_code;
}

/*
 * draw ports according to PORT_LAYOUT_... on screen 
 * show i/o's as inactive
*/
void OXRS_LCD::draw_ports(int port_layout, uint8_t mcps_found)
{ 
  _port_layout = port_layout;
  _mcps_found = mcps_found;
  _mcp_output_pins = 16;
  _mcp_output_start = 8;
  _io_values_initialised = 0;
 
  // handle input configurations
  if ((_port_layout / 1000) == 1)
  {
    // autodetect layout from mcps_found
    if (_port_layout == PORT_LAYOUT_INPUT_AUTO)
    {
      _port_layout = PORT_LAYOUT_INPUT_128;
      if (mcps_found < 0x40) {_port_layout = PORT_LAYOUT_INPUT_96;}
      if (mcps_found < 0x10) {_port_layout = PORT_LAYOUT_INPUT_64;}
      if (mcps_found < 0x04) {_port_layout = PORT_LAYOUT_INPUT_32;}
    }
    // configure outline
    switch (_port_layout) 
    {
      case PORT_LAYOUT_INPUT_32:
        _layout_config.x = 0;
        _layout_config.y = 151;
        _layout_config.xo = 25+47;
        _layout_config.bw = 23;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        break;
      case PORT_LAYOUT_INPUT_64:
        _layout_config.x = 0;
        _layout_config.y = 151;
        _layout_config.xo = 25; 
        _layout_config.bw = 23;
        _layout_config.bh = 19;
        _layout_config.index_max = 64;
        break;
      case PORT_LAYOUT_INPUT_96:
        _layout_config.x = 0;
        _layout_config.y = 151;
        _layout_config.xo = 2; 
        _layout_config.bw = 19;
        _layout_config.bh = 19;
        _layout_config.index_max = 96;
        break;
      case PORT_LAYOUT_INPUT_128:
        _layout_config.x = 0;
        _layout_config.y = 130;
        _layout_config.xo = 25; 
        _layout_config.bw = 23;
        _layout_config.bh = 19;
        _layout_config.index_max = 128;
        break;
    }
    // draw outline as configured
    for (int index = 1; index <= _layout_config.index_max; index += 16)
    {
      int state = (bitRead(mcps_found, 0)) ? PORT_STATE_OFF : PORT_STATE_NA;
      for (int i = 0; i < 16; i++)
      {
        if ((i % 4) == 0)
        {
          _update_input(TYPE_FRAME, index+i, state);
        }
        _update_input(TYPE_STATE, index+i, state);
      }
      mcps_found >>= 1;
    } 
  }   
  
  // handle output configurations
  int frame_h;
  if ((_port_layout / 1000) == 2)
  {
    // autodetect layout from mcps_found
    if (_port_layout == PORT_LAYOUT_OUTPUT_AUTO)
    {
      _port_layout = PORT_LAYOUT_OUTPUT_128;
      if (mcps_found < 0x40) {_port_layout = PORT_LAYOUT_OUTPUT_96;}
      if (mcps_found < 0x10) {_port_layout = PORT_LAYOUT_OUTPUT_64;}
      if (mcps_found < 0x04) {_port_layout = PORT_LAYOUT_OUTPUT_32;}
    }
    if (_port_layout == PORT_LAYOUT_OUTPUT_AUTO_8)
    {
      _port_layout = PORT_LAYOUT_OUTPUT_64_8;
      if (mcps_found < 0x10) {_port_layout = PORT_LAYOUT_OUTPUT_32_8;}
    }
    // check for 8/16 MCP
    if ((_port_layout % 1000) >= 800)
    {
      _mcp_output_pins = 8;
    }
    
    // configure outline
    switch (_port_layout) 
    {
      case PORT_LAYOUT_OUTPUT_32:
      case PORT_LAYOUT_OUTPUT_32_8:
        _layout_config.x = 0;
        _layout_config.y = 159;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        frame_h = _layout_config.bh + 4;
        break;
      case PORT_LAYOUT_OUTPUT_64:
      case PORT_LAYOUT_OUTPUT_64_8:
        _layout_config.x = 0;
        _layout_config.y = 148;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 64;
        frame_h = _layout_config.bh * 2 + 6;
        break;
      case PORT_LAYOUT_OUTPUT_96:
        _layout_config.x = 0;
        _layout_config.y = 138;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 96;
        frame_h = _layout_config.bh * 3 + 8;
        break;
      case PORT_LAYOUT_OUTPUT_128:
        _layout_config.x = 0;
        _layout_config.y = 127;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 128;
        frame_h = _layout_config.bh * 4 + 10;
        break;
    }
    // draw outline as configured   
    tft.fillRect(0, _layout_config.y-2, 240, frame_h,  TFT_WHITE);
    for (int index = 1; index <= _layout_config.index_max; index += _mcp_output_pins)
    {
      int state = (bitRead(mcps_found, 0)) ? PORT_STATE_OFF : PORT_STATE_NA;
      for (int i = 0; i < _mcp_output_pins; i++)
      {
        _update_output(TYPE_FRAME, index+i, state);
        _update_output(TYPE_STATE, index+i, state);
      }
      mcps_found >>= 1;
    }  
  }
  
  // handle input/output configuration (smoke detector)
  if (_port_layout == PORT_LAYOUT_IO_48)
  {    
    // configure outline
    _layout_config.x = 0;
    _layout_config.y = 137;
    _layout_config.xo = 10;
    _layout_config.bw = 27;
    _layout_config.bh = 33;
    _layout_config.index_max = 48;
    // draw outline as configured   
    for (int index = 1; index <= _layout_config.index_max; index += 16)
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

  // handle hybrid configurations
  if ((_port_layout / 1000) == 4)
  {
    // check for 8/16 MCP
    if ((_port_layout % 1000) >= 800)
    {
      _mcp_output_pins = 8;
    }

    // configure outline of input ports
    switch (_port_layout) 
    {
      case PORT_LAYOUT_IO_32_96:
      case PORT_LAYOUT_IO_32_96_8:
        _layout_config.x = 0;
        _layout_config.y = 115;
        _layout_config.xo = 25+47;
        _layout_config.bw = 23;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        break;
      case PORT_LAYOUT_IO_64_64:
      case PORT_LAYOUT_IO_64_64_8:
        _layout_config.x = 0;
        _layout_config.y = 115;
        _layout_config.xo = 25; 
        _layout_config.bw = 23;
        _layout_config.bh = 19;
        _layout_config.index_max = 64;
        break;
      case PORT_LAYOUT_IO_96_32:
      case PORT_LAYOUT_IO_96_32_8:
        _layout_config.x = 0;
        _layout_config.y = 115;
        _layout_config.xo = 2; 
        _layout_config.bw = 19;
        _layout_config.bh = 19;
        _layout_config.index_max = 96;
        break;
    }
    _layout_config_in = _layout_config;
    
    // draw outline as configured
    for (int index = 1; index <= _layout_config.index_max; index += 16)
    {
      int state = (bitRead(mcps_found, 0)) ? PORT_STATE_OFF : PORT_STATE_NA;
      for (int i = 0; i < 16; i++)
      {
        if ((i % 4) == 0)
        {
          _update_input(TYPE_FRAME, index+i, state);
        }
        _update_input(TYPE_STATE, index+i, state);
      }
      mcps_found >>= 1;
    }

    // configure outline output ports
    switch (_port_layout) 
    {
      case PORT_LAYOUT_IO_32_96:
        _layout_config.x = 0;
        _layout_config.y = 158;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 96;
        frame_h = _layout_config.bh * 3 + 8;
        _mcp_output_start = 2;
        break;
      case PORT_LAYOUT_IO_64_64:
        _layout_config.x = 0;
        _layout_config.y = 168;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 64;
        frame_h = _layout_config.bh * 2 + 6;
        _mcp_output_start = 4;
        break;
      case PORT_LAYOUT_IO_96_32:
        _layout_config.x = 0;
        _layout_config.y = 178;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        frame_h = _layout_config.bh + 4;
        _mcp_output_start = 6;
        break;
        
      case PORT_LAYOUT_IO_32_96_8:
        _layout_config.x = 0;
        _layout_config.y = 168;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 64;
        frame_h = _layout_config.bh * 2 + 6;
        _mcp_output_start = 2;
        break;
      case PORT_LAYOUT_IO_64_64_8:
        _layout_config.x = 0;
        _layout_config.y = 178;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        frame_h = _layout_config.bh + 4;
        _mcp_output_start = 4;
        break;
      case PORT_LAYOUT_IO_96_32_8:
        _layout_config.x = 0;
        _layout_config.y = 178;
        _layout_config.xo = 4;
        _layout_config.bw = 8;
        _layout_config.bh = 19;
        _layout_config.index_max = 32;
        frame_h = _layout_config.bh + 4;
        _mcp_output_start = 6;
        break;
    }
    _layout_config_out = _layout_config;
    
    // draw outline as configured   
    tft.fillRect(0, _layout_config.y-2, 240, frame_h,  TFT_WHITE);
    for (int index = 1; index <= _layout_config.index_max; index += _mcp_output_pins)
    {
      int state = (bitRead(mcps_found, 0)) ? PORT_STATE_OFF : PORT_STATE_NA;
      for (int i = 0; i < _mcp_output_pins; i++)
      {
        _update_output(TYPE_FRAME, index+i, state);
        _update_output(TYPE_STATE, index+i, state);
      }
      mcps_found >>= 1;
    }
  }

  // fill bottom field with gray (event display space)
  _clear_event();
}

/*
 * process io_value :
 * check for changes vs last stored value
 * animate port display if change detected
 */
void OXRS_LCD::process(int mcp, uint16_t io_value)
{
  int i, index;
  uint16_t changed;
  int pin_count;
  
  // nothing to do if MCP wasn't found
  if (!bitRead(_mcps_found, mcp)) return;
   
  // check if io_values initialised, if not -> force display update
  if (!bitRead(_io_values_initialised, mcp))
  {
    changed = 0xffff;
    bitSet(_io_values_initialised, mcp);
  }
  // Compare with last stored value
  else
  {
    changed = io_value ^ _io_values[mcp];
  }
  
  // figure out index and pin_count
  if (changed)
  {
    if (_mcp_output_start > 7)
    // no splitted configuration
    {
      index = mcp * _mcp_output_pins;
      pin_count = _mcp_output_pins;
    }
    else
    {
      if (mcp < _mcp_output_start)
      // input mcps (16 pins)
      {
        index = mcp * 16;
        pin_count = 16;
      }
      else
      // output mcps / handle 8/16
      {
         index = _mcp_output_start * 16 + (mcp - _mcp_output_start) * _mcp_output_pins;
         pin_count = _mcp_output_pins;
      }
    }
    // walk thrue all inputs, call _update_... if change detected
    for (i = 0; i < pin_count; i++)
    {
      if (bitRead(changed, i))
      {
        _set_backlight(_brightness_on);
        _last_lcd_trigger = millis();
        switch (_port_layout) 
        {
          // input ports
          case PORT_LAYOUT_INPUT_32:
          case PORT_LAYOUT_INPUT_64:
          case PORT_LAYOUT_INPUT_96:
          case PORT_LAYOUT_INPUT_128:
            if (bitRead(_port_config, (index+i)/4))
            {
              _update_security(TYPE_STATE, index+i+1, (io_value >> (i & 0xfc)) & 0x000f ); 
            }
            else
            {
              _update_input(TYPE_STATE, index+i+1, bitRead(io_value, i) ? PORT_STATE_OFF : PORT_STATE_ON); 
            }
            break;
          // outport ports
          case PORT_LAYOUT_OUTPUT_32:
          case PORT_LAYOUT_OUTPUT_64:
          case PORT_LAYOUT_OUTPUT_96:
          case PORT_LAYOUT_OUTPUT_128:
          case PORT_LAYOUT_OUTPUT_32_8:
          case PORT_LAYOUT_OUTPUT_64_8:
            _update_output(TYPE_STATE, index+i+1, bitRead(io_value, i) ? PORT_STATE_ON : PORT_STATE_OFF); 
            break;
          // input and output ports mixed
          case PORT_LAYOUT_IO_48:
            if (index < 16)
            {
              _update_io_48(TYPE_STATE, index+i+1, bitRead(io_value, i) ? PORT_STATE_OFF : PORT_STATE_ON);
            } 
            else
            {
              _update_io_48(TYPE_STATE, index+i+1, bitRead(io_value, i) ? PORT_STATE_ON : PORT_STATE_OFF);
            }
            break;
          // hybrid ports
          case PORT_LAYOUT_IO_32_96:
          case PORT_LAYOUT_IO_64_64:
          case PORT_LAYOUT_IO_96_32:
          case PORT_LAYOUT_IO_32_96_8:
          case PORT_LAYOUT_IO_64_64_8:
          case PORT_LAYOUT_IO_96_32_8:
            if ((index+i+1) <= _layout_config_in.index_max)
            {
              _layout_config = _layout_config_in;
              _update_input(TYPE_STATE, index+i+1, bitRead(io_value, i) ? PORT_STATE_OFF : PORT_STATE_ON); 
            }
            else
            {
              _layout_config = _layout_config_out;
              _update_output(TYPE_STATE, (index+i+1) - _layout_config_in.index_max, bitRead(io_value, i) ? PORT_STATE_ON : PORT_STATE_OFF); 
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
 * flash timer for security port fault flashing expired
 */
void OXRS_LCD::loop(void)
{
  // flash timer on / off
  if ((millis() - _last_flash_trigger) > _flash_timer_ms)
  {
    _flash_on = !_flash_on;
    for (int i=0; i<32; i++)
    {
      if (bitRead(_ports_to_flash, i))
      {
        if (_flash_on)
        {
          _update_security(TYPE_STATE, i*4+1, (_io_values[i/4] >> ((i & 0x03) * 4) & 0x000f ));
        }
        else
        {
          _update_security(TYPE_FRAME, i*4+1, PORT_STATE_OFF);
        }
      }
    }
    _flash_timer_ms = (_flash_on) ? LCD_PORT_FLASH_ON_MS : LCD_PORT_FLASH_OFF_MS;
    _last_flash_trigger = millis();  
  }
    
  // Clear event display if timed out
  if (_ontime_event_ms && _last_event_display)
  {
    if ((millis() - _last_event_display) > _ontime_event_ms)
    {
      _clear_event();      
      _last_event_display = 0L;
    }
  }

  // Dim LCD if timed out
  if (_ontime_display_ms && _last_lcd_trigger)
  {
    if ((millis() - _last_lcd_trigger) > _ontime_display_ms)
    {
      _set_backlight(_brightness_dim);
      _last_lcd_trigger = 0L;
    }
  }
  
  // turn off rx LED if timed out
  if (_last_rx_trigger)
  {
    if ((millis() - _last_rx_trigger) > RX_TX_LED_ON)
    {
      _set_mqtt_rx_led(MQTT_STATE_UP);
      _last_rx_trigger = 0L;
    }
  }
 
  // turn off tx LED if timed out
  if (_last_tx_trigger)
  {
    if ((millis() - _last_tx_trigger) > RX_TX_LED_ON)
    {
      _set_mqtt_tx_led(MQTT_STATE_UP);
      _last_tx_trigger = 0L;
    }
  }
 
  // check if IP or MQTT state has changed
  _check_IP_state(_get_IP_state());
  _check_MQTT_state(_get_MQTT_state());
}

/*
 * control mqtt rx/tx virtual leds 
 */
void OXRS_LCD::trigger_mqtt_rx_led(void)
{
  _set_mqtt_rx_led(MQTT_STATE_ACTIVE);
  _last_rx_trigger = millis(); 
}

void OXRS_LCD::trigger_mqtt_tx_led(void)
{
  _set_mqtt_tx_led(MQTT_STATE_ACTIVE);
  _last_tx_trigger = millis(); 
}

void OXRS_LCD::show_temp(float temperature, char unit)
{
  char buffer[30];
  
  tft.fillRect(0, 95, 240, 13,  TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
  sprintf(buffer, "TEMP: %2.1f %c", temperature, unit);
  tft.drawString(buffer, 12, 95);
}

/*
 * draw event on bottom line of screen
 */
void OXRS_LCD::show_event(const char * s_event)
{
  // Show last input event on bottom line
  tft.fillRect(0, 225, 240, 15,  TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FMB9);       // Select Free Mono Bold 9
  tft.drawString(s_event, 0, 225);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  _last_event_display = millis(); 
}

void OXRS_LCD::_clear_event()
{
  tft.fillRect(0, 225, 240, 240,  TFT_DARKGREY);
}

byte * OXRS_LCD::_get_MAC_address(byte * mac)
{
  if (_ethernet)
  {
    _ethernet->MACAddress(mac);
    return mac;
  }

  if (_wifi)
  {
    _wifi->macAddress(mac);
    return mac;
  }
  
  memset(mac, 0, 6);
  return mac;
}

IPAddress OXRS_LCD::_get_IP_address(void)
{
  if (_get_IP_state() == IP_STATE_UP)
  {
    if (_ethernet)
    {
      return _ethernet->localIP();
    }

    if (_wifi)
    {
      return _wifi->localIP();
    }
  }
  
  return IPAddress(0, 0, 0, 0);
}

int OXRS_LCD::_get_IP_state(void)
{
  if (_ethernet)
  {
    return _ethernet->linkStatus() == LinkON ? IP_STATE_UP : IP_STATE_DOWN;
  }
  
  if (_wifi)
  {
    return _wifi->status() == WL_CONNECTED ? IP_STATE_UP : IP_STATE_DOWN;
  }

  return IP_STATE_UNKNOWN;
}

void OXRS_LCD::_check_IP_state(int state)
{
  if (state != _ip_state)
  {
    _ip_state = state;

    // refresh IP address on state change
    IPAddress ip = _get_IP_address();
    _show_IP(ip);

    // refresh MAC on state change
    byte mac[6];
    _show_MAC(_get_MAC_address(mac));

    // update the link LED after refreshing IP address
    // since that clears that whole line on the screen
    _set_ip_link_led(_ip_state);
    
    // if the link is up check we actually have an IP address
    // since DHCP might not have issued an IP address yet
    if (_ip_state == IP_STATE_UP && ip[0] == 0)
    {
      _ip_state = IP_STATE_DOWN;
    }
  }
}

void OXRS_LCD::_show_IP(IPAddress ip)
{
  // clear anything already displayed
  tft.fillRect(0, 50, 240, 13, TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);
 
  char buffer[30];
  if (ip[0] == 0)
  {
    sprintf(buffer, "  IP: ---.---.---.---");
  }
  else
  {
    sprintf(buffer, "  IP: %03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  }
  tft.drawString(buffer, 12, 50);
  
  if (_wifi)
  {
    tft.drawBitmap(13, 51, icon_wifi, 11, 10, TFT_BLACK, TFT_WHITE);
  }
  if (_ethernet)
  {
    tft.drawBitmap(13, 51, icon_ethernet, 11, 10, TFT_BLACK, TFT_WHITE);
  }
}

void OXRS_LCD::_show_MAC(byte mac[])
{  
  // clear anything already displayed
  tft.fillRect(0, 65, 240, 13, TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);

  char buffer[30];
  sprintf(buffer, " MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  tft.drawString(buffer, 12, 65);
}

int OXRS_LCD::_get_MQTT_state(void)
{
  if (_get_IP_state() == IP_STATE_UP)
  {
    return _mqtt->connected() ? MQTT_STATE_UP : MQTT_STATE_DOWN;
  }

  return MQTT_STATE_UNKNOWN;
}

void OXRS_LCD::_check_MQTT_state(int state)
{
  if (state != _mqtt_state)
  {
    _mqtt_state = state;

    // don't show any topic if we are in an unknown state
    if (_mqtt_state == MQTT_STATE_UNKNOWN)
    {
      _show_MQTT_topic("-/------");
    }
    else
    {
      char topic[64];
      _show_MQTT_topic(_mqtt->getWildcardTopic(topic));
    }

    // update the activity LEDs after refreshing MQTT topic
    // since that clears that whole line on the screen
    _set_mqtt_tx_led(_mqtt_state);
    _set_mqtt_rx_led(_mqtt_state);
    
    // ensure any activity timers don't reset the LEDs
    _last_tx_trigger = 0L;
    _last_rx_trigger = 0L;    
  }
}

void OXRS_LCD::_show_MQTT_topic(const char * topic)
{
  // clear anything already displayed
  tft.fillRect(0, 80, 240, 13, TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&Roboto_Mono_Thin_13);

  char buffer[30];
  strcpy(buffer, "MQTT: ");
  strncat(buffer, topic, sizeof(buffer)-strlen(buffer)-1);
  tft.drawString(buffer, 12, 80);
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
void OXRS_LCD::_update_input(uint8_t type, uint8_t index, int state)
{
  // OFF, ON, NA
  uint16_t color_map[3] = {tft.color565(130,130,130), TFT_YELLOW, tft.color565(60,60,60)};
  int bw =  _layout_config.bw;
  int bh =  _layout_config.bh;
  int xo =  _layout_config.xo;
  int x =   _layout_config.x;
  int y =   _layout_config.y;
  int port;
  uint16_t color;

  if (index > _layout_config.index_max) return;
  
  index -= 1;
  port = index / 4;
  y = y + (port % 2) * bh;
  if (    _port_layout == PORT_LAYOUT_INPUT_96 
      ||  _port_layout == PORT_LAYOUT_IO_96_32 
      ||  _port_layout == PORT_LAYOUT_IO_96_32_8 )
  {
    xo = xo + (port / 8) * 3;
    x = xo + (port / 2) * bw;
  }
  else
  {
    if (port > 15) {y = y + 2 * bh + 3;}
    xo = xo + ((port % 16) / 8) * 3;
    x = xo + ((port % 16) / 2) * bw;
  }
  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = (state != PORT_STATE_NA) ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
  }
  else
  // draw virtual led in port
  {
    color = color_map[state];
    switch (index % 4)
    {
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
  // Security sensor logic table (using our internal state constants)
  //
  // Sensor     CH4   CH3   CH2   CH1       State           Event
  // -------------------------------------------------------------------
  // NORMAL     ON    OFF   ON    OFF   =>  IS_HIGH         HIGH_EVENT          B00000101   GREEN
  // ALARM      ON    ON    ON    OFF   =>  IS_LOW          LOW_EVENT           B00000001   RED
  // TAMPER     ON    ON    OFF   ON    =>  DEBOUNCE_LOW    TAMPER_EVENT        B00000010   MAGENTA
  // SHORT      OFF   OFF   ON    OFF   =>  DEBOUNCE_HIGH   SHORT_EVENT         B00001101   MAGENTA
  //                                                        FAULT_EVENT         all other   CYAN
**/
void OXRS_LCD::_update_security(uint8_t type, uint8_t index, int state)
{
  // NORMAL, ALARM, TAMPER or SHORT, NC, FAULT
  uint16_t color_map[4] = {TFT_GREEN, TFT_RED, TFT_MAGENTA, TFT_CYAN};
  int bw =  _layout_config.bw;
  int bh =  _layout_config.bh;
  int xo =  _layout_config.xo;
  int x =   _layout_config.x;
  int y =   _layout_config.y;
  int port;
  uint16_t color;
  bool  flash;

  if (index > _layout_config.index_max) return;

  index -= 1;
  port = index / 4;
  y = y + (port % 2) * bh;
  if (    _port_layout == PORT_LAYOUT_INPUT_96 
      ||  _port_layout == PORT_LAYOUT_IO_96_32 
      ||  _port_layout == PORT_LAYOUT_IO_96_32_8 )
  {
    xo = xo + (port / 8) * 3;
    x = xo + (port / 2) * bw;
  }
  else
  {
    if (port > 15) {y = y + 2 * bh + 3;}
    xo = xo + ((port % 16) / 8) * 3;
    x = xo + ((port % 16) / 2) * bw;
  }

  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = (state != PORT_STATE_NA) ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
    tft.fillRoundRect(x+2, y+2, bw-4, bh-4, 3, TFT_DARKGREY);
  }
  else
  // draw virtual led in port
  {
    switch (state)
    {
      case (B00000101):
        color = color_map[0]; 
        flash = false;
        break;
      case (B00000001):
        color = color_map[1]; 
        flash = false;
        break;
      case (B00000010):
      case (B00001101):
        color = color_map[2]; 
        flash = true;
        break;
      default:
        color = color_map[3]; 
        flash = true;
    }
    if (state == 0xff) color = TFT_DARKGREY;
    tft.fillRoundRect(x+2, y+2, bw-4, bh-4, 3, color);
    bitWrite(_ports_to_flash, port, flash);
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
void OXRS_LCD::_update_output(uint8_t type, uint8_t index, int state)
{  
  int bw =  _layout_config.bw;
  int bh =  _layout_config.bh;
  int xo =  _layout_config.xo;
  int x =   _layout_config.x;
  int y =   _layout_config.y;
  uint16_t color;
  int index_mod;

  if (index > _layout_config.index_max) return;

  index -= 1;
  index_mod = index % 32;
  
  y = y + ((index / 32) * (bh + 2));
  if (_mcp_output_pins == 16)
  {
    xo = xo + ((index_mod / 8) * 2) + ((index_mod / _mcp_output_pins) * 2);
  }
  else
  {
    xo = xo + ((index_mod / 8) * 2);
  }
  x = xo + (index_mod * (bw - 1));

  if (type == TYPE_FRAME)
  // draw port frame
  {
    color = (state != PORT_STATE_NA) ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
  }
  else
  // draw virtual led in port
  {
    tft.fillRect(x+1, y+1, bw-2, bh-2, TFT_BLACK);
    switch (state) 
    {
      case PORT_STATE_NA:
        tft.drawRect(x+2, y+bh/2+2, bw-4, bh/2-4,  TFT_DARKGREY);
        break;
      case PORT_STATE_OFF:
        tft.fillRect(x+2, y+bh/2+2, bw-4, bh/2-4,  TFT_LIGHTGREY);
        break;
      case PORT_STATE_ON:
        tft.fillRect(x+1, y+1,      bw-2, bh-2,  TFT_RED);
        break;
    }
  }     
}

/**
  animation of input and output state in ports view
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
void OXRS_LCD::_update_io_48(uint8_t type, uint8_t index, int state)
{  
  int bw =  _layout_config.bw;
  int bh =  _layout_config.bh;
  int xo =  _layout_config.xo;
  int x =   _layout_config.x;
  int y =   _layout_config.y;
  int bht = bh-bw/2;
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
    color = (state != PORT_STATE_NA) ? TFT_WHITE : TFT_DARKGREY;
    tft.drawRect(x, y, bw, bh, color);
    tft.drawRect(x, y, bw/2+1, bht, color);
    tft.drawRect(x+bw/2, y, bw/2+1, bht, color);
  }
  else
  // draw virtual led in port
  {
    switch (index % 3)
    {
      case 0:
        color = (state == PORT_STATE_ON) ? TFT_RED : TFT_DARKGREY; 
        tft.fillRect(x+1     , y+1      , bw/2-1, bht-2, color);
        break;
      case 1:
        color = (state == PORT_STATE_ON) ? TFT_RED : TFT_DARKGREY; 
        tft.fillRect(x+1+bw/2, y+1      , bw/2-1, bht-2, color);
        break;
      case 2:
        color = (state == PORT_STATE_ON) ? TFT_YELLOW : TFT_DARKGREY;
        tft.fillRoundRect(x+2     , y+bht+1 , bw/2-3, bh-bht-3, 3, color);
        break;
    }
  }     
}

/*
 * set backlight of LCD (val in % [0..100])
 */
void OXRS_LCD::_set_backlight(int val)
{
  ledcWrite(BL_PWM_CHANNEL, 255*val/100); 
}

/*
 * animated "leds"
 */
void OXRS_LCD::_set_ip_link_led(int state)
{
  // UP, DOWN, UNKNOWN
  uint16_t color[3] = {TFT_GREEN, TFT_RED, TFT_BLACK};
  if (state < 3) tft.fillRoundRect(2, 54, 8, 5, 2, color[state]);
}

void OXRS_LCD::_set_mqtt_rx_led(int state)
{
  // UP, ACTIVE, DOWN, UNKNOWN
  uint16_t color[4] = {TFT_GREEN, TFT_YELLOW, TFT_RED, TFT_BLACK};  
  if (state < 4) tft.fillRoundRect(2, 80, 8, 5, 2, color[state]);
}

void OXRS_LCD::_set_mqtt_tx_led(int state)
{
  // UP, ACTIVE, DOWN, UNKNOWN
  uint16_t color[4] = {TFT_GREEN, TFT_ORANGE, TFT_RED, TFT_BLACK};
  if (state < 4) tft.fillRoundRect(2, 88, 8, 5, 2, color[state]);
}

/*
 * Bodmers BMP image rendering function
 */
// render logo from file in SPIFFS
bool OXRS_LCD::_drawBmp(const char *filename, int16_t x, int16_t y, int16_t bmp_w, int16_t bmp_h) 
{
  uint32_t  seekOffset;
  uint16_t  w, h, row, col;
  uint8_t   r, g, b;


  if (!SPIFFS.begin())
    return false;

  File file = SPIFFS.open(filename, "r");

  if (!file) 
    return false;  

  if (file.size() == 0) 
    return false;

  if (_read16(file) == 0x4D42)
  {
    _read32(file);
    _read32(file);
    seekOffset = _read32(file);
    _read32(file);
    w = _read32(file);
    h = _read32(file);

    if ((_read16(file) == 1) && (_read16(file) == 24) && (_read32(file) == 0))
    {
      // crop to bmp_h
      y += bmp_h - 1;

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
        // crop to bmp_w
        tft.pushImage(x, y--, bmp_w, 1, (uint16_t*)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);

      file.close();
      return true;
    }
  }
  
  file.close();
  return false;
}

// These read 16- and 32-bit types from the SPIFFS file
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

// render logo from array in PROGMEM
bool OXRS_LCD::_drawBmp_P(const uint8_t *image, int16_t x, int16_t y, int16_t bmp_w, int16_t bmp_h) 
{
  uint32_t  seekOffset;
  uint16_t  w, h, row, col;
  uint8_t   r, g, b;
  uint8_t*  ptr;

  ptr = (uint8_t*)image;

  if (_read16_P(&ptr) == 0x4D42)
  {
    _read32_P(&ptr);
    _read32_P(&ptr);
    seekOffset = _read32_P(&ptr);
    _read32_P(&ptr);
    w = _read32_P(&ptr);
    h = _read32_P(&ptr);

    if ((_read16_P(&ptr) == 1) && (_read16_P(&ptr) == 24) && (_read32_P(&ptr) == 0))
    {
      // crop to bmp_h
      y += bmp_h - 1;

      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      ptr = (uint8_t*)image + seekOffset;

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++)
      {
        memcpy_P(lineBuffer, ptr, sizeof(lineBuffer));
        ptr += sizeof(lineBuffer);
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
        // crop to bmp_w
        tft.pushImage(x, y--, bmp_w, 1, (uint16_t*)lineBuffer);
      }

      tft.setSwapBytes(oldSwapBytes);
      return true;
    }
  }

  return false;
}

// These read 16- and 32-bit types from PROGMEM.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t OXRS_LCD::_read16_P(uint8_t** p) 
{
  uint16_t result;
   
  memcpy_P((uint8_t *)&result, *p, 2);
  *p += 2;
  return result;
}

uint32_t OXRS_LCD::_read32_P(uint8_t** p) 
{
  uint32_t result;
  
  memcpy_P((uint8_t *)&result, *p, 4);
  *p += 4;
  return result;
}
