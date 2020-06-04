#include <Arduino.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <set>
#include <map>
#include <iterator>

#define MICPIN 34
#define LEDPIN 23
#define NUM_LEDS 50

const char* ssid = "Guns N' Roaches";
const char* pass = "cockroachKing";

AsyncWebServer server(80);

CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette = PartyColors_p;
CRGBPalette16 targetPalette;
TBlendType    currentBlending = LINEARBLEND;

// Convert to Enums
std::set<String> effects = {"rainbowMarch", "off", "slowColorFade", "solidColor", "audioSensitive"};
std::map<String, String> effectParams;

struct Mood{
  float iVal;
  float dVal;
  Mood(float x, float y) : iVal(x), dVal(y) {}
};

Mood* sleepy_jazz = new Mood (6, 0.13);
Mood* soul = new Mood(7, 0.3);
Mood* balanced = new Mood(8, 0.4);
Mood* rock = new Mood(9, 0.5);
Mood* crazy = new Mood(11, 0.65);

String currentEffect = "solidColor";

// Convert to Enums
String musicMood = "balanced";

bool isOff = true;
uint8_t hue = 0;
uint8_t rainbowDelay = 10;

int randNumber;
int counter = 0;
int loopCounter = 0;
int samples = 200;
int audioHistoryBuffer = 0.65;
float decreaseFactor = 1;
float increaseFactor = 1;
float outputValue=0;
float rememberOutputValue=0;



void rainbow_march(uint8_t thisdelay, uint8_t deltahue);
void slowColorFade();
void solidColor();
void audioSensitive();
void changePalette(String palette);
void changeEffect(String effect);
void effectFactory();

bool keyExists(String key){
  if (effectParams.find(key) != effectParams.end())
    return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to Wifi....");
  }
  Serial.println(WiFi.localIP());

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Request recieved");
    int paramsNr = request->params();
    for(int i=0;i<paramsNr;i++){
        AsyncWebParameter* p = request->getParam(i);
        effectParams[p->name()] = p->value();
    }
    if(request->hasParam("brightness")){
      AsyncWebParameter* p = request->getParam("brightness");
      int val = p->value().toInt();
      if (val < 0) val = 0;
      else if (val > 255) val = 255;
      FastLED.setBrightness(val);
    }
    if(request->hasParam("effect")){
      AsyncWebParameter* p = request->getParam("effect");
      String val = p->value();
      Serial.println(val);
      changeEffect(val);
    }
    if(request->hasParam("palette")){
      AsyncWebParameter* p = request->getParam("palette");
      String val = p->value();
      Serial.println(val);
      changePalette(val);
    }
    request->send(200, "text/plain", "Updated");

  });
  server.on("/getParams", HTTP_GET, [](AsyncWebServerRequest *request){
    String out = "{\n";
    std::map<String, String>::iterator it = effectParams.begin();
    while(it != effectParams.end()){
      out += "\t";
      out += it->first;
      out += " : ";
      out += it->second;
      out += "\n";
      it++;
    }
    out += "}";
    request->send(200, "text/plain", out);
  });

  server.on("/setMood", HTTP_GET, [](AsyncWebServerRequest *request){
    String mood = request->getParam("mood")->value();
    if (mood == "jazz"){
      effectParams["iVal"] = sleepy_jazz->iVal;
      effectParams["dVal"] = sleepy_jazz->dVal;
    } else if (mood == "soul") {
      effectParams["iVal"] = soul->iVal;
      effectParams["dVal"] = soul->dVal;
    } else if (mood == "rock") {
      effectParams["iVal"] = rock->iVal;
      effectParams["dVal"] = rock->dVal;
    } else if (mood == "crazy") {
      effectParams["iVal"] = crazy->iVal;
      effectParams["dVal"] = crazy->dVal;
    } else {
      effectParams["iVal"] = balanced->iVal;
      effectParams["dVal"] = balanced->dVal;
    }
    request->send(200, "text/plain", "Mood changed to " + mood);
  });

  server.begin();
  delay( 3000 );
  effectParams["r"] = 0.0;
  effectParams["g"] = 0.0;
  effectParams["b"] = 255.0;
  FastLED.addLeds<WS2811, LEDPIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.show();
}
void loop () {
  effectFactory();
  FastLED.show();
} // loop()

void changeEffect(String effect){
  hue = 0;
  if(effectParams.find("rainbowDelay") != effectParams.end())
    rainbowDelay = effectParams["rainbowDelay"].toInt();
  if(effectParams.find("audioHistoryBuffer") != effectParams.end())
    audioHistoryBuffer = effectParams["audioHistoryBuffer"].toFloat();
  if(effects.find(effect) != effects.end()){
    Serial.println(effect);
    currentEffect = effect;
    return;
  }
}

void changePalette(String palette){
  // Convert palette to enums
  if(palette == "cloud")
    currentPalette = CloudColors_p;
  else if(palette == "lava")
    currentPalette = LavaColors_p;
  else if(palette == "ocean")
    currentPalette = OceanColors_p;
  else if(palette == "forest")
    currentPalette = ForestColors_p;
  else if(palette == "rainbow")
    currentPalette = RainbowColors_p;
  else if(palette == "party")
    currentPalette = PartyColors_p;
  else if(palette == "heat")
    currentPalette = HeatColors_p;
}

void effectFactory(){
  if (currentEffect == "off"){
    if(!isOff){
      FastLED.clear();
      isOff = true;
    }
    return;
  }
  else if(currentEffect == "rainbowMarch"){
    rainbow_march(200, 6);
  }
  else if(currentEffect == "slowColorFade"){
    slowColorFade();
  }
  else if(currentEffect == "solidColor"){
    solidColor();
  }
  else if(currentEffect == "audioSensitive"){
    audioSensitive();
  }
  // FIX BUG: If off and unknown effect sent
  isOff = false;
}

void rainbow_march(uint8_t thisdelay, uint8_t deltahue) {
  uint8_t thishue = millis()*(255-rainbowDelay)/255;
  fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
}

void slowColorFade(){
  EVERY_N_SECONDS(1) {
    hue++;
  }
  if (hue > 255)
    hue = 0;
  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
}

void solidColor(){
  uint8_t r = effectParams["r"].toInt();
  uint8_t g = effectParams["g"].toInt();
  uint8_t b = effectParams["b"].toInt();
  for(int i =0; i < NUM_LEDS; i++){
    leds[i] = CRGB(r, g, b);
  }
}

void audioSensitive(){
  int sensorValue;
  if (keyExists("musicMood")) musicMood = effectParams["musicMood"];
  else musicMood = "balanced";
  counter = 0;
  for (int i=0; i < samples; i++)
  {
    sensorValue = analogRead(MICPIN);
    if(sensorValue > 2600) counter++;
  }

  if(map(counter, 0, samples-100, 0, 49) > outputValue)outputValue = map(counter, 0, samples-80, 0, 49);
  else if(loopCounter %2 == 0)outputValue-=1;

  if(outputValue < 0) outputValue = 0;
  if(outputValue > 49) outputValue = 49;

  // outputValue += (rememberOutputValue-outputValue)*audioHistoryBuffer;
  // if (abs(outputValue-rememberOutputValue) >= 25  ) randNumber = random(255);

  // outputValue = (int)((float)rememberOutputValue*0.65 + (float)outputValue*0.35);

  for(int i=0;i < NUM_LEDS;i++){
    leds[i] = CRGB(0,0,0);
  }

  if(rememberOutputValue != outputValue)
  {
    if (!keyExists("modulation") || effectParams["modulation"] == "additive"){
      if (outputValue < rememberOutputValue) {
        if (!keyExists("dVal") || effectParams["dVal"]=="0")
          outputValue = outputValue;
        else
          outputValue = max(outputValue, rememberOutputValue-effectParams["dVal"].toFloat());
      }
      else if (outputValue > rememberOutputValue) {
        if (!keyExists("iVal") || effectParams["iVal"]=="0")
          outputValue = outputValue;
        else
          outputValue = min(outputValue, rememberOutputValue+effectParams["iVal"].toFloat());
      }
    }
    else if(effectParams["modulation"] == "multiplicative") {
      if (outputValue < rememberOutputValue) {
        if (!keyExists("dVal") || effectParams["dVal"]=="0")
          outputValue = outputValue;
        else
          outputValue = max(outputValue, (rememberOutputValue)*effectParams["dVal"].toFloat()-effectParams["offset"].toFloat());
      }
      else if (outputValue > rememberOutputValue) {
        if (!keyExists("iVal") || effectParams["iVal"]=="0")
          outputValue = outputValue;
        else
          outputValue = min(outputValue, (rememberOutputValue+1)*(effectParams["iVal"].toFloat()));
      }
    }
    int halfVal = (int)(outputValue/2);
    for(int i=49;i >= (49-halfVal) || (outputValue == 49 && i == 24);i-- ){
      uint8_t index = map(i, 25, 49, 0, 255);
      leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
      leds[49-i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
    }
  }
  rememberOutputValue = outputValue;
}