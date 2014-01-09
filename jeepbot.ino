/*
 * Jeep Bot - Proof of Concept 1
 *
 * In this proof of concept, we will use an Arduino MCU to drive a
 * relay board based on messages the MCU receives via a Jeep Wrangler's
 * CAN-IHS (Interior High-Speed) bus.
 *
 * These message ids work on 2012 model year Jeep Wranglers, and will
 * likely work for 2011 to 2014 model years, but not the 2007 to 2010
 * model years.
 *
 * Copyright (C) 2014 Nuclear Bunny Studios, LLC
 * All Rights Reserved
 *
 * See the file LICENSE for copying permission.
 */

#include <Arduino.h>
#include <SPI.h>

#include <AVRDebug.h>                      // https://github.com/dcgibbons/AVRDebug
#include <Canbus.h>                        // http://skpang.googlecode.com/files/Canbus_v4.zip
#include <mcp2515.h>                       // http://skpang.googlecode.com/files/Canbus_v4.zip

#define MCP2515_CS_PIN              10     // the chip-select pin to tell the mcp2515 to listen to SPI
#define MCP2515_INT_PIN             2      // the mcp2515 has a general-purpose interrupt to notify us

#define SHIFTER_CS_PIN              3      // the chip-select pin to tell the 595 shift register to listen to SPI

#define LED2_PIN                    8      // the CAN-Bus shield has two extra LEDs onboard
#define LED3_PIN                    7

#define CANSPEED_500	            1
#define CANSPEED_250                3
#define CANSPEED_125 	            7

#define DEBUG_SPEED                 9600

/* CAN Message IDs for 2012 Jeep JK (Wrangler) */
#define MSGID_EXT_LIGHTS            0x208
#define MSGID_POWER_STATUS          0x20b
#define MSGID_RADIO_SAT_STATION     0x295

/* Switch Settings */
#define SWITCH_ALWAYS_ON            0x01
#define SWITCH_RUN_ONLY             0x02
#define SWITCH_LIGHTS_ON            0x03
#define SWITCH_HIGHBEAMS_ON         0x04

uint8_t switch_settings[] =
{
  SWITCH_HIGHBEAMS_ON,              // switch1
  SWITCH_RUN_ONLY,                  // switch2
  SWITCH_ALWAYS_ON,                 // switch3
  SWITCH_ALWAYS_ON,                 // switch4
  SWITCH_ALWAYS_ON,                 // switch5
  SWITCH_ALWAYS_ON,                 // switch6
  SWITCH_ALWAYS_ON,                 // switch7
  SWITCH_ALWAYS_ON                  // switch8
};

#define MAX_SWITCHES                (sizeof(switch_settings)/sizeof(switch_settings[0]))

uint8_t switches = 0x00;

void setup_switches()
{
  for (int i = 0; i < MAX_SWITCHES; i++)
  {
    if (switch_settings[i] == SWITCH_ALWAYS_ON)
    {
      switches |= (1 << i);
    }
  }
  digitalWrite(SHIFTER_CS_PIN, LOW);
  SPI.transfer(switches);
  digitalWrite(SHIFTER_CS_PIN, HIGH);
  PRINT("Initial switch settings: 0x%08x\n", switches);
}

void setup_canbus()
{
  for (int tries = 0; tries < 10; tries++)
  {  
    PRINT("Attempt #%d to initialize mcp2515 CAN-Bus controller\n", tries+1);

    char ret = mcp2515_init(CANSPEED_125);
    if (ret)
    {
      PRINT("mcp2515 CAN-Bus controller successfully initialized\n");
      digitalWrite(LED2_PIN, HIGH);
      break;
    }
    else
    {
      PRINT("mcp2515 CAN-Bus controller could not initialize\n");
    }
    delay(250);
  }
}

void setup() 
{
  Debug.begin(DEBUG_SPEED);

  SPI.begin();

  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED2_PIN, LOW);

  pinMode(LED3_PIN, OUTPUT);
  digitalWrite(LED3_PIN, LOW);

  pinMode(MCP2515_CS_PIN, OUTPUT);
  digitalWrite(MCP2515_CS_PIN, HIGH);
  pinMode(SHIFTER_CS_PIN, OUTPUT);
  digitalWrite(SHIFTER_CS_PIN, HIGH);

  setup_switches();
  setup_canbus();
}

void loop() 
{
  if (mcp2515_check_message())
  {
    tCAN message;
    if (mcp2515_get_message(&message))
    {
      digitalWrite(LED3_PIN, HIGH);

      uint8_t new_switches = switches;

      switch (message.id)
      {
      case MSGID_POWER_STATUS:
        for (int i = 0; i < MAX_SWITCHES; i++)
        {
          if (switch_settings[i] == SWITCH_RUN_ONLY)
          {
            uint8_t run = message.data[0] == 0x61; // TODO: 61 seems to only be Accessory on not Ignition on
            if (run != ((new_switches & (1 << i)) >> i))
            {
              new_switches ^= 1 << i;
            }
          }
        }
        break;

      case MSGID_EXT_LIGHTS:
        for (int i = 0; i < MAX_SWITCHES; i++)
        {
          if (switch_settings[i] == SWITCH_HIGHBEAMS_ON)
          {
            if (((message.data[0] & 0x20) >> 5) != ((new_switches & (1 << i)) >> i))
            {
              new_switches ^= 1 << i;
            }
          }
        }
        break; 
      }

      if (new_switches != switches)
      {
        char buffer[10];
        for (int i = 0; i < MAX_SWITCHES; i++)
        {
          if ((new_switches & (1 << i)) != (switches & (1 << i)))
          {
            sprintf(buffer, "AUX%1d %s", i+1, (new_switches & (1 << i)) ? "ON" : "OFF");
            PRINT("%s\n", buffer);
          }
        }
        PRINT("New switch settings: 0x%08x\n", new_switches);

        // Send a switch update to the EVIC
        // TODO: this only grabs the last message we build, so be smarter about this
        // TODO: this requires the radio to be on SAT setting to get the EVIC to display, make that
        // happen even if the radio isn't tuned there
        // TODO: after a few seconds, turn off the message and clear the EVIC or display the last
        // message the car itself sent
        memset(&message, 0, sizeof(message));
        message.id = MSGID_RADIO_SAT_STATION;
        message.header.length = 8;
        memcpy(message.data, buffer, 8);
        if (!mcp2515_send_message(&message))
        {
          PRINT("Unable to write CAN-bus message\n");
        }
        
        // Send an update to the shift register that in turns drives the relays
        switches = new_switches;
        digitalWrite(SHIFTER_CS_PIN, LOW);
        SPI.transfer(switches);
        digitalWrite(SHIFTER_CS_PIN, HIGH);
      }     
    }
  }
  digitalWrite(LED3_PIN, LOW);
}

