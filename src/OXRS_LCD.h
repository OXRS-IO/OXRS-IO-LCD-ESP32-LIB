/*
 * OXRS_LCD.h
 * 
 */

#ifndef OXRS_LCD_H
#define OXRS_LCD_H

#include <TFT_eSPI.h>               // Hardware-specific library
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

#define     TYPE_FRAME                  0
#define     TYPE_STATE                  1

// port layout constants
#define     PORT_LAYOUT_INPUT_AUTO      1000
#define     PORT_LAYOUT_INPUT_32        1032
#define     PORT_LAYOUT_INPUT_64        1064
#define     PORT_LAYOUT_INPUT_96        1096
#define     PORT_LAYOUT_INPUT_128       1128
#define     PORT_LAYOUT_OUTPUT_AUTO     2000
#define     PORT_LAYOUT_OUTPUT_32       2032
#define     PORT_LAYOUT_OUTPUT_64       2064
#define     PORT_LAYOUT_OUTPUT_96       2096
#define     PORT_LAYOUT_OUTPUT_128      2128
#define     PORT_LAYOUT_OUTPUT_AUTO_8   2800
#define     PORT_LAYOUT_OUTPUT_32_8     2832
#define     PORT_LAYOUT_OUTPUT_64_8     2864
#define     PORT_LAYOUT_IO_48           3048
#define     PORT_LAYOUT_IO_32_96        4002
#define     PORT_LAYOUT_IO_64_64        4004
#define     PORT_LAYOUT_IO_96_32        4006
#define     PORT_LAYOUT_IO_32_96_8      4802
#define     PORT_LAYOUT_IO_64_64_8      4804
#define     PORT_LAYOUT_IO_96_32_8      4806

// port layout groups
#define     PORT_LAYOUT_GROUP_INPUT     1
#define     PORT_LAYOUT_GROUP_OUTPUT    2
#define     PORT_LAYOUT_GROUP_SMOKE     3
#define     PORT_LAYOUT_GROUP_HYBRID    4

// row start of info section
#define     Y_INFO                      50

// pin type config constants
#define     PIN_TYPE_DEFAULT            0
#define     PIN_TYPE_SECURITY           1

#define     LCD_BL_ON                   100       // LCD backlight in % when ON, i.e. after an event
#define     LCD_BL_DIM                  10        // LCD backlight in % when DIMMED (0 == OFF), i.e. after LCD_ON_MS expires
#define     LCD_ON_MS                   10000     // How long to turn on the LCD after an event
#define     LCD_EVENT_MS                3000      // How long to display an event in the bottom line
#define     RX_TX_LED_ON                300       // How long to turn mqtt rx/tx led on after trgger
#define     LCD_PORT_FLASH_ON_MS        700       // flash timer security port display
#define     LCD_PORT_FLASH_OFF_MS       300       // flash timer security port display

// LCD backlight control
// TFT_BL GPIO pin defined in user_setup.h of tft_eSPI
// setting PWM properties
#define     BL_PWM_FREQ                 5000
#define     BL_PWM_CHANNEL              0
#define     BL_PWM_RESOLUTION           8

// IP link states
#define     IP_STATE_UP                 0
#define     IP_STATE_DOWN               1
#define     IP_STATE_UNKNOWN            2

// MQTT led states
#define     MQTT_STATE_UP               0
#define     MQTT_STATE_ACTIVE           1
#define     MQTT_STATE_DOWN             2
#define     MQTT_STATE_UNKNOWN          3

// Port states
#define     PORT_STATE_OFF              0
#define     PORT_STATE_ON               1
#define     PORT_STATE_NA               2
#define     PORT_STATE_DISABLED         3

// return codes from draw_header
#define     LCD_INFO_LOGO_FROM_SPIFFS   101   // logo found on SPIFFS and displayed
#define     LCD_INFO_LOGO_FROM_PROGMEM  102   // logo found in PROGMEM and displayed
#define     LCD_INFO_LOGO_DEFAULT       103   // used default OXRS logo
#define     LCD_ERR_NO_LOGO             1     // no logo successfully rendered

typedef struct LAYOUT_CONFIG 
  {
    int x;
    int y;
    int xo;
    int bw;
    int bh;
    int index_max;
  } layout_config;
  
class OXRS_LCD
{
  public:
    OXRS_LCD(EthernetClass& ethernet, OXRS_MQTT& mqtt);
    OXRS_LCD(WiFiClass& wifi, OXRS_MQTT& mqtt);
    
    int drawHeader(const char * fwShortName, const char * fwMaker, const char * fwVersion, const char * fwPlatform, const uint8_t * fwLogo = NULL);
    void drawPorts(int port_layout, uint8_t mcps_found);

    void begin(void);
    void process(uint8_t mcp, uint16_t io_value);
    void loop(void);

    void triggerMqttRxLed(void);
    void triggerMqttTxLed(void);
    
    void hideTemp(void);
    void showTemp(float temperature, char unit = 'C');
    void showEvent(const char * s_event);
    
    void setBrightnessOn(int brightness_on);
    void setBrightnessDim(int brightness_dim);
    void setOnTimeDisplay(int ontime_display);
    void setOnTimeEvent(int ontime_event);

    void setPinType(uint8_t mcp, uint8_t pin, int type);
    void setPinInvert(uint8_t mcp, uint8_t pin, int invert);
    void setPinDisabled(uint8_t mcp, uint8_t pin, int disabled);
    
    void setIPpos(int yPos);
    void setMACpos(int yPos);
    void setMQTTpos(int yPos);
    void setTEMPpos(int yPos);
    
    TFT_eSPI* getTft(void);


  private:  
    // for timeout (clear) of bottom line input event display
    uint32_t _last_event_display = 0L;
    
    // for timeout (dim) of LCD
    uint32_t _last_lcd_trigger = 0L;
    uint32_t _last_tx_trigger = 0L;
    uint32_t _last_rx_trigger = 0L;

    uint32_t  _ontime_display_ms = LCD_ON_MS;
    uint32_t  _ontime_event_ms = LCD_EVENT_MS;
    int       _brightness_on = LCD_BL_ON;
    int       _brightness_dim = LCD_BL_DIM;

    // flash timer
    uint32_t  _flash_timer_ms = LCD_PORT_FLASH_ON_MS;
    uint32_t  _last_flash_trigger = 0L;
    bool      _flash_on = false;
    uint32_t  _ports_to_flash = 0L;
    
    // Y position (row) for info display members
    // 0 hides display
    int       _yIP    = Y_INFO;
    int       _yMAC   = Y_INFO + 15;
    int       _yMQTT  = Y_INFO + 30;
    int       _yTEMP  = Y_INFO + 45;
    
    
    EthernetClass * _ethernet;
    WiFiClass *     _wifi;
    int             _ip_state = -1;
    
    OXRS_MQTT *     _mqtt;
    int             _mqtt_state = -1;
    
    // defines how i/o ports are displayed and animated
    int             _port_layout;
    layout_config   _layout_config, _layout_config_in, _layout_config_out;
    int             _mcp_output_pins;
    int             _mcp_output_start;
     
   // history buffer of io_values to extract changes
    uint16_t _io_values[8];
    
    uint16_t _mcps_initialised = 0;
    int      _mcps_found;

    uint16_t _pin_type[8];
    uint16_t _pin_invert[8];
    uint16_t _pin_disabled[8];
    
    void _clear_event(void);
    
    byte * _get_MAC_address(byte * mac);
    IPAddress _get_IP_address(void);

    int  _get_IP_state(void);    
    void _check_IP_state(int state);
    void _show_IP(IPAddress ip);
    void _show_MAC(byte mac[]);

    int  _get_MQTT_state(void);
    void _check_MQTT_state(int state);
    void _show_MQTT_topic(const char * topic);

    void _check_port_flash(void);
    int  _getPortLayoutGroup(int port_layout);

    void _update_input(uint8_t type, uint8_t index, int state);
    void _update_output(uint8_t type, uint8_t index, int state);
    void _update_io_48(uint8_t type, uint8_t index, int state);
    void _update_security(uint8_t type, uint8_t index, int state);

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
