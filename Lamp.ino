#define BLYNK_PRINT Serial
#include <SPI.h>
#include <BlynkSimpleStream.h>
char auth[] = "0gZeXizHO017UZLQ9hRsbgC8qHl8l4EV";


#include <VS1053.h>

#include <WiFi.h>
#define VS1053_CS     22
#define VS1053_DCS    21
#define VS1053_DREQ   17

//#include <asyncHTTPrequest.h>
//asyncHTTPrequest request;

// #include <ArduinoJson.h>

//DynamicJsonDocument doc(30000);

// Encoder
#define CLK 25
#define DT 27
#define SW 26

#include <ESP32Encoder.h>
ESP32Encoder encoder;
unsigned long encoder2lastToggled;

#include <AceButton.h>
using namespace ace_button;
AceButton button(SW);
void handleButton(AceButton*, uint8_t, uint8_t);

// Default volume
unsigned int volume = 80;

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;
WiFiClient clientBlynk;

// WiFi settings example, substitute your own
const char *ssid = "my_wifi";
const char *password = "kiselev1";

//  http://icecast.omroep.nl/radio5-bb-mp3
// http://ambiance-staging.nikitakiselev.ru/storage/music/zonsonderganglite.mp3
const char *host = "ambiance.nikitakiselev.ru";
const char *path = "/storage/music/zonsonderganglite.mp3";
int httpPort = 80;

// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
uint8_t mp3buff[200];

//char* ambiances[50];
//int ambiancesCount = 0;
char *tracks[] = {
  "/storage/music/lebensessenz-farewell-letter.mp3",
  "/storage/music/lebensessenz-ber-die-brcke-der-trume.mp3",
  "/storage/music/lebensessenz-franziskas-brief.mp3",
  "/storage/music/lebensessenz-learning-with-mistakes.mp3",
  "/storage/music/lebensessenz-tu-deorum-hominumque-tyranne-amor.mp3",
  "/storage/music/lebensessenz-a-nocturnal-dolorsalgia.mp3",
  "/storage/music/lebensessenz-farewell-letter-ii.mp3",
  "/storage/music/lebensessenz-visionen-und-erinnerungen-aus-wahlheim.mp3",
  "/storage/music/lebensessenz-der-anfang-unsere-geschichte.mp3",
  "/storage/music/lebensessenz-leb-wohl-debora.mp3"
  
//  "/storage/music/sowulo-yule.mp3",
//  "/storage/music/dzivia-vataha.mp3",
//  "/storage/music/dzivia-voryva.mp3",
//  "/storage/music/ursprung-gebo.mp3",
//  "/storage/music/sowulo-f230cele.mp3",
//  "/storage/music/sowulo-mabon.mp3",
//  "/storage/music/sowulo-noodlot-proloog.mp3",
//  "/storage/music/sowulo-brego-in-breoste.mp3",
//  "/storage/music/ivar-bj248rnson-einar-selvik-um-heilage-fjell.mp3",
//  "/storage/music/ursprung-hamingja.mp3",
//  "/storage/music/dzivia-dzikaje-palava324nie.mp3",
};
byte totalTracks = 11;
byte trackIndex = 0;
bool isPlaying = false;
unsigned long totalBytes = 0;
bool playNextPressed = false;

#define BTN_NONE 0
#define BTN_PLAY 1
#define BTN_STOP 2
#define BTN_NEXT 3
#define BTN_PREV 4
byte buttonPressed = BTN_NONE;

BLYNK_WRITE(V3)
{
  int newVolume = param.asInt();

  Serial.print("New Volume: ");
  Serial.println(newVolume);

  volume = newVolume;
  encoder.setCount(newVolume);
  player.setVolume(newVolume);
}

BLYNK_WRITE(V2)
{
  String action = param.asStr();

  if (action == "play") {
    isPlaying = true;
        totalBytes = 0;
    Blynk.setProperty(V2, "label", tracks[trackIndex]);
  }
  if (action == "stop") {
    isPlaying = false;
        client.stop();
    Blynk.setProperty(V2, "label", "Stopped");
  }
  if (action == "next") {
    client.stop();
        totalBytes = 0;
        if (trackIndex + 1 > (totalTracks - 1)) {
          trackIndex = 0;
        } else {
          trackIndex++;
        }
     Blynk.setProperty(V2, "label", tracks[trackIndex]);
  }
  if (action == "prev") {
    client.stop();
        totalBytes = 0;
        if (trackIndex - 1 < 0) {
          trackIndex = 0;
        } else {
          trackIndex--;
        }
    Blynk.setProperty(V2, "label", tracks[trackIndex]);
  }

  
  Serial.print(action);
  Serial.println();
}

// This function tries to connect to your WiFi network
void connectWiFi()
{
  Serial.print("Connecting to ");
  Serial.println(ssid);

  if (password && strlen(password)) {
    WiFi.begin((char*)ssid, (char*)password);
  } else {
    WiFi.begin((char*)ssid);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

// This function tries to connect to the cloud using TCP
bool connectBlynk()
{
  clientBlynk.stop();
  return clientBlynk.connect(BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT);
}

/*
void sendRequest(){
    if(request.readyState() == 0 || request.readyState() == 4){
        request.open("GET", "http://ambiance.nikitakiselev.ru/api/playlist");
        request.send();
    }
}

void requestCB(void* optParm, asyncHTTPrequest* request, int readyState){
    if(readyState == 4){
      DeserializationError error = deserializeJson(doc, request->responseText());
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      ambiancesCount = doc.size();

      int i;
      for (i = 0; i < ambiancesCount; i++) {
        ambiances[i] = doc[i]["title"].as<char*>();
      }

      
        request->setDebug(false);
    }
}
*/

void setup() {
  pinMode(SW, INPUT_PULLUP);
  button.setEventHandler(handleButton);
  
  encoder.attachHalfQuad(DT, CLK);
  encoder.setCount(volume);

  Serial.begin(115200);

  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly
  delay(3000);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  Serial.println("\n\nLamp");

  SPI.begin();

  player.begin();
  player.switchToMp3Mode();
  player.setVolume(volume);


  connectWiFi();
  connectBlynk();

  Blynk.begin(clientBlynk, auth);

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


//  request.setDebug(false);
//  request.onReadyStateChange(requestCB);
//  sendRequest();
}

void loop() {
  
  button.check();
  if (volume != encoder.getCount()) {

    if (encoder.getCount() > 100) {
      encoder.setCount(100);  
    }

    if (encoder.getCount() < 0) {
      encoder.setCount(0);  
    }
    
    volume = encoder.getCount();
    player.setVolume(volume);
    Serial.print("New Volume: ");
    Serial.println(volume);

    Blynk.virtualWrite(V3, volume);
  }


  if (isPlaying) {
    
    if (! client.connected()) {
    Serial.print("Total Bytes Read: ");
    Serial.println(totalBytes);

    if (totalBytes > 0) {
      totalBytes = 0;
      if (trackIndex + 1 > (totalTracks - 1)) {
        trackIndex = 0;
        client.stop();
      } else {
        trackIndex++;
      }

      Serial.print("Play next index: ");
      Serial.println(trackIndex);
    }
    

    Serial.print("Connecting to ambiance: ");
    Serial.println(tracks[trackIndex]);

    if (client.connect(host, httpPort)) {
      client.print(String("GET ") + tracks[trackIndex] + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Connection: close\r\n\r\n");
    }

    totalBytes = 0;
  } else {
    if (client.available() > 0) {

      // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
      uint8_t bytesread = client.read(mp3buff, 200);

      // delay(100);

      totalBytes += bytesread;
      player.playChunk(mp3buff, bytesread);
    }
  }
  }

  Blynk.run();
}

void handleButton(AceButton* /*button*/, uint8_t eventType, uint8_t /*buttonState*/) {
  switch (eventType) {
    case AceButton::kEventPressed:
      // digitalWrite(LED_BUILTIN, LED_ON);
      break;
    case AceButton::kEventReleased:
      
      Serial.println("Button Released");
      playNextPressed = true;
      break;
  }
}
