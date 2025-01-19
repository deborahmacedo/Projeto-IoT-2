#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <FS.h>
#include "SPIFFS.h"

#define PIN_TRIG1 13
#define PIN_ECHO1 14
#define PIN_TRIG2 26
#define PIN_ECHO2 18
#define PIN_TRIG3 25
#define PIN_ECHO3 19
#define LED1 22
#define LED2 32
#define LED3 33

// Variáveis para armazenar o tempo de acendimento de cada LED
unsigned long tempoLED1 = 0;
unsigned long tempoLED2 = 0;
unsigned long tempoLED3 = 0;

// Variável para armazenar o ganho total do estacionamento
int ganhoTotal = 0;

// Variável para contar a quantidade de carros que passaram no estacionamento
int quantidadeCarros = 0;

// Configurações do WiFi e Adafruit IO
#define IO_USERNAME  "deborahmacedo"
#define IO_KEY       "aio_JhDo87G9o6Saaxn8opb1JvVaA0A2"
const char* ssid = "Wokwi GUEST"; // Nome da rede WiFi
const char* password = ""; // Senha da rede WiFi
const char* mqttserver = "io.adafruit.com";
const int mqttport = 1883;
const char* mqttUser = IO_USERNAME;
const char* mqttPassword = IO_KEY;

// Configurações do CallMeBot
const char* callMeBotAPIKey = "4843020";
const char* callMeBotPhone = "558494301318";

WiFiClient espClient;
PubSubClient client(espClient);

//???

// Função para conectar ao WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

//inicio do spiffs


void writeFile(String state, String path) {
  File rFile = SPIFFS.open(path, "w");
  if (!rFile) {
    Serial.println("Erro ao abrir arquivo para escrita!");
    return;
  }
  if (rFile.print(state)) {
    Serial.print("Gravou: ");
    Serial.println(state);
  } else {
    Serial.println("Erro ao gravar no arquivo!");
  }
  rFile.close();
}

String readFile(String path) { // Função que lê o código já gravado no ESP no endereço especificado
  File rFile = SPIFFS.open(path, "r");
  if (rFile) {
    String s = rFile.readStringUntil('\n');
    s.trim();
    Serial.print("Lido: ");
    Serial.println(s);
    rFile.close();
    return s;
  } else {
    Serial.println("Erro ao abrir arquivo!");
    return "";
  }
}

void openFS() { // Função que inicia o protocolo
  if (SPIFFS.begin()) {
    Serial.println("Sistema de arquivos aberto com sucesso!");
  } else {
    Serial.println("Erro ao abrir o sistema de arquivos");
  }
}




// Função para enviar mensagens via CallMeBot
void enviarMensagem(String mensagem) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    mensagem.replace(" ", "+");
    String url = "https://api.callmebot.com/whatsapp.php?phone=" + 
                 String(callMeBotPhone) + "&text=" + 
                 mensagem + "&apikey=" + callMeBotAPIKey;

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Mensagem enviada com sucesso.");
    } else {
      Serial.println("Erro ao enviar mensagem.");
    }
    http.end();
  } else {
    Serial.println("WiFi desconectado. Não foi possível enviar a mensagem.");
  }
}




// Função de callback para mensagens recebidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// Função para reconectar ao MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    String clientId = "ESP32-Estacionamento";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("Conectado ao MQTT");
      client.publish("deborahmacedo/feeds/Estacionamento", "Iniciando comunicação");
    } else {
      Serial.print("Falha na conexão, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

// Função para calcular o preço com base no tempo em segundos
int calcularPreco(int tempoSegundos) {
  int preco = 0;

  // Convertendo o tempo em horas e minutos
  int horas = tempoSegundos / 3600; // 1 hora = 3600 segundos
  int minutos = (tempoSegundos % 3600) / 60; // Resto dos segundos dividido por 60 para minutos
  int segundos = tempoSegundos % 60; // Resto dos segundos após dividir por 60

  // Mostra o tempo em horas, minutos e segundos
  Serial.print("Tempo: ");
  if (horas > 0) {
    Serial.print(horas);
    Serial.print(" hora");
  }
  
  if (minutos > 0 && minutos < 2) {
    Serial.print(minutos);
    Serial.print(" hora");
  } else {
    Serial.print(minutos);
    Serial.print ("horas");
  }
  if (segundos > 0 || (horas == 0 && minutos == 0)) {
    Serial.print(segundos);
    Serial.print(" segundos");
  }
  Serial.println();

  if (tempoSegundos <= 5) {
    preco = 0; // Gratuito até 5 segundos
  } else {
    // Desconta os 5 segundos iniciais antes de calcular o preço
    tempoSegundos -= 5;
  }

  if (tempoSegundos <= 15) {
    preco = 0; // Gratuito até 15 segundos 
  } else if (tempoSegundos > 15 && tempoSegundos <= 60) {
    preco = 10; // R$ 10 entre 1 e 2 minutos
  } else if (tempoSegundos > 60 && tempoSegundos <= 120) {
    preco = 12; // R$ 12 entre 2 e 3 minutos
  } else if (tempoSegundos > 120 && tempoSegundos <= 180) {
    preco = 14; // R$ 14 entre 3 e 4 minutos
  } else {
    preco = 16; // R$ 16 para mais de 4 minutos
  }

  return preco;
}

// Função para medir a distância
int medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  int duration = pulseIn(echoPin, HIGH, 20000);
  if (duration == 0) return 999;
  int cm = duration / 58;
  return cm;
}
  String strganhoTotal;
  String strquantidadeCarros;
void setup() {
  Serial.begin(115200);
  openFS();


  // Configuração dos pinos
  pinMode(PIN_TRIG1, OUTPUT);
  pinMode(PIN_ECHO1, INPUT);
  pinMode(PIN_TRIG2, OUTPUT);
  pinMode(PIN_ECHO2, INPUT);
  pinMode(PIN_TRIG3, OUTPUT);
  pinMode(PIN_ECHO3, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  // Conexão WiFi e MQTT
  setup_wifi();
  client.setServer(mqttserver, mqttport);
  client.setCallback(callback);
  strganhoTotal=readFile("/logsganhoTotal.txt");

  strquantidadeCarros=readFile( "/logsquantidadeCarros.txt");
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Medir distância dos sensores
  int distancia1 = medirDistancia(PIN_TRIG1, PIN_ECHO1);
  int distancia2 = medirDistancia(PIN_TRIG2, PIN_ECHO2);
  int distancia3 = medirDistancia(PIN_TRIG3, PIN_ECHO3);


    

  // Verificar se a distância é menor que 5 cm e acender LEDs LED1
  if (distancia1 < 5) {
    digitalWrite(LED1, HIGH);
    if (tempoLED1 == 0) { // Salva o tempo inicial
      tempoLED1 = millis();
    }
  } else {
    digitalWrite(LED1, LOW);
    if (tempoLED1 != 0) {

      // Carro saiu da vaga 1

      unsigned long tempoPassado1 = millis() - tempoLED1;
      int preco1 = calcularPreco(tempoPassado1 / 1000); // Tempo em segundos
      ganhoTotal += preco1; // Adiciona o valor ao ganho total
      quantidadeCarros++; // Incrementa a quantidade de carros
      client.publish("deborahmacedo/feeds/ganhoTotal", String(ganhoTotal).c_str());
      client.publish("deborahmacedo/feeds/quantidadeCarros", String(quantidadeCarros).c_str());

      Serial.print("Carro na vaga 1 saiu. Tempo total: ");
     
int tempoSegundos1 = tempoPassado1 / 1000;
      int horas1 = tempoSegundos1 / 3600;
      int minutos1 = (tempoSegundos1 % 3600) / 60;
      int segundos1 = tempoSegundos1 % 60;
      
      if (horas1 > 0) {
        Serial.print(horas1);
        Serial.print(" hora");
      } 

      if (minutos1 > 0 && minutos1 < 2) { //representa horas para o protótipo
        Serial.print(minutos1);
        Serial.print(" hora");

      }else {
        Serial.print(horas1);
        Serial.print ("horas");
      }
      if (segundos1 > 0 || (horas1 == 0 && minutos1 == 0)) {
        Serial.print(segundos1);
        Serial.print(" minutos");
      }
      Serial.println();

      Serial.print("Preço a pagar: ");
      Serial.print(preco1);
      Serial.println(" R$");
      String mensagem = "Carro 1 saiu. Ganho Total: " + String(ganhoTotal) + " R$. Quantidade de carros: " + String(quantidadeCarros);
      enviarMensagem(mensagem);
      Serial.println(mensagem);
      tempoLED1 = 0; // Reseta o tempo
    }
    //SPPIFS
    
  }



//LED2
  if (distancia2 < 5) {
    digitalWrite(LED2, HIGH);
    if (tempoLED2 == 0) { // Salva o tempo inicial
      tempoLED2 = millis();
    }
  } else {
    digitalWrite(LED2, LOW);
    if (tempoLED2 != 0) {
      // Carro saiu da vaga 2
      unsigned long tempoPassado2 = millis() - tempoLED2;
      int preco2 = calcularPreco(tempoPassado2 / 1000); // Tempo em segundos
      ganhoTotal += preco2; // Adiciona o valor ao ganho total
      quantidadeCarros++; // Incrementa a quantidade de carros
      client.publish("deborahmacedo/feeds/ganhoTotal", String(ganhoTotal).c_str());
      client.publish("deborahmacedo/feeds/quantidadeCarros", String(quantidadeCarros).c_str());

      Serial.print("Carro na vaga 2 saiu. Tempo total: ");

      int tempoSegundos2 = tempoPassado2 / 1000;
      int horas2 = tempoSegundos2 / 3600;
      int minutos2 = (tempoSegundos2 % 3600) / 60;
      int segundos2 = tempoSegundos2 % 60;
      
      if (horas2 > 0) {
        Serial.print(horas2);
        Serial.print(" horas ");
      }

      if (minutos2 > 0 && minutos2 < 2) {
        Serial.print(minutos2);
        Serial.print(" hora");
      } else {
        Serial.print(minutos2);
        Serial.print ("horas");
      }
      if (segundos2 > 0 || (horas2 == 0 && minutos2 == 0)) {
        Serial.print(segundos2);
        Serial.print(" minutos");
      }
      Serial.println();

      Serial.print("Preço a pagar: ");
      Serial.print(preco2);
      Serial.println(" R$");

      String mensagem = "Carro 2 saiu. Ganho Total: " + String(ganhoTotal) + " R$. Quantidade de carros: " + String(quantidadeCarros);
      enviarMensagem(mensagem);
      Serial.println(mensagem);

      tempoLED2 = 0; // Reseta o tempo
    }
  }

//LED3
  if (distancia3 < 5) { 
    digitalWrite(LED3, HIGH);
    if (tempoLED3 == 0) { // Salva o tempo inicial
      tempoLED3 = millis();
    }
  } else {
    digitalWrite(LED3, LOW);
    if (tempoLED3 != 0) {
      // Carro saiu da vaga 3
      unsigned long tempoPassado3 = millis() - tempoLED3;
      int preco3 = calcularPreco(tempoPassado3 / 1000); // Tempo em segundos
      ganhoTotal += preco3; // Adiciona o valor ao ganho total
      quantidadeCarros++; // Incrementa a quantidade de carros
      client.publish("deborahmacedo/feeds/ganhoTotal", String(ganhoTotal).c_str());
      client.publish("deborahmacedo/feeds/quantidadeCarros", String(quantidadeCarros).c_str());

      Serial.print("Carro na vaga 3 saiu. Tempo total: ");

      int tempoSegundos3 = tempoPassado3 / 1000;
      int horas3 = tempoSegundos3 / 3600;
      int minutos3 = (tempoSegundos3 % 3600) / 60;
      int segundos3 = tempoSegundos3 % 60;
      
      if (horas3 > 0) {
        Serial.print(horas3);
        Serial.print(" horas ");
      }
      if (minutos3 > 0 && minutos3 < 2) {
        Serial.print(minutos3);
        Serial.print(" hora ");
      } else  {
        Serial.print(minutos3);
        Serial.print ("horas");

      if (segundos3 > 0 || (horas3 == 0 && minutos3 == 0)) {
        Serial.print(segundos3);
        Serial.print(" minutos");
      }
      Serial.println();

      Serial.print("Preço a pagar: ");
      Serial.print(preco3);
      Serial.println(" R$");
     
     String mensagem = "Carro 3 saiu. Ganho Total: " + String(ganhoTotal) + " R$. Quantidade de carros: " + String(quantidadeCarros);
      enviarMensagem(mensagem);
      Serial.println(mensagem);
      
      tempoLED3 = 0; // Reseta o tempo
    }
  }
  }
   strganhoTotal = String(ganhoTotal);
    writeFile(strganhoTotal,"/logsganhoTotal.txt");

   strquantidadeCarros = String(quantidadeCarros);
    writeFile(strquantidadeCarros, "/logsquantidadeCarros.txt");


  delay(100); // Atraso para evitar leituras muito rápidas
}