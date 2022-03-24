/**
   @file readMifareClassic.ino
   @author rakwireless.com
   @brief  This example will wait for any ISO14443A card or tag, and depending on the size of the UID will attempt to read from it.
   @version 0.1
   @date 2021-10-14
   @copyright Copyright (c) 2021
**/

#include <Wire.h>
#include <SPI.h>
#include <RAK13600-PN532.h> // Click here to get the library: http://librarymanager/All#RAK13600-PN532
#include <ArduinoJson.h>

#define PN532_IRQ (WB_IO6)
#define PN532_RESET (WB_IO5)
NFC_PN532 nfc(PN532_IRQ, PN532_RESET);
#include "utilities.h"

void setup(void)
{
	Serial.begin(115200);
	pinMode(WB_IO2, OUTPUT);
	pinMode(LED_GREEN, OUTPUT);
	pinMode(LED_BLUE, OUTPUT);
	digitalWrite(WB_IO2, HIGH); // turn on Vcc for the modules.
	greenON();
	blueON();
	delay(500);
	blueOFF();
	delay(500);
	blueON();
	delay(500);
	blueOFF();
	time_t serialTimeout = millis();
	while (!Serial)
	{
		delay(10); // for nrf52840 with native usb
		if ((millis() - serialTimeout) > 5000)
		{
			break;
		}
	}
	Serial.println("\n\n\n\nHello!");
	nfc.begin();
	uint32_t versiondata = nfc.getFirmwareVersion();
	if (!versiondata)
	{
		Serial.print("Didn't find PN53x board");
		while (1)
			; // halt
	}
	// Got ok data, print it out!
	Serial.print("Found chip PN5");
	Serial.println((versiondata >> 24) & 0xFF, HEX);
	Serial.print("Firmware ver. ");
	Serial.print((versiondata >> 16) & 0xFF, DEC);
	Serial.print('.');
	Serial.println((versiondata >> 8) & 0xFF, DEC);
	// configure board to read RFID tags
	nfc.SAMConfig();
	Serial.println("Waiting for an ISO14443A Card ...");
}

void loop(void)
{
	uint8_t success;
	uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0}; // Buffer to store the returned UID
	uint8_t uidLength;					   // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
	// Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
	// 'uid' will be populated with the UID, and uidLength will indicate
	// if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
	success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
	if (success)
	{
		blueON();
		// Display some basic information about the card
		Serial.println("Found an ISO14443A card");
		Serial.print("  UID Length: ");
		Serial.print(uidLength, DEC);
		Serial.println(" bytes");
		Serial.println("  UID values:");
		hexDump(uid, uidLength);
		uint16_t ATQA = nfc.getATQA();
		Serial.print("  ATQA: 0x");
		Serial.println(ATQA, HEX);
		if (uidLength == 4)
		{
			getTypeSize();
			uint8_t currentblock;		// Counter to keep track of which block we're on
			bool authenticated = false; // Flag to indicate if the sector is authenticated
			uint8_t buff[1024];			// Array to store block data during reads
			// Keyb on NDEF and Mifare Classic should be the same
			uint8_t keyuniversal[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
			// Now we try to go through all 16 sectors (each having 4 blocks)
			// authenticating each sector, and then dumping the blocks
			uint8_t blockSize = mSize / 16;
			for (currentblock = 0; currentblock < blockSize; currentblock++)
			{
				// Check if this is a new block so that we can reauthenticate
				if (nfc.mifareclassic_IsFirstBlock(currentblock))
					authenticated = false;
				// If the sector hasn't been authenticated, do so first
				if (!authenticated)
				{
					if (currentblock == 0)
					{
						// This will be 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF for Mifare Classic (non-NDEF!)
						// or 0xA0 0xA1 0xA2 0xA3 0xA4 0xA5 for NDEF formatted cards using key a,
						// but keyb should be the same for both (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
						success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, currentblock, 0, keyuniversal);
					}
					else
					{
						// This will be 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF for Mifare Classic (non-NDEF!)
						// or 0xD3 0xF7 0xD3 0xF7 0xD3 0xF7 for NDEF formatted cards using key a,
						// but keyb should be the same for both (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF)
						success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, currentblock, 1, keyuniversal);
					}
					if (success)
					{
						authenticated = true;
					}
					else
					{
						Serial.println("Authentication error");
					}
				}
				// If we're still not authenticated just skip the block
				if (!authenticated)
				{
					Serial.print("Block ");
					Serial.print(currentblock, DEC);
					Serial.println(" unable to authenticate");
				}
				else
				{
					// Authenticated ... we should be able to read the block now
					// Dump the data into the 'data' array
					success = nfc.mifareclassic_ReadDataBlock(currentblock, buff + (currentblock * 16));
					if (success)
					{
						// Read successful
						Serial.write('o');
					}
					else
					{
						// Oops ... something happened
						Serial.write('x');
					}
				}
			}
			Serial.println("\nMemory Dump");
			hexDump(buff, 1024);
		}
		else if (uidLength == 7)
		{
			// Serial.println("Seems to be a Mifare Ultralight tag (7 byte UID)");
			/*
			  The original MIFARE Ultralight consists of a 64 byte EEPROM.
			  The first 4 pages are for the OTP area, manufacturer data, and locking bits.
			  They are readable and some bits are writable.
			  The final 12 pages are the user read/write area. For more information see the NXP data sheet MF0ICU1.
			*/
			/*
			  The MIFARE Ultralight C consists of a 192 byte EEPROM.
			  The first 4 pages are for OTP, manufacturer data, and locking bits.
			  The next 36 pages are the user read/write area.
			  The next 4 pages are additional locking bits, counters and authentication configuration and are readable.
			  The final 4 pages are for the authentication key and are not readable.
			  For more information see the NXP data sheet MF0ICU2.
			*/
			char ptrnt[32];
			uint8_t buff[176] = {0};
			// max 192 - 4 x 4 pages if Ultralight C
			uint8_t ix = 0, i, count = 0;
			Serial.print(" . Reading up to 176 bytes: ");
			for (i = 0; i < 192; i += 4)
			{
				success = nfc.mifareultralight_ReadPage((i / 4), buff + i);
				if (success)
				{
					// Data seems to have been read... Spit it out
					// sprintf(ptrnt, "Read page %d.", (i / 4));
					Serial.write('o');
					count += 1;
				}
				else
				{
					i = 192;
				}
			}
			Serial.println(" done");
			Serial.print(" . Read count: ");
			Serial.println(count);
			if (count == 16)
				Serial.println(" . Type: original MIFARE Ultralight");
			if (count == 45)
				Serial.println(" . Type: MIFARE Ultralight C");
			Serial.println("User memory:");
			StaticJsonDocument<256> doc;
			char values[50];
			array2hex(buff, values, 9);
			doc["UID"] = values;
			array2hex(buff + 9, values, 3);
			doc["Internal"] = values;
			array2hex(buff + 12, values, 4);
			doc["OTP"] = values;
			array2hex(buff + 16, values, 48);
			doc["bytes"] = count * 4;
			if (count == 16)
			{
				// If we have the original MIFARE Ultralight, let's dump also the memory
				doc["Memory"] = values;
				serializeJsonPretty(doc, Serial);
			}
			else
			{
				// If not just the headers, then hexdump the memory
				serializeJsonPretty(doc, Serial);
				hexDump(buff + 16, count * 4 - 16);
			}
		}
		Serial.println("");
		blueOFF();
		delay(2000);
	}
}
