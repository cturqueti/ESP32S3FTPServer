# ESP32S3FTPServer
## Servidor FTP para ESP32-S3  

![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-brightgreen)  
![Licença](https://img.shields.io/badge/licen%C3%A7a-Apache%202.0-blue.svg)  
![Versão](https://img.shields.io/badge/vers%C3%A3o-1.0.0-green.svg)  

Um servidor FTP leve e eficiente para microcontroladores ESP32-S3 com suporte ao sistema de arquivos LittleFS.

## ✨ Funcionalidades

- ✅ Suporte aos modos ativo e passivo
- 📁 Integração nativa com LittleFS
- 🔐 Autenticação por usuário/senha
- ⚡ Baixo consumo de memória
- 🐛 Logs de depuração detalhados

## 📥 Instalação

### Via PlatformIO
```ini
lib_deps = 
    https://github.com/seu-usuario/ESP32-S3-FTP-Server.git
    https://github.com/cturqueti/LogLibrary.git
```

### Arduino IDE
Baixe o .zip

Menu > Sketch > Incluir Biblioteca > Adicionar Biblioteca .ZIP

Reinicie o IDE

## 🚀 Exemplo Básico
```cpp
#include <WiFi.h>
#include "ESP32FtpServer.h"

FtpServer ftpSrv;

void setup() {
  WiFi.begin("SSID", "senha");
  ftpSrv.begin("ftp_user", "ftp_pass");
}

void loop() {
  ftpSrv.handleFTP();
}
```

## ⚙️ Configurações Avançadas
|Método	|Descrição	|Padrão |
|---|---|---|
|setActiveTimeout(min)	|Timeout modo ativo	|5 min |
|setPassivePort(port)	|Porta modo passivo	|55600 |
|setMaxLoginAttempts(n)	|Tentativas de login	|3 |

## 📌 Comandos Suportados
|Comando	|Descrição |
|---|---|
|LIST/MLSD	|Listagem de arquivos |
|STOR/RETR	|Upload/Download |
|MKD/RMD	|Gerenciar diretórios |
|RNFR/RNTO	|Renomear arquivos |

## 🐛 Depuração
Ative em ESP32FtpServer.h:

```cpp
#define FTP_DEBUG
```

## 📜 Licença
Copyright 2025 cturqueti

Licenciado sob a Apache License, Versão 2.0 (a "Licença");
você não pode usar este arquivo exceto em conformidade com a Licença.
Você pode obter uma cópia da Licença em:

http://www.apache.org/licenses/LICENSE-2.0

A menos que exigido por lei aplicável ou acordado por escrito, o software
distribuído sob a Licença é distribuído "COMO ESTÁ",
SEM GARANTIAS OU CONDIÇÕES DE QUALQUER TIPO, expressas ou implícitas.
Consulte a Licença para o idioma específico que rege as permissões e
limitações sob a Licença.

Consulte o arquivo [LICENSE](LICENSE) para o texto completo da licença e
[NOTICE](NOTICE) para informações sobre atribuições e histórico de modificações.

## 🤝 Contribuição
Faça um fork do projeto

Crie sua branch (git checkout -b feature/nova-funcionalidade)

Commit suas mudanças (git commit -m 'Adiciona nova funcionalidade')

Push para a branch (git push origin feature/nova-funcionalidade)

Abra um Pull Request  


---
Nota: Consulte o arquivo NOTICE para informações detalhadas sobre atribuições e modificações de código de terceiros.