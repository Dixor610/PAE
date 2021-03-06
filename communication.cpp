#include "communication.h"

void Communication_Receive_Interrupt() {
  // check interrups are enabled
  if (!interruptsEnabled) {
    return;
  }

  // set flag
  dataReceived = true;
}

int16_t Communication_Set_Modem(uint8_t modem) {
  int16_t state = ERR_NONE;
  FOSSASAT_DEBUG_WRITE(modem);

  // initialize requested modem
  switch (modem) {
    case MODEM_LORA:
      state = radio.begin(LORA_CARRIER_FREQUENCY,
                          LORA_BANDWIDTH,
                          LORA_SPREADING_FACTOR,
                          LORA_CODING_RATE,
                          SYNC_WORD,
                          LORA_OUTPUT_POWER,
                          LORA_PREAMBLE_LENGTH,
                          LORA_GAIN);
      radio.setCRC(true);
      radio.setCurrentLimit(LORA_CURRENT_LIMIT);
      break;
    case MODEM_FSK: {
        state = radio.beginFSK(FSK_CARRIER_FREQUENCY,
                               FSK_BIT_RATE,
                               FSK_FREQUENCY_DEVIATION,
                               FSK_RX_BANDWIDTH,
                               FSK_OUTPUT_POWER,
                               FSK_PREAMBLE_LENGTH,
                               TCXO_VOLTAGE);
        uint8_t syncWordFSK[2] = {SYNC_WORD, SYNC_WORD};
        radio.setSyncWord(syncWordFSK, 2);
        radio.setCRC(2);
        radio.setDataShaping(FSK_DATA_SHAPING);
        radio.setCurrentLimit(FSK_CURRENT_LIMIT);
      } break;
    default:
      return (ERR_UNKNOWN);
  }

  // radio.setWhitening(true, WHITENING_INITIAL);
  
  // set TCXO
  //  radio.setTCXO(TCXO_VOLTAGE);

  
  // handle possible error codes
  FOSSASAT_DEBUG_PRINT(F("Init "));
  FOSSASAT_DEBUG_PRINTLN(state);
  FOSSASAT_DEBUG_DELAY(10);
  if (state != ERR_NONE) {
    // radio chip failed, restart
    Pin_Interface_Watchdog_Restart();
  }

  // set spreading factor
  Communication_Set_SpreadingFactor(spreadingFactorMode);



  // save current modem
  currentModem = modem;
  return (state);
}

int16_t Communication_Set_SpreadingFactor(uint8_t sfMode) {
  uint8_t sfs[] = {LORA_SPREADING_FACTOR, LORA_SPREADING_FACTOR_ALT};

  // check currently active modem
  if (currentModem == MODEM_FSK) {
    return (ERR_WRONG_MODEM);
  }

  // update standard/alternative spreading factor
  int16_t state = radio.setSpreadingFactor(sfs[sfMode]);

  // only save current spreading factor mode if the change was successful
  if (state == ERR_NONE) {
    spreadingFactorMode = sfMode;
  }

  return (state);
}

int16_t Communication_Set_Configuration(uint8_t* optData, uint8_t optDataLen) {
  // check optDataLen
  if (!((optDataLen >= 7 + 1) && (optDataLen <= 7 + MAX_STRING_LENGTH))) {
    return (ERR_PACKET_TOO_LONG);
  }

  // check bandwidth value (loaded from array - rest of settings are checked by library)
  if (optData[0] > 7) {
    return (ERR_INVALID_BANDWIDTH);
  }

  // attempt to change the settings
  float bws[] = {7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0};
  uint16_t preambleLength = 0;
  memcpy(&preambleLength, optData + 3, sizeof(uint16_t));
  uint16_t state = radio.begin(LORA_CARRIER_FREQUENCY,
                               bws[optData[0]],
                               optData[1],
                               optData[2],
                               SYNC_WORD,
                               optData[6],
                               LORA_CURRENT_LIMIT,
                               preambleLength);
  if (state != ERR_NONE) {
    return (state);
  }

  // set CRC
  state = radio.setCRC(optData[5]);
  return (state);
}

void Communication_Send_Morse_Beacon(float battVoltage) {
  // initialize Morse client
  morse.begin(FSK_CARRIER_FREQUENCY, MORSE_SPEED);

  // read callsign
  uint8_t callsignLen = Persistent_Storage_Read<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH + 1];
  System_Info_Get_Callsign(callsign, callsignLen);

  // send start signals
  for (int8_t i = 0; i < MORSE_PREAMBLE_LENGTH; i++) {
    morse.startSignal();
    Pin_Interface_Watchdog_Heartbeat();
  }

  // send callsign
  for (uint8_t i = 0; i < callsignLen - 1; i++) {
    morse.print(callsign[i]);
    Pin_Interface_Watchdog_Heartbeat();
  }

  // space
  morse.print(' ');
  Pin_Interface_Watchdog_Heartbeat();

  // send battery voltage code
  char code = 'A' + (uint8_t)((battVoltage - MORSE_BATTERY_MIN) / MORSE_BATTERY_STEP);
  morse.println(code);
  Pin_Interface_Watchdog_Heartbeat();
}

void Communication_CW_Beep(uint32_t len) {
  radio.transmitDirect();
  Power_Control_Delay(len, true);
  radio.standby();
}

void Communication_Send_System_Info() {
  // build response frame
  static const uint8_t optDataLen = 10 * sizeof(uint8_t) + 3 * sizeof(int16_t) + 3 * sizeof(uint16_t) + sizeof(int8_t) + 2 * sizeof(uint32_t) + 2 * sizeof(int32_t) + sizeof(uint8_t[5]);
  uint8_t optData[optDataLen];
  uint8_t* optDataPtr = optData;

  //set IDsource
  uint8_t IDsource = 7;
  Communication_Frame_Add<uint8_t>(&optDataPtr, IDsource, "IDs");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

  //set IDdest
  uint8_t IDdestiny = 255;
  Communication_Frame_Add<uint8_t>(&optDataPtr, IDdestiny, "IDd");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

  //Nº Nodes
  uint8_t nNode = 1;
  Communication_Frame_Add<uint8_t>(&optDataPtr, nNode, "nNod");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

  //Llista nodes
  uint8_t nodeList[1]; 
  nodeList[1] = IDsource;
  Communication_Frame_Add<uint8_t>(&optDataPtr, nodeList[1], "lNod");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

  //set UNIX Time
  char tipus = 'U';
  int lectura_complerta = 0;
  uint32_t UNIX_time = Pin_Interface_Read_GPS(lectura_complerta,tipus);;
  Communication_Frame_Add<uint32_t>(&optDataPtr, UNIX_time, "UNIX");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

  //set ID
  uint8_t ID = 7;
  Communication_Frame_Add<uint8_t>(&optDataPtr, ID, "ID");
  //Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

#ifdef ENABLE_INA226
  uint8_t batteryVoltage = Power_Control_Get_Battery_Voltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
#else
  uint8_t batteryVoltage = 4.02 * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
#endif
  Communication_Frame_Add<uint8_t>(&optDataPtr, batteryVoltage, "batV");
  Persistent_Storage_Update_Stats<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR, batteryVoltage);

#ifdef ENABLE_INA226
  int16_t batteryChargingCurrent = Power_Control_Get_Charging_Current() * (CURRENT_UNIT / CURRENT_MULTIPLIER);
#else
  int16_t batteryChargingCurrent = 0.056 * (CURRENT_UNIT / CURRENT_MULTIPLIER);
#endif
  Communication_Frame_Add<int16_t>(&optDataPtr, batteryChargingCurrent, "batChI");
  Persistent_Storage_Update_Stats<int16_t>(EEPROM_CHARGING_CURRENT_STATS_ADDR, batteryChargingCurrent);

#ifdef ENABLE_INA226
  uint8_t batteryChargingVoltage = Power_Control_Get_Charging_Voltage() * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
#else
  uint8_t batteryChargingVoltage = 3.82 * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
#endif
  Communication_Frame_Add<uint8_t>(&optDataPtr, batteryChargingVoltage, "batChV");
  Persistent_Storage_Update_Stats<uint8_t>(EEPROM_CHARGING_VOLTAGE_STATS_ADDR, batteryChargingVoltage);

  // set uptimeCounter
  uint32_t uptimeCounter = Persistent_Storage_Read<uint32_t>(EEPROM_UPTIME_COUNTER_ADDR);
  Communication_Frame_Add(&optDataPtr, uptimeCounter, "up");

  // set powerConfig variable
  Power_Control_Load_Configuration();
  FOSSASAT_DEBUG_PRINTLN(powerConfig.val, BIN);
  memcpy(optDataPtr, &powerConfig.val, sizeof(uint8_t));
  optDataPtr += sizeof(uint8_t);

  // set resetCounter variable
  uint16_t resetCounter = Persistent_Storage_Read<uint16_t>(EEPROM_RESTART_COUNTER_ADDR);
  Communication_Frame_Add(&optDataPtr, resetCounter, "rst");

  uint8_t solarCellAVoltage = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_A_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
  Communication_Frame_Add<uint8_t>(&optDataPtr, solarCellAVoltage, "sAV");
  Persistent_Storage_Update_Stats<uint8_t>(EEPROM_CELL_A_VOLTAGE_STATS_ADDR, solarCellAVoltage);

  // set solarCellBVoltage variable
  uint8_t solarCellBVoltage = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_B_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
  Communication_Frame_Add<uint8_t>(&optDataPtr, solarCellBVoltage, "sBV");
  Persistent_Storage_Update_Stats<uint8_t>(EEPROM_CELL_B_VOLTAGE_STATS_ADDR, solarCellBVoltage);

  // set solarCellCVoltage variable
  uint8_t solarCellCVoltage = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_C_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
  Communication_Frame_Add<uint8_t>(&optDataPtr, solarCellCVoltage, "sCV");
  Persistent_Storage_Update_Stats<uint8_t>(EEPROM_CELL_C_VOLTAGE_STATS_ADDR, solarCellCVoltage);

  // set batteryTemperature variable
  int16_t batteryTemperature = Pin_Interface_Read_Temperature(BATTERY_TEMP_SENSOR_ADDR) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
  Communication_Frame_Add<int16_t>(&optDataPtr, batteryTemperature, "batT");
  Persistent_Storage_Update_Stats<int16_t>(EEPROM_BATTERY_TEMP_STATS_ADDR, batteryTemperature);

  // set boardTemperature variable
  int16_t boardTemperature = Pin_Interface_Read_Temperature(BOARD_TEMP_SENSOR_ADDR) * (TEMPERATURE_UNIT / TEMPERATURE_MULTIPLIER);
  Communication_Frame_Add<int16_t>(&optDataPtr, boardTemperature, "brdT");
  Persistent_Storage_Update_Stats<int16_t>(EEPROM_BOARD_TEMP_STATS_ADDR, boardTemperature);

  // set mcuTemperature variable (read twice since first value is often nonsense)
  Pin_Interface_Read_Temperature_Internal();
  int8_t mcuTemperature = Pin_Interface_Read_Temperature_Internal();
  Communication_Frame_Add<int8_t>(&optDataPtr, mcuTemperature, "mcuT");
  Persistent_Storage_Update_Stats<int8_t>(EEPROM_MCU_TEMP_STATS_ADDR, mcuTemperature);

  //set Lat
  lectura_complerta=0;
  tipus='A';
  int32_t Latitude = Pin_Interface_Read_GPS(lectura_complerta,tipus);
  Communication_Frame_Add<int32_t>(&optDataPtr, Latitude, "lat");
  //Persistent_Storage_Update_Stats<int8_t>(EEPROM_MCU_TEMP_STATS_ADDR, mcuTemperature);

  //set Long
  lectura_complerta=0;
  tipus='G';
  int32_t Longitude = Pin_Interface_Read_GPS(lectura_complerta,tipus);
  Communication_Frame_Add<int32_t>(&optDataPtr, Longitude, "lng");
  //Persistent_Storage_Update_Stats<int8_t>(EEPROM_MCU_TEMP_STATS_ADDR, mcuTemperature);

  //set Paq TX
  uint16_t paqTx = 0;
  Communication_Frame_Add<uint16_t>(&optDataPtr, paqTx, "pqTx");
  //Persistent_Storage_Update_Stats<int8_t>(EEPROM_MCU_TEMP_STATS_ADDR, mcuTemperature);

  //set Paq RX
  uint16_t paqRx = 0;
  Communication_Frame_Add<uint16_t>(&optDataPtr, paqRx, "pqRx");
  //Persistent_Storage_Update_Stats<int8_t>(EEPROM_MCU_TEMP_STATS_ADDR, mcuTemperature);

  // send as raw bytes
  Communication_Send_Response(RESP_SYSTEM_INFO, optData, optDataLen);
}


void Communication_Acknowledge(uint8_t functionId, uint8_t result) {
  uint8_t optData[] = { functionId, result };
  Communication_Send_Response(RESP_ACKNOWLEDGE, optData, 2);
}

void Communication_Process_Packet() {
  /*FOSSASAT_DEBUG_PRINT("Communication_Process_Packet ");
    FOSSASAT_DEBUG_PRINTLN(freeRam());
    FOSSASAT_DEBUG_DELAY(100);*/

  // disable interrupts
  interruptsEnabled = false;

  // read data
  size_t len = radio.getPacketLength();
  if (len == 0) {
    dataReceived = false;
    interruptsEnabled = true;
    return;
  }
  uint8_t frame[MAX_RADIO_BUFFER_LENGTH];
  int16_t state = radio.readData(frame, len);

  // check reception state
  if (state == ERR_NONE) {
    FOSSASAT_DEBUG_PRINT(F("Frm "));
    FOSSASAT_DEBUG_PRINTLN(len);
    FOSSASAT_DEBUG_PRINT_BUFF(frame, len);

    // check callsign
    uint8_t callsignLen = Persistent_Storage_Read<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
    char callsign[MAX_STRING_LENGTH + 1];
    System_Info_Get_Callsign(callsign, callsignLen);
    if (memcmp(frame, (uint8_t*)callsign, callsignLen - 1) == 0) {
      // check passed
      Comunication_Parse_Frame(frame, len);
    } else {
      FOSSASAT_DEBUG_PRINTLN(F("CErr"));
      Persistent_Storage_Increment_Frame_Counter(false);
      Communication_Acknowledge(0xFF, 0x01);
    }

  } else {
    FOSSASAT_DEBUG_PRINT(F("RxErr "));
    FOSSASAT_DEBUG_PRINT(state);
    Persistent_Storage_Increment_Frame_Counter(false);
    Communication_Acknowledge(0xFF, 0x02);
  }

  // reset flag
  dataReceived = false;

  // enable interrupts
  interruptsEnabled = true;
}

void Comunication_Parse_Frame(uint8_t* frame, size_t len) {
  /*FOSSASAT_DEBUG_PRINT("Comunication_Parse_Frame ");
    FOSSASAT_DEBUG_PRINTLN(freeRam());
    FOSSASAT_DEBUG_DELAY(100);*/

  // read callsign from EEPROM
  uint8_t callsignLen = Persistent_Storage_Read<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH + 1];
  System_Info_Get_Callsign(callsign, callsignLen);

  // get functionID
  int16_t functionId = FCP_Get_FunctionID(callsign, frame, len);
  if (functionId < 0) {
    FOSSASAT_DEBUG_PRINT(F("IdErr"));
    FOSSASAT_DEBUG_PRINTLN(functionId);
    Persistent_Storage_Increment_Frame_Counter(false);
    Communication_Acknowledge(0xFF, 0x03);
    return;
  }
  FOSSASAT_DEBUG_PRINT(F("FID="));
  FOSSASAT_DEBUG_PRINTLN(functionId, HEX);

  // check encryption
  int16_t optDataLen = 0;
  uint8_t optData[MAX_OPT_DATA_LENGTH];
  if ((functionId >= PRIVATE_OFFSET) && (functionId <= (PRIVATE_OFFSET + NUM_PRIVATE_COMMANDS))) {
    // frame contains encrypted data, decrypt

    // get optional data length
    optDataLen = FCP_Get_OptData_Length(callsign, frame, len, encryptionKey, password);
    if (optDataLen < 0) {
      FOSSASAT_DEBUG_PRINT(F("DcErr"));
      FOSSASAT_DEBUG_PRINTLN(optDataLen);

      // decryption failed, increment invalid frame counter and return
      Persistent_Storage_Increment_Frame_Counter(false);
      Communication_Acknowledge(0xFF, 0x04);
      return;
    }

    // get optional data
    if (optDataLen > 0) {
      FCP_Get_OptData(callsign, frame, len, optData, encryptionKey, password);
    }

  } else if (functionId < PRIVATE_OFFSET) {
    // no decryption necessary

    // get optional data length
    optDataLen = FCP_Get_OptData_Length(callsign, frame, len);
    if (optDataLen < 0) {
      // optional data extraction failed,
      FOSSASAT_DEBUG_PRINT(F("LenErr"));
      FOSSASAT_DEBUG_PRINTLN(optDataLen);

      // increment invalid frame counter
      Persistent_Storage_Increment_Frame_Counter(false);
      Communication_Acknowledge(0xFF, 0x05);
      return;
    }

    // get optional data
    if (optDataLen > 0) {
      FCP_Get_OptData(callsign, frame, len, optData);
    }
  } else {
    // unknown function ID
    FOSSASAT_DEBUG_PRINT(F("UID"));
    FOSSASAT_DEBUG_PRINTLN(functionId, HEX);
    Persistent_Storage_Increment_Frame_Counter(false);
    Communication_Acknowledge(0xFF, 0x06);
    return;
  }

  // check optional data presence
  if (optDataLen > 0) {
    // execute with optional data
    FOSSASAT_DEBUG_PRINT(F("optLen="));
    FOSSASAT_DEBUG_PRINTLN(optDataLen);
    FOSSASAT_DEBUG_PRINT_BUFF(optData, (uint8_t)optDataLen);
    Communication_Execute_Function(functionId, optData, optDataLen);

  } else {
    // execute without optional data
    Communication_Execute_Function(functionId);
  }
}

void Communication_Execute_Function(uint8_t functionId, uint8_t* optData, size_t optDataLen) {
  /*FOSSASAT_DEBUG_PRINT("Communication_Execute_Function ");
    FOSSASAT_DEBUG_PRINTLN(freeRam());
    FOSSASAT_DEBUG_DELAY(100);*/

  // increment valid frame counter
  Persistent_Storage_Increment_Frame_Counter(true);

  // acknowledge frame
  Communication_Acknowledge(functionId, 0x00);

  // execute function based on ID
  switch (functionId) {
    /*case CMD_PING:
     send pong
    Communication_Send_Response(RESP_PONG);
    break;*/

    case CMD_RETRANSMIT_PACKET: {
      paqRx += 1;
      uint8_t IDs_Constellation[] = {1,4,7,8,9};
      if ( ((float)random(0,101)/100) <= 0.6 ){
        boolean cancel_Retransmission = true;
        uint8_t* optDataPtr = optData;
        uint8_t source[1], destination[1], N_nodes[1];
        
        memcpy(source, optDataPtr, sizeof(source));
        for (int i=0; i<sizeof(IDs_Constallation); i++){
          if( IDs_Constellation[i] == source ){
            cancel_Retransmission = false;
          }
        }
        (*optDataPtr) += sizeof(source);
        
        memcpy(destination, optDataPtr, sizeof(destination));
        if( destination != 255 ){
          cancel_Retransmission = true;
        } 
        (*optDataPtr) += sizeof(destination);
        
        memcpy(N_nodes, optDataPtr, sizeof(N_nodes));
        N_nodes += 1;
        Communication_Frame_Add<uint8_t>(&optDataPtr, N_nodes, "N_nodes");

        uint8_t list[N_nodes];
        for(int i=0;i<sizeof(list);i++){
          list[i]=0;
        }
        memcpy(list, optDataPtr, sizeof(list)-1);
        boolean IDsource_added = false;
        for ( int i=0; i<sizeof(list); i++ ){
          if( list[i] == IDsource && IDsource_added == false){
            cancel_Retransmission = true;
          }
          if( list[i+1] == 0 ){
            list[i+1] = IDsource;
            IDsource_added = true;
          }
          Communication_Frame_Add<uint8_t>(&optDataPtr, list[i], "lNod");
        }
        
        if ( cancel_Retransmission == false){
          Communication_Send_Response(RESP_SYSTEM_INFO, optData, optDataLen);
          paqTx += 1;
        }       
      }
    } break;

    case CMD_RETRANSMIT: {
     check message length
    if (optDataLen <= MAX_STRING_LENGTH) {
     respond with the requested data
    Communication_Send_Response(RESP_REPEATED_MESSAGE, optData, optDataLen);
    }
    } break;

    case CMD_RETRANSMIT_CUSTOM: {
     check message length
    if((optDataLen >= 8) && (optDataLen <= MAX_STRING_LENGTH + 7)) {
    // change modem configuration
    int16_t state = Communication_Set_Configuration(optData, optDataLen);

    // check if the change was successful
    if(state != ERR_NONE) {
    FOSSASAT_DEBUG_PRINT(F("CfgErr"));
    FOSSASAT_DEBUG_PRINTLN(state);
    } else {
    // configuration changed successfully, transmit response
    Communication_Send_Response(RESP_REPEATED_MESSAGE_CUSTOM, optData + 7, optDataLen - 7, true);
    }
    }
    } break;

    case CMD_TRANSMIT_SYSTEM_INFO:
      // send system info via LoRa
      Communication_Send_System_Info();
      break;

      case CMD_GET_PACKET_INFO: {
       get last packet info and send it
      static const uint8_t respOptDataLen = 2*sizeof(uint8_t) + 4*sizeof(uint16_t);
      uint8_t respOptData[respOptDataLen];
      uint8_t* respOptDataPtr = respOptData;

      // SNR
      int8_t snr = (int8_t)(radio.getSNR() * 4.0);
      Communication_Frame_Add(&respOptDataPtr, snr, "SNR");

      // RSSI
      uint8_t rssi = (uint8_t)(radio.getRSSI() * -2.0);
      Communication_Frame_Add(&respOptDataPtr, rssi, "RSSI");

      uint16_t loraValid = Persistent_Storage_Read<uint16_t>(EEPROM_LORA_VALID_COUNTER_ADDR);
      Communication_Frame_Add(&respOptDataPtr, loraValid, "Lv");

      uint16_t loraInvalid = Persistent_Storage_Read<uint16_t>(EEPROM_LORA_INVALID_COUNTER_ADDR);
      Communication_Frame_Add(&respOptDataPtr, loraInvalid, "Li");

      uint16_t fskValid = Persistent_Storage_Read<uint16_t>(EEPROM_FSK_VALID_COUNTER_ADDR);
      Communication_Frame_Add(&respOptDataPtr, fskValid, "Fv");

      uint16_t fskInvalid = Persistent_Storage_Read<uint16_t>(EEPROM_FSK_INVALID_COUNTER_ADDR);
      Communication_Frame_Add(&respOptDataPtr, fskInvalid, "Fi");

      Communication_Send_Response(RESP_PACKET_INFO, respOptData, respOptDataLen);
      } break;

      case CMD_GET_STATISTICS: {
       check optional data is exactly 1 byte
      if(Communication_Check_OptDataLen(1, optDataLen)) {
       response will have maximum of 34 bytes if all stats are included
      uint8_t respOptData[34];
      uint8_t respOptDataLen = 1;
      uint8_t* respOptDataPtr = respOptData;

      // copy stat flags
      uint8_t flags = optData[0];
      memcpy(respOptDataPtr, &flags, sizeof(uint8_t));
      respOptDataPtr += sizeof(uint8_t);

      // get required stats from EEPROM
      if(flags & 0x01) {
      // charging voltage
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CHARGING_VOLTAGE_STATS_ADDR), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CHARGING_VOLTAGE_STATS_ADDR + 1), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CHARGING_VOLTAGE_STATS_ADDR + 2), "");
      respOptDataLen += 3;
      }

      if(flags& 0x02) {
      // charging current
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_CHARGING_CURRENT_STATS_ADDR), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_CHARGING_CURRENT_STATS_ADDR + 2), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_CHARGING_CURRENT_STATS_ADDR + 4), "");
      respOptDataLen += 6;
      }

      if(flags & 0x04) {
      // battery voltage
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR + 1), "");
      Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_BATTERY_VOLTAGE_STATS_ADDR + 2), "");
      respOptDataLen += 3;
      }

      if(flags & 0x08) {
        // cell A voltage
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_A_VOLTAGE_STATS_ADDR), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_A_VOLTAGE_STATS_ADDR + 1), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_A_VOLTAGE_STATS_ADDR + 2), "");
        respOptDataLen += 3;
        }

        if(flags & 0x10) {
        // cell B voltage
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_B_VOLTAGE_STATS_ADDR), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_B_VOLTAGE_STATS_ADDR + 1), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_B_VOLTAGE_STATS_ADDR + 2), "");
        respOptDataLen += 3;
        }

        if(flags & 0x20) {
        // cell C voltage
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_C_VOLTAGE_STATS_ADDR), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_C_VOLTAGE_STATS_ADDR + 1), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<uint8_t>(EEPROM_CELL_C_VOLTAGE_STATS_ADDR + 2), "");
        respOptDataLen += 3;
        }

        if(flags & 0x40) {
        // battery temperature
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BATTERY_TEMP_STATS_ADDR), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BATTERY_TEMP_STATS_ADDR + 2), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BATTERY_TEMP_STATS_ADDR + 4), "");
        respOptDataLen += 6;
        }

        if(flags & 0x80) {
        // board temperature
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BOARD_TEMP_STATS_ADDR), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BOARD_TEMP_STATS_ADDR + 2), "");
        Communication_Frame_Add(&respOptDataPtr, Persistent_Storage_Read<int16_t>(EEPROM_BOARD_TEMP_STATS_ADDR + 4), "");
        respOptDataLen += 6;
        }

        // send response
        Communication_Send_Response(RESP_STATISTICS, respOptData, respOptDataLen);
        }
        } break;

      // private function IDs
      case CMD_DEPLOY: {
          // run deployment sequence
          Deployment_Deploy();

          // get deployment counter value and send it
          uint8_t counter = Persistent_Storage_Read<uint8_t>(EEPROM_DEPLOYMENT_COUNTER_ADDR);
          Communication_Send_Response(RESP_DEPLOYMENT_STATE, &counter, 1);
        } break;
      
      case CMD_RESTART:
        // restart satellite
        Pin_Interface_Watchdog_Restart();
        break;

        case CMD_WIPE_EEPROM:
        // wipe EEPROM and reset all EEPROM variables to default values
        Persistent_Storage_Wipe();
        break;

        case CMD_SET_TRANSMIT_ENABLE: {
          // check optional data is exactly 1 byte
          if(Communication_Check_OptDataLen(1, optDataLen)) {
            // load power config from EEPROM
            Power_Control_Load_Configuration();

            // update transmit enable flag
            powerConfig.bits.transmitEnabled = optData[0];

            // save power config from EEPROM
            Power_Control_Save_Configuration();
          }
        } break;

        case CMD_SET_CALLSIGN: {
          // check optional data is less than limit
          if(optDataLen <= MAX_STRING_LENGTH) {
            // get callsign from frame
            char newCallsign[MAX_STRING_LENGTH + 1];
            memcpy(newCallsign, optData, optDataLen);
            newCallsign[optDataLen] = '\0';

            // update callsign
            System_Info_Set_Callsign(newCallsign);
            FOSSASAT_DEBUG_PRINTLN(newCallsign);
          }
        } break;

        case CMD_SET_SF_MODE: {
          // check optional data is exactly 1 byte
          if(Communication_Check_OptDataLen(1, optDataLen)) {
            // update spreading factor mode
            spreadingFactorMode = optData[0];
            Communication_Set_SpreadingFactor(spreadingFactorMode);
          }
        } break;

        case CMD_SET_MPPT_MODE: {
          // check optional data is exactly 2 bytes
          if(Communication_Check_OptDataLen(2, optDataLen)) {
            // load power config from EEPROM
            Power_Control_Load_Configuration();

            // update MPPT mode
            powerConfig.bits.mpptTempSwitchEnabled = optData[0];
            powerConfig.bits.mpptKeepAliveEnabled = optData[1];

            // save power config from EEPROM
            Power_Control_Save_Configuration();
          }
        } break;

        case CMD_SET_LOW_POWER_ENABLE: {
          // check optional data is exactly 1 byte
          if(Communication_Check_OptDataLen(1, optDataLen)) {
            // load power config from EEPROM
            Power_Control_Load_Configuration();

            // update low power enable flag
            powerConfig.bits.lowPowerModeEnabled = optData[0];

            // save power config from EEPROM
            Power_Control_Save_Configuration();
          }
        } break;

        case CMD_SET_RECEIVE_WINDOWS: {
        // check optional data is exactly 2 bytes
        if(Communication_Check_OptDataLen(2, optDataLen)) {
          // set FSK receive length
          Persistent_Storage_Write<uint8_t>(EEPROM_FSK_RECEIVE_LEN_ADDR, optData[0]);

          // set LoRa receive length
          Persistent_Storage_Write<uint8_t>(EEPROM_LORA_RECEIVE_LEN_ADDR, optData[1]);

          // check if there will be still some receive window open
          if((Persistent_Storage_Read<uint8_t>(EEPROM_LORA_RECEIVE_LEN_ADDR) == 0) && (Persistent_Storage_Read<uint8_t>(EEPROM_FSK_RECEIVE_LEN_ADDR) == 0)) {
            FOSSASAT_DEBUG_PRINT(F("Res FSK"));
            Persistent_Storage_Write<uint8_t>(EEPROM_FSK_RECEIVE_LEN_ADDR, FSK_RECEIVE_WINDOW_LENGTH);
          }
        }
        } break;

        case CMD_RECORD_SOLAR_CELLS: {
        // check optional data is exactly 3 bytes
        if(Communication_Check_OptDataLen(3, optDataLen)) {
          uint16_t numSamples = optData[0];

          // check number of samples is less than limit
          if(numSamples > 40) {
            FOSSASAT_DEBUG_PRINT(F(">40"));
            break;
          }

          // get sample period
          uint16_t period = 0;
          memcpy(&period, optData + 1, 2);
          FOSSASAT_DEBUG_PRINT(F("Rec"));

          // record all data
          uint8_t respoOptDataLen = 3 * numSamples;
          uint8_t respOptData[MAX_OPT_DATA_LENGTH];
          for(uint16_t i = 0; i < 3 * numSamples; i += 3) {
            // check if the battery is good enough to continue
            #ifdef ENABLE_INTERVAL_CONTROL
            if(!Power_Control_Check_Battery_Limit()) {
               // battery check failed, stop measurement and send what we have
               respoOptDataLen = i;
               break;
            }
            #endif

            // read voltages
            respOptData[i] = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_A_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
            respOptData[i + 1] = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_B_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);
            respOptData[i + 2] = Pin_Interface_Read_Voltage(ANALOG_IN_SOLAR_C_VOLTAGE_PIN) * (VOLTAGE_UNIT / VOLTAGE_MULTIPLIER);

            // wait for for the next measurement
            Power_Control_Delay(period * SLEEP_LENGTH_CONSTANT, true, true);
          }

          // send response
          Communication_Send_Response(RESP_RECORDED_SOLAR_CELLS, respOptData, respoOptDataLen);
        }
        } break;

        case CMD_ROUTE:
        // just transmit the optional data
        Communication_Transmit(optData, optDataLen);
        break;
  }
}

int16_t Communication_Send_Response(uint8_t respId, uint8_t* optData, size_t optDataLen, bool overrideModem) {
  /*FOSSASAT_DEBUG_PRINT("Communication_Send_Response ");
    FOSSASAT_DEBUG_PRINTLN(freeRam());
    FOSSASAT_DEBUG_DELAY(100);*/

  // get callsign from EEPROM
  uint8_t callsignLen = Persistent_Storage_Read<uint8_t>(EEPROM_CALLSIGN_LEN_ADDR);
  char callsign[MAX_STRING_LENGTH + 1];
  System_Info_Get_Callsign(callsign, callsignLen);

  // build response frame
  uint8_t len = FCP_Get_Frame_Length(callsign, optDataLen);
  uint8_t frame[MAX_RADIO_BUFFER_LENGTH];
  FCP_Encode(frame, callsign, respId, optDataLen, optData);

  // delay before responding
  FOSSASAT_DEBUG_DELAY(100);
  Power_Control_Delay(RESPONSE_DELAY, true);

  // send response
  int16_t state = Communication_Transmit(frame, len, overrideModem);

  return (state);
}

int16_t Communication_Transmit(uint8_t* data, uint8_t len, bool overrideModem) {
  /*FOSSASAT_DEBUG_PRINT("Communication_Transmit ");
    FOSSASAT_DEBUG_PRINTLN(freeRam());
    FOSSASAT_DEBUG_DELAY(100);*/

  // check transmit enable flag
#ifdef ENABLE_TRANSMISSION_CONTROL
  Power_Control_Load_Configuration();
  if (!powerConfig.bits.transmitEnabled) {
    FOSSASAT_DEBUG_PRINTLN(F("Tx 0 cmd"));
    return (ERR_TX_TIMEOUT);
  }
#endif

  // disable receive interrupt
  radio.clearDio1Action();

  // print frame for debugging
  FOSSASAT_DEBUG_PRINT(F("Send "));
  FOSSASAT_DEBUG_PRINTLN(len);
  FOSSASAT_DEBUG_PRINT_BUFF(data, len);

  // check if modem should be switched - required for transmissions with custom settings
  uint8_t modem = currentModem;
  FOSSASAT_DEBUG_PRINT(F("Use "));
  if (overrideModem) {
    FOSSASAT_DEBUG_WRITE(MODEM_LORA);
    FOSSASAT_DEBUG_PRINTLN(F(" (ovr)"));
    Communication_Set_Modem(MODEM_LORA);
  } else {
    FOSSASAT_DEBUG_WRITE(modem);
    FOSSASAT_DEBUG_PRINTLN();
  }

  // get timeout
  uint32_t timeout = 0;
  timeout = (float)500000;

  //  if(currentModem == MODEM_FSK) {
  //    timeout = (float)radio.getTimeOnAir(len) * 5.0;
  //  } else {
  //    timeout = (float)radio.getTimeOnAir(len) * 1.5;
  //  }
  FOSSASAT_DEBUG_PRINT(F("T/O="));
  FOSSASAT_DEBUG_PRINTLN(timeout);

  // start transmitting
  int16_t state = radio.startTransmit(data, len);
  if (state != ERR_NONE) {
    FOSSASAT_DEBUG_PRINT(F("TxErr"));
    FOSSASAT_DEBUG_PRINTLN(state);
    return (state);
  }

  // wait for transmission finish
  uint32_t start = micros();
  uint32_t lastBeat = 0;
  while (!digitalRead(RADIO_DIO1)) {
    // pet watchdog every second
    if (micros() - lastBeat > (uint32_t)WATCHDOG_LOOP_HEARTBEAT_PERIOD * (uint32_t)1000) {
      Pin_Interface_Watchdog_Heartbeat();

      // check whether voltage dropped below low power level
#ifdef ENABLE_INTERVAL_CONTROL
      if (powerConfig.bits.lowPowerModeActive) {
        // we're below low power level, stop the transmission
        FOSSASAT_DEBUG_PRINTLN(F("Tx 0 bat"));
        radio.standby();
        return (ERR_INVALID_DATA_RATE);
      }
#endif

      lastBeat = micros();
    }

    // check timeout
    if (micros() - start > timeout) {
      // timed out while transmitting
      radio.standby();
      Communication_Set_Modem(modem);
      FOSSASAT_DEBUG_PRINTLN(F("Tx t/o"));
      return (ERR_TX_TIMEOUT);
    }
  }

  // transmission done, set mode standby
  state = radio.standby();

  // restore modem
  if (overrideModem) {
    Communication_Set_Modem(modem);
  }

  // set receive ISR
  radio.setDio1Action(Communication_Receive_Interrupt);

  return (state);
}

bool Communication_Check_OptDataLen(uint8_t expected, uint8_t actual) {
  if (expected != actual) {
    // received length of optional data does not match expected
    return (false);
  }

  return (true);
}
