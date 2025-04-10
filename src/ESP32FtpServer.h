/*******************************************************************************
 **                                                                            **
 **                       DEFINITIONS FOR FTP SERVER                           **
 **                                                                            **
 *******************************************************************************/

// Uncomment to print debugging info to console attached to ESP32
// #define FTP_DEBUG

#ifndef FTP_SERVERESP_H
#define FTP_SERVERESP_H

#include <FS.h>
#include <LittleFS.h>
#include <LogLibrary.h>
#include <WiFi.h>

#define FTP_SERVER_VERSION "1.0.0"

// FTP Server Configuration
#define FTP_CTRL_PORT 21
#define FTP_DATA_PORT_PASV 55600
#define FTP_TIME_OUT 5
#define FTP_CMD_SIZE 256
#define FTP_CWD_SIZE 512
#define FTP_FIL_SIZE 128
#define FTP_BUF_SIZE 512

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
  enum class FTPLog
  {
    DISABLE = 0,
    ENABLE
  };

  FtpServer();

  // Server management
  void begin(const String &username, const String &password);
  void begin(const String &username, const String &password, FTPLog log);
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
  bool _started;
  FTPLog _log;

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