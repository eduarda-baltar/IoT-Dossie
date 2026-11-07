# Monitor do Depósito · Desafio Final IoT (Senac PE)

> Projeto desenvolvido por **Maria Eduarda Baltar** como solução para a avaliação **A4 · Dossiê do Projeto** da Unidade Curricular de *Formação IoT para Monitoramento e Automação* da **Faculdade Senac Pernambuco**, sob orientação do Prof. Arnott Ramos Caiado.

## 📌 Contexto e Missão

Os equipamentos de iluminação e áudio do Grupo Pleno permanecem armazenados em um depósito sujeito a elevações críticas de temperatura e acúmulo de umidade, sem acompanhamento presencial contínuo.

**Objetivo:** Construir um sistema inteligente com ESP32 que:
1. Monitore continuamente temperatura e umidade em intervalos regulares;
2. Dispare um alarme local sonoro e luminoso (*fail-safe*) com controle de histerese;
3. Publique a telemetria em tempo real via protocolo MQTT para acompanhamento remoto pelo gestor;
4. Emita notificações imediatas de evento crítico na transição de alerta.

## 🛠️ Hardware e Pinagem

Simulado na plataforma **Wokwi**:

| Componente | Pino no ESP32 | Função |
| :--- | :--- | :--- |
| **DHT22** (Data / SDA) | `GPIO 15` | Leitura digital de temperatura e umidade |
| **LED Vermelho** (Alarme) | `GPIO 2` | Sinalização óptica de superaquecimento |
| **Buzzer Piezoelétrico** | `GPIO 4` | Sinalização acústica de alarme local |
| **LED Verde** (Comando) | `GPIO 16` | Atuação remota via comandos MQTT |
| **Alimentação DHT22** | `3V3` e `GND` | Alimentação lógica do sensor |

## ⚙️ Lógica de Funcionamento e Histerese

* **Intervalos de Execução (`millis()`):**
  * **RF1 / RF2 (Amostragem Local):** Leitura do DHT22 a cada **2 segundos**, filtrando medições inconsistentes (`isnan` ou erro de barramento).
  * **RF3 (Telemetria Remota):** Publicação dos dados ambientais a cada **5 segundos**.
* **Controle de Histerese Térmica (RF2):**
  * **Limite de Ativação:** $T > 28{,}0\ ^\circ\text{C}$ $\rightarrow$ Liga LED vermelho / buzzer e emite alerta único.
  * **Limite de Desativação:** $T < 27{,}0\ ^\circ\text{C}$ (ou seja, $\text{Limite} - 1{,}0\ ^\circ\text{C}$) $\rightarrow$ Desliga alarme.
  * *Objetivo:* Evitar comutações intermitentes (*repique*) quando a temperatura flutua na borda do limiar.

## 📡 Arquitetura de Comunicação (MQTT)

* **Broker:** `broker.hivemq.com` (Porta: `1883`)
* **Identificador de Cliente:** `esp32-<MATRICULA>`

### Árvore de Tópicos

| Tópico | Tipo | Payload / Exemplo |
| :--- | :--- | :--- |
| `senac/iot/<MATRICULA>/checkin` | Publicação (Retida) | `{"mat":"<MATRICULA>","limite":28.0,"clientId":"esp32-<MATRICULA>","base":3}` |
| `senac/iot/<MATRICULA>/sala1/telemetria` | Publicação (5 s) | `{"id":"esp32-<MATRICULA>","t":24.5,"h":55,"alarme":false}` |
| `senac/iot/<MATRICULA>/sala1/alerta` | Publicação (Evento) | `{"evento":"ALERTA","t":32.7,"limite":28.0}` |
| `senac/iot/<MATRICULA>/sala1/cmd/led` | Subscrição | `ON` ou `OFF` |
| `senac/iot/<MATRICULA>/sala1/ack` | Publicação | `LED=ON` ou `LED=OFF` |

## 🧪 Roteiro de Validação e Testes

Para acompanhar a transmissão dos dados:
1. Acesse o [HiveMQ WebSocket Client](http://www.hivemq.com/demos/websocket-client/).
2. Clique em **Connect**.
3. Adicione uma subscrição ao tópico: `senac/iot/<MATRICULA>/#`.

### Casos de Teste (Demonstrados no Vídeo)

* **T1 · Normal:** Sensor abaixo do limite ($< 28\ ^\circ\text{C}$). Telemetria transmitida com `"alarme": false` e LED apagado.
* **T2 · Alarme:** Sensor elevado acima do limite ($> 28\ ^\circ\text{C}$). LED vermelho acende instantaneamente, telemetria reporta `"alarme": true` e mensagem de alerta é emitida no tópico `.../alerta`.
* **T3 · Retorno com Histerese:** Sensor reduzido abaixo do limiar de histerese ($< 27\ ^\circ\text{C}$). LED apaga e o sistema retoma o estado normal.

## 📂 Estrutura de Arquivos

```text
├── sketch.ino       # Código C++ do firmware para ESP32
├── diagram.json     # Definição e conexões de hardware no Wokwi
├── libraries.txt    # Lista de dependências de bibliotecas
└── README.md        # Documentação do projeto
```

## 🔗 Links Úteis

* **Simulação Interativa no Wokwi:** [Projeto Wokwi](https://wokwi.com/projects/480074423010085889)
* **Repositório GitHub:** [IoT-Dossie](https://github.com/eduarda-baltar/IoT-Dossie)

## 👤 Autoria e Créditos

* **Autora:** Maria Eduarda Baltar
* **Instituição:** Faculdade Senac Pernambuco (COMPET Superior)
* **Orientação:** Prof. Arnott Ramos Caiado (Base 3 IoT)
