//https://www.hivemq.com/products/mqtt-cloud-broker/

/*
  Definição das bibliotecas necessárias para o projeto

  1 - ESP8266WiFi.h -> fornece a interface para utilizar as funcionalidades 
  de conectividade Wi-Fi do chip ESP8266.

   Essa biblioteca permite que o ESP8266 se conecte a uma rede Wi-Fi existente 
   (como cliente), configure-se como um ponto de acesso (access point), 
   ou atue como um servidor web, disponibilizando dados de sensores 
   ou recebendo comandos pela internet ou rede local.

  2 - PubSubClient.h -> trata-se de um cliente para o protocolo de mensagens 
  MQTT (Message Queuing Telemetry Transport). O MQTT é um protocolo de 
  mensagens leve, ideal para dispositivos pequenos e com recursos limitados, 
  como microcontroladores.

   Essa biblioteca permite que o dispositivo publique mensagens em tópicos 
   específicos e/ou se inscreva para receber mensagens de outros 
   tópicos em um broker MQTT (um servidor central).
*/ 

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

/*
  Configurações iniciais referentes à conexão com a rede wi-fi e com
  o broker MQTT. Para testes iniciais, estou utilizando o broker em nuvem
  HiveMQ Cloud.
*/
const char* ssid = "Marcio_2G"; // Nome da rede WIFI
const char* password = "af885912"; // Senha da rede WIFI
const char* mqtt_server = "9e6964edb55440d69e16552346dc4abc.s1.eu.hivemq.cloud"; // URL do servidor MQTT
const uint16_t mqtt_port = 8883; //Porta de conexão do broker MQTT
const char* mqtt_user = "HMQC_teste1234"; // Usuário do broker para autenticação
const char* mqtt_pass = "HMQC_teste1234"; // Senha do broker para autenticação

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

/*
 Variáveis responsáveis por guardae os nomes dos tópicos MQTT que o ESP vai usar:

  topic_ldr — tópico onde o ESP publica a leitura do LDR.
  topic_led_set — tópico onde o ESP se inscreve (subscribe) para receber comandos de controle do LED.
  topic_led_status — tópico onde o ESP publica o estado atual do LED.

  OBS: const char* (string literal) consome muito menos heap e evita fragmentação de 
  memória no microcontrolador. Em dispositivos pequenos como ESP8266 é prática comum
  usar isso para tópicos fixos.
*/
const char* topic_ldr = "home/esp8266/ldr";
const char* topic_led_set = "home/esp8266/led/set";
const char* topic_led_status = "home/esp8266/led/status";

// Define intervalo para publicar novas leituras ao broker, 
//nesse caso, o valor será atualizado a cada segundo
const unsigned long PUBLISH_INTERVAL_MS = 1000; 

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

/*
  Definição dos rótulos para os pinos utilizados, sendo:
  D1 -> Pino digital para acionamento do LED (Estado binário ON/OFF)
  A0 -> Pino de entrada analógica para receber os valores lidos por sensores (Nesse caso, o LDR)
*/
const int pinLED = D1;    
const int pinLDR = A0; 

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

/*
  WiFiClient é uma classe da biblioteca ESP8266WiFi, representando um socket TCP/IP. 
  A biblioteca MQTT (PubSubClient) precisa de um objeto que saiba enviar/receber bytes pela rede, 
  portanto, o WiFiClient é instanciado e passado para o PubSubClient, que então usa esse 
  cliente de rede para abrir a conexão TCP com o broker.

  'PubSubClient' representa a instância do cliente MQTT — ele conhece tópicos, callbacks, 
  publica/subscribe, mantém estado do protocolo MQTT
*/
WiFiClientSecure espClient;
//WiFiClient espClient;
PubSubClient client(espClient);

// Variável que guarda o tempo em milisegundos da última 
// publicação do sensor, inicializando em zero.
unsigned long lastPublish = 0; 
// Variável booleana que representa o estado lógico do LED no firmware
bool ledState = false;

//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------

/*
  Função 'callback':

  CONCEITO:
  Trata-se de um mecanismo de programação orientado a eventos. É uma função que não é chamada  diretamente
  pelo usuário, mas sim registrada para ser executada automaticamente quando um determinado evento acontece.
  No contexto dessa aplicação, o evento que chama o callback é a chegada de uma mensagem em um tópico MQTT,
  logo:

    * O ESP assina tópicos (client.subscribe("home/esp8266/led/set"))
    * O broker envia mensagens nesses tópicos
    * Quando uma mensagem chega, a biblioteca MQTT invoca a callback automaticamente
    * A callback seria o "ouvido" do sistema, 'escutando' comandos vindos do broker e reagindo a eles.

  OBSERVAÇÃO 1: O MQTT é orientado a evento e não a requisição direta (como REST). Por isso, se faz 
  necessário a existencia da função callback. Sem ele, o ESP não saberia quando uma mensagem chega 
  e nem como interpretar a instrução.

  OBSERVAÇÃO 2: O que são os tópicos MQTT?:
  São endereços de mensagens usados para organizar e rotear dados em um sistema de publicação/assinatura. 
  Eles operam como rótulos para mensagens, onde um dispositivo (o "publisher") envia uma mensagem para 
  um tópico específico, e outros dispositivos (os "subscribers") que estão "inscritos" nesse tópico 
  recebem a mensagem. 
    Como exemplo -> Um sensor de temperatura pode publicar leituras no tópico "casa/temperatura", 
    e um aplicativo que se inscreve nesse tópico receberá todas as atualizações de temperatura.

  PARÂMETROS DA FUNÇÃO:
    1)  topic ->	nome do tópico em que a mensagem chegou
    2)  payload ->	conteúdo da mensagem em bytes (não é string ainda)
    3)  length ->	tamanho da mensagem

*/

void callback(char* topic, byte* payload, unsigned int length) {
  
  String msg; //Variável responsável por armazenar a mensagem já convertida para string.
  
  /*
    MQTT envia o conteúdo como vetor de bytes, sendo cada byte, um caractere.
    O loop abaixo é responsável por gerar o caractere referente à cada byte
    transmitido no Payload.
  */
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  /*
    O seguinte trecho permite controlar o acionamento do LED, caso a mensagem
    tenha sido proveniente do tópico de controle do LED, definido por topic_led_set.
    Caso a mensagem tenha os caracteres 'ON', o pino pinLED é acionado, caso a mensagem
    tenha os caracteres 'OFF' o pino é desativado.
  */ 

  if (String(topic) == topic_led_set) {
    if (msg == "ON") {
      ledState = true;
      digitalWrite(pinLED, HIGH);
      client.publish(topic_led_status, "ON", true);
    } else if (msg == "OFF") {
      ledState = false;
      digitalWrite(pinLED, LOW);
      client.publish(topic_led_status, "OFF", true);
    }
  }
}

/*
  Função 'reconnect':

  CONCEITO:
  Trata-se de um mecanismo para garantir a resiliência da conexão MQTT, ou seja,
  é um loop de auto-recuperação de comunicação para garantir que o dispositivo
  volte a se comunicar com o broker MQTT em caso de falhas como:
    * Oscilação de Wi-Fi 
    * Reinicialização do broker
    * Timeout de keep-alive
    * Perda momentânea de energia
*/
void reconnect() {

  /*
    O 'while' permanece no loop até que a conexão tenha sido efetivamente
    estabelecida, ou seja, client.connected() retorne status verdadeiro
  */ 

  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT..."); // Mensagem de LOG
    /*
      O protocolo MQTT exige ID único por cliente, com isso, é possível
      evitar conflitos caso haja 2 ou mais dispositivos conectados na 
      mesma rede. O identificador é gerado (por convenção) pela união do
      nome do dispositivo com o identificador físico do chip (transformado
      para o valor hexadecimal). O resultado seria algo como: 'ESP8266-3F2A7B'

      OBSERVAÇÃO 1: ESP.getChipId() pega o identificador físico do chip
      OBSERVAÇÃO 2: HEX converte para hexadecimal
    */
    String clientId = "ESP8266-";
    clientId += String(ESP.getChipId(), HEX);
    
    /*
      Tentativa de conexão com o broker MQTT, passando os parâmetros
      de usuário, senha e identificador do cliente (). Em caso positivo,
      as seguintes operações são realizadas:
      1 - Mensagem de log no monitor serial
      2 - Inscrição no tópico do LED (viabilizando o recebimento de comandos)
      3 - Evio (publicação) do estado atual do LED ao broker

          OBSERVAÇÃO: 'ledState ? "ON" : "OFF", true' -> Indica um operador ternário
          que atua basicamente como uma abreviação do if-else. 'ledstate' seria a 
          condição: se ela for verdadeira, retorna a string "ON". Caso contrário,
          retorna a string "OFF".

      Em caso negativo é passado uma mensagem de log ao usuário e em sequência 
      uma nova iteração do 'while' é executada (Após um dealy de 5 segundos).

      O QUE MELHORAR:
      1 - Evitar de usar o while e o delay juntos, o que pode ocasionar em 
          travamentos caso o broker fique fora do ar.
      2 - Implementar reconexão Wi-Fi em conjunto com o MQTT
    */
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("conectado");
      client.subscribe(topic_led_set); 
      client.publish(topic_led_status, ledState ? "ON" : "OFF", true);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

/*
  Função 'setup':
  
  CONCEITO:
  A função setup() é executada uma vez ao iniciar o programa (no momento de boot ou reset).
  Sendo crucial para as funções de inicialização de hardware, configuração de comunicação,
  ajuste de variáveis e definição dos estados iniciais.
*/

void setup() {
  // Inicia porta serial para debug via USB, o valor de velocidade 115200 é padrão para o ESP-8266
  Serial.begin(115200);

  // Define o pino do LED como saída digital e inicializa seu estado inicial para LOW (desligado)
  pinMode(pinLED, OUTPUT);
  digitalWrite(pinLED, LOW);

  /*
    Coloca o ESP em modo Station (STA), portanto, o dispositivo atua como cliente de rede, 
    conectando ao roteador-se. Outros modos disponíveis nessa função são:
    - WIFI_AP	-> O dispositivo atua como 'Access Point'
    - WIFI_AP_STA -> O dispositivo acessa Wi-Fi e cria rede própria
  */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); // Inicia conexão Wi-Fi com SSID e senha

  /*
    Loop de espera pela conexão com o WiFi. Aqui o programa exibe ponto sequencialmente
    até que a conexão seja devidamente estabelecida.
  */ 

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  /*
    Ao sair do loop (indicando que a conexão foi devidamente estabelecida), o programa
    gera mensagens de log informando o status da conexão e o endereço ao qual está
    conectado. WiFi.localIP() exibe o IP recebido do roteador.
  */ 
  Serial.println("");
  Serial.print("Conectado ao WiFi. IP: ");
  Serial.println(WiFi.localIP());

  /*
  Quando o MQTT é implementado com TLS/SSL (porta 8883) — como no HiveMQ Cloud — 
  o ESP8266 precisa validar certificados digitais (igual navegador validando HTTPS). 
  Para isso, ele depende do relógio interno correto, pois certificados têm datas de validade.
  O ESP8266 não possui relógio real (RTC), portanto a conexão falha ao tantar verificar 
  um certificado com data falsa.

  O trecho abaixo sincroniza com um servidor NTP (Network Time Protocol) 
  para obter a hora real da internet.
  */
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nHora sincronizada!");
  /*
    Aqui são configurados o endereço e porta do broker MQTT. Importante destacar que a
    conexão não é estabelecida aqui, apenas há um 'ponteiro' para o servidor à ser conectado.
    Em sequência é definido qual função deve ser chamada quando chegar uma mensagem MQTT,
    neste caso, a função de callback definida previamente.

    OBSERVAÇÃO:
    O espClient.setInsecure() é uma função usada no WiFiClientSecure do ESP8266 que diz ao cliente TLS:
    “Conecte usando criptografia, mas NÃO verifique se o certificado do servidor é verdadeiro.”
    Ou seja, a conexão ainda é criptografada (HTTPS/TLS), mas a validade da identidade do servidor não é verificada.
  */
  espClient.setInsecure();  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

/*
  Função 'loop':
  
  CONCEITO:
  A função loop() no Arduino/ESP8266 é executada continuamente. Nessa operação, 3 funcionalidades
  principais são gerenciadas:

  1 - Garantir conexão com o broker MQTT
  2 - Processar mensagens MQTT recebidas
  3 - Ler os sensores periodicamente e publicar o valor
*/
void loop() {

  /* 
    Valida inicialmente se o dispositivo (cliente) ainda está conectado ao broker. Em caso
    negativo, a função de reconexão é chamada novamente. 
  */
  if (!client.connected()) {
    reconnect();
  }

  /*
    client.loop()  Processa o protocolo MQTT, checando se há mensagens recebidas
    e executando a função `callback()` quando uma nova mensagem chega.
  */
  client.loop();

  /*
    A função millis() retorna o tempo (em ms) desde que o ESP ligou. Com isso, a variável
    now armazena o tempo atual a fim de controlar o intervalo de publicação.
  */
  unsigned long now = millis();

  //  A condicional se já passou o tempo necessário desde a última publicação.
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    
    // Atualiza o tempo da última publicação
    lastPublish = now; 
    // Realiza a leitura crua dos valores no pino analógico do LDR (variando entre 0 e 1023)
    int raw = analogRead(pinLDR); 
    // Transforma o valor analógico lido em uma porcentagem
    float percent = (raw / 1023.0) * 100.0;

    /*
      Cria um buffer (payload) para armazenar o JSON que será enviado por MQTT.
      'snprintf' escreve dentro do buffer um JSON no formato: {"valor":500,"porcentagem":48.8},
    */
    char payload[80];
    snprintf(payload, sizeof(payload), "{\"valor\":%d,\"porcent\":%d}", raw, (int)percent);

    /*
      Publica a leitura do LDR no tópico correspondente
      o parâmetro 'true' indica mensagem com retain, ou seja, o último valor enviado
      fica guardado no broker
    */
    client.publish(topic_ldr, payload, true);
    // Não precisa republicar o status do LED aqui porque já enviamos quando ele muda no callback

    //Mensagens de LOG no monitor serial informando o payload enviado
    Serial.print("Publicado LDR: ");
    Serial.println(payload);
  }
}