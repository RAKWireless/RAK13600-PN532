/**************************************************************************/
/*!
    @file     Adafruit_PN532.cpp
    @author   Adafruit Industries
    @license  BSD (see license.txt)
          Driver for NXP's PN532 NFC/13.56MHz RFID Transceiver
          This is a library for the Adafruit PN532 NFC/RFID breakout boards
          This library works with the Adafruit NFC breakout
          ----> https://www.adafruit.com/products/364
          Check out the links above for our tutorials and wiring diagrams
          These chips use SPI or I2C to communicate.
          Adafruit invests time and resources providing this open source code,
          please support Adafruit and open-source hardware by purchasing
          products from Adafruit!
    @section  HISTORY
    v2.2 - Added startPassiveTargetIDDetection() to start card detection and
            readDetectedPassiveTargetID() to read it, useful when using the
            IRQ pin.
    v2.1 - Added NTAG2xx helper functions
    v2.0 - Refactored to add I2C support from Adafruit_NFCShield_I2C library.
    v1.4 - Added setPassiveActivationRetries()
    v1.2 - Added writeGPIO()
         - Added readGPIO()
    v1.1 - Changed readPassiveTargetID() to handle multiple UID sizes
         - Added the following helper functions for text display
             static void PrintHex(const byte * data, const uint32_t numBytes)
             static void PrintHexChar(const byte * pbtData, const uint32_t
   numBytes)
         - Added the following Mifare Classic functions:
             bool mifareclassic_IsFirstBlock (uint32_t uiBlock)
             bool mifareclassic_IsTrailerBlock (uint32_t uiBlock)
             uint8_t mifareclassic_AuthenticateBlock (uint8_t * uid, uint8_t
   uidLen, uint32_t blockNumber, uint8_t keyNumber, uint8_t * keyData) uint8_t
   mifareclassic_ReadDataBlock (uint8_t blockNumber, uint8_t * data) uint8_t
   mifareclassic_WriteDataBlock (uint8_t blockNumber, uint8_t * data)
         - Added the following Mifare Ultalight functions:
             uint8_t mifareultralight_ReadPage (uint8_t page, uint8_t * buffer)
*/
/**************************************************************************/
/**
   @file NFC_PN532.h
   @author rakwireless.com
   @brief  This is a library for the PN532 NFC/RFID breakout boards
   			Based on Adafruits library, changed to work with RAKWireless RAK13600 module
   @version 1.0
   @date 2021-09-18

*/

#include "Arduino.h"

#include <Wire.h>
#ifdef __SAM3X8E__ // arduino due
#define WIRE Wire1
#else
#define WIRE Wire
#endif

#include <SPI.h>

#include "RAK13600-PN532.h"

byte pn532ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
byte pn532response_firmwarevers[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD5};

// Uncomment these lines to enable debug output for PN532(SPI) and/or MIFARE
// related code

// #define PN532DEBUG
// #define MIFAREDEBUG

// If using Native Port on Arduino Zero or Due define as SerialUSB
#define PN532DEBUGPRINT Serial
//#define PN532DEBUGPRINT SerialUSB

#define PN532_PACKBUFFSIZ 64
byte pn532_packetbuffer[PN532_PACKBUFFSIZ];

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

/**************************************************************************/
/*!
    @brief  Sends a single byte via I2C

    @param  x    The byte to send
*/
/**************************************************************************/
static inline void i2c_send(uint8_t x)
{
#if ARDUINO >= 100
  WIRE.write((uint8_t)x);
#else
  WIRE.send(x);
#endif
}

/**************************************************************************/
/*!
    @brief  Reads a single byte via I2C
*/
/**************************************************************************/
static inline uint8_t i2c_recv(void)
{
#if ARDUINO >= 100
  return WIRE.read();
#else
  return WIRE.receive();
#endif
}

/**************************************************************************/
/*!
    @brief  Instantiates a new PN532 class using software SPI.

    @param  clk       SPI clock pin (SCK)
    @param  miso      SPI MISO pin
    @param  mosi      SPI MOSI pin
    @param  ss        SPI chip select pin (CS/SSEL)
*/
/**************************************************************************/
NFC_PN532::NFC_PN532(uint8_t clk, uint8_t miso, uint8_t mosi,
                     uint8_t ss)
{
  spi_dev = new Adafruit_SPIDevice(ss, clk, miso, mosi, 1000000,
                                   SPI_BITORDER_LSBFIRST, SPI_MODE0);
}

/**************************************************************************/
/*!
    @brief  Instantiates a new PN532 class using I2C.

    @param  irq       Location of the IRQ pin
    @param  reset     Location of the reset pin
*/
/**************************************************************************/
NFC_PN532::NFC_PN532(uint8_t irq, uint8_t reset)
    : _irq(irq), _reset(reset)
{
  pinMode(_irq, INPUT);
  pinMode(_reset, OUTPUT);
}

/**************************************************************************/
/*!
    @brief  Instantiates a new PN532 class using hardware SPI.

    @param  ss        SPI chip select pin (CS/SSEL)
*/
/**************************************************************************/
NFC_PN532::NFC_PN532(uint8_t ss)
{
  spi_dev =
      new Adafruit_SPIDevice(ss, 1000000, SPI_BITORDER_LSBFIRST, SPI_MODE0);
}

/**************************************************************************/
/*!
    @brief PN532 initialization, Setups the HW
*/
/**************************************************************************/
void NFC_PN532::begin()
{
  if (spi_dev != NULL)
  {
    // SPI initialization
    spi_dev->begin();

    // not exactly sure why but we have to send a dummy command to get synced up
    pn532_packetbuffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;
    sendCommandCheckAck(pn532_packetbuffer, 1);
    // ignore response!
  }
  else
  {
    // I2C initialization.
    WIRE.begin();

    // Reset the PN532
    digitalWrite(_reset, HIGH);
    digitalWrite(_reset, LOW);
    delay(400);
    digitalWrite(_reset, HIGH);
    delay(10); // Small delay required before taking other actions after reset.
               // See timing diagram on page 209 of the datasheet, section 12.23.
  }
}

/**************************************************************************/
/*!
    @brief  Prints a hexadecimal value in plain characters

    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void NFC_PN532::PrintHex(const byte *data, const uint32_t numBytes)
{
  uint32_t szPos;
  for (szPos = 0; szPos < numBytes; szPos++)
  {
    PN532DEBUGPRINT.print(F("0x"));
    // Append leading 0 for small values
    if (data[szPos] <= 0xF)
      PN532DEBUGPRINT.print(F("0"));
    PN532DEBUGPRINT.print(data[szPos] & 0xff, HEX);
    if ((numBytes > 1) && (szPos != numBytes - 1))
    {
      PN532DEBUGPRINT.print(F(" "));
    }
  }
  PN532DEBUGPRINT.println();
}

/**************************************************************************/
/*!
    @brief  Prints a hexadecimal value in plain characters, along with
            the char equivalents in the following format

            00 00 00 00 00 00  ......

    @param  data      Pointer to the byte data
    @param  numBytes  Data length in bytes
*/
/**************************************************************************/
void NFC_PN532::PrintHexChar(const byte *data, const uint32_t numBytes)
{
  uint32_t szPos;
  for (szPos = 0; szPos < numBytes; szPos++)
  {
    // Append leading 0 for small values
    if (data[szPos] <= 0xF)
      PN532DEBUGPRINT.print(F("0"));
    PN532DEBUGPRINT.print(data[szPos], HEX);
    if ((numBytes > 1) && (szPos != numBytes - 1))
    {
      PN532DEBUGPRINT.print(F(" "));
    }
  }
  PN532DEBUGPRINT.print(F("  "));
  for (szPos = 0; szPos < numBytes; szPos++)
  {
    if (data[szPos] <= 0x1F)
      PN532DEBUGPRINT.print(F("."));
    else
      PN532DEBUGPRINT.print((char)data[szPos]);
  }
  PN532DEBUGPRINT.println();
}

/**************************************************************************/
/*!
    @brief  Checks the firmware version of the PN5xx chip

    @returns  The chip's firmware version and ID
*/
/**************************************************************************/
uint32_t NFC_PN532::getFirmwareVersion(void)
{
  uint32_t response;

  pn532_packetbuffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;

  if (!sendCommandCheckAck(pn532_packetbuffer, 1))
  {
    return 0;
  }

  // read data packet
  readdata(pn532_packetbuffer, 12);

  // check some basic stuff
  if (0 != memcmp((char *)pn532_packetbuffer,
                  (char *)pn532response_firmwarevers, 6))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("Firmware doesn't match!"));
#endif
    return 0;
  }

  int offset = 7;
  response = pn532_packetbuffer[offset++];
  response <<= 8;
  response |= pn532_packetbuffer[offset++];
  response <<= 8;
  response |= pn532_packetbuffer[offset++];
  response <<= 8;
  response |= pn532_packetbuffer[offset++];

  return response;
}

/**************************************************************************/
/*!
    @brief  Sends a command and waits a specified period for the ACK

    @param  cmd       Pointer to the command buffer
    @param  cmdlen    The size of the command in bytes
    @param  timeout   timeout before giving up

    @returns  TRUE if everything is OK, FALSE if timeout occured before an
              ACK was recieved
*/
/**************************************************************************/
// default timeout of one second
bool NFC_PN532::sendCommandCheckAck(uint8_t *cmd, uint8_t cmdlen,
                                    uint16_t timeout)
{
  // uint16_t timer = 0;

  // write the command
  writecommand(cmd, cmdlen);

  // Wait for chip to say its ready!
  if (!waitready(timeout))
  {
    return false;
  }

#ifdef PN532DEBUG
  if (spi_dev == NULL)
  {
    PN532DEBUGPRINT.println(F("IRQ received"));
  }
#endif

  // read acknowledgement
  if (!readack())
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("No ACK frame received!"));
#endif
    return false;
  }
  // For SPI only wait for the chip to be ready again.
  // This is unnecessary with I2C.
  if (spi_dev != NULL)
  {
    if (!waitready(timeout))
    {
      return false;
    }
  }

  return true; // ack'd command
}

/**************************************************************************/
/*!
    @brief  Sends a command and waits a specified period for the ACK

    @param  cmd       Pointer to the command buffer
    @param  cmdlen    The size of the command in bytes
    @param  timeout   timeout before giving up

    @returns  TRUE if everything is OK, FALSE if timeout occured before an
              ACK was recieved
*/
/**************************************************************************/
bool NFC_PN532::sendCommandCheckAck_Felica(uint8_t *cmd, uint8_t cmdlen, const uint8_t *body, uint8_t blen, uint16_t timeout)
{
  // uint16_t timer = 0;

  // write the command
  writecommand_Felica(cmd, cmdlen, body, blen);

  // Wait for chip to say its ready!
  if (!waitready(timeout))
  {
    return false;
  }

#ifdef PN532DEBUG
  if (spi_dev == NULL)
  {
    PN532DEBUGPRINT.println(F("IRQ received"));
  }
#endif

  // read acknowledgement
  if (!readack())
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("No ACK frame received!"));
#endif
    return false;
  }
  // For SPI only wait for the chip to be ready again.
  // This is unnecessary with I2C.
  if (spi_dev != NULL)
  {
    if (!waitready(timeout))
    {
      return false;
    }
  }
  return true; // ack'd command
}

/**************************************************************************/
/*!
    Writes an 8-bit value that sets the state of the PN532's GPIO pins

    @warning This function is provided exclusively for board testing and
             is dangerous since it will throw an error if any pin other
             than the ones marked "Can be used as GPIO" are modified!  All
             pins that can not be used as GPIO should ALWAYS be left high
             (value = 1) or the system will become unstable and a HW reset
             will be required to recover the PN532.

             pinState[0]  = P30     Can be used as GPIO
             pinState[1]  = P31     Can be used as GPIO
             pinState[2]  = P32     *** RESERVED (Must be 1!) ***
             pinState[3]  = P33     Can be used as GPIO
             pinState[4]  = P34     *** RESERVED (Must be 1!) ***
             pinState[5]  = P35     Can be used as GPIO

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
bool NFC_PN532::writeGPIO(uint8_t pinstate)
{
  // uint8_t errorbit;

  // Make sure pinstate does not try to toggle P32 or P34
  pinstate |= (1 << PN532_GPIO_P32) | (1 << PN532_GPIO_P34);

  // Fill command buffer
  pn532_packetbuffer[0] = PN532_COMMAND_WRITEGPIO;
  pn532_packetbuffer[1] = PN532_GPIO_VALIDATIONBIT | pinstate; // P3 Pins
  pn532_packetbuffer[2] = 0x00;                                // P7 GPIO Pins (not used ... taken by SPI)

#ifdef PN532DEBUG
  PN532DEBUGPRINT.print(F("Writing P3 GPIO: "));
  PN532DEBUGPRINT.println(pn532_packetbuffer[1], HEX);
#endif

  // Send the WRITEGPIO command (0x0E)
  if (!sendCommandCheckAck(pn532_packetbuffer, 3))
    return 0x0;

  // Read response packet (00 FF PLEN PLENCHECKSUM D5 CMD+1(0x0F) DATACHECKSUM
  // 00)
  readdata(pn532_packetbuffer, 8);

#ifdef PN532DEBUG
  PN532DEBUGPRINT.print(F("Received: "));
  PrintHex(pn532_packetbuffer, 8);
  PN532DEBUGPRINT.println();
#endif

  int offset = 6;
  return (pn532_packetbuffer[offset] == 0x0F);
}

/**************************************************************************/
/*!
    Reads the state of the PN532's GPIO pins

    @returns An 8-bit value containing the pin state where:

             pinState[0]  = P30
             pinState[1]  = P31
             pinState[2]  = P32
             pinState[3]  = P33
             pinState[4]  = P34
             pinState[5]  = P35
*/
/**************************************************************************/
uint8_t NFC_PN532::readGPIO(void)
{
  pn532_packetbuffer[0] = PN532_COMMAND_READGPIO;

  // Send the READGPIO command (0x0C)
  if (!sendCommandCheckAck(pn532_packetbuffer, 1))
    return 0x0;

  // Read response packet (00 FF PLEN PLENCHECKSUM D5 CMD+1(0x0D) P3 P7 IO1
  // DATACHECKSUM 00)
  readdata(pn532_packetbuffer, 11);

  /* READGPIO response should be in the following format:

    byte            Description
    -------------   ------------------------------------------
    b0..5           Frame header and preamble (with I2C there is an extra 0x00)
    b6              P3 GPIO Pins
    b7              P7 GPIO Pins (not used ... taken by SPI)
    b8              Interface Mode Pins (not used ... bus select pins)
    b9..10          checksum */

  int p3offset = 7;

#ifdef PN532DEBUG
  PN532DEBUGPRINT.print(F("Received: "));
  PrintHex(pn532_packetbuffer, 11);
  PN532DEBUGPRINT.println();
  PN532DEBUGPRINT.print(F("P3 GPIO: 0x"));
  PN532DEBUGPRINT.println(pn532_packetbuffer[p3offset], HEX);
  PN532DEBUGPRINT.print(F("P7 GPIO: 0x"));
  PN532DEBUGPRINT.println(pn532_packetbuffer[p3offset + 1], HEX);
  PN532DEBUGPRINT.print(F("IO GPIO: 0x"));
  PN532DEBUGPRINT.println(pn532_packetbuffer[p3offset + 2], HEX);
  // Note: You can use the IO GPIO value to detect the serial bus being used
  switch (pn532_packetbuffer[p3offset + 2])
  {
  case 0x00: // Using UART
    PN532DEBUGPRINT.println(F("Using UART (IO = 0x00)"));
    break;
  case 0x01: // Using I2C
    PN532DEBUGPRINT.println(F("Using I2C (IO = 0x01)"));
    break;
  case 0x02: // Using SPI
    PN532DEBUGPRINT.println(F("Using SPI (IO = 0x02)"));
    break;
  }
#endif

  return pn532_packetbuffer[p3offset];
}

/**************************************************************************/
/*!
    @brief  Configures the SAM (Secure Access Module)
    @return  TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::SAMConfig(void)
{
  pn532_packetbuffer[0] = PN532_COMMAND_SAMCONFIGURATION;
  pn532_packetbuffer[1] = 0x01; // normal mode;
  pn532_packetbuffer[2] = 0x14; // timeout 50ms * 20 = 1 second
  pn532_packetbuffer[3] = 0x01; // use IRQ pin!

  if (!sendCommandCheckAck(pn532_packetbuffer, 4))
    return false;

  // read data packet
  readdata(pn532_packetbuffer, 8);

  int offset = 6;
  return (pn532_packetbuffer[offset] == 0x15);
}

/**************************************************************************/
/*!
    Sets the MxRtyPassiveActivation byte of the RFConfiguration register

    @param  maxRetries    0xFF to wait forever, 0x00..0xFE to timeout
                          after mxRetries

    @returns TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::setPassiveActivationRetries(uint8_t maxRetries)
{
  pn532_packetbuffer[0] = PN532_COMMAND_RFCONFIGURATION;
  pn532_packetbuffer[1] = 5;    // Config item 5 (MaxRetries)
  pn532_packetbuffer[2] = 0xFF; // MxRtyATR (default = 0xFF)
  pn532_packetbuffer[3] = 0x01; // MxRtyPSL (default = 0x01)
  pn532_packetbuffer[4] = maxRetries;

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Setting MxRtyPassiveActivation to "));
  PN532DEBUGPRINT.print(maxRetries, DEC);
  PN532DEBUGPRINT.println(F(" "));
#endif

  if (!sendCommandCheckAck(pn532_packetbuffer, 5))
    return 0x0; // no ACK

  return 1;
}

/***** ISO14443A Commands ******/

/**************************************************************************/
/*!
    Waits for an ISO14443A_B target to enter the field

    @param  cardbaudrate  Baud rate of the card
    @param  uid           Pointer to the array that will be populated
                          with the card's UID (up to 7 bytes)
    @param  uidLength     Pointer to the variable that will hold the
                          length of the card's UID.
    @param  timeout       timeout before giving up.

    @returns TRUE if everything executed properly, FALSE for an error
*/
/********************
 * ******************************************************/
bool NFC_PN532::readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout)
{
  pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  pn532_packetbuffer[1] = 1; // max 1 cards at once (we can set this to 2 later)
  pn532_packetbuffer[2] = cardbaudrate;

  if (cardbaudrate == PN532_MIFARE_ISO14443B)
  {
    pn532_packetbuffer[3] = 0x00; // AFI

    if (!sendCommandCheckAck(pn532_packetbuffer, 4, timeout))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("No card(s) read"));
#endif
      return 0x0; // no cards read
    }

    // wait for a card to enter the field (only possible with I2C)
    if (spi_dev == NULL)
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Waiting for IRQ (indicates card presence)"));
#endif
      if (!waitready(timeout))
      {
#ifdef PN532DEBUG
        PN532DEBUGPRINT.println(F("IRQ Timeout"));
#endif
        return 0x0;
      }
    }

    // read data packet
    readdata(pn532_packetbuffer, 35); // We may not need so many bytes
                                      // check some basic stuff

    /* ISO14443B card response should be in the following format:

      byte                 Description
      ------------------   ------------------------------------------
      b0..b6               Frame header and preamble
      b7                   Tags found
      b8                   Tag Number (only one used in this example)
      b9..20               ATQB
      b21                  ATTRIB_RES Length
      b22..ATTRIB_RESLen   ATTRIB_RES
    */

#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("Found "));
    PN532DEBUGPRINT.print(pn532_packetbuffer[7], DEC);
    PN532DEBUGPRINT.println(F(" tags"));
#endif
    if (pn532_packetbuffer[7] != 1)
      return 0;

    _inListedTag = pn532_packetbuffer[8];

    /* ISO14443B ATQ_B format:
      byte    Description
      -----   -------
      b0      0x50
      b1..4   PUPI
      b5..8   Application Data
      b9      Bit Rate Capacity
      b10     Max Frame Size/-4 Info
      b11     FWI/Coding Options
    */

    uint8_t atqb[12];
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("ATQB:"));
#endif

    /* Sometimes, we get a lot of 0x01 bytes preceding the actual ATQ_B packet
       To mitigate that, we'll add a offset so that we can skip those 0x01 bytes
    */
    uint8_t offset = 0;
    while (pn532_packetbuffer[offset + 9] == 0x1)
    {
      offset++;
    }
    if (pn532_packetbuffer[offset + 9] == 0x50)
    {
      for (uint8_t i = 0; i < 12; i++)
      {
        atqb[i] = pn532_packetbuffer[offset + 9 + i];
#ifdef MIFAREDEBUG
        PN532DEBUGPRINT.print(F(" 0x"));
        PN532DEBUGPRINT.print(atqb[i], HEX);
#endif
      }
#ifdef MIFAREDEBUG
      PN532DEBUGPRINT.println();
#endif
    }
    else
    {
      return 0;
    }

#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("ATTRIB_RES: "));
    for (uint8_t i = 0; i < pn532_packetbuffer[offset + 21]; i++)
    {
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print(pn532_packetbuffer[offset + 22 + i]);
    }
    PN532DEBUGPRINT.println();
#endif

    *uidLength = 4;

#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("UID:"));
#endif
    for (uint8_t i = 0; i < 4; i++)
    {
      uid[i] = atqb[1 + i];
#ifdef MIFAREDEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print(uid[i], HEX);
#endif
    }
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println();
#endif
  }
  else
  {
    if (!sendCommandCheckAck(pn532_packetbuffer, 3, timeout))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("No card(s) read"));
#endif
      return 0x0; // no cards read
    }

    // wait for a card to enter the field (only possible with I2C)
    if (spi_dev == NULL)
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Waiting for IRQ (indicates card presence)"));
#endif
      if (!waitready(timeout))
      {
#ifdef PN532DEBUG
        PN532DEBUGPRINT.println(F("IRQ Timeout"));
#endif
        return 0x0;
      }
    }

    // read data packet
    readdata(pn532_packetbuffer, 20);
    // check some basic stuff

    /* ISO14443A card response should be in the following format:

      byte            Description
      -------------   ------------------------------------------
      b0..6           Frame header and preamble
      b7              Tags Found
      b8              Tag Number (only one used in this example)
      b9..10          SENS_RES
      b11             SEL_RES
      b12             NFCID Length
      b13..NFCIDLen   NFCID                                      */

#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("Found "));
    PN532DEBUGPRINT.print(pn532_packetbuffer[7], DEC);
    PN532DEBUGPRINT.println(F(" tags"));
#endif
    if (pn532_packetbuffer[7] != 1)
      return 0;

    _inListedTag = pn532_packetbuffer[8];

    uint16_t sens_res = pn532_packetbuffer[9];
    sens_res <<= 8;
    sens_res |= pn532_packetbuffer[10];

    // save these 2 pieces of information for later use
    _SAK = pn532_packetbuffer[11];
    _ATQA = sens_res;
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("ATQA: 0x"));
    PN532DEBUGPRINT.println(sens_res, HEX);
    PN532DEBUGPRINT.print(F("SAK: 0x"));
    PN532DEBUGPRINT.println(pn532_packetbuffer[11], HEX);
#endif

    /* Card appears to be Mifare Classic */
    *uidLength = pn532_packetbuffer[12];
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.print(F("UID:"));
#endif
    for (uint8_t i = 0; i < pn532_packetbuffer[12]; i++)
    {
      uid[i] = pn532_packetbuffer[13 + i];
#ifdef MIFAREDEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print(uid[i], HEX);
#endif
    }
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println();
#endif
  }
  return 1;
}

/**************************************************************************/
/*!
    Puts PN532 into passive detection state with IRQ while waiting for an ISO14443A target

    @param  cardBaudRate  Baud rate of the card

    @returns TRUE if everything executed properly, FALSE for an error
*/
bool NFC_PN532::startPassiveTargetIDDetection(uint8_t cardbaudrate)
{
  pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  pn532_packetbuffer[1] = 1; // max 1 cards at once (we can set this to 2 later)
  pn532_packetbuffer[2] = cardbaudrate;

  if (!sendCommandCheckAck(pn532_packetbuffer, 3))
  {
    return false; // command failed
  }
  return true;
}

/**************************************************************************/
/*!
    @brief  Exchanges an APDU with the currently inlisted peer

    @param  send            Pointer to data to send
    @param  sendLength      Length of the data to send
    @param  response        Pointer to response data
    @param  responseLength  Pointer to the response data length
    @return TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::inDataExchange(uint8_t *send, uint8_t sendLength, uint8_t *response, uint8_t *responseLength)
{
  if (sendLength > PN532_PACKBUFFSIZ - 2)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("APDU length too long for packet buffer"));
#endif
    return false;
  }
  uint8_t i;

  pn532_packetbuffer[0] = 0x40; // PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = _inListedTag;
  for (i = 0; i < sendLength; ++i)
  {
    pn532_packetbuffer[i + 2] = send[i];
  }

  if (!sendCommandCheckAck(pn532_packetbuffer, sendLength + 2, 1000))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("Could not send APDU"));
#endif
    return false;
  }

  if (!waitready(1000))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("Response never received for APDU..."));
#endif
    return false;
  }

  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  if (pn532_packetbuffer[0] == 0 && pn532_packetbuffer[1] == 0 && pn532_packetbuffer[2] == 0xff)
  {
    uint8_t length = pn532_packetbuffer[3];
    if (pn532_packetbuffer[4] != (uint8_t)(~length + 1))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Length check invalid"));
      PN532DEBUGPRINT.println(length, HEX);
      PN532DEBUGPRINT.println((~length) + 1, HEX);
#endif
      return false;
    }
    if (pn532_packetbuffer[5] == PN532_PN532TOHOST && pn532_packetbuffer[6] == PN532_RESPONSE_INDATAEXCHANGE)
    {
      if ((pn532_packetbuffer[7] & 0x3f) != 0)
      {
#ifdef PN532DEBUG
        PN532DEBUGPRINT.println(F("Status code indicates an error"));
#endif
        return false;
      }

      length -= 3;

      // if (length > *responseLength) {
      // length = *responseLength; // silent truncation...
      // }

      for (i = 0; i < length; ++i)
      {
        response[i] = pn532_packetbuffer[8 + i];
      }
      *responseLength = length;

      return true;
    }
    else
    {
      PN532DEBUGPRINT.print(F("Don't know how to handle this command: "));
      PN532DEBUGPRINT.println(pn532_packetbuffer[6], HEX);
      return false;
    }
  }
  else
  {
    PN532DEBUGPRINT.println(F("Preamble missing"));
    return false;
  }
  return false;
}

/**************************************************************************/
/*!
    @brief  'InLists' a passive target. PN532 acting as reader/initiator,
            peer acting as card/responder.
*/
/**************************************************************************/
bool NFC_PN532::inListPassiveTarget()
{
  return inListPassiveTarget(0);
}

/*!
    @brief  'InLists' a passive target. PN532 acting as reader/initiator,
            peer acting as card/responder.

    @param  cardBaudRate  Baud rate of the card

    @returns TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::inListPassiveTarget(uint8_t cardBaudRate)
{
  pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  pn532_packetbuffer[1] = 1;
  pn532_packetbuffer[2] = cardBaudRate;

#ifdef PN532DEBUG
  PN532DEBUGPRINT.print(F("About to inList passive target"));
#endif

  if (cardBaudRate == PN532_MIFARE_ISO14443B)
  {
    pn532_packetbuffer[3] = 0;
    if (!sendCommandCheckAck(pn532_packetbuffer, 4, 1000))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Could not send inlist message"));
#endif
      return false;
    }
  }
  else
  {
    if (!sendCommandCheckAck(pn532_packetbuffer, 3, 1000))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Could not send inlist message"));
#endif
      return false;
    }
  }

  if (!waitready(30000))
  {
    return false;
  }

  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  if (pn532_packetbuffer[0] == 0 && pn532_packetbuffer[1] == 0 && pn532_packetbuffer[2] == 0xff)
  {
    uint8_t length = pn532_packetbuffer[3];
    if (pn532_packetbuffer[4] != (uint8_t)(~length + 1))
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.println(F("Length check invalid"));
      PN532DEBUGPRINT.println(length, HEX);
      PN532DEBUGPRINT.println((~length) + 1, HEX);
#endif
      return false;
    }
    if (pn532_packetbuffer[5] == PN532_PN532TOHOST && pn532_packetbuffer[6] == PN532_RESPONSE_INLISTPASSIVETARGET)
    {
      if (pn532_packetbuffer[7] != 1)
      {
#ifdef PN532DEBUG
        PN532DEBUGPRINT.println(F("Unhandled number of targets inlisted"));
#endif
        PN532DEBUGPRINT.println(F("Number of tags inlisted:"));
        PN532DEBUGPRINT.println(pn532_packetbuffer[7]);
        return false;
      }

      _inListedTag = pn532_packetbuffer[8];
      PN532DEBUGPRINT.print(F("Tag number: "));
      PN532DEBUGPRINT.println(_inListedTag);

      return true;
    }
    else
    {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F("Unexpected response to inlist passive host"));
#endif
      return false;
    }
  }
  else
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println(F("Preamble missing"));
#endif
    return false;
  }

  return true;
}

/***** Mifare Classic Functions ******/

/**************************************************************************/
/*!
    @brief Indicates whether the specified block number is the first block
      in the sector (block 0 relative to the current sector)

    @param  uiBlock         input the block number

    @returns TRUE if everything executed properly, FALSE for an error

*/
/**************************************************************************/
bool NFC_PN532::mifareclassic_IsFirstBlock(uint32_t uiBlock)
{
  // Test if we are in the small or big sectors
  if (uiBlock < 128)
    return ((uiBlock) % 4 == 0);
  else
    return ((uiBlock) % 16 == 0);
}

/**************************************************************************/
/*!
    @brief  Indicates whether the specified block number is the sector trailer
    @param  uiBlock         input the block number
    @returns TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::mifareclassic_IsTrailerBlock(uint32_t uiBlock)
{
  // Test if we are in the small or big sectors
  if (uiBlock < 128)
    return ((uiBlock + 1) % 4 == 0);
  else
    return ((uiBlock + 1) % 16 == 0);
}

/**************************************************************************/
/*!
    @brief Tries to authenticate a block of memory on a MIFARE card using the
    INDATAEXCHANGE command.  See section 7.3.8 of the PN532 User Manual
    for more information on sending MIFARE and other commands.

    @param  uid           Pointer to a byte array containing the card UID
    @param  uidLen        The length (in bytes) of the card's UID (Should
                          be 4 for MIFARE Classic)
    @param  blockNumber   The block number to authenticate.  (0..63 for
                          1KB cards, and 0..255 for 4KB cards).
    @param  keyNumber     Which key type to use during authentication
                          (0 = MIFARE_CMD_AUTH_A, 1 = MIFARE_CMD_AUTH_B)
    @param  keyData       Pointer to a byte array containing the 6 byte
                          key value

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareclassic_AuthenticateBlock(uint8_t *uid,
                                                   uint8_t uidLen,
                                                   uint32_t blockNumber,
                                                   uint8_t keyNumber,
                                                   uint8_t *keyData)
{
  // uint8_t len;
  uint8_t i;

  // Hang on to the key and uid data
  memcpy(_key, keyData, 6);
  memcpy(_uid, uid, uidLen);
  _uidLen = uidLen;

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Trying to authenticate card "));
  PrintHex(_uid, _uidLen);
  PN532DEBUGPRINT.print(F("Using authentication KEY "));
  PN532DEBUGPRINT.print(keyNumber ? 'B' : 'A');
  PN532DEBUGPRINT.print(F(": "));
  PrintHex(_key, 6);
#endif

  // Prepare the authentication command //
  pn532_packetbuffer[0] =
      PN532_COMMAND_INDATAEXCHANGE; /* Data Exchange Header */
  pn532_packetbuffer[1] = 1;        /* Max card numbers */
  pn532_packetbuffer[2] = (keyNumber) ? MIFARE_CMD_AUTH_B : MIFARE_CMD_AUTH_A;
  pn532_packetbuffer[3] = blockNumber; /* Block Number (1K = 0..63, 4K = 0..255 */
  memcpy(pn532_packetbuffer + 4, _key, 6);
  for (i = 0; i < _uidLen; i++)
  {
    pn532_packetbuffer[10 + i] = _uid[i]; /* 4 byte card ID */
  }

  if (!sendCommandCheckAck(pn532_packetbuffer, 10 + _uidLen))
    return 0;

  // Read the response packet
  readdata(pn532_packetbuffer, 12);

  // check if the response is valid and we are authenticated???
  // for an auth success it should be bytes 5-7: 0xD5 0x41 0x00
  // Mifare auth error is technically byte 7: 0x14 but anything other and 0x00
  // is not good
  if (pn532_packetbuffer[7] != 0x00)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F("Authentification failed: "));
    PrintHexChar(pn532_packetbuffer, 12);
#endif
    return 0;
  }

  return 1;
}

/**************************************************************************/
/*!
    @brief Tries to read an entire 16-byte data block at the specified block
    address.

    @param  blockNumber   The block number to authenticate.  (0..63 for
                          1KB cards, and 0..255 for 4KB cards).
    @param  data          Pointer to the byte array that will hold the
                          retrieved data (if any)

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareclassic_ReadDataBlock(uint8_t blockNumber,
                                               uint8_t *data)
{
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Trying to read 16 bytes from block "));
  PN532DEBUGPRINT.println(blockNumber);
#endif

  /* Prepare the command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1;               /* Card number */
  pn532_packetbuffer[2] = MIFARE_CMD_READ; /* Mifare Read command = 0x30 */
  pn532_packetbuffer[3] = blockNumber;     /* Block Number (0..63 for 1K, 0..255 for 4K) */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 4))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for read command"));
#endif
    return 0;
  }

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);

  /* If byte 8 isn't 0x00 we probably have an error */
  if (pn532_packetbuffer[7] != 0x00)
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Unexpected response"));
    PrintHexChar(pn532_packetbuffer, 26);
#endif
    return 0;
  }

  /* Copy the 16 data bytes to the output buffer        */
  /* Block content starts at byte 9 of a valid response */
  memcpy(data, pn532_packetbuffer + 8, 16);

/* Display data for debug if requested */
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Block "));
  PN532DEBUGPRINT.println(blockNumber);
  PrintHexChar(data, 16);
#endif

  return 1;
}

/**************************************************************************/
/*!
    @brief Tries to write an entire 16-byte data block at the specified block
    address.

    @param  blockNumber   The block number to authenticate.  (0..63 for
                          1KB cards, and 0..255 for 4KB cards).
    @param  data          The byte array that contains the data to write.

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareclassic_WriteDataBlock(uint8_t blockNumber,
                                                uint8_t *data)
{
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Trying to write 16 bytes to block "));
  PN532DEBUGPRINT.println(blockNumber);
#endif

  /* Prepare the first command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1;                /* Card number */
  pn532_packetbuffer[2] = MIFARE_CMD_WRITE; /* Mifare Write command = 0xA0 */
  pn532_packetbuffer[3] =
      blockNumber;                          /* Block Number (0..63 for 1K, 0..255 for 4K) */
  memcpy(pn532_packetbuffer + 4, data, 16); /* Data Payload */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 20))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for write command"));
#endif
    return 0;
  }
  delay(10);

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);

  return 1;
}

/**************************************************************************/
/*!
    @brief Formats a Mifare Classic card to store NDEF Records

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareclassic_FormatNDEF(void)
{
  uint8_t sectorbuffer1[16] = {0x14, 0x01, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1,
                               0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
  uint8_t sectorbuffer2[16] = {0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1,
                               0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1, 0x03, 0xE1};
  uint8_t sectorbuffer3[16] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0x78, 0x77,
                               0x88, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Note 0xA0 0xA1 0xA2 0xA3 0xA4 0xA5 must be used for key A
  // for the MAD sector in NDEF records (sector 0)

  // Write block 1 and 2 to the card
  if (!(mifareclassic_WriteDataBlock(1, sectorbuffer1)))
    return 0;
  if (!(mifareclassic_WriteDataBlock(2, sectorbuffer2)))
    return 0;
  // Write key A and access rights card
  if (!(mifareclassic_WriteDataBlock(3, sectorbuffer3)))
    return 0;

  // Seems that everything was OK (?!)
  return 1;
}

/**************************************************************************/
/*!
    @brief Writes an NDEF URI Record to the specified sector (1..15)

    Note that this function assumes that the Mifare Classic card is
    already formatted to work as an "NFC Forum Tag" and uses a MAD1
    file system.  You can use the NXP TagWriter app on Android to
    properly format cards for this.

    @param  sectorNumber  The sector that the URI record should be written
                          to (can be 1..15 for a 1K card)
    @param  uriIdentifier The uri identifier code (0 = none, 0x01 =
                          "http://www.", etc.)
    @param  url           The uri text to write (max 38 characters).

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareclassic_WriteNDEFURI(uint8_t sectorNumber,
                                              uint8_t uriIdentifier,
                                              const char *url)
{
  // Figure out how long the string is
  uint8_t len = strlen(url);

  // Make sure we're within a 1K limit for the sector number
  if ((sectorNumber < 1) || (sectorNumber > 15))
    return 0;

  // Make sure the URI payload is between 1 and 38 chars
  if ((len < 1) || (len > 38))
    return 0;

  // Note 0xD3 0xF7 0xD3 0xF7 0xD3 0xF7 must be used for key A
  // in NDEF records

  // Setup the sector buffer (w/pre-formatted TLV wrapper and NDEF message)
  uint8_t sectorbuffer1[16] = {0x00,
                               0x00,
                               0x03,
                               (uint8_t)(len + 5),
                               0xD1,
                               0x01,
                               (uint8_t)(len + 1),
                               0x55,
                               uriIdentifier,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00};
  uint8_t sectorbuffer2[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t sectorbuffer3[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t sectorbuffer4[16] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07,
                               0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (len <= 6)
  {
    // Unlikely we'll get a url this short, but why not ...
    memcpy(sectorbuffer1 + 9, url, len);
    sectorbuffer1[len + 9] = 0xFE;
  }
  else if (len == 7)
  {
    // 0xFE needs to be wrapped around to next block
    memcpy(sectorbuffer1 + 9, url, len);
    sectorbuffer2[0] = 0xFE;
  }
  else if ((len > 7) && (len <= 22))
  {
    // Url fits in two blocks
    memcpy(sectorbuffer1 + 9, url, 7);
    memcpy(sectorbuffer2, url + 7, len - 7);
    sectorbuffer2[len - 7] = 0xFE;
  }
  else if (len == 23)
  {
    // 0xFE needs to be wrapped around to final block
    memcpy(sectorbuffer1 + 9, url, 7);
    memcpy(sectorbuffer2, url + 7, len - 7);
    sectorbuffer3[0] = 0xFE;
  }
  else
  {
    // Url fits in three blocks
    memcpy(sectorbuffer1 + 9, url, 7);
    memcpy(sectorbuffer2, url + 7, 16);
    memcpy(sectorbuffer3, url + 23, len - 24);
    sectorbuffer3[len - 22] = 0xFE;
  }

  // Now write all three blocks back to the card
  if (!(mifareclassic_WriteDataBlock(sectorNumber * 4, sectorbuffer1)))
    return 0;
  if (!(mifareclassic_WriteDataBlock((sectorNumber * 4) + 1, sectorbuffer2)))
    return 0;
  if (!(mifareclassic_WriteDataBlock((sectorNumber * 4) + 2, sectorbuffer3)))
    return 0;
  if (!(mifareclassic_WriteDataBlock((sectorNumber * 4) + 3, sectorbuffer4)))
    return 0;

  // Seems that everything was OK (?!)
  return 1;
}

/***** Mifare Ultralight Functions ******/

/**************************************************************************/
/*!
    @brief Tries to read an entire 4-byte page at the specified address.

    @param  page        The page number (0..63 in most cases)
    @param  buffer      Pointer to the byte array that will hold the
                        retrieved data (if any)
    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareultralight_ReadPage(uint8_t page,
                                             uint8_t *buffer)
{
  if (page >= 64)
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Page value out of range"));
#endif
    return 0;
  }

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Reading page "));
  PN532DEBUGPRINT.println(page);
#endif

  /* Prepare the command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1;               /* Card number */
  pn532_packetbuffer[2] = MIFARE_CMD_READ; /* Mifare Read command = 0x30 */
  pn532_packetbuffer[3] = page;            /* Page Number (0..63 in most cases) */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 4))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for write command"));
#endif
    return 0;
  }

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.println(F("Received: "));
  PrintHexChar(pn532_packetbuffer, 26);
#endif

  /* If byte 8 isn't 0x00 we probably have an error */
  if (pn532_packetbuffer[7] == 0x00)
  {
    /* Copy the 4 data bytes to the output buffer         */
    /* Block content starts at byte 9 of a valid response */
    /* Note that the command actually reads 16 byte or 4  */
    /* pages at a time ... we simply discard the last 12  */
    /* bytes                                              */
    memcpy(buffer, pn532_packetbuffer + 8, 4);
  }
  else
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Unexpected response reading block: "));
   PrintHexChar(pn532_packetbuffer, 26);
#endif
    return 0;
  }

/* Display data for debug if requested */
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Page "));
  PN532DEBUGPRINT.print(page);
  PN532DEBUGPRINT.println(F(":"));
  PrintHexChar(buffer, 4);
#endif

  // Return OK signal
  return 1;
}

/**************************************************************************/
/*!
    @brief Tries to write an entire 4-byte page at the specified block address.

    @param  page          The page number to write.  (0..63 for most cases)
    @param  data          The byte array that contains the data to write.
                          Should be exactly 4 bytes long.

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::mifareultralight_WritePage(uint8_t page,
                                              uint8_t *data)
{
  if (page >= 64)
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Page value out of range"));
#endif
    // Return Failed Signal
    return 0;
  }

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Trying to write 4 byte page"));
  PN532DEBUGPRINT.println(page);
#endif

  /* Prepare the first command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1;                           /* Card number */
  pn532_packetbuffer[2] = MIFARE_ULTRALIGHT_CMD_WRITE; /* Mifare Ultralight Write command = 0xA2 */
  pn532_packetbuffer[3] = page;                        /* Page Number (0..63 for most cases) */
  memcpy(pn532_packetbuffer + 4, data, 4);             /* Data Payload */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 8))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for write command"));
#endif

    // Return Failed Signal
    return 0;
  }
  delay(10);

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);

  // Return OK Signal
  return 1;
}

/***** NTAG2xx Functions ******/

/**************************************************************************/
/*!
    @brief Tries to read an entire 4-byte page at the specified address.

    @param  page        The page number (0..63 in most cases)
    @param  buffer      Pointer to the byte array that will hold the
                        retrieved data (if any)
    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::ntag2xx_ReadPage(uint8_t page, uint8_t *buffer)
{
  // TAG Type       PAGES   USER START    USER STOP
  // --------       -----   ----------    ---------
  // NTAG 203       42      4             39
  // NTAG 213       45      4             39
  // NTAG 215       135     4             129
  // NTAG 216       231     4             225

  if (page >= 231)
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Page value out of range"));
#endif
    return 0;
  }

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Reading page "));
  PN532DEBUGPRINT.println(page);
#endif

  /* Prepare the command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1;               /* Card number */
  pn532_packetbuffer[2] = MIFARE_CMD_READ; /* Mifare Read command = 0x30 */
  pn532_packetbuffer[3] = page;            /* Page Number (0..63 in most cases) */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 4))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for write command"));
#endif
    return 0;
  }

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.println(F("Received: "));
  PrintHexChar(pn532_packetbuffer, 26);
#endif

  /* If byte 8 isn't 0x00 we probably have an error */
  if (pn532_packetbuffer[7] == 0x00)
  {
    /* Copy the 4 data bytes to the output buffer         */
    /* Block content starts at byte 9 of a valid response */
    /* Note that the command actually reads 16 byte or 4  */
    /* pages at a time ... we simply discard the last 12  */
    /* bytes                                              */
    memcpy(buffer, pn532_packetbuffer + 8, 4);
  }
  else
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Unexpected response reading block: "));
    PrintHexChar(pn532_packetbuffer, 26);
#endif
    return 0;
  }

/* Display data for debug if requested */
#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Page "));
  PN532DEBUGPRINT.print(page);
  PN532DEBUGPRINT.println(F(":"));
  PrintHexChar(buffer, 4);
#endif

  // Return OK signal
  return 1;
}

/**************************************************************************/
/*!
    @brief Tries to write an entire 4-byte page at the specified block address.


    @param  page          The page number to write.  (0..63 for most cases)
    @param  data          The byte array that contains the data to write.
                          Should be exactly 4 bytes long.

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::ntag2xx_WritePage(uint8_t page, uint8_t *data)
{
  // TAG Type       PAGES   USER START    USER STOP
  // --------       -----   ----------    ---------
  // NTAG 203       42      4             39
  // NTAG 213       45      4             39
  // NTAG 215       135     4             129
  // NTAG 216       231     4             225

  if ((page < 4) || (page > 225))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Page value out of range"));
#endif
    // Return Failed Signal
    return 0;
  }

#ifdef MIFAREDEBUG
  PN532DEBUGPRINT.print(F("Trying to write 4 byte page"));
  PN532DEBUGPRINT.println(page);
#endif

  /* Prepare the first command */
  pn532_packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = 1; /* Card number */
  pn532_packetbuffer[2] =
      MIFARE_ULTRALIGHT_CMD_WRITE;         /* Mifare Ultralight Write command = 0xA2 */
  pn532_packetbuffer[3] = page;            /* Page Number (0..63 for most cases) */
  memcpy(pn532_packetbuffer + 4, data, 4); /* Data Payload */

  /* Send the command */
  if (!sendCommandCheckAck(pn532_packetbuffer, 8))
  {
#ifdef MIFAREDEBUG
    PN532DEBUGPRINT.println(F("Failed to receive ACK for write command"));
#endif

    // Return Failed Signal
    return 0;
  }
  delay(10);

  /* Read the response packet */
  readdata(pn532_packetbuffer, 26);

  // Return OK Signal
  return 1;
}

/**************************************************************************/
/*!
    @brief Writes an NDEF URI Record starting at the specified page (4..nn)

    Note that this function assumes that the NTAG2xx card is
    already formatted to work as an "NFC Forum Tag".

    @param  uriIdentifier The uri identifier code (0 = none, 0x01 ="http://www.", etc.)
    @param  url           The uri text to write (null-terminated string).
    @param  dataLen       The size of the data area for overflow checks.

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::ntag2xx_WriteNDEFURI(uint8_t uriIdentifier, char *url,
                                        uint8_t dataLen)
{
  uint8_t pageBuffer[4] = {0, 0, 0, 0};

  // Remove NDEF record overhead from the URI data (pageHeader below)
  uint8_t wrapperSize = 12;

  // Figure out how long the string is
  uint8_t len = strlen(url);

  // Make sure the URI payload will fit in dataLen (include 0xFE trailer)
  if ((len < 1) || (len + 1 > (dataLen - wrapperSize)))
    return 0;

  // Setup the record header
  // See NFCForum-TS-Type-2-Tag_1.1.pdf for details
  uint8_t pageHeader[12] = {
      /* NDEF Lock Control TLV (must be first and always present) */
      0x01, /* Tag Field (0x01 = Lock Control TLV) */
      0x03, /* Payload Length (always 3) */
      0xA0, /* The position inside the tag of the lock bytes (upper 4 = page
               address, lower 4 = byte offset) */
      0x10, /* Size in bits of the lock area */
      0x44, /* Size in bytes of a page and the number of bytes each lock bit can
               lock (4 bit + 4 bits) */
      /* NDEF Message TLV - URI Record */
      0x03,               /* Tag Field (0x03 = NDEF Message) */
      (uint8_t)(len + 5), /* Payload Length (not including 0xFE trailer) */
      0xD1,               /* NDEF Record Header (TNF=0x1:Well known record + SR + ME + MB) */
      0x01,               /* Type Length for the record type indicator */
      (uint8_t)(len + 1), /* Payload len */
      0x55,               /* Record Type Indicator (0x55 or 'U' = URI Record) */
      uriIdentifier       /* URI Prefix (ex. 0x01 = "http://www.") */
  };

  // Write 12 byte header (three pages of data starting at page 4)
  memcpy(pageBuffer, pageHeader, 4);
  if (!(ntag2xx_WritePage(4, pageBuffer)))
    return 0;
  memcpy(pageBuffer, pageHeader + 4, 4);
  if (!(ntag2xx_WritePage(5, pageBuffer)))
    return 0;
  memcpy(pageBuffer, pageHeader + 8, 4);
  if (!(ntag2xx_WritePage(6, pageBuffer)))
    return 0;

  // Write URI (starting at page 7)
  uint8_t currentPage = 7;
  char *urlcopy = url;
  while (len)
  {
    if (len < 4)
    {
      memset(pageBuffer, 0, 4);
      memcpy(pageBuffer, urlcopy, len);
      pageBuffer[len] = 0xFE; // NDEF record footer
      if (!(ntag2xx_WritePage(currentPage, pageBuffer)))
        return 0;
      // DONE!
      return 1;
    }
    else if (len == 4)
    {
      memcpy(pageBuffer, urlcopy, len);
      if (!(ntag2xx_WritePage(currentPage, pageBuffer)))
        return 0;
      memset(pageBuffer, 0, 4);
      pageBuffer[0] = 0xFE; // NDEF record footer
      currentPage++;
      if (!(ntag2xx_WritePage(currentPage, pageBuffer)))
        return 0;
      // DONE!
      return 1;
    }
    else
    {
      // More than one page of data left
      memcpy(pageBuffer, urlcopy, 4);
      if (!(ntag2xx_WritePage(currentPage, pageBuffer)))
        return 0;
      currentPage++;
      urlcopy += 4;
      len -= 4;
    }
  }

  // Seems that everything was OK (?!)
  return 1;
}

/************** high level communication functions (handles both I2C and SPI) */

/**************************************************************************/
/*!
    @brief  Tries to read the SPI or I2C ACK signal
    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
bool NFC_PN532::readack()
{
  uint8_t ackbuff[6];

  if (spi_dev)
  {
    uint8_t cmd = PN532_SPI_DATAREAD;
    spi_dev->write_then_read(&cmd, 1, ackbuff, 6);
  }
  else
  {
    readdata(ackbuff, 6);
  }

  return (0 == memcmp((char *)ackbuff, (char *)pn532ack, 6));
}

/**************************************************************************/
/*!
    @brief  Return true if the PN532 is ready with a response

*/
/**************************************************************************/
bool NFC_PN532::isready()
{
  if (spi_dev != NULL)
  {
    uint8_t cmd = PN532_SPI_STATREAD;
    uint8_t reply;
    spi_dev->write_then_read(&cmd, 1, &reply, 1);
    // Check if status is ready.
    // Serial.print("Ready? 0x"); Serial.println(reply, HEX);
    return reply == PN532_SPI_READY;
  }
  else
  {
    // I2C check if status is ready by IRQ line being pulled low.
    uint8_t x = digitalRead(_irq);
    return x == 0;
  }
}

/**************************************************************************/
/*!
    @brief  Waits until the PN532 is ready.

    @param  timeout   Timeout before giving up
    @returns TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
bool NFC_PN532::waitready(uint16_t timeout)
{
  uint16_t timer = 0;
  while (!isready())
  {
    if (timeout != 0)
    {
      timer += 10;
      if (timer > timeout)
      {
#ifdef PN532DEBUG
        PN532DEBUGPRINT.println("TIMEOUT!");
#endif
        return false;
      }
    }
    delay(10);
  }
  return true;
}

/**************************************************************************/
/*!
    @brief  Reads n bytes of data from the PN532 via SPI or I2C.

    @param  buff      Pointer to the buffer where data will be written
    @param  n         Number of bytes to be read
*/
/**************************************************************************/
void NFC_PN532::readdata(uint8_t *buff, uint8_t n)
{
  if (spi_dev)
  {
    uint8_t cmd = PN532_SPI_DATAREAD;

    spi_dev->write_then_read(&cmd, 1, buff, n);

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F("Reading: "));
    for (uint8_t i = 0; i < n; i++)
    {
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print(buff[i], HEX);
    }
    PN532DEBUGPRINT.println();
#endif
  }
  else
  {
    // I2C write.
    uint16_t timer = 0;

    delay(2);

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F("Reading: "));
#endif
    // Start read (n+1 to take into account leading 0x01 with I2C)
    WIRE.requestFrom((uint8_t)PN532_I2C_ADDRESS, (uint8_t)(n + 1));
    // Discard the leading 0x01
    i2c_recv();
    for (uint8_t i = 0; i < n; i++)
    {
      delay(1);
      buff[i] = i2c_recv();
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print(buff[i], HEX);
#endif
    }
    // Discard trailing 0x00 0x00
    // i2c_recv();

#ifdef PN532DEBUG
    PN532DEBUGPRINT.println();
#endif
  }
}

/**************************************************************************/
/*!
    @brief  set the PN532 as iso14443a Target behaving as a SmartCard
    @param  None
    @returns TRUE if everything executed properly, FALSE for an error
*/
/**************************************************************************/
uint8_t NFC_PN532::AsTarget()
{
  pn532_packetbuffer[0] = 0x8C;
  uint8_t target[] = {
      0x8C,             // INIT AS TARGET
      0x00,             // MODE -> BITFIELD
      0x08, 0x00,       // SENS_RES - MIFARE PARAMS
      0xdc, 0x44, 0x20, // NFCID1T
      0x60,             // SEL_RES
      0x01, 0xfe,       // NFCID2T MUST START WITH 01fe - FELICA PARAMS - POL_RES
      0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xc0,
      0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, // PAD
      0xff, 0xff,                               // SYSTEM CODE
      0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44,
      0x33, 0x22, 0x11, 0x01, 0x00, // NFCID3t MAX 47 BYTES ATR_RES
      0x0d, 0x52, 0x46, 0x49, 0x44, 0x49, 0x4f,
      0x74, 0x20, 0x50, 0x4e, 0x35, 0x33, 0x32 // HISTORICAL BYTES
  };
  if (!sendCommandCheckAck(target, sizeof(target)))
    return false;

  // read data packet
  readdata(pn532_packetbuffer, 8);

  int offset = 6;
  return (pn532_packetbuffer[offset] == 0x15);
}
/**************************************************************************/
/*!
    @brief  retrieve response from the emulation mode

    @param  cmd    = data
    @param  cmdlen = data length
    @return returns 1 if the configuration is successful otherwise returns 0
*/
/**************************************************************************/
uint8_t NFC_PN532::getDataTarget(uint8_t *cmd, uint8_t *cmdlen)
{
  uint8_t length;
  pn532_packetbuffer[0] = 0x86;
  if (!sendCommandCheckAck(pn532_packetbuffer, 1, 1000))
  {
    PN532DEBUGPRINT.println(F("Error en ack"));
    return false;
  }

  // read data packet
  readdata(pn532_packetbuffer, 64);
  length = pn532_packetbuffer[3] - 3;

  // if (length > *responseLength) {// Bug, should avoid it in the reading
  // target data
  //  length = *responseLength; // silent truncation...
  //}

  for (int i = 0; i < length; ++i)
  {
    cmd[i] = pn532_packetbuffer[8 + i];
  }
  *cmdlen = length;
  return true;
}

/**************************************************************************/
/*!
    @brief  set data in PN532 in the emulation mode

    @param  cmd    = data
    @param  cmdlen = data length
    @return returns 1 if the configuration is successful otherwise returns 0
*/
/**************************************************************************/
uint8_t NFC_PN532::setDataTarget(uint8_t *cmd, uint8_t cmdlen)
{
  uint8_t length;
  // cmd1[0] = 0x8E; Must!

  if (!sendCommandCheckAck(cmd, cmdlen))
    return false;

  // read data packet
  readdata(pn532_packetbuffer, 8);
  length = pn532_packetbuffer[3] - 3;
  for (int i = 0; i < length; ++i)
  {
    cmd[i] = pn532_packetbuffer[8 + i];
  }
  // cmdl = 0
  cmdlen = length;

  int offset = 6;
  return (pn532_packetbuffer[offset] == 0x15);
}

/**************************************************************************/
/*!
    @brief  Writes a command to the PN532, automatically inserting the
            preamble and required frame details (checksum, len, etc.)

    @param  cmd       Pointer to the command buffer
    @param  cmdlen    Command length in bytes
*/
/**************************************************************************/
void NFC_PN532::writecommand(uint8_t *cmd, uint8_t cmdlen)
{
  if (spi_dev != NULL)
  {
    // SPI command write.
    uint8_t checksum;
    uint8_t packet[8 + cmdlen];
    uint8_t *p = packet;
    cmdlen++;

    p[0] = PN532_SPI_DATAWRITE;
    p++;

    p[0] = PN532_PREAMBLE;
    p++;
    p[0] = PN532_STARTCODE1;
    p++;
    p[0] = PN532_STARTCODE2;
    p++;
    checksum = PN532_PREAMBLE + PN532_STARTCODE1 + PN532_STARTCODE2;

    p[0] = cmdlen;
    p++;
    p[0] = ~cmdlen + 1;
    p++;

    p[0] = PN532_HOSTTOPN532;
    p++;
    checksum += PN532_HOSTTOPN532;

    for (uint8_t i = 0; i < cmdlen - 1; i++)
    {
      p[0] = cmd[i];
      p++;
      checksum += cmd[i];
    }

    p[0] = ~checksum;
    p++;
    p[0] = PN532_POSTAMBLE;
    p++;

#ifdef PN532DEBUG
    Serial.print("Sending : ");
    for (int i = 1; i < 8 + cmdlen; i++)
    {
      Serial.print("0x");
      Serial.print(packet[i], HEX);
      Serial.print(", ");
    }
    Serial.println();
#endif

    spi_dev->write(packet, 8 + cmdlen);
  }
  else
  {
    // I2C command write.
    uint8_t checksum;

    cmdlen++;

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F("\nSending: "));
#endif

    delay(2); // or whatever the delay is for waking up the board

    // I2C START
    WIRE.beginTransmission(PN532_I2C_ADDRESS);
    checksum = PN532_PREAMBLE + PN532_PREAMBLE + PN532_STARTCODE2;
    i2c_send(PN532_PREAMBLE);
    i2c_send(PN532_PREAMBLE);
    i2c_send(PN532_STARTCODE2);

    i2c_send(cmdlen);
    i2c_send(~cmdlen + 1);

    i2c_send(PN532_HOSTTOPN532);
    checksum += PN532_HOSTTOPN532;

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_PREAMBLE, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_PREAMBLE, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_STARTCODE2, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)cmdlen, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)(~cmdlen + 1), HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_HOSTTOPN532, HEX);
#endif

    for (uint8_t i = 0; i < cmdlen - 1; i++)
    {
      i2c_send(cmd[i]);
      checksum += cmd[i];
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print((byte)cmd[i], HEX);
#endif
    }

    i2c_send((byte)~checksum);
    i2c_send((byte)PN532_POSTAMBLE);

    // I2C STOP
    WIRE.endTransmission();

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)~checksum, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_POSTAMBLE, HEX);
    PN532DEBUGPRINT.println();
#endif
  }
}

void NFC_PN532::writecommand_Felica(uint8_t *cmd, uint8_t cmdlen, const uint8_t *body, uint8_t blen)
{
  if (spi_dev != NULL)
  {
    // SPI command write.
    uint8_t checksum;
    uint8_t packet[8 + cmdlen];
    uint8_t *p = packet;
    cmdlen++;

    p[0] = PN532_SPI_DATAWRITE;
    p++;

    p[0] = PN532_PREAMBLE;
    p++;
    p[0] = PN532_STARTCODE1;
    p++;
    p[0] = PN532_STARTCODE2;
    p++;
    checksum = PN532_PREAMBLE + PN532_STARTCODE1 + PN532_STARTCODE2;

    p[0] = cmdlen;
    p++;
    p[0] = ~cmdlen + 1;
    p++;

    p[0] = PN532_HOSTTOPN532;
    p++;
    checksum += PN532_HOSTTOPN532;

    for (uint8_t i = 0; i < cmdlen - 1; i++)
    {
      p[0] = cmd[i];
      p++;
      checksum += cmd[i];
    }

    p[0] = ~checksum;
    p++;
    p[0] = PN532_POSTAMBLE;
    p++;

#ifdef PN532DEBUG
    Serial.print("Sending : ");
    for (int i = 1; i < 8 + cmdlen; i++)
    {
      Serial.print("0x");
      Serial.print(packet[i], HEX);
      Serial.print(", ");
    }
    Serial.println();
#endif

    spi_dev->write(packet, 8 + cmdlen);
  }
  else
  {
    // I2C command write.
    uint8_t checksum;

    uint8_t length = cmdlen + blen + 1; // length of data field: TFI + DATA

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F("\nSending: "));
#endif

    delay(2); // or whatever the delay is for waking up the board

    // I2C START
    WIRE.beginTransmission(PN532_I2C_ADDRESS);
    checksum = PN532_PREAMBLE + PN532_PREAMBLE + PN532_STARTCODE2;
    i2c_send(PN532_PREAMBLE);
    i2c_send(PN532_PREAMBLE);
    i2c_send(PN532_STARTCODE2);

    i2c_send(length);
    i2c_send(~length + 1);

    i2c_send(PN532_HOSTTOPN532);
    checksum += PN532_HOSTTOPN532;

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_PREAMBLE, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_PREAMBLE, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_STARTCODE2, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)cmdlen, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)(~cmdlen + 1), HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_HOSTTOPN532, HEX);
#endif

    for (uint8_t i = 0; i < cmdlen; i++)
    {
      i2c_send(cmd[i]);
      checksum += cmd[i];
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print((byte)cmd[i], HEX);
#endif
    }

    for (uint8_t j = 0; j < blen; j++)
    {
      i2c_send(body[j]);
      checksum += body[j];
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print((byte)body[j], HEX);
#endif
    }

    i2c_send((byte)~checksum);
    i2c_send((byte)PN532_POSTAMBLE);

    // I2C STOP
    WIRE.endTransmission();

#ifdef PN532DEBUG
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)~checksum, HEX);
    PN532DEBUGPRINT.print(F(" 0x"));
    PN532DEBUGPRINT.print((byte)PN532_POSTAMBLE, HEX);
    PN532DEBUGPRINT.println();
#endif
  }
}

/***** FeliCa Functions ******/
/**************************************************************************/
/*!
    @brief  Poll FeliCa card. PN532 acting as reader/initiator,peer acting as card/responder.
    @param[in]  systemCode             Designation of System Code. When sending FFFFh as System Code,all FeliCa cards can return response.
    @param[in]  requestCode            Designation of Request Data as follows:
                                         00h: No Request
                                         01h: System Code request (to acquire System Code of the card)
                                         02h: Communication perfomance request
    @param[out] idm                    IDm of the card (8 bytes)
    @param[out] pmm                    PMm of the card (8 bytes)
    @param[out] systemCodeResponse     System Code of the card (Optional, 2bytes)
    @return                            = 1: A FeliCa card has detected
                                       = 0: No card has detected
                                       < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_Polling(uint16_t systemCode, uint8_t requestCode, uint8_t *idm, uint8_t *pmm, uint16_t *systemCodeResponse, uint16_t timeout)
{
  pn532_packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
  pn532_packetbuffer[1] = 1;
  pn532_packetbuffer[2] = 1;
  pn532_packetbuffer[3] = FELICA_CMD_POLLING;
  pn532_packetbuffer[4] = (systemCode >> 8) & 0xFF;
  pn532_packetbuffer[5] = systemCode & 0xFF;
  pn532_packetbuffer[6] = requestCode;
  pn532_packetbuffer[7] = 0;

  if (!sendCommandCheckAck(pn532_packetbuffer, 8, timeout))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Could not send Polling command");
#endif
    return -1;
  }

  /* Read the response packet */
  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer) / sizeof(byte));

  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer) / sizeof(byte));

  // Check NbTg (pn532_packetbuffer[7])
  if (pn532_packetbuffer[7] == 0)
  {
    PN532DEBUGPRINT.println("No card had detected");
    return 0;
  }
  else if (pn532_packetbuffer[7] != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print("Unhandled number of targets inlisted. NbTg: ");
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[14], HEX);
    PN532DEBUGPRINT.println();
    return -3;
#endif
  }

  inListedTag = pn532_packetbuffer[8];
#ifdef PN532DEBUG
  PN532DEBUGPRINT.print("Tag number: ");
  PN532DEBUGPRINT.print((byte)pn532_packetbuffer[14], HEX);
  PN532DEBUGPRINT.println();
#endif

  // length check
  uint8_t responseLength = pn532_packetbuffer[9];
  if (responseLength != 18 && responseLength != 20)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Wrong response length");
#endif
    return -4;
  }

  uint8_t i;
  for (i = 0; i < 8; ++i)
  {
    idm[i] = pn532_packetbuffer[11 + i];
    _felicaIDm[i] = pn532_packetbuffer[11 + i];
    pmm[i] = pn532_packetbuffer[19 + i];
    _felicaPMm[i] = pn532_packetbuffer[19 + i];
  }

  if (responseLength == 20)
  {
    *systemCodeResponse = (uint16_t)((pn532_packetbuffer[27] << 8) + pn532_packetbuffer[28]);
  }

  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa command to the currently inlisted peer

    @param[in]  command         FeliCa command packet. (e.g. 00 FF FF 00 00  for Polling command)
    @param[in]  commandlength   Length of the FeliCa command packet. (e.g. 0x05 for above Polling command )
    @param[out] response        FeliCa response packet. (e.g. 01 NFCID2(8 bytes) PAD(8 bytes)  for Polling response)
    @param[out] responselength  Length of the FeliCa response packet. (e.g. 0x11 for above Polling command )
    @return                          = 1: Success
                                     < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_SendCommand(const uint8_t *command, uint8_t commandlength, uint8_t *response, uint8_t *responseLength)
{
  if (commandlength > 0xFE)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Command length too long");
#endif
    return -1;
  }

  pn532_packetbuffer[0] = 0x40; // PN532_COMMAND_INDATAEXCHANGE;
  pn532_packetbuffer[1] = inListedTag;
  pn532_packetbuffer[2] = commandlength + 1;

  if (!sendCommandCheckAck_Felica(pn532_packetbuffer, 3, command, commandlength))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Could not send FeliCa command");
#endif
    return -2;
  }

  /* Read the response packet */
  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  // Check status (pn532_packetbuffer[0])
  if ((pn532_packetbuffer[7] & 0x3F) != 0)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print("Status code indicates an error: ");
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[7], HEX);
    PN532DEBUGPRINT.println();
#endif
    return -4;
  }

  // length check
  *responseLength = pn532_packetbuffer[8] - 1;
  /*if ( (status - 2) != *responseLength) {
    DMSG("Wrong response length\n");
    return -5;
    }*/

  memcpy(response, &pn532_packetbuffer[9], *responseLength);

  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa Request Service command

    @param[in]  numNode           length of the nodeCodeList
    @param[in]  nodeCodeList      Node codes(Big Endian)
    @param[out] keyVersions       Key Version of each Node (Big Endian)
    @return                          = 1: Success
                                     < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_RequestService(uint8_t numNode, uint16_t *nodeCodeList, uint16_t *keyVersions)
{
  if (numNode > FELICA_REQ_SERVICE_MAX_NODE_NUM)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("numNode is too large");
#endif
    return -1;
  }

  uint8_t i, j = 0;
  uint8_t cmdLen = 1 + 8 + 1 + 2 * numNode;
  uint8_t cmd[cmdLen];
  cmd[j++] = FELICA_CMD_REQUEST_SERVICE;
  for (i = 0; i < 8; ++i)
  {
    cmd[j++] = _felicaIDm[i];
  }
  cmd[j++] = numNode;
  for (i = 0; i < numNode; ++i)
  {
    cmd[j++] = nodeCodeList[i] & 0xFF;
    cmd[j++] = (nodeCodeList[i] >> 8) & 0xff;
  }

  uint8_t response[10 + 2 * numNode];
  uint8_t responseLength;

  if (felica_SendCommand(cmd, cmdLen, response, &responseLength) != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request Service command failed");
#endif
    return -2;
  }

  // length check
  if (responseLength != 10 + 2 * numNode)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request Service command failed (wrong response length)");
#endif
    return -3;
  }

  for (i = 0; i < numNode; i++)
  {
    keyVersions[i] = (uint16_t)(response[10 + i * 2] + (response[10 + i * 2 + 1] << 8));
  }
  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa Request Service command

    @param[out]  mode         Current Mode of the card
    @return                   = 1: Success
                              < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_RequestResponse(uint8_t *mode)
{
  uint8_t cmd[9];
  cmd[0] = FELICA_CMD_REQUEST_RESPONSE;
  memcpy(&cmd[1], _felicaIDm, 8);

  uint8_t response[10];
  uint8_t responseLength;
  if (felica_SendCommand(cmd, 9, response, &responseLength) != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request Response command failed");
#endif
    return -1;
  }

  // length check
  if (responseLength != 10)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request Response command failed (wrong response length)");
#endif
    return -2;
  }

  *mode = response[9];
  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa Read Without Encryption command

    @param[in]  numService         Length of the serviceCodeList
    @param[in]  serviceCodeList    Service Code List (Big Endian)
    @param[in]  numBlock           Length of the blockList
    @param[in]  blockList          Block List (Big Endian, This API only accepts 2-byte block list element)
    @param[out] blockData          Block Data
    @return                        = 1: Success
                                   < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_ReadWithoutEncryption(uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16])
{
  if (numService > FELICA_READ_MAX_SERVICE_NUM)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("numService is too large");
#endif
    return -1;
  }
  if (numBlock > FELICA_READ_MAX_BLOCK_NUM)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("numBlock is too large");
#endif
    return -2;
  }

  uint8_t i, j = 0, k;
  uint8_t cmdLen = 1 + 8 + 1 + 2 * numService + 1 + 2 * numBlock;
  uint8_t cmd[cmdLen];
  cmd[j++] = FELICA_CMD_READ_WITHOUT_ENCRYPTION;
  for (i = 0; i < 8; ++i)
  {
    cmd[j++] = _felicaIDm[i];
  }
  cmd[j++] = numService;
  for (i = 0; i < numService; ++i)
  {
    cmd[j++] = serviceCodeList[i] & 0xFF;
    cmd[j++] = (serviceCodeList[i] >> 8) & 0xff;
  }
  cmd[j++] = numBlock;
  for (i = 0; i < numBlock; ++i)
  {
    cmd[j++] = (blockList[i] >> 8) & 0xFF;
    cmd[j++] = blockList[i] & 0xff;
  }

  uint8_t response[12 + 16 * numBlock];
  uint8_t responseLength;
  if (felica_SendCommand(cmd, cmdLen, response, &responseLength) != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Read Without Encryption command failed");
#endif
    return -3;
  }

  /*PN532DEBUGPRINT.println("felica_ReadWithoutEncryption------------------------------");
   for (uint8_t j = 0; j < (12 + 16 * numBlock); j++) {
#ifdef PN532DEBUG
      PN532DEBUGPRINT.print(F(" 0x"));
      PN532DEBUGPRINT.print((byte)response[j], HEX);
#endif
    }
    PN532DEBUGPRINT.println();
*/
  // length check
  if (responseLength != 12 + 16 * numBlock)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Read Without Encryption command failed (wrong response length)");
#endif
    return -4;
  }

  // status flag check
  if (response[9] != 0 || response[10] != 0)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print("Read Without Encryption command failed (Status Flag: ");
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[9], HEX);
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[10], HEX);
    PN532DEBUGPRINT.println();
#endif
    return -5;
  }

  k = 12;
  for (i = 0; i < numBlock; i++)
  {
    for (j = 0; j < 16; j++)
    {
      blockData[i][j] = response[k++];
    }
  }

  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa Write Without Encryption command

    @param[in]  numService         Length of the serviceCodeList
    @param[in]  serviceCodeList    Service Code List (Big Endian)
    @param[in]  numBlock           Length of the blockList
    @param[in]  blockList          Block List (Big Endian, This API only accepts 2-byte block list element)
    @param[in]  blockData          Block Data (each Block has 16 bytes)
    @return                        = 1: Success
                                   < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_WriteWithoutEncryption(uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16])
{
  if (numService > FELICA_WRITE_MAX_SERVICE_NUM)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("numService is too large");
#endif
    return -1;
  }
  if (numBlock > FELICA_WRITE_MAX_BLOCK_NUM)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("numBlock is too large");
#endif
    return -2;
  }

  uint8_t i, j = 0, k;
  uint8_t cmdLen = 1 + 8 + 1 + 2 * numService + 1 + 2 * numBlock + 16 * numBlock;
  uint8_t cmd[cmdLen];
  cmd[j++] = FELICA_CMD_WRITE_WITHOUT_ENCRYPTION;
  for (i = 0; i < 8; ++i)
  {
    cmd[j++] = _felicaIDm[i];
  }
  cmd[j++] = numService;
  for (i = 0; i < numService; ++i)
  {
    cmd[j++] = serviceCodeList[i] & 0xFF;
    cmd[j++] = (serviceCodeList[i] >> 8) & 0xff;
  }
  cmd[j++] = numBlock;
  for (i = 0; i < numBlock; ++i)
  {
    cmd[j++] = (blockList[i] >> 8) & 0xFF;
    cmd[j++] = blockList[i] & 0xff;
  }
  for (i = 0; i < numBlock; ++i)
  {
    for (k = 0; k < 16; k++)
    {
      cmd[j++] = blockData[i][k];
    }
  }

  uint8_t response[11];
  uint8_t responseLength;
  if (felica_SendCommand(cmd, cmdLen, response, &responseLength) != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Write Without Encryption command failed");
#endif
    return -3;
  }

  // length check
  if (responseLength != 11)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Write Without Encryption command failed (wrong response length)");
#endif
    return -4;
  }

  // status flag check
  if (response[9] != 0 || response[10] != 0)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print("Write Without Encryption command failed (Status Flag: ");
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[9], HEX);
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[10], HEX);
    PN532DEBUGPRINT.println();
#endif
    return -5;
  }

  return 1;
}

/**************************************************************************/
/*!
    @brief  Sends FeliCa Request System Code command

    @param[out] numSystemCode        Length of the systemCodeList
    @param[out] systemCodeList       System Code list (Array length should longer than 16)
    @return                          = 1: Success
                                     < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_RequestSystemCode(uint8_t *numSystemCode, uint16_t *systemCodeList)
{
  uint8_t cmd[9];
  cmd[0] = FELICA_CMD_REQUEST_SYSTEM_CODE;
  memcpy(&cmd[1], _felicaIDm, 8);

  uint8_t response[10 + 2 * 16];
  uint8_t responseLength;
  if (felica_SendCommand(cmd, 9, response, &responseLength) != 1)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request System Code command failed");
#endif
    return -1;
  }
  *numSystemCode = response[9];

  // length check
  if (responseLength < 10 + 2 * *numSystemCode)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("Request System Code command failed (wrong response length)");
#endif
    return -2;
  }

  uint8_t i;
  for (i = 0; i < *numSystemCode; i++)
  {
    systemCodeList[i] = (uint16_t)((response[10 + i * 2] << 8) + response[10 + i * 2 + 1]);
  }

  return 1;
}

/**************************************************************************/
/*!
    @brief  Release FeliCa card
    @return                          = 1: Success
                                     < 0: error
*/
/**************************************************************************/
int8_t NFC_PN532::felica_Release()
{
  // InRelease
  pn532_packetbuffer[0] = PN532_COMMAND_INRELEASE;
  pn532_packetbuffer[1] = 0x00; // All target
#ifdef PN532DEBUG
  PN532DEBUGPRINT.println("Release all FeliCa target");
#endif

  if (!sendCommandCheckAck(pn532_packetbuffer, 2))
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.println("No ACK");
#endif
    return -1; // no ACK
  }

  // Wait card response
  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  readdata(pn532_packetbuffer, sizeof(pn532_packetbuffer));

  // Check status (pn532_packetbuffer[0])
  if ((pn532_packetbuffer[7] & 0x3F) != 0)
  {
#ifdef PN532DEBUG
    PN532DEBUGPRINT.print("Status code indicates an error: ");
    PN532DEBUGPRINT.print((byte)pn532_packetbuffer[7], HEX);
    PN532DEBUGPRINT.println();
#endif
    return -3;
  }
  return 1;
}
uint8_t NFC_PN532::getSAK() {
  return _SAK;
}

uint16_t NFC_PN532::getATQA() {
  return _ATQA;
}

