#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

#define DATA_PIN    9
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    106
#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

CRGB leds[NUM_LEDS];

#define NUM_STATIONS 1

// Update these with values suitable for the hardware/network.
// byte mac[] = { 0xB3, 0x5C, 0xED, 0xF8, 0x15, 0xD6 };
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 97);
IPAddress myDns(192, 168, 0, 1);

void stateMachine (int state);

// The Client ID for connecting to the MQTT Broker
const char* CLIENT_ID = "oBarClient";

// The Topic for mqtt messaging
const char* TOPIC = "oBar";

// States
const int ON_STATE = 1;
const int OFF_STATE = 0;

// Initialize the current state ON or OFF
int currentState = ON_STATE;
int tempState;

// Initialize the ethernet library
EthernetClient net;
// Initialize the MQTT library
PubSubClient mqttClient(net);

const char* mqttServer = "192.168.1.100";

// Station states, used as MQTT Messages
const char states[2][10] = {'OFF', 'ON'};

// Reconnect to the MQTT broker when the connection is lost
void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect with the client ID
    if (mqttClient.connect(CLIENT_ID)) {
        Serial.println("Connected!");
        // Once connected, publish an announcement...
        mqttClient.publish(CLIENT_ID, "CONNECTED");

        // Subscribe to topic
        mqttClient.subscribe("oBar");

    } else {
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
    }
  }
}
unsigned long PauseTime;
void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  char myNewArray[length+1];
  for (int i=0;i<length;i++)
  {
    myNewArray[i] = (char)payload[i];
  }
  myNewArray[length] = NULL;

  Serial.println(myNewArray);  // null terminated array
  const char *on = "ON";
//  const char off = 'OFF';
  const char *p = reinterpret_cast<const char*>(myNewArray);
  
  if (p == states[1]) tempState = ON_STATE;
  if (p == states[0]) tempState = OFF_STATE;
  // Serial.println(states[0]);
  // Serial.println(states[1]);
   Serial.println(p);
   Serial.println(tempState);
  stateMachine(tempState);
}

// List of patterns to cycle through.
typedef void (*EffectsList[])();
EffectsList effects = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

uint8_t currentEffect = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void setup() {
  // Initialize serial communication:
  Serial.begin(9600);
  
  Ethernet.init(10);
  
  // Initialize the ethernet connection
  // Ethernet.begin(mac, ip);
  Ethernet.begin(mac, ip, myDns); // Connect with DNS 

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(messageReceived);
  
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // Set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  if (currentState == OFF_STATE) {
//    Serial.println("OFF");
    FastLED.clear();
  } else {
  
    // Call the current pattern function once, updating the 'leds' array
    effects[currentEffect]();
  
    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/FRAMES_PER_SECOND); 
  
    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  }
}

void stateMachine (int state) {
  // Get the current sensor state
  
  /*
   * Only send the state update on the first loop.
   *
   * IF the current (temporary) PS sensor state is not equal to the actual current broadcasted state,
   * THEN we can safely change the actual state and broadcast it.
   *
   */
  if (state != currentState) {
#if DEBUG == 1    
    Serial.print("State Changed: ");
    Serial.println();
    Serial.print("Last State: ");
    Serial.println(currentState);
    Serial.print("New State: ");
    Serial.println(state);
#endif
    currentState = state;
    // Publish the message for this station. i.e. client.publish("PS1", "ACTIVE")
    mqttClient.publish(TOPIC, states[currentState]);
  }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  currentEffect = (currentEffect + 1) % ARRAY_SIZE(effects);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}
