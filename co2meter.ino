/**
 * CO2 meter by Aleksandr.ru
 * @see https://aleksandr.ru/blog/domashniy_co2_metr
 */
 
#include <EEPROM.h> // builtin
#include <DHT.h> // https://github.com/adafruit/DHT-sensor-library
#include <MQ135.h> // https://github.com/Phoenix1747/MQ135
#include <iarduino_OLED.h> // https://iarduino.ru/file/340.html
#include <PinButton.h> // https://github.com/poelstra/arduino-multi-button/

// #define DEBUG 9600 // serial port speed  for debug data

#define BUTTON_PIN 9
#define DHT_PIN 2
#define MQ_PIN A0
#define MQ_RZERO 53.35 // 76.63 default https://github.com/GeorgK/MQ135/blob/master/MQ135.h#L27
#define MQ_RLOAD 1.0 // kOhm
#define OLED_ADDR 0x3C // I2C
#define RZERO_ADDR 0 // EEPROM

#define NUM_MEASURES 32 // >= GRAPTH_NUM_COLS
#define NUM_AVG_PPM 36 // x2 sec. to graph col
#define CALIBRATION_SEC 43200
#define NUM_INTERVALS 2

#define SMALL_ROW 10
#define MEDIUM_ROW 16
#define SMALL_COL 6

#define GRAPH_MIN 300
#define GRAPH_MAX 999
#define GRAPH_COL_WIDTH 5
#define GRAPTH_NUM_COLS floor(128 / GRAPH_COL_WIDTH) // 25

DHT myDht(DHT_PIN, DHT11);
MQ135 myMq(MQ_PIN, MQ_RZERO, MQ_RLOAD);
iarduino_OLED myOled(OLED_ADDR);
PinButton myButton(BUTTON_PIN);

extern uint8_t SmallFontRus[];
extern uint8_t BigNumbers[];

float rZero = 0.00f;
float temperature = 0.00f;
float humidity = 0.00f;
float ppm = 0.00f;
float avg_ppm = 0.00f;
byte avg_ppm_count = 0;
float measurements[NUM_MEASURES];
float lowMeasure = 0.00f;
float highMeasure = 0.00f;
bool calibrationMode = false;
bool displayMode = false;
struct INTERVAL {
      unsigned int step;
      unsigned long last;
      bool active;      
};
INTERVAL intervals[NUM_INTERVALS] = {
      {1000, 0, false},
      {2000, 0, false}
};

void setup()
{    
      pinMode(MQ_PIN, INPUT);
      pinMode(BUTTON_PIN, INPUT_PULLUP);

      #ifdef DEBUG
      Serial.begin(DEBUG);
      Serial.println(F("Warming up..."));   
      #endif
      
      myDht.begin();
      myOled.begin(); 
      myOled.autoUpdate(false);  
      myOled.setFont(SmallFontRus); 
      myOled.print("aleksandr.ru", 64-SMALL_COL*6, 32+SMALL_ROW/2);
      myOled.update();

      EEPROM.get(RZERO_ADDR, rZero);
      if (!rZero || isnan(rZero) || isinf(rZero)) {
            #ifdef DEBUG
            Serial.println(F("EEPROM rZero malformed, using default, calibration required!"));
            #endif
            rZero = MQ_RZERO;
      }
      else {
            #ifdef DEBUG
            Serial.print(F("EEPROM rZero: "));
            Serial.println(rZero, DEC);
            #endif
      }

      calibrationMode = !digitalRead(BUTTON_PIN);
      if (calibrationMode) setup_calibration();
      else setup_normal();

      delay(1000);
}    

void setup_calibration()
{
      myMq = MQ135(MQ_PIN, rZero, MQ_RLOAD);
      init_measurements();

      myOled.clrScr();  
      myOled.print(F("Temp C:"), 0, MEDIUM_ROW);
      myOled.print(F("Humidity:"), 0, MEDIUM_ROW*2);        
      myOled.print(F("CO2 PPM:"), 0, MEDIUM_ROW*3);           
      myOled.print(F("Time left:"), 0, MEDIUM_ROW*4);     
}

void setup_normal()
{
      myMq = MQ135(MQ_PIN, rZero, MQ_RLOAD);
      init_measurements();
}

void loop()
{  
      myButton.update();
      
      for (byte i = 0; i < NUM_INTERVALS; i++) {
            if(millis() >= intervals[i].last + intervals[i].step) {
                  intervals[i].last += intervals[i].step;
                  intervals[i].active = true;
            }
      }

      if (calibrationMode) loop_calibration();
      else loop_normal();    

      for (byte i = 0; i < NUM_INTERVALS; i++) intervals[i].active = false;
}

void loop_calibration()
{
      unsigned long sec = millis() / 1000;

      if (intervals[0].active) {
            read_dht(temperature, humidity);
            
            myOled.print(format_seconds(CALIBRATION_SEC - sec), 64, MEDIUM_ROW*4);
            myOled.update();
      }

      if (intervals[1].active) {
            if (sec >= CALIBRATION_SEC) {
                  rZero = avg_measure();
                  EEPROM.put(RZERO_ADDR, rZero);

                  #ifdef DEBUG
                  Serial.print(F("Calibration done, rZero in EEPROM: "));
                  Serial.println(rZero, DEC);
                  #endif

                  calibrationMode = false;
                  setup_normal();
            }
            else {
                  float ppm = myMq.getPPM();
                  float rzero = myMq.getRZero();

                  #ifdef DEBUG
                  Serial.print(F("Temp: ")); Serial.print(temperature, DEC);
                  Serial.print(F(" Humidity: ")); Serial.print(humidity, DEC);
                  Serial.print(F(" CO2 PPM: ")); Serial.print(ppm, DEC);
                  Serial.print(F(" rZero: ")); Serial.println(rzero, DEC);                
                  #endif

                  if (rzero && !isinf(rzero)) push_measure(rzero, highMeasure, lowMeasure);

                  myOled.print(temperature, 64, MEDIUM_ROW);
                  myOled.print(humidity, 64, MEDIUM_ROW*2);
                  myOled.print(ppm, 64, MEDIUM_ROW*3);      
                  myOled.update();     
            }
      }
}

void loop_normal()
{
      if (myButton.isClick()) {
            displayMode = !displayMode;
            display_data();
      }
      
      if (intervals[0].active) {
            read_dht(temperature, humidity);
      }

      if (intervals[1].active) {
            if (temperature && humidity) {
                  ppm = myMq.getCorrectedPPM(temperature, humidity);
            }
            else {
                  ppm = myMq.getPPM();
            }

            avg_ppm += ppm;
            if (avg_ppm_count++) avg_ppm = avg_ppm / 2;
            if (avg_ppm_count >= NUM_AVG_PPM) {
                  #ifdef DEBUG
                  Serial.print(F("Adding PPM: ")); Serial.println(avg_ppm, DEC);
                  #endif
                  push_measure(avg_ppm, highMeasure, lowMeasure);
                  avg_ppm = 0.00f;
                  avg_ppm_count = 0;
            }

            #ifdef DEBUG
            Serial.print(F("Temp: ")); Serial.print(temperature, DEC);
            Serial.print(F(" Humidity: ")); Serial.print(humidity, DEC);
            Serial.print(F(" CO2 PPM: ")); Serial.print(ppm, DEC);
            Serial.print(F(" Avg PPM: ")); Serial.println(avg_ppm, DEC);      
            #endif

            display_data();
      }
}

void display_data()
{
      bool alarm = ppm > GRAPH_MAX;

      myOled.clrScr();  
      if (alarm || displayMode) {
            myOled.invScr(alarm && millis() / 1000 % 4);
            myOled.setFont(BigNumbers); 
            if (ppm > 1000) myOled.print(round(ppm), 64 - 32, 44);
            else            myOled.print(round(ppm), 64 - 21, 44);
            
            myOled.setFont(SmallFontRus); 
            myOled.print(F("CO2"), 64-SMALL_COL*3/2, SMALL_ROW);
            myOled.print(F("PPM"), 64-SMALL_COL*3/2, 64);                 
      }
      else {   
            myOled.invScr(false);               
            myOled.print(round(humidity), 0, SMALL_ROW); 
            myOled.print(F("%"), SMALL_COL*3, SMALL_ROW); 
                              
            myOled.print(round(temperature), 64-SMALL_COL*3, SMALL_ROW); 
            myOled.print(F("C"), 64, SMALL_ROW); 
            
            myOled.print(round(ppm), 128-SMALL_COL*7, SMALL_ROW); 
            myOled.print(F("PPM"), 128-SMALL_COL*3, SMALL_ROW);
            
            float measure;
            byte colHeight;
            byte c;
            for(c = 0; c < GRAPTH_NUM_COLS; c++) {
                  byte i = NUM_MEASURES - GRAPTH_NUM_COLS + c;
                  if (measurements[i]) {
                        measure = constrain(measurements[i], GRAPH_MIN, GRAPH_MAX);
                        colHeight = map(measure, GRAPH_MIN, GRAPH_MAX, 1, 64-SMALL_ROW*1.5);
                        myOled.drawRect(c*GRAPH_COL_WIDTH, 64-colHeight , (c+1)*GRAPH_COL_WIDTH-3, 64, true, 1);
                  }
            }

            measure = constrain(ppm, GRAPH_MIN, GRAPH_MAX);
            colHeight = map(measure, GRAPH_MIN, GRAPH_MAX, 1, 64-SMALL_ROW*1.5);
            myOled.drawRect(c*GRAPH_COL_WIDTH, 64-colHeight , (c+1)*GRAPH_COL_WIDTH-3, 64, true, 1);
      }
      myOled.update();
}

bool read_dht(float& temp, float& hum)
{
      float h = myDht.readHumidity();
      float t = myDht.readTemperature();

      if (isnan(t) || isnan(h)) {
            #ifdef DEBUG
            Serial.println(F("Temp or humidity malformed!"));
            #endif
            return false;
      }
      else {
            temp = t;
            hum = h;
            return true;
      }
}

void push_measure(float value, float& max, float& min)
{
      max = min = value;
      for(byte i = 0; i < NUM_MEASURES - 1; i++) {
            measurements[i] = measurements[i+1];
            if (measurements[i] < min) min = measurements[i];
            else if (measurements[i] > max) max = measurements[i];
      }
      measurements[NUM_MEASURES - 1] = value;
}

void init_measurements()
{
      for(byte i = 0; i < NUM_MEASURES; i++) measurements[i] = 0;
      highMeasure = lowMeasure = 0;
}

float avg_measure()
{
      float ret = 0;
      for(byte i = 0; i < NUM_MEASURES; i++) {
        ret += measurements[i];
      }
      return ret / NUM_MEASURES;
}

String format_seconds(unsigned long sec)
{
      byte hours = floor(sec / 3600);
      byte minutes = floor((sec % 3600) / 60);
      byte seconds = (sec % 3600) % 60; 
      String times = "";
      if (hours < 10) times.concat("0");
      times.concat(hours);
      times.concat(":");
      if (minutes < 10) times.concat("0");
      times.concat(minutes);
      times.concat(":");
      if (seconds < 10) times.concat("0");
      times.concat(seconds);   
      return times;
}
