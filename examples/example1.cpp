#include "ESP32FtpServer.h"
#include <LittleFS.h>
#include <LogLibrary.h>
#include <WiFi.h>
#include <WiFiClient.h>

#define UART_RX_PIN 44
#define UART_TX_PIN 43

#define MAIN_SERIAL_BAUDRATE 115200

const char *ssid = "YourSSID";         // WiFi SSID
const char *password = "YourPassword"; // WiFi Password

HardwareSerial HardwareSerialPort(0);
FtpServer ftpSrv; // set #define FTP_DEBUG in ESP32FtpServer.h to see ftp verbose on serial

void setup(void)
{
  HardwareSerialPort.begin(MAIN_SERIAL_BAUDRATE, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Log::begin(&HardwareSerialPort);
  Log::enableColors(false);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    LOG_DEBUG(".");
  }
  LOG_DEBUG("\n");
  LOG_DEBUG("WiFi connected to %s", WiFi.SSID());

  LOG_INFO("IP address: %s", WiFi.localIP().toString().c_str());

  if (!LittleFS.begin())
  {
    LOG_ERROR("Falha ao montar LittleFS!");
    return;
  }
  LOG_INFO("LittleFS montado com sucesso");

  ftpSrv.begin("esp32", "esp32");
}

void loop(void)
{
  ftpSrv.handleFTP();
}
