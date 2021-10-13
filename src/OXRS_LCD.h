#ifndef OXRS_LCD_H
#define OXRS_LCD_H

#include <OXRS_MQTT.h>
#include <Ethernet.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#define SPIFFS LittleFS
#else
#include <WiFi.h>
#include <SPIFFS.h>
#endif

#define     TYPE_FRAME              0
#define     TYPE_STATE              1

#define     PORT_LAYOUT_INPUT_96    1096
#define     PORT_LAYOUT_INPUT_128   1128
#define     PORT_LAYOUT_OUTPUT_128  2128
#define     PORT_LAYOUT_IO_48       3048

#define     LCD_BL_ON               100       // LCD backlight in % when ON, i.e. after an event
#define     LCD_BL_DIM              10        // LCD backlight in % when DIMMED (0 == OFF), i.e. after LCD_ON_MS expires
#define     LCD_ON_MS               10000     // How long to turn on the LCD after an event
#define     LCD_EVENT_MS            3000      // How long to display an event in the bottom line
#define     RX_TX_LED_ON            300       // How long to turn mqtt rx/tx led on after trgger

// LCD backlight control
// TFT_BL GPIO pin defined in user_setup.h of tft_eSPI
// setting PWM properties
#define     BL_PWM_FREQ             5000
#define     BL_PWM_CHANNEL          0
#define     BL_PWM_RESOLUTION       8

// IP link states
#define     IP_STATE_UP             0
#define     IP_STATE_DOWN           1
#define     IP_STATE_UNKNOWN        2

// MQTT led states
#define     MQTT_STATE_IDLE         0
#define     MQTT_STATE_ACTIVE       1
#define     MQTT_STATE_DOWN         2
#define     MQTT_STATE_UNKNOWN      3

class OXRS_LCD
{
  public:
    OXRS_LCD(EthernetClass& ethernet, OXRS_MQTT& mqtt);
    OXRS_LCD(WiFiClass& wifi, OXRS_MQTT& mqtt);
    
    void draw_header(
      const char * fwShortName, 
      const char * fwMaker, 
      const char * fwVersion, 
      const char * fwPlatform, 
      const uint8_t * fwLogo = NULL);
    void draw_ports (int port_layout, uint8_t mcps_found);

    void begin (uint32_t ontime_event=LCD_EVENT_MS, uint32_t ontime_display=LCD_ON_MS);
    void process (int mcp, uint16_t io_value);
    void loop(void);
    
    void trigger_mqtt_rx_led (void);
    void trigger_mqtt_tx_led (void);
    
    void show_temp (float temperature);
    void show_event (const char * s_event);
    
  private:  
    // for timeout (clear) of bottom line input event display
    uint32_t _last_event_display = 0L;
    
    // for timeout (dim) of LCD
    uint32_t _last_lcd_trigger = 0L;
    uint32_t _last_tx_trigger = 0L;
    uint32_t _last_rx_trigger = 0L;

    uint32_t _ontime_display;
    uint32_t _ontime_event;

    EthernetClass * _ethernet;
    WiFiClass *     _wifi;
    int             _ip_state = -1;
    
    OXRS_MQTT *     _mqtt;
    int             _mqtt_state = -1;
    
    // defines how i/o ports are displayed and animated
    int _port_layout;
     
   // history buffer of io_values to extract changes
    uint16_t _io_values[8];
    
    void _clear_event(void);
    
    byte * _get_MAC_address(byte * mac);
    IPAddress _get_IP_address(void);

    int _get_IP_state(void);    
    void _check_IP_state(int state);
    void _show_IP(IPAddress ip);
    void _show_MAC(byte mac[]);

    int _get_MQTT_state(void);
    void _check_MQTT_state(int state);
    void _show_MQTT_topic(const char * topic);

    void _update_input_96(uint8_t type, uint8_t index, int active);
    void _update_input_128(uint8_t type, uint8_t index, int active);
    void _update_output_128(uint8_t type, uint8_t index, int active);
    void _update_io_48(uint8_t type, uint8_t index, int active);

    void _set_backlight(int val);
    void _set_ip_link_led(int state);
    void _set_mqtt_rx_led(int state);
    void _set_mqtt_tx_led(int state);

    bool _drawBmp(const char *filename, int16_t x, int16_t y, int16_t bmp_w, int16_t bmp_h);
    uint16_t _read16(File &f);
    uint32_t _read32(File &f);   

    bool _drawBmp_P(const uint8_t *image, int16_t x, int16_t y, int16_t bmp_w, int16_t bmp_h);
    uint16_t _read16_P(uint8_t** p);
    uint32_t _read32_P(uint8_t** p);   

};


#endif
