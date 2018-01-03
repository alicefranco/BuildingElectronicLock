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
#define TRAVA 13
#define LED_C 0
#define LED_O 2

//US pins
#define pino_trigger 5
#define pino_echo 4

//connection parameters
const char *ssid =  "Dermoestetica" ;// change according to your Network - cannot be longer than 32 characters!
const char *pass =  "dermoaju2017se"; // change according to your Network
const char *httpdestinationauth = "http://192.168.15.59:8081/token";// "http://httpbin.org/post"; // //
const char *httpdestination = "http://192.168.15.59:8081/api/cartoes_RFID/verifyrfid";


//auxs
unsigned char aux[14];
unsigned char auxf[1];
unsigned char next;

//tag vars
int num_card;
int const num_card_t = 2;
int long_tag;
String saved_cards[num_card_t];
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
//SoftwareSerial serialArtificial(15, 16, false, 256); //1º TX do leitor, 2º RX do leitor
HardwareSerial serialArtificial(0);
Ultrasonic ultrasonic(pino_trigger, pino_echo);

StaticJsonBuffer<1000> b;
JsonObject* payload = &(b.createObject());

void setup() {
  num_card = 0;
  long_tag = 0;

  pinMode(TRAVA, OUTPUT); //Initiate lock
  digitalWrite(TRAVA, LOW); //set locked( by default
  tr_dest = 1; //door locked

  pinMode(LED_C, OUTPUT); //led indicator for connection
  digitalWrite(LED_C, LOW); //set turned off
  pinMode(LED_O, OUTPUT); //led indicator for connection
  digitalWrite(LED_O, HIGH); //set turned off

  Serial.begin(9600);    // Initialize serial communications
  serialArtificial.begin(9600);
  serialArtificial.flush();

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
    Serial.println("Wifi conectado.");
    delay(1000);
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
  delay(1000);
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
  Serial.println("AQQQuiiiiii");
  mensagemInicial();
  //time setup
  time1 = millis();
  time2 = millis();
}

void loop() {
  //status led setup
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_C, HIGH);
    digitalWrite(LED_O, LOW);
    connected = 1;
  }
  else{ 
    digitalWrite(LED_C, LOW);
    digitalWrite(LED_O, HIGH);
    connected = 0;
  }

  
  serialArtificial.begin(9600);
  //timing 
  if((start == 0) || ((millis() - time1) >= 5000)){
    delay(1000);
    //read a tag with 14 or 15 digits (HEX)
    
    if(serialArtificial.available() !=0){
      Serial.print("available: ");
      Serial.println(serialArtificial.available());
    }
    if(serialArtificial.available() > 0 ){
      start = 1;
      time1 = millis();
      serialArtificial.readBytes(aux, 14);
      delay(250);

      next = serialArtificial.peek();
      if(next == aux[0]){
        long_tag = 0;
      }
      else if(next == 0xFF){
        long_tag = 0;
      }
      else{
        long_tag = 1;
        delay(250);
        serialArtificial.readBytes(auxf, 1);
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
      serialArtificial.flush();
      delay(1000);

      int httpCode;
      String rfid;

      for(int i = 0; i<15; i++){
        rfid += String(card[i], HEX);
      }
      Serial.println(rfid);

      if(connected){
        //check if the tag is registered
        String messageAuth = createForm();
        httpCode = sendPOST(httpdestinationauth, "", messageAuth, false);
        if(httpCode == 200){
          String message = createMsgUrlEnc(rfid, st);
          String access_token = (*payload)["access_token"];
          String token_type = (*payload)["token_type"];
          String header = token_type + " " + access_token;
  
          httpCode = sendPOST(httpdestination, header, message, true);
          if(httpCode == 200){
            online = 1;
            saved_cards[num_card] = rfid;
            num_card++;
            if(num_card >= num_card_t) num_card = 0;
          }
          else if(httpCode == 403){
            online = 0;
            mensagemCartaoNaoAut();
            mensagemInicial();
          }
          else{
            online = 0;
            mensagemAcaoNegada();
            mensagemInicial();
          }
        }
      }
      else{
        stored = 0;
        for(int i = 0; i < num_card; i++){
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
  serialArtificial.flush();
  serialArtificial.end();
}


//send to server
int sendPOST(String httpdestination, String header, String body, bool auth){
  int httpCode;
  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status

      HTTPClient http;    //Declare object of class HTTPClient

      http.begin(httpdestination);
      if(auth) http.addHeader("Authorization", header);
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
