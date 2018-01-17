#include <Arduino.h>
#include <ArduinoJson.h>
#include <aJSON.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
//#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>
#include <Ultrasonic.h>

//led and lock pins
#define TRAVA 15
#define LED_C 26
#define LED_O 27

//US pins
#define pino_trigger 0
#define pino_echo 4

//connection parameters
const char *ssid =  "Dermoestetica" ;// change according to your Network - cannot be longer than 32 characters!
const char *pass =  "dermoaju2017se"; // change according to your Network
//const char *httpdestinationauth = "http://192.168.15.59:8081/token";// "http://httpbin.org/post"; // //
const char *httpdestination = "http://www.appis.com.br/pontoapi/api/registro_acessos";

//auxs
unsigned char aux[14];
unsigned char auxf[1];
unsigned char next;

//tag vars
int num_card_saved;
int const num_card_max = 100;
int long_tag;
String saved_cards[num_card_max];
unsigned char card[15];

//stats vars
int tr_dest = 1;
int connected = 0;
int start = 0;
int online, stored;

//US aux vars
long time1, time2;
float sensorUS;
long sensorUSms;

//server side auth and stat parameters
String grant_type = "password";
String UserName = "sala2";
String password = "@Sala2";

String ID_Local_Acesso = "1";
String st = "false";

//init
HardwareSerial antena1(0);
HardwareSerial antena2(1);
Ultrasonic ultrasonic(pino_trigger, pino_echo);

StaticJsonBuffer<1000> b;
JsonObject* payload = &(b.createObject());

void setup() {
  num_card_saved = 0;
  long_tag = 0;

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked

  pinMode(LED_C, OUTPUT); //led indicator for connection
  digitalWrite(LED_C, LOW); //set turned off
  pinMode(LED_O, OUTPUT); //led indicator for connection
  digitalWrite(LED_O, HIGH); //set turned off

  Serial.begin(9600);    // Initialize serial communications
  antena1.begin(9600);
  antena1.flush();
  
  antena2.begin(9600, SERIAL_8N1, 12, 13);
  antena2.flush();

  delay(250);
  Serial.println("Conectando....");

  //connection initialization
  if(WiFi.status() != WL_CONNECTED){
    WiFi.begin(ssid, pass); // Initialize wifi connection
  }

  int retries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (retries < 100)) {
    retries++;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    mensagemConectado();
    connected = 1;
    digitalWrite(LED_C, HIGH);
    digitalWrite(LED_O, LOW);
   
  }
  else{
    Serial.println("Wifi não conectado.");
    connected = 0;  
    digitalWrite(LED_C, LOW);
    digitalWrite(LED_O, HIGH);
    
  } 
  mensagemInicial();
  /*
  //if connection is established, gets ready to ready tag
  if(connected == 1){
    Serial.println(F("======================================================"));
    Serial.println(F("Pronto para ler tag: \n"));
    mensagemInicial(); //Print init message
    delay(1000);
  }
  else{
    delay(1000);
    connected = 0;
  }*/
  //time setup
  time1 = millis();
  time2 = millis();  
}

void loop() {
  //status led setup
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.begin(ssid, pass);
    Serial.println("Não conectado. Conectando...");
    digitalWrite(LED_C, LOW);
    digitalWrite(LED_O, HIGH);
    connected = 0;
  }
  else{
    mensagemConectado();
    digitalWrite(LED_C, HIGH);
    digitalWrite(LED_O, LOW);
    connected = 1;
  }

  antena1.begin(9600);
  antena2.begin(9600, SERIAL_8N1, 12, 13);
  //timing 
  if((start == 0) || ((millis() - time1) >= 5000)){
    delay(200);
    //read a tag with 14 or 15 digits (HEX)
    
    if(antena2.available() !=0){
      Serial.print("available: ");
      Serial.println(antena2.available());
    }
    if(antena2.available() > 0 ){
      start = 1;
      time1 = millis();
      antena2.readBytes(aux, 14);
      delay(200);

      next = antena2.peek();
      if(next == aux[0]){
        long_tag = 0;
      }
      else if(next == 0xFF){
        long_tag = 0;
      }
      else{
        long_tag = 1;
        delay(200);
        antena2.readBytes(auxf, 1);
      }

      //copy the tag to the array card
      for(int i = 0; i < 14; i++){
        card[i] = aux[i];
      }
      if(long_tag == 1){
        card[14] = auxf[0];
        long_tag = 0;
      }
      else{
        card[14] = 0;
      }
      antena2.flush();
      delay(1000);

      int httpCode;
      String rfid;

      for(int i = 0; i<15; i++){
        rfid += String(card[i], HEX);
      }
      Serial.println(rfid);

      //if connected test if the card is registered
      if(connected){
        String message = createMsgUrlEnc(rfid, st);
        //httpCode = sendPOST(httpdestination, header, message, true);
        httpCode = sendPOST(httpdestination, message);
        
        if(httpCode == 201){
          online = 1;
          int already_saved = 0;
          for(int i = 0; i < num_card_saved; i++){
            if(saved_cards[i] == rfid) already_saved = 1;
          }
          if(already_saved == 0){
            saved_cards[num_card_saved] = rfid;
            num_card_saved++;
            Serial.print("Salvando: ");
            Serial.println(rfid);
          }
          Serial.println(num_card_saved);
          if(num_card_saved >= num_card_max) num_card_saved = 0;
        }
        else if(httpCode == 403){
          online = 0;
          mensagemCartaoNaoAut();
          mensagemInicial();
        }
        else{
          online = 0;
          stored = 0;
          for(int i = 0; i < num_card_saved; i++){
            if(saved_cards[i] == rfid){
              Serial.print("Cartao salvo encontrado: ");
              Serial.println(rfid);
              stored = 1;
              break;
            }
          }
        }
      }
      else{
      //if not connected, test if the card is saved locally
        stored = 0;
        for(int i = 0; i < num_card_saved; i++){
          if(saved_cards[i] == rfid){
            stored = 1;
            break;
          }
        }
      }
      Serial.print("connected: ");
      Serial.println(connected);
      Serial.print("online: ");
      Serial.println(online);
 
      //open the door
      if(online || stored){
        online = 0;
        stored = 0;
        digitalWrite(TRAVA, HIGH);
        mensagemEntradaLiberada();
        delay(5000);

        sensorUS = ultrasonic.timing();
        sensorUSms = ultrasonic.convert(sensorUS, Ultrasonic::CM);
        Serial.println(sensorUSms);
        //test if can close the door using data from US sensors
        while(sensorUSms < 100) {
          sensorUS = ultrasonic.timing();
          sensorUSms = ultrasonic.convert(sensorUS, Ultrasonic::CM);
        }
        Serial.println(sensorUSms);
        //lock door
        digitalWrite(TRAVA, LOW);
        mensagemPortaTravada();
        mensagemInicial();
      }
      else{
        mensagemCartaoNaoAut();
        mensagemInicial();
      }
    }
  }

  antena1.flush();
  antena1.end();
  
  antena2.flush();
  antena2.end();
}

//send to server
int sendPOST(String httpdestination, String body){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient

      http.begin(httpdestination);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");  //Specify content-type header


      httpCode = http.POST(body);   //Send the request
      String pl = http.getString();                  //Get the response payload
      char pl2[1000];
      pl.toCharArray(pl2, 1000);


      JsonObject* x;
      StaticJsonBuffer<1000> JSONBuffer;   //Memory pool
      x = &(JSONBuffer.parseObject(pl2)); //Parse message

      payload = x;
      Serial.println(httpCode);   //Print HTTP return code
      Serial.println(pl);    //Print request response payload


      http.end();  //Close connection

    }else{

       Serial.println("Error in WiFi connection");
    }
    return httpCode;
}

String createForm(){
  String form = "grant_type=" + grant_type + "&"
    + "UserName=" + UserName + "&"
    + "password=" + password;
  return form;
}

String createMsgUrlEnc(String rfid, String st){
  String form = "RFID=" + rfid + "&"
    + "Status=" + st + "&"
    +"ID_Local_Acesso=" + ID_Local_Acesso;
  return form;
}

//Serial messages
void mensagemInicial() {
  Serial.println("Aproxime seu cartão");
  delay(1000);
}

void mensagemEntradaLiberada(){
  Serial.println("Entrada liberada.");
  delay(1000);
}

void mensagemPortaTravada(){
  Serial.println("Porta travada.");
  delay(1000);
}

void mensagemAcaoNegada(){
  Serial.println("Ação negada!");
  delay(1000);
}

void mensagemCartaoNaoAut(){
  Serial.println("Cartão não autorizado!");
  delay(1000);
}

void mensagemConectado(){
  Serial.println("Wifi conectado!");
  delay(1000);
}
