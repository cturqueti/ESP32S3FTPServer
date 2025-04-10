/*
 * FTP Server for ESP32-S3
 * Based on original work by Jean-Michel Gallego and modifications by David Paiva
 * Completely rewritten with performance improvements and additional features
 *
 * Features:
 * - Optimized memory usage
 * - Enhanced security
 * - Better error handling
 * - Additional FTP commands support
 * - Configurable timeouts
 * - Improved data transfer handling
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ESP32FtpServer.h"
#include <LittleFS.h>
#include <WiFi.h>

// Static member initialization
WiFiServer FtpServer::ftpServer(FTP_CTRL_PORT);
WiFiServer FtpServer::dataServer(FTP_DATA_PORT_PASV);

FtpServer::FtpServer() : _username(""),
                         _password(""),
                         _activeTimeout(FTP_TIME_OUT * 60 * 1000),
                         _passivePort(FTP_DATA_PORT_PASV),
                         _maxAttempts(3),
                         _currentAttempts(0),
                         _transferStatus(0),
                         _cmdStatus(FTP_CMD_IDLE),
                         _dataConnType(FTP_DATA_PASSIVE),
                         _rnfrCmd(false),
                         _bytesTransferred(0),
                         _log(FTPLog::DISABLE)
{
  strlcpy(_cwd, "/", sizeof(_cwd));
}

void FtpServer::begin(const String &username, const String &password)
{
  _username = username;
  _password = password;

  if (!LittleFS.begin(true))
  {
    return;
  }

  ftpServer.begin();
  dataServer.begin();
  _cmdStatus = FTP_CMD_WAIT_CONNECTION;
}

void FtpServer::begin(const String &username, const String &password, FTPLog log)
{
  _username = username;
  _password = password;
  _log = log;

  if (!LittleFS.begin(true))
  {
    if (_log == FTPLog::ENABLE)
    {
      LOG_INFO("Failed to mount LittleFS");
    }

    return;
  }

  ftpServer.begin();
  dataServer.begin();
  _cmdStatus = FTP_CMD_WAIT_CONNECTION;

  if (_log == FTPLog::ENABLE)
  {
    LOG_INFO("FTP Server initialized");
  }
}

void FtpServer::setActiveTimeout(uint32_t timeout)
{
  _activeTimeout = timeout * 60 * 1000;
}

void FtpServer::setPassivePort(uint16_t port)
{
  _passivePort = port;
}

void FtpServer::setMaxLoginAttempts(uint8_t attempts)
{
  _maxAttempts = attempts;
}

bool FtpServer::handleFTP()
{
  if ((int32_t)(_millisDelay - millis()) > 0)
  {
    return false;
  }

  // Handle new client connections
  if (ftpServer.hasClient())
  {
    if (_client.connected())
    {
      _client.stop(); // Disconnect previous client
    }
    _client = ftpServer.accept();
  }

  switch (_cmdStatus)
  {
  case FTP_CMD_IDLE:
    if (_client.connected())
    {
      disconnectClient();
    }
    _cmdStatus = FTP_CMD_WAIT_CONNECTION;
    break;

  case FTP_CMD_WAIT_CONNECTION:
    abortTransfer();
    initVariables();
    _cmdStatus = FTP_CMD_READY;
    break;

  case FTP_CMD_READY:
    if (_client.connected())
    {
      clientConnected();
      _millisEndConnection = millis() + 10 * 1000; // 10s for login
      _cmdStatus = FTP_CMD_WAIT_USER;
    }
    break;

  case FTP_CMD_WAIT_USER:
  case FTP_CMD_WAIT_PASS:
  case FTP_CMD_WAIT_COMMAND:
    if (readCommand() > 0)
    {
      processCurrentState();
    }
    break;
  }

  // Handle data transfers
  handleDataTransfers();

  // Check for timeout or disconnection
  if ((_cmdStatus > FTP_CMD_READY) &&
      (!_client.connected() || (millis() > _millisEndConnection)))
  {
    _client.println("530 Timeout");
    _millisDelay = millis() + 200;
    _cmdStatus = FTP_CMD_IDLE;
  }

  return _transferStatus != FTP_TRANSFER_IDLE || _cmdStatus != FTP_CMD_IDLE;
}

// Private method implementations

void FtpServer::initVariables()
{
  _dataPort = _passivePort;
  _dataConnType = FTP_DATA_PASSIVE;
  strlcpy(_cwd, "/", sizeof(_cwd));
  _rnfrCmd = false;
  _transferStatus = FTP_TRANSFER_IDLE;
  _currentAttempts = 0;
}

void FtpServer::clientConnected()
{
  _client.println("220 Welcome to ESP32-S3 FTP Server");
  _client.println("220 Version " + String(FTP_SERVER_VERSION));
  _cmdBufferIndex = 0;
}

void FtpServer::disconnectClient()
{
  abortTransfer();
  _client.println("221 Goodbye");
  _client.stop();
}

bool FtpServer::authenticateUser()
{
  if (strcmp(_command, "USER") != 0)
  {
    _client.println("500 Syntax error");
    delayResponse(100);
    return false;
  }

  if (strcmp(_parameters, _username.c_str()) != 0)
  {
    _currentAttempts++;
    if (_currentAttempts >= _maxAttempts)
    {
      _client.println("530 Too many attempts");
      delayResponse(1000); // Longer delay after max attempts
      _cmdStatus = FTP_CMD_IDLE;
    }
    else
    {
      _client.println("530 User not found");
      delayResponse(100);
    }
    return false;
  }

  _client.println("331 Password required");
  return true;
}

bool FtpServer::authenticatePassword()
{
  if (strcmp(_command, "PASS") != 0)
  {
    _client.println("500 Syntax error");
    delayResponse(100);
    return false;
  }

  if (strcmp(_parameters, _password.c_str()) != 0)
  {
    _currentAttempts++;
    if (_currentAttempts >= _maxAttempts)
    {
      _client.println("530 Too many attempts");
      delayResponse(1000);
      _cmdStatus = FTP_CMD_IDLE;
    }
    else
    {
      _client.println("530 Invalid password");
      delayResponse(100);
    }
    return false;
  }

  _client.println("230 Login successful");
  _millisEndConnection = millis() + _activeTimeout;
  _currentAttempts = 0;
  return true;
}

bool FtpServer::processCommand()
{
  if (_log == FTPLog::ENABLE)
  {
    LOG_DEBUG("Comando=%s", _command);
  }
  // Access Control Commands
  if (strcmp(_command, "CDUP") == 0)
  {
    handleCdupCommand();
  }
  else if (strcmp(_command, "CWD") == 0)
  {
    handleCwdCommand();
  }
  else if (strcmp(_command, "PWD") == 0)
  {
    _client.println("257 \"" + String(_cwd) + "\" is current directory");
  }
  else if (strcmp(_command, "QUIT") == 0)
  {
    disconnectClient();
    return false;
  }
  // Transfer Parameter Commands
  else if (strcmp(_command, "PASV") == 0)
  {
    handlePasvCommand();
  }
  else if (strcmp(_command, "PORT") == 0)
  {
    handlePortCommand();
  }
  else if (strcmp(_command, "TYPE") == 0)
  {
    handleTypeCommand();
  }
  // Service Commands
  else if (strcmp(_command, "LIST") == 0)
  {
    handleListCommand();
  }
  else if (strcmp(_command, "MLSD") == 0)
  {
    handleMlsdCommand();
  }
  else if (strcmp(_command, "RETR") == 0)
  {
    handleRetrCommand();
  }
  else if (strcmp(_command, "STOR") == 0)
  {
    handleStorCommand();
  }
  else if (strcmp(_command, "DELE") == 0)
  {
    handleDeleCommand();
  }
  else if (strcmp(_command, "MKD") == 0)
  {
    handleMkdCommand();
  }
  else if (strcmp(_command, "RMD") == 0)
  {
    handleRmdCommand();
  }
  else if (strcmp(_command, "RNFR") == 0)
  {
    handleRnfrCommand();
  }
  else if (strcmp(_command, "RNTO") == 0)
  {
    handleRntoCommand();
  }
  // Extended Commands
  else if (strcmp(_command, "FEAT") == 0)
  {
    _client.println("211-Extensions supported:");
    _client.println(" MLSD");
    _client.println(" SIZE");
    _client.println(" MDTM");
    _client.println("211 End");
  }
  else if (strcmp(_command, "SIZE") == 0)
  {
    handleSizeCommand();
  }
  else if (strcmp(_command, "SYST") == 0)
  {
    _client.println("215 UNIX Type: L8");
  }
  else
  {
    _client.println("500 Unknown command");
    if (_log == FTPLog::ENABLE)
    {
      LOG_WARN("Comando=%s desconhecido", _command);
    }
  }

  return true;
}

// Command Handlers Implementation

void FtpServer::handleCdupCommand()
{
  // Verifica se o diretório atual não está vazio
  if (_cwd[0] == '\0')
  {
    _client.println("550 Current directory not set");
    return;
  }

  // Encontra a última barra (exceto a raiz)
  char *lastSlash = strrchr(_cwd, '/');

  // Verifica se encontrou uma barra e não é a raiz
  if (lastSlash != nullptr && lastSlash != _cwd)
  {
    *lastSlash = '\0'; // Remove o último componente do caminho
  }
  else if (lastSlash == _cwd)
  {
    // Já está na raiz, não faz nada
    _client.println("250 Already at root directory");
    return;
  }

  _client.println("250 CDUP command successful. Current directory: \"" + String(_cwd) + "\"");
}

void FtpServer::handleCwdCommand()
{
  if (strcmp(_parameters, ".") == 0)
  {
    _client.println("257 \"" + String(_cwd) + "\" is current directory");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory())
  {
    _client.println("550 Directory not found");
    if (dir)
      dir.close();
    return;
  }
  dir.close();

  strlcpy(_cwd, path, sizeof(_cwd));
  _client.println("250 CWD command successful");
}

void FtpServer::handlePasvCommand()
{
  if (_data.connected())
  {
    _data.stop();
  }

  IPAddress ip = WiFi.localIP();
  _dataPort = _passivePort;
  _dataConnType = FTP_DATA_PASSIVE;

  char response[100];
  snprintf(response, sizeof(response),
           "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
           ip[0], ip[1], ip[2], ip[3],
           _dataPort >> 8, _dataPort & 255);
  _client.println(response);
}

void FtpServer::handlePortCommand()
{
  if (_data.connected())
  {
    _data.stop();
  }

  char *token = strtok(_parameters, ",");
  for (uint8_t i = 0; i < 4; i++)
  {
    if (token == NULL)
    {
      _client.println("501 Invalid PORT format");
      return;
    }
    _dataIp[i] = atoi(token);
    token = strtok(NULL, ",");
  }

  if (token == NULL)
  {
    _client.println("501 Invalid PORT format");
    return;
  }
  _dataPort = atoi(token) << 8;

  token = strtok(NULL, ",");
  if (token == NULL)
  {
    _client.println("501 Invalid PORT format");
    return;
  }
  _dataPort += atoi(token);

  _dataConnType = FTP_DATA_ACTIVE;
  _client.println("200 PORT command successful");
}

void FtpServer::handleListCommand()
{
  if (!dataConnect())
  {
    _client.println("425 Can't open data connection");
    return;
  }

  _client.println("150 Opening ASCII mode data connection for file list");

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path), strlen(_parameters) > 0 ? _parameters : nullptr))
  {
    _data.stop();
    return;
  }

  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory())
  {
    _client.println("550 Directory not found");
    _data.stop();
    if (dir)
      dir.close();
    return;
  }

  uint16_t count = 0;
  File file = dir.openNextFile();
  while (file)
  {
    String line;
    if (file.isDirectory())
    {
      line = "drwxr-xr-x 1 owner group " + String(file.size()) + " Jan 1 2000 " + String(file.name());
    }
    else
    {
      line = "-rw-r--r-- 1 owner group " + String(file.size()) + " Jan 1 2000 " + String(file.name());
    }
    _data.println(line);
    count++;
    file.close();
    file = dir.openNextFile();
  }
  dir.close();

  _client.println("226 " + String(count) + " matches total");
  _data.stop();
}

void FtpServer::handleMlsdCommand()
{

  if (!dataConnect())
  {
    _client.println("425 Can't open data connection");
    return;
  }

  _client.println("150 Opening ASCII mode data connection for MLSD");

  char path[FTP_CWD_SIZE] = {0};

  const char *paramToUse = (_parameters == nullptr || strlen(_parameters) == 0) ? "." : _parameters;

  if (!makePath(path, sizeof(path), paramToUse))
  {
    _client.println("550 Invalid path");
    _data.stop();
    if (_log == FTPLog::ENABLE)
    {
      LOG_DEBUG("Falha no makePath para: %s", path);
    }
    return;
  }

  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory())
  {
    _client.println("550 Directory not found");
    _data.stop();
    if (dir)
      dir.close();
    return;
  }
  uint16_t count = 0;
  File file = dir.openNextFile();
  while (file)
  {
    if (_log == FTPLog::ENABLE)
    {
      LOG_DEBUG("File Name = %s", file.name());
    }
    String line;
    if (file.isDirectory())
    {
      line = "Type=dir;Size=" + String(file.size()) + ";Modify=20000101000000; " + String(file.name());
    }
    else
    {
      line = "Type=file;Size=" + String(file.size()) + ";Modify=20000101000000; " + String(file.name());
    }
    _data.println(line);
    count++;
    file.close();
    file = dir.openNextFile();
  }
  dir.close();

  _client.println("226 " + String(count) + " matches total");
  _data.stop();
}

void FtpServer::handleRetrCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  _file = LittleFS.open(path, "r");
  if (!_file)
  {
    _client.println("550 File not found");
    return;
  }

  if (!dataConnect())
  {
    _client.println("425 Can't open data connection");
    _file.close();
    return;
  }

  _client.println("150 Opening data connection");
  _millisBeginTransfer = millis();
  _bytesTransferred = 0;
  _transferStatus = FTP_TRANSFER_RETR;
}

void FtpServer::handleStorCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  // Check if file exists and is writable
  if (LittleFS.exists(path))
  {
    File testFile = LittleFS.open(path, "r+");
    if (!testFile)
    {
      _client.println("550 File exists but can't be opened");
      return;
    }
    testFile.close();
  }

  _file = LittleFS.open(path, "w");
  if (!_file)
  {
    _client.println("451 Can't create file");
    return;
  }

  if (!dataConnect())
  {
    _client.println("425 Can't open data connection");
    _file.close();
    LittleFS.remove(path);
    return;
  }

  _client.println("150 Ready to receive data");
  _millisBeginTransfer = millis();
  _bytesTransferred = 0;
  _transferStatus = FTP_TRANSFER_STOR;
}

void FtpServer::handleDeleCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  if (!LittleFS.exists(path))
  {
    _client.println("550 File not found");
    return;
  }

  if (LittleFS.remove(path))
  {
    _client.println("250 File deleted");
  }
  else
  {
    _client.println("450 Could not delete file");
  }
}

void FtpServer::handleMkdCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No directory name given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  if (LittleFS.mkdir(path))
  {
    _client.println("257 \"" + String(path) + "\" created");
  }
  else
  {
    _client.println("550 Can't create directory");
  }
}

void FtpServer::handleRmdCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No directory name given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  // Check if directory is empty
  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory())
  {
    _client.println("550 Not a directory or doesn't exist");
    if (dir)
      dir.close();
    return;
  }

  // Check for empty directory
  File file = dir.openNextFile();
  if (file)
  {
    _client.println("550 Directory not empty");
    file.close();
    dir.close();
    return;
  }
  dir.close();

  if (LittleFS.rmdir(path))
  {
    _client.println("250 Directory removed");
  }
  else
  {
    _client.println("550 Could not remove directory");
  }
}

void FtpServer::handleRnfrCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    return;
  }

  if (!makePath(_renameFrom, sizeof(_renameFrom), _parameters))
  {
    return;
  }

  if (!LittleFS.exists(_renameFrom))
  {
    _client.println("550 File not found");
    return;
  }

  _rnfrCmd = true;
  _client.println("350 RNFR accepted - ready for destination");
}

void FtpServer::handleRntoCommand()
{
  if (!_rnfrCmd)
  {
    _client.println("503 RNFR required first");
    return;
  }

  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    _rnfrCmd = false;
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    _rnfrCmd = false;
    return;
  }

  if (LittleFS.exists(path))
  {
    _client.println("553 Destination already exists");
    _rnfrCmd = false;
    return;
  }

  if (LittleFS.rename(_renameFrom, path))
  {
    _client.println("250 Rename successful");
  }
  else
  {
    _client.println("553 Rename failed");
  }
  _rnfrCmd = false;
}

void FtpServer::handleSizeCommand()
{
  if (strlen(_parameters) == 0)
  {
    _client.println("501 No filename given");
    return;
  }

  char path[FTP_CWD_SIZE];
  if (!makePath(path, sizeof(path)))
  {
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file)
  {
    _client.println("550 File not found");
    return;
  }

  _client.println("213 " + String(file.size()));
  file.close();
}

void FtpServer::handleTypeCommand()
{
  if (strcmp(_parameters, "A") == 0)
  {
    _client.println("200 Type set to ASCII");
  }
  else if (strcmp(_parameters, "I") == 0)
  {
    _client.println("200 Type set to binary");
  }
  else
  {
    _client.println("504 Unsupported type");
  }
}

void FtpServer::handleNoopCommand()
{
  _client.println("200 NOOP command successful");
}

void FtpServer::handleAborCommand()
{
  abortTransfer();
  _client.println("226 ABOR command successful");
}

void FtpServer::handleSystCommand()
{
  _client.println("215 UNIX Type: L8");
}

void FtpServer::handleFeatCommand()
{
  _client.println("211-Extensions supported:");
  _client.println(" MLST");
  _client.println(" SIZE");
  _client.println(" MDTM");
  _client.println(" PASV");
  _client.println(" EPSV");
  _client.println("211 END");
}

bool FtpServer::dataConnect()
{
  if (_data.connected())
  {
    return true;
  }

  if (_dataConnType == FTP_DATA_PASSIVE)
  {
    unsigned long start = millis();
    while (!dataServer.hasClient() && (millis() - start < 10000))
    {
      delay(10);
    }

    if (dataServer.hasClient())
    {
      _data.stop();
      _data = dataServer.accept();
      return _data.connected();
    }
  }
  else
  {
    // Active mode connection
    _data.stop();
    if (_data.connect(_dataIp, _dataPort))
    {
      return true;
    }
  }

  return false;
}

void FtpServer::handleDataTransfers()
{
  if (_transferStatus == FTP_TRANSFER_RETR)
  {
    if (!doRetrieve())
    {
      _transferStatus = FTP_TRANSFER_IDLE;
    }
  }
  else if (_transferStatus == FTP_TRANSFER_STOR)
  {
    if (!doStore())
    {
      _transferStatus = FTP_TRANSFER_IDLE;
    }
  }
}

bool FtpServer::doRetrieve()
{
  int16_t bytesRead = _file.readBytes(_buffer, FTP_BUF_SIZE);
  if (bytesRead > 0)
  {
    _data.write((uint8_t *)_buffer, bytesRead);
    _bytesTransferred += bytesRead;
    return true;
  }
  closeTransfer();
  return false;
}

bool FtpServer::doStore()
{
  if (!_data.connected())
  {
    closeTransfer();
    return false;
  }

  int16_t bytesRead = _data.readBytes((uint8_t *)_buffer, FTP_BUF_SIZE);
  if (bytesRead > 0)
  {
    _file.write((uint8_t *)_buffer, bytesRead);
    _bytesTransferred += bytesRead;
    return true;
  }

  closeTransfer();
  return false;
}

void FtpServer::closeTransfer()
{
  uint32_t duration = millis() - _millisBeginTransfer;
  if (duration > 0 && _bytesTransferred > 0)
  {
    float rate = (_bytesTransferred * 1000.0) / (duration * 1024.0);
    _client.println("226 Transfer complete (" + String(rate, 2) + " kB/s)");
  }
  else
  {
    _client.println("226 Transfer complete");
  }

  _file.close();
  _data.stop();
  _transferStatus = FTP_TRANSFER_IDLE;
}

void FtpServer::abortTransfer()
{
  if (_transferStatus != FTP_TRANSFER_IDLE)
  {
    _file.close();
    _data.stop();
    _client.println("426 Transfer aborted");
    _transferStatus = FTP_TRANSFER_IDLE;
  }
}

int8_t FtpServer::readCommand()
{
  if (!_client.available())
  {
    return -1;
  }

  char c = _client.read();
  if (c == '\\')
    c = '/'; // Normalize path separators

  if (c != '\r' && c != '\n')
  {
    if (_cmdBufferIndex < FTP_CMD_SIZE)
    {
      _cmdLine[_cmdBufferIndex++] = c;
    }
    return -2; // Command too long
  }

  if (_cmdBufferIndex == 0)
  {
    return 0; // Empty line
  }

  _cmdLine[_cmdBufferIndex] = '\0';
  parseCommandLine();
  _cmdBufferIndex = 0;

  return 1;
}

void FtpServer::parseCommandLine()
{
  _parameters = "";

  // Find space separating command from parameters
  char *spacePos = strchr(_cmdLine, ' ');
  if (spacePos != nullptr)
  {
    *spacePos = '\0'; // Terminate command string
    _parameters = spacePos + 1;
    // Skip leading spaces in parameters
    while (*_parameters == ' ')
      _parameters++;
  }

  strlcpy(_command, _cmdLine, sizeof(_command));
  // Convert command to uppercase
  for (char *p = _command; *p; p++)
    *p = toupper(*p);
}

bool FtpServer::makePath(char *fullPath, size_t pathSize, const char *param)
{
  if (param == nullptr)
    param = _parameters;

  if (strcmp(param, "/") == 0 || strlen(param) == 0)
  {
    strlcpy(fullPath, "/", pathSize);
    return true;
  }

  if (param[0] != '/')
  {
    strlcpy(fullPath, _cwd, FTP_CWD_SIZE);
    if (fullPath[strlen(fullPath) - 1] != '/')
    {
      strlcat(fullPath, "/", FTP_CWD_SIZE);
    }
    strlcat(fullPath, param, FTP_CWD_SIZE);
  }
  else
  {
    strlcpy(fullPath, param, FTP_CWD_SIZE);
  }

  // Remove trailing slash if not root
  size_t len = strlen(fullPath);
  if (len > 1 && fullPath[len - 1] == '/')
  {
    fullPath[len - 1] = '\0';
  }

  // Security check - prevent directory traversal
  if (strstr(fullPath, "../") != nullptr)
  {
    _client.println("550 Invalid path");
    return false;
  }

  return true;
}

void FtpServer::delayResponse(uint32_t ms)
{
  _millisDelay = millis() + ms;
}

void FtpServer::processCurrentState()
{
  switch (_cmdStatus)
  {
  case FTP_CMD_WAIT_USER:
    if (authenticateUser())
    {
      _cmdStatus = FTP_CMD_WAIT_PASS;
    }
    else
    {
      _cmdStatus = FTP_CMD_IDLE;
    }
    break;

  case FTP_CMD_WAIT_PASS:
    if (authenticatePassword())
    {
      _cmdStatus = FTP_CMD_WAIT_COMMAND;
      _millisEndConnection = millis() + _activeTimeout;
    }
    else
    {
      _cmdStatus = FTP_CMD_IDLE;
    }
    break;

  case FTP_CMD_WAIT_COMMAND:
    if (!processCommand())
    {
      _cmdStatus = FTP_CMD_IDLE;
    }
    else
    {
      _millisEndConnection = millis() + _activeTimeout;
    }
    break;
  }
}