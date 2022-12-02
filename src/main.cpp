// Default Arduino includes
#include <Arduino.h>
#include <WiFi.h>

// Includes for BLE
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include <Preferences.h>

/** Build time */
const char compileDate[] = __DATE__ " " __TIME__;

/** Unique device name */
char apName[] = "ESP32-xxxxxxxxxxxx";

/** Flag if stored AP credentials are available */
bool hasCredentials = false;
/** Connection status */
volatile bool isConnected = false;
/** Connection change status */
bool connStatusChanged = false;

/**
 * Create unique device name from MAC address
 **/
void createName() {
  uint8_t baseMac[6];
  // Get MAC address for WiFi station
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // Write unique name into apName
  sprintf(apName, "ESP32-%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

// List of Service and Characteristic UUIDs

/**
 * @brief UUID to identify our service, this is a random UUID.
 */
#define ESP_AP_SERVICE_UUID   "21c04d09-c884-4af1-96a9-52e4e4ba195b"

/**
 * @brief UUID to identify a characteristic that will be used to
 * set the SSID of the WiFi network we want to connect to.
 */
#define WIFI_SSID_NAME_UUID   "1e500043-6b31-4a3d-b91e-025f92ca9763"

/**
 * @brief UUID to identify a characteristic that will be used to
 * set the password of the WiFi network we want to connect to.
 */
#define WIFI_CONN_PASS_UUID   "1e500043-6b31-4a3d-b91e-025f92ca9764"

/**
 * @brief UUID to identify a characteristic that will be used to
 * change the WiFi connection status (CONNECT or DISCONNECT).
 */
#define WIFI_CONN_STATUS_UUID "1e500043-6b31-4a3d-b91e-025f92ca9765"

/**
 * @brief UUID to identify a characteristic that will be used to
 * check if some error occurred in the device.
 */
#define DEVICE_ERROR_SERVICE_UUID "1e500043-6b31-4a3d-b91e-025f92ca9765"

#define MAX_SSID_NAME_LENGTH 32
#define MAX_CONN_PASS_LENGTH 32

typedef enum {
  ESP_SSID_NAME_NOT_SET,
  ESP_SSID_NAME_TOO_LONG,
  ESP_SSID_CONN_PASS_TOO_LONG,
  ESP_SSID_NOT_FOUND,
  ESP_NO_CREDENTIALS,
  ESP_NO_ERROR
} ESP_ErrorCode_t;

ESP_ErrorCode_t DEVICE_ERROR = ESP_NO_ERROR;

std::string ESP_GetDeviceErrorAsString(ESP_ErrorCode_t errorCode) {
  switch (errorCode) {
    case ESP_SSID_NAME_NOT_SET:
      return "SSID_NAME_NOT_SET";
    case ESP_SSID_NAME_TOO_LONG:
      return "SSID_NAME_TOO_LONG";
    case ESP_SSID_CONN_PASS_TOO_LONG:
      return "SSID_CONN_PASS_TOO_LONG";
    case ESP_SSID_NOT_FOUND:
      return "SSID_NOT_FOUND";
    case ESP_NO_CREDENTIALS:
      return "NO_CREDENTIALS";
    case ESP_NO_ERROR:
      return "NO_ERROR";
    default:
      return "UNKNOWN_ERROR";
  }
}


/** SSIDs of local WiFi networks */
char *wifiSSIDName = NULL;
/** Password for local WiFi network */
char *wifiPassword = NULL;

/**
 * @brief BLE Characteristic to set the SSID of the WiFi network 
 * we want to connect to.
 */
BLECharacteristic *pSSIDName;
/**
 * @brief BLE Characteristic to set the password of the WiFi network
 * we want to connect to.
 */
BLECharacteristic *pConnPass;

/**
 * @brief BLE Characteristic to change the WiFi connection status
 * (CONNECT or DISCONNECT).
 */
BLECharacteristic *pConnStatus;

/**
 * @brief BLE Characteristic to check if some error occurred in the device.
 */
BLECharacteristic *pDeviceError;

/** BLE Advertiser */
BLEAdvertising* pAdvertising;
/** BLE Service */
BLEService *pService;
/** BLE Server */
BLEServer *pServer;

Preferences preferences;

bool wifiSSIDNameIsValid();
bool wifiPasswordIsValid();
void connectWiFi();

/**
 * ServerCallbacks
 * Callbacks for client connection and disconnection
 */
class ServerCallbacks: public BLEServerCallbacks {
  // TODO this doesn't take into account several clients being connected
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE client connected");
  };

  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE client disconnected");
    pAdvertising->start();
  }
};

/**
 * BLECallbackHandler
 * Callbacks for BLE client read/write requests
 */
class SSIDNameCallbackHandler: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) {
      Serial.println("SSID name not set");
      DEVICE_ERROR = ESP_SSID_NAME_NOT_SET;
      return;
    }
    Serial.print("New SSID name: ");
    Serial.println(value.c_str());
    const std::size_t valueLen = value.length();
    if (valueLen > MAX_SSID_NAME_LENGTH) {
      DEVICE_ERROR = ESP_SSID_NAME_TOO_LONG;
      return;
    }
    wifiSSIDName = (char *)malloc(value.length() + 1);
    strcpy(wifiSSIDName, value.c_str());
    
    preferences.begin("credentials", false);
    preferences.putString("wifiSSIDName", wifiSSIDName);
    preferences.end();
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Read SSID name");
    pCharacteristic->setValue(wifiSSIDName);
  }
};

class ConnPassCallbackHandler: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) {
      Serial.println("Connection password not set. Using open network");
      return;
    }
    Serial.print("New connection password: ");
    Serial.println(value.c_str());
    const std::size_t valueLen = value.length();
    if (valueLen > MAX_CONN_PASS_LENGTH) {
      DEVICE_ERROR = ESP_SSID_CONN_PASS_TOO_LONG;
      return;
    }
    wifiPassword = (char *)malloc(value.length() + 1);
    strcpy(wifiPassword, value.c_str());
    
    preferences.begin("credentials", false);
    preferences.putString("wifiPassword", wifiPassword);
    preferences.end();
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Read connection password");
    pCharacteristic->setValue(wifiPassword);
  }
};

class ConnStatusCallbackHandler: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) {
      Serial.println("Connection status not set");
      return;
    }
    Serial.print("New connection status: ");
    Serial.println(value.c_str());
    if (strcmp(value.c_str(), "CONNECT") == 0) {
      Serial.println("Checking for on-memory credentials");
      if (!wifiSSIDNameIsValid() || !wifiPasswordIsValid()) {
        Serial.println("No on-memory credentials found. Checking for saved credentials");
        
        preferences.begin("credentials", true);
        preferences.getString("wifiSSIDName", wifiSSIDName, 32);
        preferences.getString("wifiPassword", wifiPassword, 32);

        if (!wifiSSIDNameIsValid() || !wifiPasswordIsValid()) {
          Serial.println("No saved credentials found");
          preferences.clear();
          preferences.end();

          DEVICE_ERROR = ESP_NO_CREDENTIALS;
          return;
        }
      }
      Serial.println("Connecting to WiFi network");
      connectWiFi();
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nWiFi connected");
    } else if (strcmp(value.c_str(), "DISCONNECT") == 0) {
      Serial.println("Disconnecting from WiFi network");
      WiFi.disconnect();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Read connection status");
    if (WiFi.status() == WL_CONNECTED) {
      pCharacteristic->setValue("CONNECTED");
    } else {
      pCharacteristic->setValue("DISCONNECTED");
    }
  }
};

class DeviceErrorCallbackHandler: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Read device error");
    pCharacteristic->setValue(ESP_GetDeviceErrorAsString(DEVICE_ERROR));
  }
};

bool wifiSSIDNameIsValid() {
  if (wifiSSIDName == NULL) {
    Serial.println("SSID name not set");
    DEVICE_ERROR = ESP_SSID_NAME_NOT_SET;
    return false;
  }

  const size_t wifiSSIDNameLen = strlen(wifiSSIDName);
  if (wifiSSIDNameLen > MAX_SSID_NAME_LENGTH) {
    Serial.println("SSID name too long");
    DEVICE_ERROR = ESP_SSID_NAME_TOO_LONG;
    return false;
  }
  return true;
}

bool wifiPasswordIsValid() {
  if (wifiPassword == NULL) {
    Serial.println("Connection password not set. Using open network");
    return true;
  }

  const size_t wifiPasswordLen = strlen(wifiPassword);
  if (wifiPasswordLen > MAX_CONN_PASS_LENGTH) {
    Serial.println("Connection password too long");
    DEVICE_ERROR = ESP_SSID_CONN_PASS_TOO_LONG;
    return false;
  }
  return true;
}

void initCharacteristics() {
  // Create BLE Characteristic for WiFi settings
  pSSIDName = pService->createCharacteristic(
    BLEUUID(WIFI_SSID_NAME_UUID),
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  pSSIDName->setCallbacks(new SSIDNameCallbackHandler());
  pSSIDName->setValue("WiFi SSID Name");

  // Create BLE Characteristic for WiFi password
  pConnPass = pService->createCharacteristic(
    BLEUUID(WIFI_CONN_PASS_UUID),
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  pConnPass->setCallbacks(new ConnPassCallbackHandler());
  pConnPass->setValue("WiFi password");

  // Create BLE Characteristic for WiFi connection status
  pConnStatus = pService->createCharacteristic(
    BLEUUID(WIFI_CONN_STATUS_UUID),
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  pConnStatus->setCallbacks(new ConnStatusCallbackHandler());
  pConnStatus->setValue("WiFi Connection Status");

  // Create BLE Characteristic for device error
  pDeviceError = pService->createCharacteristic(
    BLEUUID(DEVICE_ERROR_SERVICE_UUID),
    BLECharacteristic::PROPERTY_READ
  );
  pDeviceError->setCallbacks(new DeviceErrorCallbackHandler());
  pDeviceError->setValue("Device Error");
}

/**
 * initBLE
 * Initialize BLE service and characteristic
 * Start BLE server and service advertising
 */
void initBLE() {
  // Initialize BLE and set output power
  BLEDevice::init(apName);
  BLEDevice::setPower(ESP_PWR_LVL_P7);

  // Create BLE Server
  pServer = BLEDevice::createServer();

  // Set server callbacks
  pServer->setCallbacks(new ServerCallbacks());

  // Create BLE Service
  pService = pServer->createService(BLEUUID(ESP_AP_SERVICE_UUID), 5);

  initCharacteristics();

  // Start the service
  pService->start();

  // Start advertising
  pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

/** Callback for receiving IP address from AP */
void gotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/** Callback for connection loss */
void connectionLost(WiFiEvent_t event, WiFiEventInfo_t info) {
  isConnected = false;
  connStatusChanged = true;
  Serial.println("Lost connection to WiFi network");
}

void checkConnectionStatus() {
  if (connStatusChanged) {
    if (isConnected) {
      pConnStatus->setValue("CONNECTED");
    } else {
      pConnStatus->setValue("DISCONNECTED");
    }
    pConnStatus->notify();
    connStatusChanged = false;
  }
}

/**
 * @brief Scan for WiFi networks and look for the saved SSID
 * 
 * @return true if the saved SSID is found, false otherwise
 */
bool scanWiFi() {
  bool result = false;

  Serial.println("Start scanning for networks");

  WiFi.disconnect(true);
  WiFi.enableSTA(true);
  WiFi.mode(WIFI_STA);

  // Scan for AP
  uint8_t accessPoints = WiFi.scanNetworks(false, true, false,50);
  if (accessPoints == 0) {
    Serial.println("No networks found");
    return false;
  }
  

  for (uint8_t i = 0; i < accessPoints; i++) {
    String ssidFound = WiFi.SSID(i);
    Serial.printf("Found AP: %s RSSI: %s\n", ssidFound, WiFi.RSSI(i));
    if (!strcmp((const char*) &ssidFound[0], (const char*) &wifiSSIDName[0])) {
      Serial.println("Found primary AP");
      result = true;
    }
  }

  return result;
}

/**
 * Start connection to AP
 */
void connectWiFi() {
  // Setup callback function for successful connection
  WiFi.onEvent(gotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  // Setup callback function for lost connection
  WiFi.onEvent(connectionLost, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  WiFi.disconnect(true);
  WiFi.enableSTA(true);
  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.printf("Start connection to %s", wifiSSIDName);
  WiFi.begin(wifiSSIDName, wifiPassword);
}

void setup() {
  // Create unique device name
  createName();

  // Initialize Serial port
  Serial.begin(115200);
  // Send some device info
  Serial.print("Build: ");
  Serial.println(compileDate);

  preferences.begin("credentials", true);

  bool hasPref = preferences.getBool("valid", false);
  if (hasPref) {
    Serial.println("Found stored WiFi credentials");
    preferences.getString("wifiSSIDName", wifiSSIDName, 32);
    preferences.getString("wifiPassword", wifiPassword, 32);

    if (!wifiSSIDNameIsValid() || !wifiPasswordIsValid()) {
      Serial.println("Found preferences but credentials are invalid");
    } else {
      Serial.println("Read from preferences:\n");
      Serial.printf("Primary SSID: %s PASSWORD: %s\n", wifiSSIDName, wifiPassword);
      hasCredentials = true;
    }
  } else {
    Serial.println("Could not find preferences, need send data over BLE");
  }
  preferences.end();

  // Start BLE server
  initBLE();

  if (hasCredentials) {
    // Check for available AP's
    if (!scanWiFi()) {
      Serial.println("Could not find any AP");
    } else {
      // If AP was found, start connection
      connectWiFi();
    }
  }
}

void loop() {
  if (connStatusChanged) {
    if (isConnected) {
      Serial.printf("Connected to AP: %s IP: %s RSSI: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      if (hasCredentials) {
        Serial.println("Lost WiFi connection");
        // Received WiFi credentials
        if (!scanWiFi) { // Check for available AP's
          Serial.println("Could not find any AP");
        } else { // If AP was found, start connection
          connectWiFi();
        }
      } 
    }
    connStatusChanged = false;
  }
}
