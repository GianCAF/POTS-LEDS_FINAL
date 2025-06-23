#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h" // Asegúrate de que tu IDE de Arduino tenga el plugin LittleFS Data Upload
#include <ArduinoJson.h> // Para manejar JSON de manera más robusta

// Reemplaza con tus credenciales de red
const char* ssid = "S21";
const char* password = "12345678";

// Pines de los LEDs
const int ledPin1 = 14;
const int ledPin2 = 12;
const int ledPin3 = 13;

// Pines de los Potenciómetros
const int potPin1 = 36; // VP
const int potPin2 = 39; // VN
const int potPin3 = 34; // GPIO34

// Variables para el control de los LEDs (PWM)
int dutyCycle1 = 0;
int dutyCycle2 = 0;
int dutyCycle3 = 0;

// Configuración de PWM para los LEDs
const int freq = 5000;
const int ledChannel1 = 0;
const int ledChannel2 = 1;
const int ledChannel3 = 2;
const int resolution = 8; // 8 bits de resolución (0-255)

// Variables para las lecturas de los Potenciómetros
int potValue1 = 0;
int potValue2 = 0;
int potValue3 = 0;
int prevPotValue1 = -1; // Para detectar cambios
int prevPotValue2 = -1;
int prevPotValue3 = -1;
const int changeThreshold = 5; // Umbral de cambio para enviar actualizaciones de potenciómetros
unsigned long lastPotReadTime = 0;
const unsigned long potReadInterval = 100; // Intervalo de lectura de potenciómetros (ms)

// Objeto servidor y WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Tamaño del documento JSON para sliders y potenciómetros
// Considera el tamaño de cada string y el número de elementos.
// JSON_OBJECT_SIZE(num_elements) + buffer para strings
const size_t JSON_DOC_SIZE = JSON_OBJECT_SIZE(6) + 120; // 3 sliders + 3 pots

// Función para configurar los canales PWM
void setupPWM() {
  ledcAttachChannel(ledPin1, freq, resolution, ledChannel1);
  ledcAttachChannel(ledPin2, freq, resolution, ledChannel2);
  ledcAttachChannel(ledPin3, freq, resolution, ledChannel3);
}

// Obtener todas las lecturas (sliders y potenciómetros) en formato JSON
String getAllSensorReadings() {
  StaticJsonDocument<JSON_DOC_SIZE> doc;

  // Obtener los valores de brillo actuales de los LEDs
  // Se asume que dutyCycle es el valor actual que se está aplicando al LED
  // y lo mapeamos de vuelta a un porcentaje (0-100) para la UI.
  doc["sliderValue1"] = String(map(dutyCycle1, 0, 255, 0, 100));
  doc["sliderValue2"] = String(map(dutyCycle2, 0, 255, 0, 100));
  doc["sliderValue3"] = String(map(dutyCycle3, 0, 255, 0, 100));

  // Valores de potenciómetros
  doc["potValue1"] = potValue1;
  doc["potValue2"] = potValue2;
  doc["potValue3"] = potValue3;

  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

// Inicializar LittleFS
void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS");
  } else {
    Serial.println("LittleFS montado correctamente");
  }
}

// Inicializar WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    Serial.print('.');
    delay(500);
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nError al conectar al WiFi. Reiniciando...");
    delay(3000);
    ESP.restart();
  }
}

// Notificar a todos los clientes WebSocket con los datos actuales
void notifyClients(String readings) {
  if (ws.count() > 0) {
    ws.textAll(readings);
  }
}

// Manejar mensajes WebSocket entrantes
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;

    // Manejar comandos de sliders (ej. "1s50")
    if (message.startsWith("1s")) {
      String sliderValStr = message.substring(2);
      dutyCycle1 = map(sliderValStr.toInt(), 0, 100, 0, 255);
      Serial.print("Slider 1 ajustado a: ");
      Serial.println(dutyCycle1);
      // Notificar a todos los clientes para sincronizar el slider
      notifyClients(getAllSensorReadings());
    } else if (message.startsWith("2s")) {
      String sliderValStr = message.substring(2);
      dutyCycle2 = map(sliderValStr.toInt(), 0, 100, 0, 255);
      Serial.print("Slider 2 ajustado a: ");
      Serial.println(dutyCycle2);
      notifyClients(getAllSensorReadings());
    } else if (message.startsWith("3s")) {
      String sliderValStr = message.substring(2);
      dutyCycle3 = map(sliderValStr.toInt(), 0, 100, 0, 255);
      Serial.print("Slider 3 ajustado a: ");
      Serial.println(dutyCycle3);
      notifyClients(getAllSensorReadings());
    }
    // Manejar solicitud de todos los valores iniciales ("getReadings")
    else if (message == "getReadings") {
      notifyClients(getAllSensorReadings());
    }
  }
}

// Eventos de conexión/desconexión WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("Cliente WebSocket #%u conectado desde %s\n", client->id(), client->remoteIP().toString().c_str());
      // Enviar todos los valores al cliente recién conectado
      notifyClients(getAllSensorReadings());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Cliente WebSocket #%u desconectado\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);

  // Configurar pines de los LEDs como salida
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  // Configurar pines de los potenciómetros como entrada
  pinMode(potPin1, INPUT);
  pinMode(potPin2, INPUT);
  pinMode(potPin3, INPUT);
  
  // Configurar PWM para los LEDs
  setupPWM();

  // Inicializar LittleFS para servir los archivos web
  initLittleFS();
  
  // Inicializar WiFi
  initWiFi();

  // Inicializar WebSocket
  initWebSocket();

  // Rutas del servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  // Servir otros archivos estáticos (CSS, JS, favicon) desde la carpeta /data
  server.serveStatic("/", LittleFS, "/");

  // Iniciar el servidor web
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  // Leer potenciómetros a un intervalo regular o cuando haya un cambio significativo
  unsigned long currentTime = millis();
  if (currentTime - lastPotReadTime >= potReadInterval) {
    int currentPotValue1 = map(analogRead(potPin1), 0, 4095, 0, 255); // Mapear 0-4095 a 0-255
    int currentPotValue2 = map(analogRead(potPin2), 0, 4095, 0, 255);
    int currentPotValue3 = map(analogRead(potPin3), 0, 4095, 0, 255);

    bool changed = false;
    if (abs(currentPotValue1 - prevPotValue1) > changeThreshold ||
        abs(currentPotValue2 - prevPotValue2) > changeThreshold ||
        abs(currentPotValue3 - prevPotValue3) > changeThreshold) {
      changed = true;
    }

    if (changed) {
      potValue1 = currentPotValue1;
      potValue2 = currentPotValue2;
      potValue3 = currentPotValue3;

      // Notificar a los clientes solo si hay cambios significativos en los potenciómetros
      notifyClients(getAllSensorReadings());

      prevPotValue1 = currentPotValue1;
      prevPotValue2 = currentPotValue2;
      prevPotValue3 = currentPotValue3;
    }
    lastPotReadTime = currentTime;
  }

  // Escribir los valores PWM a los LEDs
  ledcWrite(ledPin1, dutyCycle1);
  ledcWrite(ledPin2, dutyCycle2);
  ledcWrite(ledPin3, dutyCycle3);

  // Limpiar clientes WebSocket desconectados
  ws.cleanupClients();
  delay(1); // Pequeño delay para permitir que otras tareas se ejecuten
}