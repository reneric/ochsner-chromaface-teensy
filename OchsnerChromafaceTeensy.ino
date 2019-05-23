#define FASTLED_ALLOW_INTERRUPTS 0
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <FastLED.h>

FASTLED_USING_NAMESPACE

// #define LOCAL true

#define DATA_PIN    5
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

#define NUM_LEDS 300
#define BRIGHTNESS 180
#define FRAMES_PER_SECOND  96

// Upload at 24 MHz for stability

CRGB leds[NUM_LEDS];

#define NUM_STATIONS 1

// Update these with values suitable for the hardware/network.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#ifdef LOCAL
  IPAddress ip(192, 168, 1, 82);
#else
  IPAddress ip(192, 168, 2, 101);
#endif

void stateMachine (int state);

// The Client ID for connecting to the MQTT Broker
const char* CLIENT_ID = "LED_O";

// The Topic for mqtt messaging
const char* TOPIC = "command/LED_O";

// States
const int ON_STATE = 1;
const int OFF_STATE = 0;

// Initialize the current state ON or OFF
int CF_State = ON_STATE;
int tempState;

// Initialize the ethernet library
EthernetClient net;
// Initialize the MQTT library
PubSubClient mqttClient(net);

#ifdef LOCAL
  const char* mqttServer = "192.168.1.97";
#else
  const char* mqttServer = "192.168.2.10";
#endif

// Station states, used as MQTT Messages
const char states[2][10] = {"stop", "start"};

/*********** RECONNECT WITHOUT BLOCKING LOOP ************/
long lastReconnectAttempt = 0;
boolean reconnect_non_blocking() {
  if (mqttClient.connect(CLIENT_ID)) {
    Serial.println("CONNECTED");
    // Once connected, publish an announcement...
    mqttClient.publish(CLIENT_ID, "CONNECTED", true);

    mqttClient.subscribe(TOPIC);
    mqttClient.subscribe("command/LED_O/next");
    
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
  }
  return mqttClient.connected();
}

// List of patterns to cycle through.
typedef void (*EffectsList[])();
EffectsList effects = { blue_pulse, blue_pulse_split, blue_pulse_white, blue_pulse_split_white, rainbow, confetti, sinelon, juggle, bpm};

uint8_t currentEffect = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void nextPattern();

/**** CALLBACK RUN WHEN AN MQTT MESSAGE IS RECEIVED *****/
void messageReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);  
  Serial.println("] ");
  char payloadArr[length+1];
  
  for (unsigned int i=0;i<length;i++)
  {
    payloadArr[i] = (char)payload[i];
  }
  payloadArr[length] = 0;

  Serial.println(payloadArr);  // null terminated array
  if (strcmp(topic, "command/LED_O/next") == 0) {
    Serial.println("nextPattern");
    nextPattern();
  }
  else {  
    if (strcmp(payloadArr, "stop") == 0) CF_State = OFF_STATE;
    if (strcmp(payloadArr, "start") == 0) CF_State = ON_STATE;
    
    Serial.println(CF_State);
    // stateMachine(CF_State);
  }
}

void setup() {
  // Initialize serial communication:
  Serial.begin(9600);
  
  Ethernet.init(10);
  
  // Initialize the ethernet connection
  Ethernet.begin(mac, ip);
  // Ethernet.begin(mac, ip, myDns); // Connect with DNS 

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(messageReceived);
  
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setDither(0);
  // Set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  lastReconnectAttempt = 0;
}

void loop() {
  if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 1000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect_non_blocking()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }

  if (CF_State == OFF_STATE) {
   Serial.println("OFF");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.clear();
  } else {

    // fill_solid(leds, NUM_LEDS, CRGB::Red);
    // // Call the current pattern function once, updating the 'leds' array
    effects[currentEffect]();
    
  }
  FastLED.show();
  FastLED.delay(1000/FRAMES_PER_SECOND);
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  currentEffect = (currentEffect + 1) % ARRAY_SIZE(effects);
  Serial.print("CURRENT EFFECT: ");
  Serial.println(currentEffect);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

int speed = 3;
void blue_pulse() 
{
  static uint8_t startIndex = 0;
  startIndex = startIndex + 4; /* motion speed */

  FillLEDsFromPaletteColors(startIndex, CRGB::Blue, CRGB::DarkBlue);
}

void blue_pulse_split() 
{
  static uint8_t startIndex = 0;
  startIndex = startIndex + 4; /* motion speed */
  
  FillLEDsFromPaletteColorsSplit(startIndex, CRGB::Blue, CRGB::DarkBlue);
}

void blue_pulse_white() 
{
  static uint8_t startIndex = 0;
  startIndex = startIndex + 4; /* motion speed */
  
  FillLEDsFromPaletteColors(startIndex, CRGB::RoyalBlue, CRGB::LightSkyBlue);
}

void blue_pulse_split_white() 
{
  static uint8_t startIndex = 0;
  startIndex = startIndex + 4; /* motion speed */
  
  FillLEDsFromPaletteColorsSplit(startIndex, CRGB::RoyalBlue, CRGB::LightSkyBlue);
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
long start = random(NUM_LEDS);
void FillLEDsFromPaletteColors( uint8_t colorIndex, CRGB blue, CRGB darkBlue)
{
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(SetupActivePalette(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex += speed;
    }
}

void FillLEDsFromPaletteColorsSplit( uint8_t colorIndex, CRGB blue, CRGB darkBlue)
{
    for( int i = 0; i < 52; i++) {
        leds[i] = ColorFromPalette(SetupActivePalette(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex += speed;
    }
    for(int i = 0; i < 53; i++) {
        leds[i+51] = ColorFromPalette(SetupActivePalette(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex -= speed;
    }
}

void FillLEDsFromPaletteColorsWhite( uint8_t colorIndex, CRGB blue, CRGB darkBlue)
{
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(SetupActivePalette(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex += speed;
    }
}

void FillLEDsFromPaletteColorsSplitWhite( uint8_t colorIndex, CRGB blue, CRGB darkBlue)
{
    for( int i = 0; i < 52; i++) {
        leds[i] = ColorFromPalette(SetupActivePalette2(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex += speed;
    }
    for(int i = 0; i < 53; i++) {
        leds[i+51] = ColorFromPalette(SetupActivePalette2(blue, darkBlue), colorIndex, BRIGHTNESS, LINEARBLEND);
        colorIndex -= speed;
    }
}

CRGBPalette16 SetupActivePalette(CRGB blue, CRGB darkBlue)
{
    CRGB black  = darkBlue;
    
    return CRGBPalette16(
                                   blue,  blue,  black,  black,
                                   black,  blue,  blue,  blue,
                                   blue,  blue,  black,  black,
                                   darkBlue, darkBlue, black, black);
}

CRGBPalette16 SetupActivePalette2(CRGB blue, CRGB darkBlue)
{
  CRGB black  = darkBlue;
    
    return CRGBPalette16(
                                   blue,  blue,  black,  black,
                                   blue,  blue,  blue,  blue,
                                   blue,  blue,  black,  black,
                                   darkBlue, darkBlue, black, black);
}
// CRGBPalette16 SetupActivePalette()
// {
  
//     CRGB blue = CHSV( HUE_BLUE, 255, 255);
//     CRGB darkBlue  = CRGB::DarkBlue;
//     CRGB black  = CRGB::DarkBlue;
    
//     return CRGBPalette16(
//                                    blue,  blue,  black,  black,
//                                    darkBlue, darkBlue, black,  black,
//                                    blue,  blue,  black,  black,
//                                    darkBlue, darkBlue, black,  black );
// }
