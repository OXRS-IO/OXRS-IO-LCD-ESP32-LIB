
#ifndef OXRS_LCD_H
#define OXRS_LCD_H

#include <TFT_eSPI.h>               // Hardware-specific library
#include "bitmaps.h"                // images bitmaps
#include "Free_Fonts.h"             // Include the header file attached to this sketch
#include "roboto_fonts.h"
#include <Ethernet.h>
#include <WiFi.h>

#define TYPE_FRAME 0
#define TYPE_STATE 1

#define PORT_LAYOUT_INPUT_96          1096
#define PORT_LAYOUT_INPUT_128         1128
#define PORT_LAYOUT_OUTPUT_128        2128

#define       LCD_BL_ON               100                   // LCD backlight in % when ON, i.e. after an event
#define       LCD_BL_DIM              10                    // LCD backlight in % when DIMMED (0 == OFF), i.e. after LCD_ON_MS expires
#define       LCD_ON_MS               10000                 // How long to turn on the LCD after an event
#define       LCD_EVENT_MS            3000                  // How long to display an event in the bottom line
#define       RX_TX_LED_ON            300                   // How long to turn mqtt rx/tx led on after trgger

// LCD backlight control
// TFT_BL GPIO pin defined in user_setup.h of tft_eSPI
// setting PWM properties
#define BL_PWM_FREQ         5000
#define BL_PWM_CHANNEL      0
#define BL_PWM_RESOLUTION   8


class OXRS_LCD
{
  public:
    OXRS_LCD(EthernetClass& ethernet);
    OXRS_LCD(WiFiClass& wifi);
    void begin (uint32_t ontime_event=LCD_EVENT_MS, uint32_t ontime_display=LCD_ON_MS);
    void draw_header(char * fw_maker_code, char * fw_name, char * fw_version,  char * fw_platform );
    void draw_ports (int port_layout, uint8_t mcps_found);
    void show_MQTT_topic (char * topic);
    void show_temp (float temperature);
    void show_event (char * s_event);
    void process (int mcp, uint16_t io_value);
    void loop(void);
    void trigger_mqtt_rx_led (void);
    void trigger_mqtt_tx_led (void);

    
  private:  
    // for timeout (clear) of bottom line input event display
    uint32_t _last_event_display;
    
    // for timeout (dim) of LCD
    uint32_t _last_lcd_trigger;
    uint32_t _last_tx_trigger;
    uint32_t _last_rx_trigger;

    uint32_t _ontime_display;
    uint32_t _ontime_event;

    EthernetClass * _ethernet;
    int             _ethernet_link_status;
    WiFiClass *     _wifi;
    int             _wifi_connection_status;

    // defines how i/o ports are displayed and animated
    int _port_layout;
     
   // history buffer of io_values to extract changes
    uint16_t _io_values[8];
    
    // LCD
    TFT_eSPI tft = TFT_eSPI();   // Invoke library

    void _oxrs_lcd (void);
    void _show_ethernet(void);
    void _show_wifi(void);
    void _show_IP(IPAddress ip, int link_status);
    void _show_MAC(byte mac[]);
    void _update_input_96(uint8_t type, uint8_t index, int active);
    void _update_input_128(uint8_t type, uint8_t index, int active);
    void _update_output_128(uint8_t type, uint8_t index, int active);
    void _clear_event(void);
    void _set_backlight(int val);
    void _set_mqtt_rx_led(int active);
    void _set_mqtt_tx_led(int active);
    void _set_ip_link_led(int active);
};


#endif
