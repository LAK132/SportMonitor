#include "FlinBITWiFi.h"

// Serial

String SerialInputString = "";
bool SerialInputComplete = false;

// WiFi

char AP_SSID[80];
char AP_Password[80] = {"12345678"};
char AP_IP_Text[80];
int AP_Channel = random(0, 13) + 1;

IPAddress AP_IP(192,168,1,1);
IPAddress AP_Gateway = AP_IP;
IPAddress AP_Subnet(255,255,255,0);

// DNS

DNSServer DNS;
const uint16_t DNSPort = 53;

// Web Server

const uint16_t ServerPort = 80;
ESP8266WebServer Server(ServerPort);

// Web Socket

const uint16_t SocketPort = 81;
WebSocketsServer Socket(SocketPort);

// SPIFFS

File LogFile;
const String LogDir = "/logs/";

//#define DEBUG

void setup()
{
    //
    // Set up Serial
    //
    Serial.begin(9600);

    #ifdef DEBUG_ESP_CORE
    Serial.setDebugOutput(true);
    #endif

    SerialInputString.reserve(200);

    //
    // Set up SPIFFS
    //
    SPIFFS.begin();

    // 
    // Get WiFi MAC Address and set SSID
    //

    //Serial.println("WiFi MAC Address:");
    //Serial.println(WiFi.macAddress());

    byte mac[6];
    WiFi.macAddress(mac);
    sprintf(AP_SSID, "FlinBit_%02X%02X%02X", mac[3], mac[4], mac[5]); 
    ReadParameterFromSPIFFS("/SSID.txt", AP_SSID, sizeof(AP_SSID));
    Serial.print("WiFi SSID: ");
    Serial.println(AP_SSID);

    ReadParameterFromSPIFFS("/password.txt", AP_Password, sizeof(AP_Password));
    //ReadParameterFromSPIFFS("/ipaddr.txt", AP_IP_Text, sizeof(AP_IP_Text));
    //AP_IP.fromString(AP_IP_Text);
 
    #ifdef DEBUG
    Serial.print("SSID: ");
    Serial.print(AP_SSID);
    Serial.print("\r\n");
    Serial.print("Password: ");
    Serial.print(AP_Password);
    Serial.print("\r\n");
    Serial.print("IP: ");
    Serial.print(AP_IP_Text);
    Serial.print("\r\n");
    #endif
    
    //
    // Set up WiFi
    //
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_Gateway, AP_Subnet);
    WiFi.softAP(AP_SSID, AP_Password, AP_Channel, false);

    //
    // Set up DNS
    //
    DNS.setErrorReplyCode(DNSReplyCode::NoError);
    DNS.start(DNSPort, "*", AP_IP);

    //
    // Set up WebServer
    //
    // Server.on("/", &ServerHandleRequest);
    Server.onNotFound(&ServerHandleRequest);
    Server.begin();

    //
    // Set up WebSocket
    //
    Socket.begin();
    Socket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
    {
        switch (type)
        {
            //
            // Device connected to the web socket
            //
            case WStype_CONNECTED: {

            } break;

            //
            // Device disconnected from the web socket
            //
            case WStype_DISCONNECTED: {

            } break;

            //
            // Received text over web socket
            //
            case WStype_TEXT: {
                // payload should be null-terminated-string
                if (payload[length-1] == '\0' || payload[length] == '\0')
                {
                    String str = (const char *)payload;

                    bool wasLogger = false;
                    if (str.length() > 1 && str[0] == '/' && str[1] != '/')
                    {
                        tokeniser_t commands(str);
                        const String &command = commands.nextLower();
                        auto boardcastLogging = [](){
                            if (LogFile)
                                Socket.broadcastTXT("/logger running");
                            else
                                Socket.broadcastTXT("/logger notrunning");
                        };
                        if (command == "/logstart")
                        {
                            wasLogger = true;
                            bool overwrite = commands.nextLower() == "overwrite";
                            String &&fname = commands.next();
                            ServerStartLogging(fname, overwrite);
                            boardcastLogging();
                        }
                        else if (command == "/logstop")
                        {
                            wasLogger = true;
                            ServerStopLogging();
                            boardcastLogging();
                        }
                        else if (command == "/islogging")
                        {
                            wasLogger = true;
                            boardcastLogging();
                        }
                        else if (command == "/logclear")
                        {
                            Dir dir = SPIFFS.openDir(LogDir);

                            while (dir.next())
                            {
                                SPIFFS.remove(dir.fileName());
                                dir = SPIFFS.openDir(LogDir);
                            }
                        }
                        else if (command == "/store")
                        {
                            String &&filename = commands.next();
                            String &&text = commands.next();
                            WriteParameterToSPIFFS(filename, text);
                        }
                    }

                    if (!wasLogger)
                    {
                        Serial.print(str);

                        #ifdef DEBUG
                        Socket.broadcastTXT(str);
                        #endif
                        if (LogFile)
                        {
                            size_t sz = str.length();
                            if (LogFile.write((const uint8_t *)str.c_str(), sz) != sz)
                                Socket.broadcastTXT("> Failed to save\n");
                            #ifdef DEBUG
                            else
                                Socket.broadcastTXT("> Saved\n");
                            #endif
                        }
                    }
                }
                // else error
            } break;

            //
            // Received binary blob over web socket
            //
            case WStype_BIN: {
                // payload is binary
            } break;
        }
    });
}

void loop()
{
    DNS.processNextRequest();
    Socket.loop();
    Server.handleClient();
    if (SerialEvent())
    {
        Socket.broadcastTXT(SerialInputString);
        if (LogFile)
        {
            size_t sz = SerialInputString.length();
            if (LogFile.write((const uint8_t *)SerialInputString.c_str(), sz) != sz)
                Socket.broadcastTXT("> Failed to save\n");
            #ifdef DEBUG
            else
                Socket.broadcastTXT("> Saved");
            #endif
        }

        if (SerialInputString.length() > 1 && SerialInputString[0] == '/' && SerialInputString[1] != '/')
        {
            tokeniser_t commands(SerialInputString);
            const String &command = commands.nextLower();
            if (command == "/store")
            {
                String filename = commands.next();
                String text = commands.next();
                #ifdef DEBUG
                Serial.print("/store ");
                Serial.print(filename);
                Serial.print(text);
                #endif
                WriteParameterToSPIFFS(filename, text);
            }
        }
    }
    delay(1);
}

void ReadParameterFromSPIFFS(String filename, char buffer[], uint16_t maxbytes)
{
    //
    // If available, obtain parameter from SPIFFS 
    //   
    //String filename = "/password.txt";
    if (SPIFFS.exists(filename)) {
        #ifdef DEBUG
        Serial.println(filename + " exists. Reading.\r\n");
        #endif
        File hParameterFile = SPIFFS.open(filename, "r");
        if (hParameterFile) {
            while (hParameterFile.available()){
              int l = hParameterFile.readBytesUntil('\n', buffer, maxbytes);
              buffer[l] = 0;
            }
            hParameterFile.close();
        } else {
            #ifdef DEBUG
            Serial.print("Error reading " + filename + " file\r\n");      
            #endif
        }
    } else {
        #ifdef DEBUG
        Serial.println(filename + " doesn't exist. Please use deafult password.\r\n");
        #endif
    }
}

void WriteParameterToSPIFFS(String filename, String buffer)
{
    //
    // Write parameter to SPIFFS. Always overwrite.
    //
    File hParameterFile = SPIFFS.open(filename, "w");
    if (hParameterFile){
      hParameterFile.println(buffer);
      hParameterFile.close();
    }
}

void ServerStartLogging(String fname, bool overwrite)
{
    if (LogFile)
        return;

    fname = LogDir + fname;

    if (fname.endsWith("/"))
        fname += "log.txt";
    else if (!fname.endsWith(".txt"))
        fname += ".txt";

    String message = "Opened file '";
    message += fname + "'\n";
    Socket.broadcastTXT(message);

    LogFile = SPIFFS.open(fname, ((overwrite || !SPIFFS.exists(fname)) ? "w" : "a"));
}

void ServerStopLogging()
{
    if (!LogFile)
        return;

    LogFile.close();
}

void ServerHandleRequest()
{
    String uri = Server.uri();
    uri = uri.endsWith("/") ? uri + "index.html" : uri;

    if (!ServerSendFile(Server, uri))
    {
        ServerSendDirectory(Server, uri);
    }
}

bool ServerSendFile(ESP8266WebServer &server, const String &path)
{
    String dataType = F("text/plain");
    String lowerPath = path.substring(path.length() - 5, path.length());
    lowerPath.toLowerCase();
    if      (lowerPath.endsWith(".src"))    lowerPath = lowerPath.substring(0, path.lastIndexOf("."));
    else if (lowerPath.endsWith(".gz"))     dataType = F("application/x-gzip");
    else if (lowerPath.endsWith(".html"))   dataType = F("text/html");
    else if (lowerPath.endsWith(".htm"))    dataType = F("text/html");
    else if (lowerPath.endsWith(".png"))    dataType = F("image/png");
    else if (lowerPath.endsWith(".js"))     dataType = F("application/javascript");
    else if (lowerPath.endsWith(".css"))    dataType = F("text/css");
    else if (lowerPath.endsWith(".gif"))    dataType = F("image/gif");
    else if (lowerPath.endsWith(".jpg"))    dataType = F("image/jpeg");
    else if (lowerPath.endsWith(".ico"))    dataType = F("image/x-icon");
    else if (lowerPath.endsWith(".svg"))    dataType = F("image/svg+xml");
    else if (lowerPath.endsWith(".mp3"))    dataType = F("audio/mpeg");
    else if (lowerPath.endsWith(".wav"))    dataType = F("audio/wav");
    else if (lowerPath.endsWith(".ogg"))    dataType = F("audio/ogg");
    else if (lowerPath.endsWith(".xml"))    dataType = F("text/xml");
    else if (lowerPath.endsWith(".pdf"))    dataType = F("application/x-pdf");
    else if (lowerPath.endsWith(".zip"))    dataType = F("application/x-zip");

    String pathWithGz = path + ".gz";

    File file;

    if (SPIFFS.exists(pathWithGz))
    {
        file = SPIFFS.open(pathWithGz, "r");
    }
    else if (SPIFFS.exists(path))
    {
        file = SPIFFS.open(path, "r");
    }
    else
    {
        return false;
    }

    server.setContentLength(file.size());
    size_t sent = server.streamFile(file, dataType);
    file.close();
    return true;
}

void ServerSendDirectory(ESP8266WebServer &server, const String &path)
{
    int lastIndex = path.lastIndexOf("/") + 1;
    if (lastIndex <= 0)
        lastIndex = 1;
    String &&substr = path.substring(0, lastIndex);

    #ifndef DEBUG
    if (substr != LogDir)
    {
        String error = "Failed to read file. '";
        error += path + "'\n\n" + substr;
        server.send(404, "text/plain", error);
    }
    else
    #endif
    {
        Dir dir = SPIFFS.openDir(substr);

        String doc = R"(<html><head><meta charset="utf-8"></head><body><h1 style="margin:0 0 0 0;">)"
            +substr+R"(</h1><br/><a href="/"><h2 style="margin:0 0 0 0;">Home</h2></a><br/>)";
        while (dir.next())
        {
            String &&fname = dir.fileName();
            if (fname.endsWith(".gz"))
                fname = fname.substring(0, fname.length() - 3);
            doc += String(R"(<a href=")") + fname + R"(">)" + fname + "</a><br/>";
        }
        doc += "</body></html>";

        server.send(200, "text/html", doc);
    }
}

bool SerialEvent()
{
    if (SerialInputComplete)
    {
        SerialInputString = "";
        SerialInputComplete = false;
    }
    while (Serial.available())
    {
        char in = (char)Serial.read();
        SerialInputString += in;
        if (in == '\n')
        {
            SerialInputComplete = true;
            break;
        }
    }
    return SerialInputComplete;
}

bool IsWhitespace(const char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

tokeniser_t::tokeniser_t(const String &str) : _str(str) {}

String tokeniser_t::next()
{
    size_t i = 0;
    for (; i < _str.length() && IsWhitespace(_str[i]); ++i); // find next not-space (start of token)
    if (i >= _str.length())
    {
        _str = "";
        return "";
    }

    size_t j = i;
    for (; j < _str.length() && !IsWhitespace(_str[j]); ++j); // find next whitespace (end of token)

    String rtn = _str.substring(i, j);
    _str = _str.substring(j, _str.length());
    return rtn;
}

String tokeniser_t::nextLower()
{
    String str = next();
    str.toLowerCase();
    return str;
}
