#ifndef Main_h
#define Main_h

#include <arduino.h>

//DomoChip Informations
//------------Compile for 1M 64K SPIFFS------------
//Configuration Web Pages :
//http://IP/
//http://IP/config
//http://IP/fw

//include Application header file
#include "WirelessInput.h"

#define APPLICATION1_NAME "WInput"
#define APPLICATION1_DESC "DomoChip Wireless Input"
#define APPLICATION1_CLASS WebInput

#define VERSION_NUMBER "1.2.2"

#define DEFAULT_AP_SSID "WirelessInput"
#define DEFAULT_AP_PSK "PasswordInput"

//FOR ESP-01 : reduced amount of GPIO and the fact that they need to be up at start, enforce to control signal arrival
//(I cabled GPIO0 instead of GND to the Mosfet with a pull-up resistor, that way, signal comes only if GPIO0 go to GND)
#define SIGNAL_CONTROL_PIN 0

//Signal pin
//(if using RX(3) pin, Serial will be stopped and restarted for reading)
#define SIGNAL_PIN 3

//Enable developper mode (fwdev webpage and SPIFFS is used)
#define DEVELOPPER_MODE 0

//Choose Serial Speed
#define SERIAL_SPEED 115200

//Choose Pin used to boot in Rescue Mode
#define RESCUE_BTN_PIN 2

//Status LED
//#define STATUS_LED_SETUP pinMode(XX, OUTPUT);pinMode(XX, OUTPUT);
//#define STATUS_LED_OFF digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_ERROR digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_WARNING digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);
//#define STATUS_LED_GOOD digitalWrite(XX, HIGH);digitalWrite(XX, HIGH);

//construct Version text
#if DEVELOPPER_MODE
#define VERSION VERSION_NUMBER "-DEV"
#else
#define VERSION VERSION_NUMBER
#endif

#endif

