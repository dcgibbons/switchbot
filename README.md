jeepbot
=======

Jeep Bot - an intelligent accessory control system for Jeep Wranglers

In this proof of concept, the sketch assumes the following hardware has been 
configured:

  * An Arduino board with hardware SPI (any)
  * A Microchip mcp2515 CAN controller interface, such as the CAN-BUS
    shield from https://www.sparkfun.com/products/10039, wired to SPI
  * A SN74HC595 or compatible 8-bit shift register wired to SPI
  * An eight-or-more channel relay module, such as this one from http://www.sainsmart.com/8-channel-dc-5v-relay-module-for-arduino-pic-arm-dsp-avr-msp430-ttl-logic.html, that can be driven directly from GPIO logic

