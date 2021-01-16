#include <SoftwareSerial.h>

SoftwareSerial ss(4,3); // GPS Module’s TX to D4 & RX to D3

byte Type[4];
String comprobacio_Type = "";
String hora = "";
String lat_strg = "";
String lng_strg = "";
char dada_inici = 'B';
char dada = 'A';
char lat_orientation;
char lng_orientation;
char tipus;

int lectura_complerta = 0;
int32_t lat;
int32_t lng;
uint32_t hora_unix;
int commes = 0;
int dadaint = 0;

void setup(){

Serial.begin(9600);
ss.begin(9600); //sobrara

}


void loop(){

lectura_complerta=0;
tipus='U';
Pin_Interface_Read_GPS(lectura_complerta,tipus);

}


int Pin_Interface_Read_GPS(uint8_t lectura_correcta,char tipus){
  while (ss.available() > 0  && !lectura_complerta){  
    //Serial.write(ss.read());
    dada_inici = (char)ss.read(); 
    //Serial.print(dada_inici);
    if(dada_inici=='$'){
      for(int i=0;i<6;i++){
        Type[i]=ss.read(); 
        char conversio = Type[i];
        comprobacio_Type=comprobacio_Type+conversio;
      } 
      //Serial.println(comprobacio_Type);
      if(comprobacio_Type.indexOf("GPRMC") > 0){
        dada = (char)ss.read(); 
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
        dada = (char)ss.read();
        //Serial.print(commes);
        //Serial.print(dada);
        }
      
        //Hora UNIX
        int dies = 0;
        hora_unix = hora_unix + 1610651623 + dies*86400; //avui + fins dia 15 gener + dies des del 15 passats
        
        //lat i long numeric
        
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