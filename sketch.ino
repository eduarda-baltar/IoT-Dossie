/*
  Desafio Final IoT: Monitor do Depósito (Caminho B - MQTT no HiveMQ)
  Base 3 adaptada com RF1, RF2, RF3 e RF4 completos.
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// ===================== CONFIGURAÇÃO =====================
const char* MATRICULA = "SUA_MATRICULA";        // <-- Coloque a sua matrícula aqui!
const char* BROKER    = "broker.hivemq.com";
const int   PORTA     = 1883;
const float LIMITE_TEMP = 28.0;                 // Limite escolhido (entre 26 °C e 32 °C)
// ========================================================

const int PINO_DHT     = 15;
const int PINO_LED     = 2;    // LED vermelho: alarme local
const int PINO_BUZZER  = 4;
const int PINO_LED_CMD = 16;   // LED verde: comandos remotos

DHTesp       dht;
WiFiClient   wifi;
PubSubClient mqtt(wifi);

char clientId[40];
char tCheckin[64], tTelemetria[80], tCmd[80], tAck[80], tAlerta[80], tStatus[64];

float limite = LIMITE_TEMP;
bool alarme = false;

// Temporizadores independentes
unsigned long ultimaLeitura = 0;   // RF1: a cada 2000 ms (2 s)
unsigned long ultimoEnvio   = 0;   // RF3: a cada 5000 ms (5 s)
unsigned long ultimaTentativa = 0;

// Últimas leituras válidas guardadas na memória
float tempAtual = 0.0;
float umidAtual = 0.0;

// RF2: Histerese (liga acima do limite e desliga abaixo de limite - 1 °C)
bool decidirAlarme(float t, bool alarmeAtual) {
  if (!alarmeAtual && t > limite)      return true;
  if (alarmeAtual  && t < limite - 1.0) return false;
  return alarmeAtual;
}

void atualizarSaidas(bool ligado) {
  digitalWrite(PINO_LED, ligado ? HIGH : LOW);
  if (ligado) {
    tone(PINO_BUZZER, 1000, 150);
  }
}

void montarTopicos() {
  snprintf(clientId,    sizeof(clientId),    "esp32-%s", MATRICULA);
  snprintf(tCheckin,    sizeof(tCheckin),    "senac/iot/%s/checkin", MATRICULA);
  snprintf(tTelemetria, sizeof(tTelemetria), "senac/iot/%s/sala1/telemetria", MATRICULA);
  snprintf(tCmd,        sizeof(tCmd),        "senac/iot/%s/sala1/cmd/led", MATRICULA);
  snprintf(tAck,        sizeof(tAck),        "senac/iot/%s/sala1/ack", MATRICULA);
  snprintf(tAlerta,     sizeof(tAlerta),     "senac/iot/%s/sala1/alerta", MATRICULA);
  snprintf(tStatus,     sizeof(tStatus),     "senac/iot/%s/status", MATRICULA);
}

// Receptor de comandos MQTT
void aoReceber(char* topico, byte* payload, unsigned int tamanho) {
  char msg[16];
  unsigned int n = (tamanho < sizeof(msg) - 1) ? tamanho : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = '\0';
  Serial.printf("Comando recebido em %s: %s\n", topico, msg);

  if (strcmp(msg, "ON") == 0) {
    digitalWrite(PINO_LED_CMD, HIGH);
    mqtt.publish(tAck, "LED=ON");
  } else if (strcmp(msg, "OFF") == 0) {
    digitalWrite(PINO_LED_CMD, LOW);
    mqtt.publish(tAck, "LED=OFF");
  }
}

bool conectarMqtt() {
  if (!mqtt.connect(clientId)) return false;

  char checkin[128];
  snprintf(checkin, sizeof(checkin),
           "{\"mat\":\"%s\",\"limite\":%.1f,\"clientId\":\"%s\",\"base\":3}",
           MATRICULA, limite, clientId);
  mqtt.publish(tCheckin, checkin, true);
  mqtt.subscribe(tCmd);
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  pinMode(PINO_LED_CMD, OUTPUT);
  dht.setup(PINO_DHT, DHTesp::DHT22);

  montarTopicos();
  Serial.printf("Monitor do Deposito | Matricula=%s | Limite=%.1f C\n", MATRICULA, limite);

  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");

  mqtt.setServer(BROKER, PORTA);
  mqtt.setBufferSize(512);
  mqtt.setCallback(aoReceber);
}

void loop() {
  // Reconexão MQTT não-bloqueante
  if (!mqtt.connected() && millis() - ultimaTentativa > 3000) {
    ultimaTentativa = millis();
    Serial.print("Conectando ao broker HiveMQ... ");
    Serial.println(conectarMqtt() ? "Conectado!" : "Falhou, tentando em 3s...");
  }
  mqtt.loop();

  unsigned long agora = millis();

  // =========================================================
  // RF1 e RF2: Medição local e acionamento a cada 2 segundos
  // =========================================================
  if (agora - ultimaLeitura >= 2000) {
    ultimaLeitura = agora;

    TempAndHumidity leitura = dht.getTempAndHumidity();

    // RF1: Ignorar leituras inválidas
    if (dht.getStatus() == DHTesp::ERROR_NONE && !isnan(leitura.temperature) && !isnan(leitura.humidity)) {
      tempAtual = leitura.temperature;
      umidAtual = leitura.humidity;

      bool novoAlarme = decidirAlarme(tempAtual, alarme);

      // RF4: Publica no tópico de alerta apenas na borda de subida (transição falso -> verdadeiro)
      if (!alarme && novoAlarme) {
        char alertaJson[128];
        snprintf(alertaJson, sizeof(alertaJson),
                 "{\"evento\":\"ALERTA\",\"t\":%.1f,\"limite\":%.1f}", tempAtual, limite);
        if (mqtt.connected()) {
          mqtt.publish(tAlerta, alertaJson);
        }
        Serial.printf(">>> ALERTA PUBLICADO em %s: %s <<<\n", tAlerta, alertaJson);
      }

      alarme = novoAlarme;
      atualizarSaidas(alarme);
    } else {
      Serial.println("Aviso: Falha na leitura do sensor DHT22. Ignorando...");
    }
  }

  // =========================================================
  // RF3: Publicação da telemetria em JSON a cada 5 segundos
  // =========================================================
  if (agora - ultimoEnvio >= 5000) {
    ultimoEnvio = agora;

    // Apenas publica se tivermos pelo menos uma leitura válida
    if (tempAtual != 0.0 || umidAtual != 0.0) {
      char json[128];
      snprintf(json, sizeof(json),
               "{\"id\":\"%s\",\"t\":%.1f,\"h\":%.0f,\"alarme\":%s}",
               clientId, tempAtual, umidAtual, alarme ? "true" : "false");

      if (mqtt.connected()) {
        mqtt.publish(tTelemetria, json);
        Serial.printf("Telemetria enviada [%s]: %s\n", tTelemetria, json);
      }
    }
  }
}
