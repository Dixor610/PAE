
#define ANALOG_IN_SOLAR_A_VOLTAGE_PIN                   A0          /*!< PC0 */
#define ANALOG_IN_SOLAR_B_VOLTAGE_PIN                   A7          /*!< ADC7 */
#define ANALOG_IN_SOLAR_C_VOLTAGE_PIN                   A2          /*!< PC2 */
#define ANALOG_IN_RANDOM_SEED                           A6          /*!< ADC6; used as source for randomSeed(), should be left floating */
#define DIGITAL_OUT_MPPT_PIN                            5           /*!< PB2 */
#define DIGITAL_OUT_MOSFET_1                            9           /*!< PB1 */
#define DIGITAL_OUT_MOSFET_2                            8           /*!< PB0 */
#define DIGITAL_OUT_WATCHDOG_HEARTBEAT                  4           /*!< PD4 */
#define RADIO_NSS                                       7           /*!< PD7 */
#define RADIO_DIO1                                      2           /*!< PD2 */
#define RADIO_BUSY                                      6           /*!< PD6 */
#define RADIO_NRST                                      RADIOLIB_NC /*!< Not connected*/


#define LORA_CARRIER_FREQUENCY                          433         /*!< MHz */
#define LORA_BANDWIDTH                                  125.0       /*!< kHz dual sideband */
#define LORA_SPREADING_FACTOR                           8
#define LORA_SPREADING_FACTOR_ALT                       8
#define LORA_CODING_RATE                                5           /*!< 4/5, Extended Hamming */
#define LORA_OUTPUT_POWER                               20          /*!< dBm */
#define LORA_PREAMBLE_LENGTH                            8           /*!< symbols */
#define LORA_CURRENT_LIMIT                              140.0       /*!< mA */
#define LORA_GAIN                                       1.6


extern volatile bool interruptsEnabled;                             /*!< Flag to signal interrupts enabled/disabled. */
extern volatile bool dataReceived;                                  /*!< Flag to signal data was received from ISR. */
extern uint8_t currentModem;                                        /*!< Current modem configuration. */
extern uint8_t spreadingFactorMode;                                 /*!< Current spreading factor mode. */
extern uint32_t lastHeartbeat;                                      /*!< Timestamp for the watchdog. */
extern INA226 ina;                                                  /*!< INA226 object. */
extern SX1278 radio;                                                /*!< SX1268 object. */
extern MorseClient morse;                                           /*!< MorseClient object. */
extern const char* password;										                    /*!< Transmission password (AES). */
extern const uint8_t encryptionKey[];								                /*!< Encryption key (AES). */


//GPS Variables
extern byte Type[4];
extern String comprobacio_Type = "";
extern String hora = "";
extern String lat_strg = "";
extern String lng_strg = "";
extern char dada_inici = 'B';
extern char dada = 'A';
extern char lat_orientation;
extern char lng_orientation;
extern char tipus;
extern int lectura_complerta = 0;
extern int32_t lat;
extern int32_t lng;
extern uint32_t hora_unix;
extern int commes = 0;
extern int dadaint = 0;
