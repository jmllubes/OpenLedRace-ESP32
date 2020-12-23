/*
   ____                     _      ______ _____    _____
  / __ \                   | |    |  ____|  __ \  |  __ \
  | |  | |_ __   ___ _ __   | |    | |__  | |  | | | |__) |__ _  ___ ___
  | |  | | '_ \ / _ \ '_ \  | |    |  __| | |  | | |  _  // _` |/ __/ _ \
  | |__| | |_) |  __/ | | | | |____| |____| |__| | | | \ \ (_| | (_|  __/
  \____/| .__/ \___|_| |_| |______|______|_____/  |_|  \_\__,_|\___\___|
        | |
        |_|
  Open LED Race
  An minimalist cars race for LED strip

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.


  by gbarbarov@singulardevices.com  for Arduino day Seville 2019
  https://www.hackster.io/gbarbarov/open-led-race-a0331a
  https://twitter.com/openledrace


  https://gitlab.com/open-led-race
  https://openledrace.net/open-software/
*/


//version Basic for PCB Rome Edition
// 2 Player , without Boxes Track


#include "WiFi.h"
#include <PubSubClient.h>
const char* mqtt_server = "mqtt.eclipseprojects.io";
const char* ssid = "XXX";
const char* password = "XXX";

WiFiClient espClient;
PubSubClient client(espClient);

#include <Adafruit_NeoPixel.h>
#define MAXLED         150 // MAX LEDs actives on strip

#define PIN_LED        2  // R 500 ohms to DI pin for WS2812 and WS2813, for WS2813 BI pin of first LED to GND  ,  CAP 1000 uF to VCC 5v/GND,power supplie 5V 2A  
#define PIN_P1         4   // switch player 1 to PIN and GND
#define PIN_P2         A0   // switch player 2 to PIN and GND 
#define PIN_AUDIO      3   // through CAP 2uf to speaker 8 ohms
const int Player1 = 1;
const int Player2 = 2;
const int A = 0;
const int B = 0;
const int A_ant = 0;
const int B_ant = 0;


#define INI_RAMP 80
#define MED_RAMP 90
#define END_RAMP 100
#define HIGH_RAMP 12
bool ENABLE_RAMP = 0;
bool VIEW_RAMP = 0;

int NPIXELS = MAXLED; // leds on track
int cont_print = 0;

#define COLOR1   Color(255,0,0)
#define COLOR2   Color(0,255,0)

#define COLOR1_tail   Color(i*3,0,0)
#define COLOR2_tail   Color(0,i*3,0)

int win_music[] = {
  2637, 2637, 0, 2637,
  0, 2093, 2637, 0,
  3136
};

byte  gravity_map[MAXLED];

int TBEEP = 0;
int FBEEP = 0;
byte SMOTOR = 0;
float speed1 = 0;
float speed2 = 0;
float dist1 = 0;
float dist2 = 0;

byte loop1 = 0;
byte loop2 = 0;

byte leader = 0;
byte loop_max = 5; //total laps race


float ACEL = 0.2;
float kf = 0.015; //friction constant
float kg = 0.003; //gravity constant

byte flag_sw1 = 0;
byte flag_sw2 = 0;
byte draworder = 0;

unsigned long timestamp = 0;
char txbuff[64];

Adafruit_NeoPixel track = Adafruit_NeoPixel(MAXLED, PIN_LED, NEO_GRB + NEO_KHZ800);

int tdelay = 5;

void set_ramp(byte H, byte a, byte b, byte c)
{ for (int i = 0; i < (b - a); i++) {
    gravity_map[a + i] = 127 - i * ((float)H / (b - a));
  };
  gravity_map[b] = 127;
  for (int i = 0; i < (c - b); i++) {
    gravity_map[b + i + 1] = 127 + H - i * ((float)H / (c - b));
  };
}

void set_loop(byte H, byte a, byte b, byte c)
{ for (int i = 0; i < (b - a); i++) {
    gravity_map[a + i] = 127 - i * ((float)H / (b - a));
  };
  gravity_map[b] = 255;
  for (int i = 0; i < (c - b); i++) {
    gravity_map[b + i + 1] = 127 + H - i * ((float)H / (c - b));
  };
}


void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  for (int i = 0; i < NPIXELS; i++) {
    gravity_map[i] = 127;
  };
  track.begin();
  //pinMode(PIN_P1,INPUT_PULLUP);
  //pinMode(PIN_P2,INPUT_PULLUP);


  if ((digitalRead(PIN_P1) == 0)) //push switch 1 on reset for activate physics
  { ENABLE_RAMP = 1;
    set_ramp(HIGH_RAMP, INI_RAMP, MED_RAMP, END_RAMP);
    for (int i = 0; i < (MED_RAMP - INI_RAMP); i++) {
      track.setPixelColor(INI_RAMP + i, track.Color(24 + i * 4, 0, 24 + i * 4) );
    };
    for (int i = 0; i < (END_RAMP - MED_RAMP); i++) {
      track.setPixelColor(END_RAMP - i, track.Color(24 + i * 4, 0, 24 + i * 4) );
    };
    track.show();
    delay(1000);
    //tone(PIN_AUDIO,500);delay(500);noTone(PIN_AUDIO);delay(500);
    if ((digitalRead(PIN_P1) == 0)) {
      VIEW_RAMP = 1; // if retain push switch 1 set view ramp
    }
    else {
      for (int i = 0; i < NPIXELS; i++) {
        track.setPixelColor(i, track.Color(0, 0, 0));
      };
      track.show();
      VIEW_RAMP = 0;
    };

  };

  //if ((digitalRead(PIN_P2)==0)) {delay(1000); tone(PIN_AUDIO,1000);delay(500);noTone(PIN_AUDIO);delay(500);if ((digitalRead(PIN_P2)==1)) SMOTOR=1;} //push switch 2 until a tone beep on reset for activate magic FX  ;-)

  start_race();
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  /*for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
    }
    Serial.println();*/

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "p1") {

    speed1 += ACEL;

  } if (String(topic) == "p2") {

    speed2 += ACEL;

  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("p1");
      client.subscribe("p2");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void start_race() {
  Serial.println("r4");
  for (int i = 0; i < NPIXELS; i++) {
    track.setPixelColor(i, track.Color(0, 0, 0));
  };
  track.show();
  delay(2000);
  track.setPixelColor(12, track.Color(0, 255, 0));
  track.setPixelColor(11, track.Color(0, 255, 0));
  track.show();
  /*tone(PIN_AUDIO,400);
    delay(2000);
    noTone(PIN_AUDIO);  */
  track.setPixelColor(12, track.Color(0, 0, 0));
  track.setPixelColor(11, track.Color(0, 0, 0));
  track.setPixelColor(10, track.Color(255, 255, 0));
  track.setPixelColor(9, track.Color(255, 255, 0));
  track.show();
  /*tone(PIN_AUDIO,600);
    delay(2000);
    noTone(PIN_AUDIO);   */
  track.setPixelColor(9, track.Color(0, 0, 0));
  track.setPixelColor(10, track.Color(0, 0, 0));
  track.setPixelColor(8, track.Color(255, 0, 0));
  track.setPixelColor(7, track.Color(255, 0, 0));
  track.show();
  /*tone(PIN_AUDIO,1200);
    delay(2000);
    noTone(PIN_AUDIO);   */
  timestamp = 0;
  Serial.println("r5");
};

void winner_fx(byte w) {
  /*int msize = sizeof(win_music) / sizeof(int);
    for (int note = 0; note < msize; note++) {
    if (SMOTOR==1) {tone(PIN_AUDIO, win_music[note]/(3-w),200);} else {tone(PIN_AUDIO, win_music[note],200);};
    delay(230);
    noTone(PIN_AUDIO);}    */
};


int get_relative_position1( void ) {
  enum {
    MIN_RPOS = 0,
    MAX_RPOS = 99,
  };
  int trackdist = 0;
  int pos = 0;
  trackdist = (int)dist1 % NPIXELS;
  pos = map(trackdist, 0, NPIXELS - 1, MIN_RPOS, MAX_RPOS);
  return pos;
}

int get_relative_position2( void ) {
  enum {
    MIN_RPOS = 0,
    MAX_RPOS = 99,
  };
  int trackdist = 0;
  int pos = 0;
  trackdist = (int)dist2 % NPIXELS;
  pos = map(trackdist, 0, NPIXELS - 1, MIN_RPOS, MAX_RPOS);
  return pos;
}

void print_cars_position( void ) {
  int  rpos = get_relative_position1();
  //sprintf( txbuff, "p%d%s%d,%d%c", i + 1, tracksID[cars[i].trackID], cars[i].nlap, rpos, EOL );
  sprintf( txbuff, "p%d%s%d,%d", 1, 1, loop1, rpos );
  Serial.println( txbuff );
  rpos = get_relative_position2();
  sprintf( txbuff, "p%d%s%d,%d", 2, 1, loop2, rpos );
  Serial.println( txbuff );
}




void burning1() {
  //to do
}

void burning2() {
  //to do
}

void track_rain_fx() {
  //to do
}

void track_oil_fx() {
  //to do
}

void track_snow_fx() {
  //to do
}


void fuel_empty() {
  //to do
}

void fill_fuel_fx() {
  //to do
}

void in_track_boxs_fx() {
  //to do
}

void pause_track_boxs_fx() {
  //to do
}

void flag_boxs_stop() {
  //to do
}

void flag_boxs_ready() {
  //to do
}

void draw_safety_car() {
  //to do
}

void telemetry_rx() {
  //to do
}

void telemetry_tx() {
  //to do
}

void telemetry_lap_time_car1() {
  //to do
}

void telemetry_lap_time_car2() {
  //to do
}

void telemetry_record_lap() {
  //to do
}

void telemetry_total_time() {
  //to do
}

int read_sensor(byte player) {
  //to do
}

int calibration_sensor(byte player) {
  //to do
}

int display_lcd_laps() {
  //to do
}

int display_lcd_time() {
  //to do
}

void draw_car1(void) {
  for (int i = 0; i <= loop1; i++) {
    track.setPixelColor(((word)dist1 % NPIXELS) + i, track.COLOR1);
  };
}

void draw_car2(void) {
  for (int i = 0; i <= loop2; i++) {
    track.setPixelColor(((word)dist2 % NPIXELS) + i, track.COLOR2);
  };
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  for (int i = 0; i < NPIXELS; i++) {
    track.setPixelColor(i, track.Color(0, 0, 0));
  };
  if ((ENABLE_RAMP == 1) && ( VIEW_RAMP == 1)) {
    for (int i = 0; i < (MED_RAMP - INI_RAMP); i++) {
      track.setPixelColor(INI_RAMP + i, track.Color(24 + i * 4, 0, 24 + i * 4) );
    };
    for (int i = 0; i < (END_RAMP - MED_RAMP); i++) {
      track.setPixelColor(END_RAMP - i, track.Color(24 + i * 4, 0, 24 + i * 4) );
    };
  };

  /* if ( (flag_sw1 == 1) && (A != A_ant) ) {
     flag_sw1 = 0;
     speed1 += ACEL;
    };
    if ( (flag_sw1 == 0) && (A == A_ant) ) {
     flag_sw1 = 1;
    };*/

  if ((gravity_map[(word)dist1 % NPIXELS]) < 127) speed1 -= kg * (127 - (gravity_map[(word)dist1 % NPIXELS]));
  if ((gravity_map[(word)dist1 % NPIXELS]) > 127) speed1 += kg * ((gravity_map[(word)dist1 % NPIXELS]) - 127);

  speed1 -= speed1 * kf;

  if ( (flag_sw2 == 1) && (B != B_ant) ) {
    flag_sw2 = 0;
    speed2 += ACEL;
  };
  if ( (flag_sw2 == 0) && (B != B_ant) ) {
    flag_sw2 = 1;
  };

  if ((gravity_map[(word)dist2 % NPIXELS]) < 127) speed2 -= kg * (127 - (gravity_map[(word)dist2 % NPIXELS]));
  if ((gravity_map[(word)dist2 % NPIXELS]) > 127) speed2 += kg * ((gravity_map[(word)dist2 % NPIXELS]) - 127);

  speed2 -= speed2 * kf;

  dist1 += speed1;
  dist2 += speed2;

  if (dist1 > dist2) {
    if (leader == 2) {
      FBEEP = 440;
      TBEEP = 10;
    }
    leader = 1;
  }
  if (dist2 > dist1) {
    if (leader == 1) {
      FBEEP = 440 * 2;
      TBEEP = 10;
    }
    leader = 2;
  };



  if (dist1 > NPIXELS * loop1) {
    loop1++;
    TBEEP = 10;
    FBEEP = 440;
  };
  if (dist2 > NPIXELS * loop2) {
    loop2++;
    TBEEP = 10;
    FBEEP = 440 * 2;
  };

  if (loop1 > loop_max) {
    Serial.println("w1");
    for (int i = 0; i < NPIXELS / 10; i++) {
      track.setPixelColor(i, track.COLOR1_tail);
    }; track.show();
    winner_fx(1); loop1 = 0; loop2 = 0; dist1 = 0; dist2 = 0; speed1 = 0; speed2 = 0; timestamp = 0;
    start_race();
  }
  if (loop2 > loop_max) {
    Serial.println("w2");
    for (int i = 0; i < NPIXELS / 10; i++) {
      track.setPixelColor(i, track.COLOR2_tail);
    }; track.show();
    winner_fx(2); loop1 = 0; loop2 = 0; dist1 = 0; dist2 = 0; speed1 = 0; speed2 = 0; timestamp = 0;
    start_race();
  }

  if ((millis() & 512) == (512 * draworder)) {
    if (draworder == 0) {
      draworder = 1;
    }
    else {
      draworder = 0;
    }
  };
  if (abs(round(speed1 * 100)) > abs(round(speed2 * 100))) {
    draworder = 1;
  };
  if (abs(round(speed2 * 100)) > abs(round(speed1 * 100))) {
    draworder = 0;
  };

  if ( draworder == 0 ) {
    draw_car1();
    draw_car2();
  }
  else {
    draw_car2();
    draw_car1();
  }

  track.show();
  /*if (SMOTOR==1) tone(PIN_AUDIO,FBEEP+int(speed1*440*2)+int(speed2*440*3));
    delay(tdelay);
    if (TBEEP>0) {TBEEP--;} else {FBEEP=0;};
    cont_print++; */
  if (cont_print > 100) {
    print_cars_position();
    cont_print = 0;
  }

}
