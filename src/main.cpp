#include <Arduino.h>

#include <WiFicreds.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

// sid SK59c605e1c770d91fce9a53e75e638ede
// DVmGmscLmcJjp1st6AAO3m2Y1TmaTRdP
ESP8266WebServer server(80);
void handleRoot();              // function prototypes for HTTP handlers
void handleNotFound();

#include <InfluxDb.h>

#define INFLUXDB_HOST "192.168.1.161"
#define INFLUXDB_PORT "1337"
#define INFLUXDB_DATABASE "boiler"

#define BOILER_OFF 0
#define BOILER_ON 1
#define BOILER_NO_CHANGE 2
#define BOILER_STARTING 3

#define ON_THRESHOLD 400
#define OFF_THRESHOLD 250

#define WATCHDOG_INTERVAL 300

bool DEBUG_ON=false;

bool quiet = false;
unsigned long epoch;
unsigned long time_now;
unsigned long since_epoch;

unsigned int boiler;
unsigned int boiler_status= BOILER_OFF;
unsigned int boiler_switched_on_time;

unsigned int min_level = 1023;
unsigned int max_level = 0;
unsigned int current_level = 0;

Influxdb influx(INFLUXDB_HOST);

unsigned int smooth_on = 0;
unsigned int smooth_off = 0;

unsigned short int history[300];
unsigned int history_entry=0; 

void tell_influx(unsigned int status, unsigned int time_interval)
{
  InfluxData measurement("boiler_status");
  
  measurement.addValue("interval",time_interval);
  measurement.addValue("interval_mins",time_interval/60);
  measurement.addValue("interval_secs",time_interval%60);

  if ( status == BOILER_OFF )
    measurement.addValue("value",0);
  else
    measurement.addValue("value",1);

    
    influx.write(measurement);
  
}

void tick_influx(String text,unsigned int min, unsigned int max,unsigned int current)
{
  InfluxData measurement("boiler_iot");
  measurement.addTag("lifecycle",text);
  measurement.addValue("min_reading",min);
  measurement.addValue("max_reading",max);
  measurement.addValue("current",current);
  measurement.addValue("value",1);
  measurement.addValue("uptime_hours",millis()/1000/60/60);
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
  
  String address = WiFi.localIP().toString();
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(address);

if (MDNS.begin("esp8266")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  influx.setDb(INFLUXDB_DATABASE);
  epoch = millis()/1000;
  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.begin(); 

  // Tell the database we are starting up
  tick_influx(String("Initialised_1_0_"+ address),0,1000,0);

}



void loop() {

  delay(500);

  unsigned int a0pin ;
    
  a0pin = analogRead(A0);

  current_level = a0pin;

  if ( a0pin > max_level )
    max_level = a0pin;
  if ( a0pin < min_level )
    min_level = a0pin;

  since_epoch = (millis()/1000) - epoch;

if ( DEBUG_ON  || !(since_epoch % WATCHDOG_INTERVAL) )
 {
  float voltage;
  voltage = a0pin * 3.3/1024.0;
  since_epoch = millis() - epoch;
  Serial.print(since_epoch);
  Serial.print("\t");
  Serial.print(a0pin);
  Serial.print("\t");
  Serial.println(voltage);
  tick_influx("alive",min_level,max_level,current_level);
 }
  boiler = BOILER_NO_CHANGE;

  if ( (boiler_status == BOILER_OFF ) && (a0pin > ON_THRESHOLD) )
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

if ( (boiler_status == BOILER_ON ) && (a0pin <  OFF_THRESHOLD ) )
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

  server.handleClient(); 

}
void handleRoot() {
  server.send(200, "text/plain", String( "Current level " + String(current_level,DEC)));   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}