void greenON()
{
	digitalWrite(LED_GREEN, HIGH);
}

void greenOFF()
{
	digitalWrite(LED_GREEN, LOW);
}

void blueON()
{
	digitalWrite(LED_BLUE, HIGH);
}

void blueOFF()
{
	digitalWrite(LED_BLUE, LOW);
}

void hexDump(unsigned char *buf, uint16_t len)
{
	char alphabet[17] = "0123456789abcdef";
	Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
	Serial.print(F("   |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |\n"));
	for (uint16_t i = 0; i < len; i += 16)
	{
		if (i % 128 == 0)
			Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
		char s[] = "|                                                | |                |\n";
		uint8_t ix = 1, iy = 52;
		for (uint8_t j = 0; j < 16; j++)
		{
			if (i + j < len)
			{
				uint8_t c = buf[i + j];
				s[ix++] = alphabet[(c >> 4) & 0x0F];
				s[ix++] = alphabet[c & 0x0F];
				ix++;
				if (c > 31 && c < 128)
					s[iy++] = c;
				else
					s[iy++] = '.';
			}
		}
		uint8_t index = i / 16;
		if (i < 256)
			Serial.write(' ');
		Serial.print(index, HEX);
		Serial.write('.');
		Serial.print(s);
	}
	Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
}

void array2hex(uint8_t *buf, char *x, size_t sLen)
{
	size_t i, len, n = 0;
	const char *alphabet = "0123456789ABCDEF";
	for (i = 0; i < sLen; ++i)
	{
		x[n++] = alphabet[(buf[i] >> 4) & 0xF];
		x[n++] = alphabet[buf[i] & 0xF];
	}
	x[n++] = 0;
}

uint8_t mType;
uint16_t mSize;
bool mIsEmulated;
#define TYPE_CLASSIC 0
#define TYPE_PLUS 1
#define TYPE_PRO 2
#define SIZE_MINI 320
#define SIZE_1K 1024
#define SIZE_2K 2048
#define SIZE_4K 4096

void getTypeSize()
{
	// https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/nfc/tech/MifareClassic.java
	uint8_t SAQ = nfc.getSAK();
	Serial.println("SAK = 0x" + String(SAQ, HEX));
	mIsEmulated = false;
	switch (SAQ)
	{
	case 0x01:
	case 0x08:
		mType = TYPE_CLASSIC;
		mSize = SIZE_1K;
		break;
	case 0x09:
		mType = TYPE_CLASSIC;
		mSize = SIZE_MINI;
		break;
	case 0x10:
		mType = TYPE_PLUS;
		mSize = SIZE_2K;
		// SecLevel = SL2
		break;
	case 0x11:
		mType = TYPE_PLUS;
		mSize = SIZE_4K;
		// Seclevel = SL2
		break;
	case 0x18:
		mType = TYPE_CLASSIC;
		mSize = SIZE_4K;
		break;
	case 0x28:
		mType = TYPE_CLASSIC;
		mSize = SIZE_1K;
		mIsEmulated = true;
		break;
	case 0x38:
		mType = TYPE_CLASSIC;
		mSize = SIZE_4K;
		mIsEmulated = true;
		break;
	case 0x88:
		mType = TYPE_CLASSIC;
		mSize = SIZE_1K;
		// NXP-tag: false
		break;
	case 0x98:
	case 0xB8:
		mType = TYPE_PRO;
		mSize = SIZE_4K;
		break;
	default:
		// Stack incorrectly reported a MifareClassic. We cannot handle this
		// gracefully - we have no idea of the memory layout. Bail.
		Serial.println("Tag incorrectly enumerated as MIFARE Classic, SAK = " + String(SAQ));
		return;
	}
	Serial.println("mType: " + String(mType));
	Serial.println("mSize: " + String(mSize));
}
