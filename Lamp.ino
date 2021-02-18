#include <VS1053.h>

#include <WiFi.h>
#define VS1053_CS     22
#define VS1053_DCS    21
#define VS1053_DREQ   17

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

char *tracks[] = {
  "/storage/music/sowulo-yule.mp3",
  "/storage/music/dzivia-vataha.mp3",
  "/storage/music/dzivia-voryva.mp3",
  "/storage/music/ursprung-gebo.mp3",
  "/storage/music/sowulo-f230cele.mp3",
  "/storage/music/sowulo-mabon.mp3",
  "/storage/music/sowulo-noodlot-proloog.mp3",
  "/storage/music/sowulo-brego-in-breoste.mp3",
  "/storage/music/ivar-bj248rnson-einar-selvik-um-heilage-fjell.mp3",
  "/storage/music/ursprung-hamingja.mp3",
  "/storage/music/dzivia-dzikaje-palava324nie.mp3",
};
byte totalTracks = 11;
byte trackIndex = 0;
bool isPlaying = false;
unsigned long totalBytes = 0;
bool playNextPressed = false;

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

  Serial.println("\n\nSimple Radio Node WiFi Radio");

  SPI.begin();

  player.begin();
  player.switchToMp3Mode();
  player.setVolume(volume);

  Serial.print("Connecting to SSID ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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
  }

  if (playNextPressed) {
    playNextPressed = false;
    client.stop();
    totalBytes = 0;

    if (trackIndex + 1 > (totalTracks - 1)) {
        trackIndex = 0;
      } else {
        trackIndex++;
      }

    Serial.println("Play next clicked");
  }

  if (! client.connected()) {
      Serial.print("Total Bytes Read: ");
    Serial.println(totalBytes);

    if (totalBytes > 0) {
      totalBytes = 0;
      if (trackIndex + 1 > (totalTracks - 1)) {
        trackIndex = 0;
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
