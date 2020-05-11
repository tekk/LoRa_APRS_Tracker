#include <Arduino.h>
#include <LoRa.h>
#include <APRS-Decoder.h>
#if !defined(ARDUINO_T_Beam_V0_7)
#include <axp20x.h>
#endif
#include <TinyGPS++.h>

#include "settings.h"
#include "display.h"

#if !defined(ARDUINO_T_Beam_V0_7)
#define RXPin 12
#define TXPin 34
#else
#define RXPin 15
#define TXPin 12
#endif

void setup_lora();
String create_lat_aprs(RawDegrees lat);
String create_long_aprs(RawDegrees lng);

HardwareSerial ss(1);
#if !defined(ARDUINO_T_Beam_V0_7)
AXP20X_Class axp;
#endif
TinyGPSPlus gps;
int next_update = -1;

void setup()
{
	Serial.begin(115200);
	setup_display();
	
	delay(500);
	Serial.println("[INFO] LoRa APRS Tracker by OE5BPA (Peter Buchegger)");
	show_display("OE5BPA", "LoRa APRS Tracker", "by Peter Buchegger", 2000);

#if !defined(ARDUINO_T_Beam_V0_7)
	Wire.begin(SDA, SCL);
	if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS))
	{
		Serial.println("LoRa-APRS / Init / AXP192 Begin PASS");
	} else {
		Serial.println("LoRa-APRS / Init / AXP192 Begin FAIL");
	}
	axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);  // LORA
	axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);  // GPS
	axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON); // OLED
	axp.setDCDC1Voltage(3300);
#endif

	ss.begin(9600, SERIAL_8N1, TXPin, RXPin);

	setup_lora();
	
	delay(500);
}

#define BROADCAST_TIMEOUT 1

void loop()
{
	static int update_min = -1;
	while (ss.available() > 0)
	{
		char c = ss.read();
		//Serial.print(c);
		gps.encode(c);
	}

	if(gps.time.isUpdated())
	{
		if(gps.time.isValid()
			&& (gps.time.minute() == update_min || update_min == -1)
			&& gps.location.isValid()
			&& gps.location.isUpdated())
		{
			APRSMessage msg;
			msg.setSource("OE5BPA-9");
			msg.setDestination("APRS");
			char body_char[50];
			sprintf(body_char, "=%s/%s>LoRa APRS Tracker test", create_lat_aprs(gps.location.rawLat()).c_str(), create_long_aprs(gps.location.rawLng()).c_str());
			msg.getAPRSBody()->setData(String(body_char));
			String data = msg.encode();
			Serial.println(data);
			show_display("<< TX >>", data);
			LoRa.beginPacket();
			// Header:
			LoRa.write('<');
			LoRa.write(0xFF);
			LoRa.write(0x01);
			// APRS Data:
			LoRa.write((const uint8_t *)data.c_str(), data.length());
			LoRa.endPacket();
			update_min = (gps.time.minute() + BROADCAST_TIMEOUT) % 60;
		}
		show_display("OE5BPA",
			String("Time: ") + gps.time.hour() + String(":") + gps.time.minute() + String(":") + gps.time.second(),
			String("Date: ") + gps.date.day()  + String(".") + gps.date.month()  + String(".") + gps.date.year(),
			String("Sat's: ") + gps.satellites.value() + String(" HDOP: ") + gps.hdop.hdop(),
			String("Lat: ") + gps.location.lat() + String(" Lng: ") + gps.location.lng(),
			String("") + create_lat_aprs(gps.location.rawLat()) + String(" ") + create_long_aprs(gps.location.rawLng())
			);
	}

	if(millis() > 5000 && gps.charsProcessed() < 10)
	{
		Serial.println("No GPS detected!");
	}
}

void setup_lora()
{
	Serial.println("[INFO] Set SPI pins!");
	SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
	LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
	Serial.println("[INFO] Set LoRa pins!");

	long freq = 433775000;
	Serial.print("[INFO] frequency: ");
	Serial.println(freq);
	if (!LoRa.begin(freq)) {
		Serial.println("[ERROR] Starting LoRa failed!");
		show_display("ERROR", "Starting LoRa failed!");
		while (1);
	}
	LoRa.setSpreadingFactor(12);
	LoRa.setSignalBandwidth(125E3);
	LoRa.setCodingRate4(5);
	LoRa.enableCrc();

	LoRa.setTxPower(20);
	Serial.println("[INFO] LoRa init done!");
	show_display("INFO", "LoRa init done!", 2000);
}

String create_lat_aprs(RawDegrees lat)
{
	char str[20];
	char n_s = 'N';
	if(lat.negative)
	{
		n_s = 'S';
	}
	sprintf(str, "%02d%05.2f%c", lat.deg, lat.billionths / 1000000000.0 * 60.0, n_s);
	String lat_str(str);
	return lat_str;
}

String create_long_aprs(RawDegrees lng)
{
	char str[20];
	char e_w = 'E';
	if(lng.negative)
	{
		e_w = 'W';
	}
	sprintf(str, "%03d%05.2f%c", lng.deg, lng.billionths / 1000000000.0 * 60.0, e_w);
	String lng_str(str);
	return lng_str;
}