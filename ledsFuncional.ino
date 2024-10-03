#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <AESLib.h>




#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
#define DEVICE "ESP32"
  #elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>


// WiFi AP SSID
#define WIFI_SSID "Flia_Certuche"
// WiFi password
#define WIFI_PASSWORD "Alejandro2020"
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> Generate API Token)
#define INFLUXDB_TOKEN "ZAjMedODQBE4DcapOxrxrd1UJARHxfsIOkRByc3p_QtJufP2h90RK2iKCZRcK8tV2jbAWG8MJFVbxsdIgeyMnQ=="
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "c4c6b195ee3f25fd"
  #define INFLUXDB_BUCKET "Sensores"
// Timezone information
#define TZ_INFO "<-03>3"


// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("environment_data");

#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

// Definimos los pines para los LEDs
#define yellowLED 25
#define whiteLED  26
#define greenLED  27

Scheduler userScheduler;
painlessMesh mesh;

String nodeId2 = "nodo2";
int state=0;
int state1=0;

// Clave de encriptación y vector de inicialización
byte aesKey[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
byte iv[16] = {0};
AESLib aesLib;

void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
int handleTemperature(float temp, int hum);

void setup() {
  Serial.begin(115200);

  // Configurar LEDs
  pinMode(yellowLED, OUTPUT);
  pinMode(whiteLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  // Apagar LEDs al inicio
  digitalWrite(yellowLED, LOW);
  digitalWrite(whiteLED, LOW);
  digitalWrite(greenLED, LOW);

  // Inicializar red mesh
  mesh.setDebugMsgTypes(ERROR | STARTUP);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}



void loop() {
  mesh.update();
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Mensaje recibido de %u: %s\n", from, msg.c_str());

  // Desencriptar el mensaje
  byte decrypted[128];
  int len = aesLib.decrypt((byte*)msg.c_str(), msg.length(), decrypted, aesKey, 128, iv);
  String decryptedString((char*)decrypted, len);

  // Parsear el mensaje JSON
  JSONVar jsonData = JSON.parse(decryptedString);
  if (JSON.typeof(jsonData) == "undefined") {
    Serial.println("Error en el JSON.");
    return;
  }

  // Extraer la temperatura y humedad
  String nodeid1 = (const char*)jsonData["node"];
  float temp = (double)jsonData["temp"];
  int hum = (int)jsonData["hum"];
  Serial.printf("Node ID: %s\n", nodeid1.c_str());
  Serial.printf("Temperatura: %.2f, Humedad: %d\n", temp, hum);

  // Manejar LEDs según la temperatura
  state1 = handleTemperature(temp, hum);

  // Subir estado del nodo LED (nodo2) a InfluxDB
  sensor.clearFields();  // Limpia campos previos
  sensor.clearTags();    // Limpia etiquetas previas
  sensor.addTag("id", nodeId2); // Agregar ID del nodo como etiqueta
  sensor.addField("estado_del_led", int(state1)); // Añadir estado del LED
  Serial.print("Writing estado del LED: ");
  Serial.println(client.pointToLineProtocol(sensor));
  
    // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Subir datos de temperatura y humedad (nodo1) a InfluxDB
  sensor.clearFields();  // Limpia campos previos
  sensor.clearTags();    // Limpia etiquetas previas
  sensor.addTag("id", nodeid1); // Agregar ID del nodo 1 como etiqueta
  sensor.addField("temperature", temp); // Añadir temperatura
  sensor.addField("humidity", hum); // Añadir humedad
  Serial.print("Writing temperatura y humedad: ");
  Serial.println(client.pointToLineProtocol(sensor));

    // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

int handleTemperature(float temp, int hum) {
  
  if (temp < 17) {
    digitalWrite(yellowLED, HIGH);
    digitalWrite(whiteLED, LOW);
    digitalWrite(greenLED, LOW);
    state=1; //estado 1->T<17

  } else if (temp >= 17 && temp <= 25) {
    digitalWrite(yellowLED, LOW);
    digitalWrite(whiteLED, HIGH);
    digitalWrite(greenLED, LOW);
        state=2; //estado 2->17>T<25
   
  } else {
    digitalWrite(yellowLED, LOW);
    digitalWrite(whiteLED, LOW);
    digitalWrite(greenLED, HIGH);
        state=3; //estado 3->T>25
 
  }
  return state;
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("Nueva conexión: %u\n", nodeId);
}
