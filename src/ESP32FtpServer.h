
/*
 *  FTP SERVER FOR ESP32 S3
 * based on FTP Serveur for Arduino Due and Ethernet shield (W5100) or WIZ820io (W5200)
 * based on Jean-Michel Gallego's work
 * modified to work with esp8266 SPIFFS by David Paiva (david@nailbuster.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//  2017: modified by @robo8080
//  2019: modified by @HenrikSte
//  2025: modified by @Cturqueti

/*******************************************************************************
 **                                                                            **
 **                       DEFINITIONS FOR FTP SERVER                           **
 **                                                                            **
 *******************************************************************************/

// Uncomment to print debugging info to console attached to ESP32
#define FTP_DEBUG

#ifndef FTP_SERVERESP_H
#define FTP_SERVERESP_H

#include <FS.h>
#include <LittleFS.h>
// #include <WiFiClient.h>
#include <WiFi.h>
#ifdef FTP_DEBUG
#include <LogLibrary.h>
#endif

#define FTP_SERVER_VERSION "1.2.0"

// FTP Server Configuration
#define FTP_CTRL_PORT 21
#define FTP_DATA_PORT_PASV 55600
#define FTP_TIME_OUT 5
#define FTP_CMD_SIZE 256
#define FTP_CWD_SIZE 512
#define FTP_FIL_SIZE 128
#define FTP_BUF_SIZE 512

// Debug options
// #define FTP_DEBUG

// FTP Server States
enum
{
  FTP_CMD_IDLE = 0,
  FTP_CMD_WAIT_CONNECTION,
  FTP_CMD_READY,
  FTP_CMD_WAIT_USER,
  FTP_CMD_WAIT_PASS,
  FTP_CMD_WAIT_COMMAND
};

// Data Connection Types
enum
{
  FTP_DATA_PASSIVE = 0,
  FTP_DATA_ACTIVE
};

// Transfer Status
enum
{
  FTP_TRANSFER_IDLE = 0,
  FTP_TRANSFER_RETR,
  FTP_TRANSFER_STOR
};

class FtpServer
{
public:
  FtpServer();

  // Server management
  void begin(const String &username, const String &password);
  bool handleFTP();

  // Configuration
  void setActiveTimeout(uint32_t timeout);
  void setPassivePort(uint16_t port);
  void setMaxLoginAttempts(uint8_t attempts);

private:
  // Server state
  static WiFiServer ftpServer;
  static WiFiServer dataServer;

  // Clients
  WiFiClient _client;
  WiFiClient _data;

  // Authentication
  String _username;
  String _password;
  uint8_t _maxAttempts;
  uint8_t _currentAttempts;

  // Connection parameters
  IPAddress _dataIp;
  uint16_t _dataPort;
  uint8_t _dataConnType;
  uint32_t _activeTimeout;
  uint16_t _passivePort;

  // File transfer
  File _file;
  uint8_t _transferStatus;
  uint32_t _bytesTransferred;
  uint32_t _millisBeginTransfer;

  // Command processing
  char _command[6]; // FTP commands are 4 chars max
  char *_parameters;
  char _cwd[FTP_CWD_SIZE];
  char _renameFrom[FTP_CWD_SIZE];
  bool _rnfrCmd;

  // Buffers and timing
  char _cmdLine[FTP_CMD_SIZE];
  char _buffer[FTP_BUF_SIZE];
  uint16_t _cmdBufferIndex;
  uint8_t _cmdStatus;
  uint32_t _millisDelay;
  uint32_t _millisEndConnection;

  // Private methods
  void initVariables();
  void clientConnected();
  void disconnectClient();
  bool authenticateUser();
  bool authenticatePassword();
  bool processCommand();
  bool dataConnect();
  void handleDataTransfers();
  bool doRetrieve();
  bool doStore();
  void closeTransfer();
  void abortTransfer();
  int8_t readCommand();
  void parseCommandLine();
  bool makePath(char *fullPath, size_t pathSize, const char *param = nullptr);
  void delayResponse(uint32_t ms);
  void processCurrentState();

  // Command handlers
  void handleCdupCommand();
  void handleCwdCommand();
  void handlePasvCommand();
  void handlePortCommand();
  void handleListCommand();
  void handleMlsdCommand();
  void handleRetrCommand();
  void handleStorCommand();
  void handleDeleCommand();
  void handleMkdCommand();
  void handleRmdCommand();
  void handleRnfrCommand();
  void handleRntoCommand();
  void handleSizeCommand();
  void handleTypeCommand();
  void handleNoopCommand();
  void handleAborCommand();
  void handleSystCommand();
  void handleFeatCommand();
};

#endif // ESP32FTPSERVER_H