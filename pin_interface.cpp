#include "pin_interface.h"

void Pin_Interface_Set_Temp_Resolution(uint8_t sensorAddr, uint8_t res) {
  // set resolution
  Wire.beginTransmission(sensorAddr);
  Wire.write(TEMP_SENSOR_REG_CONFIG);
  Wire.write(res);
  Wire.endTransmission();

  // set mode back to temperature reading
  Wire.beginTransmission(sensorAddr);
  Wire.write((uint8_t)0x00);
  Wire.endTransmission();
}

float Pin_Interface_Read_Temperature(uint8_t sensorAddr) {
  // read data from I2C sensor
  Wire.requestFrom(sensorAddr, (uint8_t)2);
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  // convert raw data to temperature
  int16_t tempRaw = ((msb << 8) | lsb) >> 4;
  float temp = tempRaw * 0.0625f;
  return(temp);
}

int8_t Pin_Interface_Read_Temperature_Internal() {
  // select temperature sensor and reference
  ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
  // start AD conversion
  ADCSRA |= _BV(ADEN) | _BV(ADSC);
  // Detect end-of-conversion
  while (bit_is_set(ADCSRA, ADSC));
  // get raw data
  uint16_t raw =  ADCL | (ADCH << 8);

  // convert to real temperature
  int8_t temp = (int8_t)(((float)raw - MCU_TEMP_OFFSET) / MCU_TEMP_COEFFICIENT);
  return(temp);
}

float Pin_Interface_Read_Voltage(uint8_t pin) {
  // map ADC value to voltage
  return((analogRead(pin) * 3.3) / 1023.0);
}

void Pin_Interface_Watchdog_Heartbeat(bool manageBattery) {
  // toggle watchdog pin
  digitalWrite(DIGITAL_OUT_WATCHDOG_HEARTBEAT, !digitalRead(DIGITAL_OUT_WATCHDOG_HEARTBEAT));

  // check voltage
  if(manageBattery) {
    Power_Control_Check_Battery_Limit();
  }
}

int Pin_Interface_Read_GPS(uint8_t lectura_correcta,char tipus){
  while (Serial.available() > 0  && !lectura_complerta){  
    //Serial.write(ss.read());
    dada_inici = (char)Serial.read(); 
    //Serial.print(dada_inici);
    if(dada_inici=='$'){
      for(i=0;i<6;i++){
        Type[i]=Serial.read(); 
        char conversio = Type[i];
        comprobacio_Type=comprobacio_Type+conversio;
      } 
      //Serial.println(comprobacio_Type);
      if(comprobacio_Type.indexOf("GPRMC") > 0){
        dada = (char)Serial.read(); 
        while (dada != '$'){
          //Serial.print(dada);
          if(dada == ','){
            commes++;
            //Serial.print(commes);
          }
        
          else{
            if(commes==1){
              hora = hora + dada; 
              dadaint = (int)dada - '0';
              if(hora.length()<2) hora_unix = 36000*dadaint;
              else if (hora.length()<3) hora_unix = hora_unix + 3600*dadaint;
              else if (hora.length()<4) hora_unix = hora_unix + 600*dadaint;
              else if (hora.length()<5) hora_unix = hora_unix + 60*dadaint;
              else if (hora.length()<6) hora_unix = hora_unix + 10*dadaint;
              else if (hora.length()<7) hora_unix = hora_unix + dadaint;
            }
            else if(commes==3){
              if(dada!='.'&&lat_strg.length()<5){ 
                lat_strg = lat_strg + dada;
                dadaint = (int)dada - '0';  
                if(lat_strg.length()<2) lat = 1000*dadaint;
                else if (lat_strg.length()<3) lat = lat + 100*dadaint;
                else if (lat_strg.length()<4) lat = lat + 10*dadaint;
                else if (lat_strg.length()<5) lat = lat + dadaint;
              }
            }
            
            else if (commes==4) lat_orientation = dada; // N/S 
           
            else if(commes==5){
              if(dada!='.'&&lng_strg.length()<5) {
                lng_strg = lng_strg + dada;
                dadaint = (int)dada - '0';  
                if(lng_strg.length()<2) lng = 1000*dadaint;
                else if (lng_strg.length()<3) lng = lng + 100*dadaint;
                else if (lng_strg.length()<4) lng = lng + 10*dadaint;
                else if (lng_strg.length()<5) lng = lng + dadaint;
              }
            }
            
            else if(commes==6)lng_orientation = dada; // E/W
          }
        dada = (char)Serial.read();
        //Serial.print(commes);
        //Serial.print(dada);
        }
      
        //Hora UNIX
        int dies = 0;
        hora_unix = hora_unix + 1610651623 + dies*86400; //avui + fins dia 15 gener + dies des del 15 passats
        
        //lat i long orientation
        if(lat_orientation=='S') lat=-lat;
        if(lng_orientation=='W') lng=-lng;

        //Serial.println(hora);
        //Serial.println(hora_unix);
        //Serial.println(lat);
        //Serial.println(lng);
        
        if(tipus == 'U') return hora_unix;
        else if(tipus == 'A') return lat;
        else if(tipus == 'G') return lng;
        
        commes = 0;
        hora = "";
        hora_unix = 0;
        lat = 0;
        lng = 0;
        lat_strg = "";
        lng_strg = "";
        comprobacio_Type ="";
        lectura_complerta = 1;
      }
      else comprobacio_Type = "";

    }
  }
}

void Pin_Interface_Watchdog_Restart() {
  FOSSASAT_DEBUG_PRINTLN(F("Rst"));
  
  // do not pet watchdog for more than 30 seconds to restart
  for(uint8_t i = 0; i < WATCHDOG_RESET_NUM_SLEEP_CYCLES; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}