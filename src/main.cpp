#include <Arduino.h>

#include <WiFicreds.h>
#include <ESP8266WiFi.h>

#include <InfluxDb.h>

#define INFLUXDB_HOST "192.168.1.161"
#define INFLUXDB_PORT "1337"
#define INFLUXDB_DATABASE "boiler"

#define BOILER_OFF 0
#define BOILER_ON 1
#define BOILER_NO_CHANGE 2
#define BOILER_STARTING 3

#define DEBUG_ON false

bool quiet = false;
unsigned long epoch;
unsigned long time_now;
unsigned long since_epoch;

unsigned int boiler;
unsigned int boiler_status= BOILER_OFF;
unsigned int boiler_switched_on_time;

unsigned int min_level = 1023;
unsigned int max_level = 0;

Influxdb influx(INFLUXDB_HOST);

unsigned int smooth_on = 0;
unsigned int smooth_off = 0;

void tell_influx(unsigned int status, unsigned int time_interval)
{
  InfluxData measurement("boiler_status");
  
  measurement.addTag("interval",String(time_interval));

  if ( status == BOILER_OFF )
    measurement.addValue("value",0);
  else
    measurement.addValue("value",1);
    
    influx.write(measurement);
  
}

void tick_influx(const char *text,unsigned int min, unsigned int max)
{
  InfluxData measurement("boiler_iot");
  measurement.addTag("lifecycle",text);
  measurement.addTag("min_reading",String(min));
  measurement.addTag("max_reading",String(max));
  measurement.addValue("value",1);
  influx.write(measurement);
}

void setup(void){
  Serial.begin(9600);
  Serial.println("I'm alive");
  
  WiFi.begin(ssid, pass);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  influx.setDb(INFLUXDB_DATABASE);

  epoch = millis()/1000;

  // Tell the database we are starting up
  tick_influx("Initialised",0,1000);

}




void loop() {

  delay(500);

  unsigned int a0pin ;
    
  a0pin = analogRead(A0);

  if ( a0pin > max_level )
    max_level = a0pin;
  if ( a0pin < min_level )
    min_level = a0pin;

  since_epoch = (millis()/1000) - epoch;

if ( DEBUG_ON  || !(since_epoch % 300) )
 {
  float voltage;
  voltage = a0pin * 3.3/1024.0;
  since_epoch = millis() - epoch;
  Serial.print(since_epoch);
  Serial.print("\t");
  Serial.print(a0pin);
  Serial.print("\t");
  Serial.println(voltage);
  tick_influx("alive",min_level,max_level);
 }
  boiler = BOILER_NO_CHANGE;

  if ( (boiler_status == BOILER_OFF ) && (a0pin > 700) )
  {
      smooth_on = smooth_on + 1 ;
      if ( smooth_on == 3 )
      {
         //Serial.println(" Boiler On");
         boiler = BOILER_ON;
         smooth_off = 0;
      }
  }
  else
  {
    smooth_on = 0;
  }

if ( (boiler_status == BOILER_ON ) && (a0pin <  650) )
  {
      smooth_off = smooth_off + 1 ;
      if ( smooth_off == 3 )
      {
         //Serial.println(" Boiler Off");
         boiler = BOILER_OFF;
         smooth_on = 0;
      }
  }
  else
  {
    smooth_off = 0;
  }

  switch(boiler)
  {
    case BOILER_NO_CHANGE:
    break;
    case BOILER_ON:
      Serial.println("Boiler switched on");
      boiler_status = BOILER_ON;
      boiler_switched_on_time = millis()/1000;
      tell_influx(BOILER_ON,0);
    break;
    case BOILER_OFF:
      if ( boiler_status == BOILER_ON )
      {
        int boiler_on_for;
        boiler_on_for = (millis()/1000) - boiler_switched_on_time;
        Serial.print("Boiler on for ");
        Serial.print(boiler_on_for);
        Serial.print(" seconds , boiler switched OFF\n");
        tell_influx(BOILER_OFF,boiler_on_for);
      }
      boiler_status = BOILER_OFF;
    break;
    default:
      Serial.println("ERROR - Default case in Boiler switch\n");
  }
  



}
