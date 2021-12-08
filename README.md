| <center><img src="./assets/rakstar.jpg" alt="RAKstar" width=25%></center>  | ![RAKWireless](./assets/RAK-Whirls.png) | [![Build Status](https://github.com/RAKWireless/RAK13600-PN532/workflows/RAK%20Library%20Build%20CI/badge.svg)](https://github.com/RAKWireless/RAK13600-PN532/actions) |
| -- | -- | -- |

# RAK13600

PN532 is the NFC chip of NXP, which can read and write NFC cards. This library provides basic support for configuring functions and reading data.

[*RAKwireless RAK13600 RFID reader*](https://store.rakwireless.com/products/rfid-module-rak13600)

# Documentation

* **[Product Repository](https://github.com/RAKWireless/RAK13600-PN532)** - Product repository for the RAKWireless RAK13600 NFC module.
* **[Documentation](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK13600/Overview/)** - Documentation and Quick Start Guide for the RAK13600 NFC module.

# Installation

In Arduino IDE, open Sketch->Include Library->Manage Libraries then search for RAK13600.

In PlatformIO open PlatformIO Home, switch to libraries and search for RAK13600.
Or install the library project dependencies by adding:

```log
lib_deps = rakwireless/RAK13600-PN532 library
```

into **`platformio.ini`**

For manual installation, download the archive, unzip it and place the RAK13600-PN532 folder into the library directory.
In Arduino IDE this is usually <arduinosketchfolder>/libraries/
In PlatformIO this is usually <user/.platformio/lib>

# Usage

The library provides an NFC class that allows you to communicate with PN532 and get the chip data. Check out the examples how to get the chip data.

## This class provides the following methods:
**`NFC_PN532(uint8_t irq, uint8_t reset);`**    
Constructor for NFC interface. Instantiates a new PN532 class using I2C.
Parameters:

| Direction | Name | Function |
| --------- | ---- | :------- |
| in        | irq | Location of the IRQ pin |
| in | reset | Location of the reset pin |
| return    |          | none                                              |

**`void begin();`**    
PN532 initialization.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| return    |      | none     |

**`bool SAMConfig(void);`**    
Configures the SAM (Secure Access Module).
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| return | | TRUE if everything executed properly, FALSE for an error |

**`uint32_t getFirmwareVersion(void);`**    
Checks the firmware version of the PN5xx chip.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| return |  | The chip's firmware version and ID |

**`bool sendCommandCheckAck(uint8_t *cmd, uint8_t cmdlen, uint16_t timeout = 100);`**     
Before conversion, wait for the LC sensor to stabilize for a short time.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | cmd | Pointer to the command buffer |
| in | cmdlen | The size of the command in byte |
| in | timeout | Timeout before giving up |
| return |  | TRUE if everything is OK, FALSE if timeout occured before an ACK was received |

**`bool setPassiveActivationRetries(uint8_t maxRetries);`**    
Sets the MxRtyPassiveActivation byte of the RFConfiguration register.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | maxRetries | 0xFF to wait forever, 0x00..0xFE to time out after mxRetries |
| return    |         | TRUE if everything executed properly, FALSE for an error |

**`bool readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout = 0);`**     
Waits for an ISO14443A_B target to enter the field.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in | cardbaudrate | Baud rate of the card |
| out | uid | Pointer to the array that will be populated with the card's UID (up to 7 bytes) |
| in | uidLength | Pointer to the variable that will hold the length of the card's UID |
| in | timeout | Timeout before giving up |
|  return | | TRUE if everything executed properly, FALSE for an error |

**`bool startPassiveTargetIDDetection(uint8_t cardbaudrate);`**     
Puts PN532 into passive detection state with IRQ while waiting for an ISO14443A target.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | cardbaudrate | Baud rate of the card                                    |
| return    |       | TRUE if everything executed properly, FALSE for an error |

**`bool inDataExchange(uint8_t *send, uint8_t sendLength, uint8_t *response, uint8_t *responseLength);`**       
Exchanges an APDU with the currently enlisted peer
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | send | Pointer to data to send                   |
| in | sendLength | Length of the data to send |
| in | response | Pointer to response data |
| in | responseLength | Pointer to the response data length |
| return    |       | TRUE if everything executed properly, FALSE for an error |

**`bool inListPassiveTarget(uint8_t cardBaudRate);`**       
'InLists' a passive target. PN532 acting as reader/initiator, peer acting as card/responder.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in | cardBaudRate | Baud rate of the card |
|  return | | Returns 1 if the configuration is successful otherwise returns 0 |

**`uint8_t AsTarget();`**     
Set the PN532 as ISO14443A target behaving as a Smart Card
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| return    |       | TRUE if everything executed properly, FALSE for an error |

**`uint8_t getDataTarget(uint8_t *cmd, uint8_t *cmdlen);`**    
Retrieve response from the emulation mode.
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| out     | cmd | Get data |
| in | cmdlen | Data length |
|  return | | Returns 1 if the configuration is successful, otherwise returns 0 |

**`uint8_t setDataTarget(uint8_t *cmd, uint8_t cmdlen);`**    
Set data in PN532 in the emulation mode
Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | cmd    | Set data                                                     |
| in        | cmdlen | Data length                                                  |
| return | | Returns 1 if the configuration is successful otherwise returns 0 |

**`bool mifareclassic_IsFirstBlock(uint32_t uiBlock);`**     

Indicates whether the specified block number is the first block in the sector (block 0 relative to the current sector).

Parameters:

| Direction | Name | Function |
| --------- | ---- | -------- |
| in        | uiBlock | Input the block number          |
|  return | | TRUE if everything executed properly, FALSE for an error |

**`bool mifareclassic_IsTrailerBlock(uint32_t uiBlock);`**     

Indicates whether the specified block number is the sector trailer.

Parameters:

| Direction | Name    | Function                                                 |
| --------- | ------- | -------------------------------------------------------- |
| in        | uiBlock | Input the block number                                   |
| return    |         | TRUE if everything executed properly, FALSE for an error |

**`uint8_t mifareclassic_AuthenticateBlock(uint8_t *uid, uint8_t uidLen, uint32_t blockNumber, uint8_t keyNumber, uint8_t *keyData);`**     

Tries to authenticate a block of memory on a MIFARE card using the INDATAEXCHANGE command. See section 7.3.8 of the PN532 User Manual for more information on sending MIFARE and other commands.

Parameters:

| Direction | Name        | Function                                                     |
| --------- | ----------- | ------------------------------------------------------------ |
| in        | uid         | Pointer to a byte array containing the card UID              |
| in        | uidLen      | The length (in bytes) of the card's UID (should  be 4 for MIFARE Classic) |
| in        | blockNumber | The block number to authenticate. (0..63 for 1KB cards, and 0..255 for 4KB cards). |
| in        | keyNumber   | Which key type to use during authentication (0 = MIFARE_CMD_AUTH_A, 1 = MIFARE_CMD_AUTH_B) |
| in        | keyData     | Pointer to a byte array containing the 6-byte key value      |
| return    |             | 1 if everything executed properly, 0 for an error            |

**`uint8_t mifareclassic_ReadDataBlock(uint8_t blockNumber, uint8_t *data);`**      

Tries to read an entire 16-byte data block at the specified block address.

Parameters:

| Direction | Name        | Function                                                     |
| --------- | ----------- | ------------------------------------------------------------ |
| in        | blockNumber | The block number to authenticate.  (0..63 for 1KB cards, and 0..255 for 4KB cards) |
| out       | data        | Pointer to the byte array that will hold the retrieved data (if any) |
| return    |             | 1 if everything executed properly, 0 for an error            |

**`uint8_t mifareclassic_WriteDataBlock(uint8_t blockNumber, uint8_t *data);`**      

Tries to write an entire 16-byte data block at the specified block address.

Parameters:

| Direction | Name        | Function                                                     |
| --------- | ----------- | ------------------------------------------------------------ |
| in        | blockNumber | The block number to authenticate.  (0..63 for 1KB cards, and 0..255 for 4KB cards) |
| out       | data        | The byte array that contains the data to write               |
| return    |             | 1 if everything executed properly, 0 for an error            |

**`uint8_t mifareclassic_FormatNDEF(void);`**      

Formats a Mifare Classic card to store NDEF Records.

Parameters:

| Direction | Name | Function                                          |
| --------- | ---- | ------------------------------------------------- |
| return    |      | 1 if everything executed properly, 0 for an error |

**`uint8_t mifareclassic_WriteNDEFURI(uint8_t sectorNumber, uint8_t uriIdentifier, const char *url);`**      

Writes an NDEF URI Record to the specified sector (1..15).

Parameters:

| Direction | Name          | Function                                                     |
| --------- | ------------- | ------------------------------------------------------------ |
| in        | sectorNumber  | The sector that the URI record should be written to (can be 1..15 for a 1K card) |
| in        | uriIdentifier | UriIdentifier The uri identifier code (0 = none, 0x01 = "http://www.", etc.) |
| in        | url           | The URI text to write (max 38 characters)                    |
| return    |               | 1 if everything executed properly, 0 for an error            |

**`uint8_t mifareultralight_ReadPage(uint8_t page, uint8_t *buffer);`**      

Tries to read an entire 4-byte page at the specified address.

Parameters:

| Direction | Name   | Function                                                     |
| --------- | ------ | ------------------------------------------------------------ |
| in        | page   | The page number (0..63 in most cases)                        |
| out       | buffer | Pointer to the byte array that will hold the retrieved data (if any) |
| return    |        | 1 if everything executed properly, 0 for an error            |

**`uint8_t mifareultralight_WritePage(uint8_t page, uint8_t *buffer);`**      

Tries to write an entire 4-byte page at the specified block address.

Parameters:

| Direction | Name   | Function                                                     |
| --------- | ------ | ------------------------------------------------------------ |
| in        | page   | The page number (0..63 in most cases)                        |
| in        | buffer | The byte array that contains the data to write. Should be exactly 4 bytes long |
| return    |        | 1 if everything executed properly, 0 for an error            |

**`uint8_t ntag2xx_ReadPage(uint8_t page, uint8_t *buffer);`**      

Tries to read an entire 4-byte page at the specified address.

Parameters:

| Direction | Name   | Function                                                     |
| --------- | ------ | ------------------------------------------------------------ |
| in        | page   | The page number (0..63 in most cases)                        |
| out       | buffer | Pointer to the byte array that will hold the retrieved data (if any) |
| return    |        | 1 if everything executed properly, 0 for an error            |

**`uint8_t ntag2xx_WritePage(uint8_t page, uint8_t *buffer);`**      

Tries to write an entire 4-byte page at the specified block address.

Parameters:

| Direction | Name   | Function                                                     |
| --------- | ------ | ------------------------------------------------------------ |
| in        | page   | The page number (0..63 in most cases)                        |
| in        | buffer | The byte array that contains the data to write. Should be exactly 4 bytes long |
| return    |        | 1 if everything executed properly, 0 for an error            |

**`uint8_t ntag2xx_WriteNDEFURI(uint8_t uriIdentifier, char *url, uint8_t dataLen);`**      

Writes an NDEF URI Record starting at the specified page (4..nn).

Parameters:

| Direction | Name          | Function                                                     |
| --------- | ------------- | ------------------------------------------------------------ |
| in        | uriIdentifier | UriIdentifier The uri identifier code (0 = none, 0x01 ="http://www.", etc.) |
| in        | url           | The uri text to write (null-terminated string)               |
| in        | dataLen       | The size of the data area for overflow checks                |
| return    |               | 1 if everything executed properly, 0 for an error            |

**`int8_t felica_Polling(uint16_t systemCode, uint8_t requestCode, uint8_t *idm, uint8_t *pmm, uint16_t *systemCodeResponse, uint16_t timeout = 1000);`**       

Poll FeliCa card. PN532 acting as reader/initiator, peer acting as card/responder.

Parameters:

| Direction | Name               | Function                                                     |
| --------- | ------------------ | ------------------------------------------------------------ |
| in        | systemCode         | Designation of System Code. When sending FFFFh as System Code, all FeliCa cards can return response |
| in        | requestCode        | Designation of Request Data as follows: 00h: No Request; 01h: System Code request (to acquire System Code of the card)、02h: Communication perfomance request |
| out       | idm                | IDm of the card (8 bytes)                                    |
| out       | pmm                | PMm of the card (8 bytes)                                    |
| out       | systemCodeResponse | System Code of the card (Optional, 2bytes)                   |
| in        | timeout            | Timeout before giving up                                     |
| return    |                    | return 1: A FeliCa card has detected; return  0: No card has detected、return < 0: error |

**`int8_t felica_SendCommand (const uint8_t * command, uint8_t commandlength, uint8_t * response, uint8_t * responseLength);`**      

Sends FeliCa command to the currently inlisted peer.

Parameters:

| Direction | Name           | Function                                                     |
| --------- | -------------- | ------------------------------------------------------------ |
| in        | command        | FeliCa command packet. (e.g. 00 FF FF 00 00  for Polling command) |
| in        | commandlength  | Length of the FeliCa command packet. (e.g. 0x05 for above Polling command ) |
| out       | response       | FeliCa response packet. (e.g. 01 NFCID2(8 bytes) PAD(8 bytes)  for Polling response) |
| out       | responseLength | Length of the FeliCa response packet. (e.g. 0x11 for above Polling command ) |
| return    |                | return 1: success; return < 0: error                        |

**`int8_t felica_RequestService(uint8_t numNode, uint16_t *nodeCodeList, uint16_t *keyVersions);`**       

Sends FeliCa Request Service command.

Parameters:

| Direction | Name         | Function                              |
| --------- | ------------ | ------------------------------------- |
| in        | numNode      | length of the nodeCodeList            |
| in        | nodeCodeList | Node codes(Big Endian)                |
| out       | keyVersions  | Key Version of each Node (Big Endian) |
| return    |              | return 1: success; return < 0: error |

**`int8_t felica_RequestResponse(uint8_t *mode);`**      

Sends FeliCa Request Service command.

Parameters:

| Direction | Name | Function                              |
| --------- | ---- | ------------------------------------- |
| out       | mode | Current Mode of the card              |
| return    |      | return 1: success; return < 0: error |

**`int8_t felica_ReadWithoutEncryption (uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16]);`**      

Sends FeliCa Read Without Encryption command.

Parameters:

| Direction | Name            | Function                                                     |
| --------- | --------------- | ------------------------------------------------------------ |
| in        | numService      | Length of the serviceCodeList                                |
| in        | serviceCodeList | Service Code List (Big Endian)                               |
| in        | numBlock        | Length of the blockList                                      |
| in        | blockList       | Block List (Big Endian. This API only accepts 2-byte block list elements) |
| out       | blockData       | Block Data                                                   |
| return    |                 | return 1: success; return < 0: error                        |

**`int8_t felica_WriteWithoutEncryption (uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16]);`**       

Sends FeliCa Write Without Encryption command

Parameters:

| Direction | Name            | Function                                                     |
| --------- | --------------- | ------------------------------------------------------------ |
| in        | numService      | Length of the serviceCodeList                                |
| in        | serviceCodeList | Service Code List (Big Endian)                               |
| in        | numBlock        | Length of the blockList                                      |
| in        | blockList       | Block List (Big Endian. This API only accepts 2-byte block list element) |
| in        | blockData       | Block Data (each Block has 16 bytes)                         |
| return    |                 | return 1: success; return < 0: error                        |

**`int8_t felica_RequestSystemCode(uint8_t *numSystemCode, uint16_t *systemCodeList);`**      

Sends FeliCa Request System Code command

Parameters:

| Direction | Name           | Function                                              |
| --------- | -------------- | ----------------------------------------------------- |
| out       | numSystemCode  | Length of the systemCodeList                          |
| out       | systemCodeList | System Code list (Array length should be longer than 16) |
| return    |                | return 1: success; return < 0: error                 |
