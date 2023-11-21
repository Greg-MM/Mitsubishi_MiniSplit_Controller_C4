#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HeatPump.h>
#include "Arduino_Secrets.h" // add header and define SECRET_* below

const char* ssid			= SECRET_SSID;
const char* password		= SECRET_WIFI_PW;
const char* password_ota	= SECRET_OTA_PW;

HeatPump HP;
WebServer MyWebServer(80);

void setup()
{
	Serial.begin(115200);
	Serial.println("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}

	ArduinoOTA.setPassword(password_ota);

	ArduinoOTA
		.onStart([]()
		{
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH)
				type = "sketch";
			else  // U_SPIFFS
				type = "filesystem";

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println("Start updating " + type);
		})
		.onEnd([]()
		{
			Serial.println("\nEnd");
		})
		.onProgress([](unsigned int progress, unsigned int total)
		{
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
		})
		.onError([](ota_error_t error)
		{
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
			else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
			else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
			else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
			else if (error == OTA_END_ERROR) Serial.println("End Failed");
		});

	ArduinoOTA.begin();

	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	HP.connect(&Serial2);
	HP.enableExternalUpdate();
	SetupWebServer();
}

void loop()
{
	ArduinoOTA.handle();
	HP.sync();
	MyWebServer.handleClient();
}

void SetupWebServer()
{
	Serial.println("Setting Up HTTP Server...");

	MyWebServer.on("/", WebRoot);
	MyWebServer.on("/metrics", WebGetMetrics);
	MyWebServer.on("/Metrics", WebGetMetrics);
	MyWebServer.on("/Status", WebGetStatus);
	MyWebServer.on("/UpdateSettings", WebUpdateSettings);

	MyWebServer.begin();
	Serial.println("HTTP Server Setup Complete!");
}

void WebUpdateSettings()
{
	MyWebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
	MyWebServer.send(200, "application/json", "");
	MyWebServer.sendContent("{");
	int Updated = 0;
	if(MyWebServer.hasArg("Mode"))
	{
		String NewMode = MyWebServer.arg("Mode");
		if(NewMode.equalsIgnoreCase("OFF"))
		{
			HP.setPowerSetting(false);
		}
		else
		{
			HP.setPowerSetting(true);
			HP.setModeSetting(Convert_Mode_C4_Library(NewMode.c_str()));
		}
		Updated = 1;
	}
	MyWebServer.sendContent(GetJSON("mode_updated", String(Updated)) + ", ");

	Updated = 0;
	if(MyWebServer.hasArg("SetPointF"))
	{
		HP.setTemperature(HP.FahrenheitToCelsius((int)MyWebServer.arg("SetPointF").toFloat())); 
		Updated = 1;
	}
	MyWebServer.sendContent(GetJSON("setpoint_f_updated", String(Updated)) + ", ");

	Updated = 0;
	if(MyWebServer.hasArg("SetPointC"))
	{
		HP.setTemperature(MyWebServer.arg("SetPointC").toFloat()); 
		Updated = 1;
	}
	MyWebServer.sendContent(GetJSON("setpoint_c_updated", String(Updated)) + ", ");

	if(MyWebServer.hasArg("FanSpeed"))
	{
		const char* Converted = Convert_FanSpeed_C4_Library(MyWebServer.arg("FanSpeed").c_str());
		HP.setFanSpeed(Converted);
		MyWebServer.sendContent(GetJSON_S("fan_speed", MyWebServer.arg("FanSpeed") + "->" + Converted) + ", ");
	}
	

	Updated = 0;
	if(MyWebServer.hasArg("VaneMode"))
	{
		HP.setVaneSetting(Convert_VaneMode_C4_Library(MyWebServer.arg("VaneMode").c_str()));
		Updated = 1;
	}
	MyWebServer.sendContent(GetJSON("vanemode_updated", String(Updated)) + ", ");

	Updated = 0;
	if(MyWebServer.hasArg("WideVaneMode"))
	{
		HP.setWideVaneSetting(Convert_WideVaneMode_C4_Library(MyWebServer.arg("WideVaneMode").c_str()));
		Updated = 1;
	}
	MyWebServer.sendContent(GetJSON("widevanemode_updated", String(Updated)) + ", ");


	MyWebServer.sendContent(GetJSON("success", "1"));
	MyWebServer.sendContent("}");
	MyWebServer.sendContent("");

	HP.update();
}

void WebRoot()
{
	MyWebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
	MyWebServer.send(200, "text/html", "<!DOCTYPE html>");
	MyWebServer.sendContent("<html>");
	MyWebServer.sendContent("<head>");
	MyWebServer.sendContent("<title>Mitsubishi Mini-Split Controller for C4</title>");
	MyWebServer.sendContent("</head>");
	MyWebServer.sendContent("<body>");
	MyWebServer.sendContent("<table>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Is Connected:</td>");
	if (HP.isConnected())
		MyWebServer.sendContent("<td>YES</td>");
	else
		MyWebServer.sendContent("<td>NO</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Current Room Temp:</td>");
	MyWebServer.sendContent("<td>" + String(HP.CelsiusToFahrenheit(HP.getRoomTemperature())) + "&deg; F / " + String(HP.getRoomTemperature(), 1) + "&deg; C</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Current SET Temp:</td>");
	MyWebServer.sendContent("<td>" + String(HP.CelsiusToFahrenheit(HP.getTemperature())) + "&deg; F / " + String(HP.getTemperature(), 1) + "&deg; C</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>IsOperating:</td>");
	if (HP.getOperating())
		MyWebServer.sendContent("<td>true</td>");
	else
		MyWebServer.sendContent("<td>false</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Compressor Frequency:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getStatus().compressorFrequency) + " Hz?</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Power Setting:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getPowerSetting()) + "</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Mode Setting:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getModeSetting()) + "</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Fan Speed:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getFanSpeed()) + "</td>");
	MyWebServer.sendContent("</tr>");


	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Vane Setting:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getVaneSetting()) + "</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("<tr>");
	MyWebServer.sendContent("<td>Wide Vane Setting:</td>");
	MyWebServer.sendContent("<td>" + String(HP.getWideVaneSetting()) + "</td>");
	MyWebServer.sendContent("</tr>");

	MyWebServer.sendContent("</table>");
	
	MyWebServer.sendContent("<script>");
	MyWebServer.sendContent("setTimeout(function(){ window.location.reload(1); }, 7500);");
	MyWebServer.sendContent("</script>");

	MyWebServer.sendContent("</body>");
	MyWebServer.sendContent("</html>");
	MyWebServer.sendContent("");
}

void WebGetMetrics()
{
	MyWebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
	MyWebServer.send(200, "text/plain", "#Prometheus Metrics\n");
	
	if (HP.isConnected())
		MyWebServer.sendContent("minisplit_connected 1\n");
	else
		MyWebServer.sendContent("minisplit_connected 0\n");

	MyWebServer.sendContent("minisplit_room_temp " + String(HP.CelsiusToFahrenheit(HP.getRoomTemperature())) + "\n");
	MyWebServer.sendContent("minisplit_set_temp " + String(HP.CelsiusToFahrenheit(HP.getTemperature())) + "\n");
	
	if (HP.getOperating())
		MyWebServer.sendContent("minisplit_compressor_operating 1\n");
	else
		MyWebServer.sendContent("minisplit_compressor_operating 1\n");

	MyWebServer.sendContent("minisplit_compressor_frequency " + String(HP.getStatus().compressorFrequency, 1) + "\n");
	if(HP.getPowerSettingBool())
		MyWebServer.sendContent("minisplit_power_status 1\n");
	else
		MyWebServer.sendContent("minisplit_power_status 0\n");

	MyWebServer.sendContent("fan_speed ");		MyWebServer.sendContent(Convert_FanSpeed_Library_Metric(HP.getFanSpeed())); MyWebServer.sendContent("\n");
	MyWebServer.sendContent("vane_mode ");		MyWebServer.sendContent(Convert_VaneMode_Library_Metric(HP.getVaneSetting())); MyWebServer.sendContent("\n");
	MyWebServer.sendContent("wide_vane_mode ");	MyWebServer.sendContent(Convert_WideVaneMode_Library_Metric(HP.getWideVaneSetting())); MyWebServer.sendContent("\n");
	
	MyWebServer.sendContent("");
}

void WebGetStatus()
{
	MyWebServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
	MyWebServer.send(200, "application/json", "");
	
	MyWebServer.sendContent("{");
	MyWebServer.sendContent(GetJSON("connected",	String(HP.isConnected()))	+ ",");
	
	MyWebServer.sendContent(GetJSON("power_status",	String(HP.getPowerSettingBool()))	+ ",");

	if(HP.getPowerSettingBool())
		MyWebServer.sendContent(GetJSON_S("mode",   Convert_Mode_Library_C4(HP.getModeSetting()))+ ",");
	else
		MyWebServer.sendContent(GetJSON_S("mode",   "Off")+ ",");

	MyWebServer.sendContent(GetJSON("room_temp_f",			String(HP.CelsiusToFahrenheit(HP.getRoomTemperature()))	+ ","));
	MyWebServer.sendContent(GetJSON("room_temp_f_float",	String(CelsiusToFahrenheitFloat(HP.getRoomTemperature()),2)	+ ","));
	MyWebServer.sendContent(GetJSON("room_temp_c",			String(HP.getRoomTemperature(), 1)	+ ","));
	
	MyWebServer.sendContent(GetJSON("set_temp_f",			String(HP.CelsiusToFahrenheit(HP.getTemperature())) + ","));
	MyWebServer.sendContent(GetJSON("set_temp_f_float",		String(CelsiusToFahrenheitFloat(HP.getTemperature()),2)	+ ","));
	MyWebServer.sendContent(GetJSON("set_temp_c",			String(HP.getTemperature(), 1) + ","));
	
	MyWebServer.sendContent(GetJSON("compressor_operating",	String(HP.getOperating()))	+ ",");
	MyWebServer.sendContent(GetJSON("compressor_frequency",	String(HP.getStatus().compressorFrequency) + ","));
	
	MyWebServer.sendContent(GetJSON_S("fan_speed",			Convert_FanSpeed_Library_C4(HP.getFanSpeed())) + ",");
	MyWebServer.sendContent(GetJSON_S("vane_mode",			Convert_VaneMode_Library_C4(HP.getVaneSetting())) + ",");
	MyWebServer.sendContent(GetJSON_S("wide_vane_mode",		Convert_WideVaneMode_Library_C4(HP.getWideVaneSetting())) + "");
	
	MyWebServer.sendContent("}");
	MyWebServer.sendContent("");
}

String GetJSON(String Name, String Value)
{
	return "\"" + Name + "\":" + Value;
}
String GetJSON_S(String Name, String Value)
{
	return "\"" + Name + "\":\"" + Value + "\"";
}

const char* MODE_MAP[5]			= {"HEAT", "DRY", "COOL", "FAN", "AUTO"};// <[LIBRARY]>
const char* Modes[5]			= {"Heat", "Dry", "Cool", "Fan", "Auto"};

const char* FAN_MAP[6]			= {"AUTO", "QUIET", "1", "2", "3", "4"};// <[LIBRARY]>
const char* FanSpeeds[6]		= {"Auto", "Silent", "Low", "Medium", "High", "Max"};
const char* M_FanSpeeds[6]		= {"0", "1", "2", "3", "4", "5"};

const char* VANE_MAP[7]			= {"AUTO", "1", "2", "3", "4", "5", "SWING"};// <[LIBRARY]>
const char* VaneModes[7]		= {"Auto", "1", "2", "3", "4", "5", "Swing"};
const char* M_VaneModes[7]		= {"0", "1", "2", "3", "4", "5", "9"};

const char* WIDEVANE_MAP[7]		= {"<<", "<", "|", ">", ">>", "<>", "SWING"};// <[LIBRARY]>
const char* WideVaneModes[7]	= {"FAR_LEFT", "LEFT", "CENTER", "RIGHT", "FAR_RIGHT", "<>", "SWING"};
const char* M_WideVaneModes[7]	= {"0", "1", "2", "3", "4", "8", "9"};

const char* Convert_Mode_Library_C4(const char* Mode)				{ return	ConvertMaps(MODE_MAP, Modes, 5, Mode); }
const char* Convert_Mode_C4_Library(const char* Mode)				{ return	ConvertMaps(Modes, MODE_MAP, 5, Mode); }

const char* Convert_FanSpeed_Library_Metric(const char* Mode)		{ return	ConvertMaps(FAN_MAP, M_FanSpeeds, 6, Mode); }
const char* Convert_FanSpeed_Library_C4(const char* Mode)			{ return	ConvertMaps(FAN_MAP, FanSpeeds, 6, Mode); }
const char* Convert_FanSpeed_C4_Library(const char* Mode)			{ return	ConvertMaps(FanSpeeds, FAN_MAP, 6, Mode); }

const char* Convert_VaneMode_Library_Metric(const char* Mode)		{ return	ConvertMaps(VANE_MAP, M_VaneModes, 7, Mode); }
const char* Convert_VaneMode_Library_C4(const char* Mode)			{ return	ConvertMaps(VANE_MAP, VaneModes, 7, Mode); }
const char* Convert_VaneMode_C4_Library(const char* Mode)			{ return	ConvertMaps(VaneModes, VANE_MAP, 7, Mode); }

const char* Convert_WideVaneMode_Library_Metric(const char* Mode)	{ return	ConvertMaps(WIDEVANE_MAP, M_WideVaneModes, 7, Mode); }
const char* Convert_WideVaneMode_Library_C4(const char* Mode)		{ return	ConvertMaps(WIDEVANE_MAP, WideVaneModes, 7, Mode); }
const char* Convert_WideVaneMode_C4_Library(const char* Mode)		{ return	ConvertMaps(WideVaneModes, WIDEVANE_MAP, 7, Mode); }

const char* ConvertMaps(const char* Source[], const char* Destination[], int Length, const char* Value)
{
	for(int i=0 ; i < Length ; i++)
	{
		if(strcmp(Source[i], Value) == 0)
			return Destination[i];
	}
	return "UNKNOWN";
}


float CelsiusToFahrenheitFloat(float tempC)
{
	float temp = (tempC * 1.8) + 32;
	return (temp);
}