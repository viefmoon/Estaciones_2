#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <Stream.h>
#include <Adafruit_FONA.h>
#include "DS3231/DS3231_Simple.h"
#include "main/main.h"
#include <modbus/modbus.h>
#include "json/json.h"
#include "GPRS_GPS/GPRS_GPS.h"
#include "macro.h"
#include <Wire.h>
#include <Adafruit_INA219/Adafruit_INA219.h>

DS3231_Simple Clock;
DateTime MyDateAndTime;

Adafruit_INA219 ina219; //DEFAULT ADRRES 0x40 A1->GND A0->GND

MODBUS Sensors;
SIM_808 GPRS_MODULE;

void time_to_String(){
  char arrayCharTime[25];
  if((MyDateAndTime.Minute<10)&&(MyDateAndTime.Hour<10)){
    sprintf(arrayCharTime, "20%d-%d-%dT0%d:0%d:00", MyDateAndTime.Year, MyDateAndTime.Month, MyDateAndTime.Day,MyDateAndTime.Hour,MyDateAndTime.Minute);
  }
  else if(MyDateAndTime.Hour<10){
    sprintf(arrayCharTime, "20%d-%d-%dT0%d:%d:00", MyDateAndTime.Year, MyDateAndTime.Month, MyDateAndTime.Day,MyDateAndTime.Hour,MyDateAndTime.Minute);
  }
  else if(MyDateAndTime.Minute<10){
    sprintf(arrayCharTime, "20%d-%d-%dT%d:0%d:00", MyDateAndTime.Year, MyDateAndTime.Month, MyDateAndTime.Day,MyDateAndTime.Hour,MyDateAndTime.Minute);
  }
  else{
  sprintf(arrayCharTime, "20%d-%d-%dT%d:%d:00", MyDateAndTime.Year, MyDateAndTime.Month, MyDateAndTime.Day,MyDateAndTime.Hour,MyDateAndTime.Minute);
  }
  DeviceMeasures.addAttribute(arrayCharTime, F(RTC_CODE_MEASURE));
}

void BlinkyPost(){
  digitalWrite(pinPostLed, HIGH);   
  delay(1000);                      
  digitalWrite(pinPostLed, LOW);   
  delay(1000); 
  digitalWrite(pinPostLed, HIGH);      
  delay(1000);                      
  digitalWrite(pinPostLed, LOW);   
  delay(1000); 
  digitalWrite(pinPostLed, HIGH);   
  delay(2000);                      
  digitalWrite(pinPostLed, LOW);   
}

bool postData(){
  uint8_t postAttempts = 0;
  while (postAttempts < 5) {
    Sprintln(F("Trying to post..."));
    if (GPRS_MODULE.postJson(jsonRoot)) {
      return true;
    }
    else{
      postAttempts++;
      bool _INIT=0;
      while(!_INIT){
        GPRS_MODULE.RESET_SIM808();
        _INIT = GPRS_MODULE.setup_SIM808();
      }
    }
  }
  return false;
}

void CheckPower(){
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  //float power_mW = ina219.getPower_mW();
  float current_mA = ina219.getCurrent_mA();
  float loadvoltage = busvoltage + (shuntvoltage / 1000);
  Sprint(F("Voltage: ")); Sprint((loadvoltage)); Sprintln(F(" V"));
  Sprint(F("Current: ")); Sprint((current_mA)); Sprintln(F(" mA"));
  DeviceMeasures.addMeasure( loadvoltage, F("vol"));
  DeviceMeasures.addMeasure( current_mA, F("cur"));
}

void CheckPow(){
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  //float power_mW = ina219.getPower_mW();
  float current_mA = ina219.getCurrent_mA();
  float loadvoltage = busvoltage + (shuntvoltage / 1000);
  Sprint(F("Voltage: ")); Sprint((loadvoltage)); Sprintln(F(" V"));
  Sprint(F("Current: ")); Sprint((current_mA)); Sprintln(F(" mA"));
}

void checktime(){
  printTime();
  uint8_t AlarmsFired = Clock.checkAlarms();
  if(AlarmsFired & 2)
  {
    MyDateAndTime = Clock.read();
    if(MyDateAndTime.Minute % PostTime  == 00){
      printTime();
      Sprintln(F("Time to make measures"));
      //Si el tiempo se perdio, guardarlo de nuevo, se obtiene del gps
      if(MyDateAndTime.Year<20){
        Sprintln(F("Tiempo perdido, volviendo a setear el tiempo"));
        setTime();
      }
      init_Json();
      time_to_String();
      CheckPower();
      GPRS_MODULE.updateGps();
      Sensors.makeMeasures();
      Sprintln(F("Posting data.."));
      bool postState = postData();
      if(postState){
        Sprintln(F("Post Success!"));
        BlinkyPost();
      }
      else{
        Sprintln(F("Post Error, waiting next measures"));
      }
    }
  }
}

void setAlarm(){
  Clock.begin();
  Clock.disableAlarms();
  Clock.setAlarm(DS3231_Simple::ALARM_EVERY_MINUTE); 
}


void setTime(){
  Sprintln(F("Setting time..  "));
  uint8_t year =0, month = 0, day = 0, hour = 0, minute = 0, second = 0; 

  bool state = GPRS_MODULE.getTime(&year, &month, &day, &hour, &minute, &second);
  if (state){  
    DateTime MyTimestamp;
    MyTimestamp.Day    = day;
    MyTimestamp.Month  = month;
    MyTimestamp.Year   = year; 
    MyTimestamp.Hour   = hour;
    MyTimestamp.Minute = minute;
    MyTimestamp.Second = second;
    Clock.write(MyTimestamp);
    Sprintln(F("Date updated correctly"));

  }
  else{
    Sprintln(F("Error obtaining date"));
    GPRS_MODULE.RESET_SIM808();
    delay(60000);
  }
}

void printTime(){
  DateTime MyDateAndTime;
  MyDateAndTime = Clock.read();

  Sprint(F("Hour: "));  Sprint(MyDateAndTime.Hour); Sprint(F("  "));
  Sprint(F("Minute: "));  Sprint(MyDateAndTime.Minute); Sprint(F("  "));
  Sprint(F("Second: "));  Sprint(MyDateAndTime.Second); Sprint(F("  "));
  Sprint(F("Year: "));  Sprint(MyDateAndTime.Year); Sprint(F("  "));
  Sprint(F("Month: "));  Sprint(MyDateAndTime.Month); Sprint(F("  "));
  Sprint(F("Day: "));  Sprint(MyDateAndTime.Day); Sprintln(F("  "));
}

void setup() {

  Serial.begin(SERIAL_BAUDRATE);

  pinMode(SIM808_RST_PIN, OUTPUT); //RST pin for SIM808
  digitalWrite(SIM808_RST_PIN, LOW);
  pinMode(pinPostLed, OUTPUT); //SET PIN FOR BLINK LED
  digitalWrite(pinPostLed, LOW);
  pinMode(pinRs485, OUTPUT); //SET PIN FOR ACTIVE SENSORS
  digitalWrite(pinRs485, LOW);

  setAlarm();
  //Begin ina219 for measure voltage and current
  ina219.begin();
  //ina219.setCalibration_16V_400mA();
  ina219.setCalibration_32V_1A();

  bool SIM_INIT=0;
  while(!SIM_INIT){
    Sprintln("Trying start SIM808..");
    SIM_INIT = GPRS_MODULE.setup_SIM808();
    if(!SIM_INIT){
      GPRS_MODULE.RESET_SIM808();
    }
    else{
      Sprintln("Sim configured");
      delay(30000);
    }
  }
}

void loop() {
  checktime(); 
  //CheckPow();
}
