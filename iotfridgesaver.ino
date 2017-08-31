#include "ConfigData.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>          // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "OTAhelper.h"
#include "WifiManagerSetup.h"

#define TEST

#define EMONLIB
#ifdef EMONLIB
#include "EmonLib.h"                    // https://github.com/openenergymonitor/EmonLib
EnergyMonitor emon1;                    ///< Instancia del monitor de consumo
#endif

bool OTAupdating;                       ///< Verdadero si se está haciendo una actualización OTA

#define NUMBER_OF_SENSORS 4             ///< Número de sensores esperados 
float temperatures[NUMBER_OF_SENSORS];  ///< Espacio para almacenar los valores de temperatura
uint8_t tempAmbient_idx;                ///< Índice del sensor que mide la temperatura ambiente
uint8_t tempRadiator_idx;               ///< Índice del sensor que mide la temperatura del radiador del frigorífico
uint8_t tempFridge_idx;                 ///< Índice del sensor que mide la temperatura del frigorífico
uint8_t tempFreezer_idx;                ///< Índice del sensor que mide la temperatura del congelador
uint8_t numberOfDevices = 0;            ///< Número de sensores detectados. Debería ser igual a NUMBER_OF_SENSORS

#define ONE_WIRE_BUS D5                 ///< Pin de datos para los sensores de temperatura. D5 = GPIO16
#define TEMPERATURE_PRECISION 11        ///< Número de bits con los que se calcula la temperatura
OneWire oneWire (ONE_WIRE_BUS);         ///< Instancia OneWire para comunicar con los sensores de temperatura
DallasTemperature sensors (&oneWire);   ///< Pasar el bus de datos como referencia
const int minTemperature = -100;        ///< Temperatura mínima válida

int fanOn = 0;                          ///< Velocidad del ventilador

bool shouldSaveConfig = false;          ///< Verdadero si WiFi Manager activa una configuración nueva
bool configLoaded = false;

String emonCMSserverAddress = "";  ///< Dirección del servidor EmonCMS
String emonCMSserverPath = "";
String emonCMSwriteApiKey = ""; ///< API key del usuario
int mainsVoltage = 230;       ///< Tensión de alimentación
const char *configFileName = "/config.json";


/********************************************//**
*  Función para buscar la posición del valor máximo en el array de temperaturas
***********************************************/
int MaxValue (float *temperatures, uint8_t size) {

    float max_v = minTemperature;
    int max_i = 0;

    for (int i = 0; i < size; i++) {
        if (temperatures[i] > max_v) {
            max_v = temperatures[i];
            max_i = i;
        }
    }
    return max_i;

}


/********************************************//**
*  Funcion para ordenar los sensores de temperatura

*  Ordena, de mayor a menor, los índices de los sensores según la secuencia
*       Radiador, Ambiente, Frigorífico, Congelador
***********************************************/
void sortSensors () {
    //sensors.setWaitForConversion (false);  // makes it async
    int max_i = 0;

#ifndef TEST
    sensors.requestTemperatures ();


    //  script to automatically allocate the postion of each sensor based on their temperature

    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        temperatures[i] = sensors.getTempCByIndex (i);
    }
#else
    temperatures[0] = 30;
    temperatures[1] = 35;
    temperatures[2] = 6;
    temperatures[3] = -18;
#endif
    max_i = MaxValue (temperatures, NUMBER_OF_SENSORS);
    tempRadiator_idx = max_i;
    temperatures[max_i] = minTemperature;

    max_i = MaxValue (temperatures, NUMBER_OF_SENSORS);
    tempAmbient_idx = max_i;
    temperatures[max_i] = minTemperature;

    max_i = MaxValue (temperatures, NUMBER_OF_SENSORS);
    tempFridge_idx = max_i;
    temperatures[max_i] = minTemperature;

    max_i = MaxValue (temperatures, NUMBER_OF_SENSORS);
    tempFreezer_idx = max_i;
    //  temperatures[max_i]= minTemperature;


#ifdef DEBUG_ENABLED
    Serial.print ("Posicion sensor radiador: ");
    Serial.println (tempRadiator_idx);
    Serial.print ("Posicion sensor ambiente: ");
    Serial.println (tempAmbient_idx);
    Serial.print ("Posicion sensor frigorifico: ");
    Serial.println (tempFridge_idx);
    Serial.print ("Posicion sensor congelador: ");
    Serial.println (tempFreezer_idx);

#endif


}


/********************************************//**
*  Función para iniciar los sensores de temperatura
***********************************************/
uint8_t initTempSensors () {
    DeviceAddress tempDeviceAddress;    ///< Almacenamiento temporal para las direcciones encontradas
    uint8_t numberOfDevices;            ///< Número de sensores encontrados

#ifdef DEBUG_ENABLED
    Serial.println ("Init Dallas Temperature Control Library");
#endif

    // Inicializar el bus de los sensores de temperatura 
    sensors.begin ();

    // Preguntar por el número de sensores detectados
    numberOfDevices = sensors.getDeviceCount ();

#ifdef DEBUG_ENABLED
    Serial.print ("Locating devices...");
    Serial.print ("Found ");
    Serial.print (numberOfDevices, DEC);
    Serial.println (" devices.");

    Serial.print ("Parasite power is: ");
    if (sensors.isParasitePowerMode ())
        Serial.println ("ON");
    else
        Serial.println ("OFF");
#endif

    // Ajustar la precisión de todos los sensores
    sensors.setResolution (TEMPERATURE_PRECISION);

#ifdef DEBUG_ENABLED
    // Imprime la dirección de cada sensor
    for (int i = 0; i < numberOfDevices; i++) {
        // Search the wire for address
        if (sensors.getAddress (tempDeviceAddress, i)) {

            Serial.print ("Found device ");
            Serial.print (i, DEC);
            Serial.print ("Setting resolution to ");
            Serial.println (TEMPERATURE_PRECISION, DEC);

            Serial.print ("Resolution actually set to: ");
            Serial.print (sensors.getResolution (tempDeviceAddress), DEC);
            Serial.println ();


        } else {

            Serial.print ("Found ghost device at ");
            Serial.print (i, DEC);
            Serial.print (" but could not detect address. Check power and cabling");

        }

    }
#endif // DEBUG

    return numberOfDevices;
}

void configModeCallback () {
#ifdef DEBUG_ENABLED
    Serial.printf ("%s: Guardar configuración\n", __FUNCTION__);
#endif // DEBUG_ENABLED
    shouldSaveConfig = true;
}

void getCustomData (MyWiFiManager &wifiManager) {
    //	Obtener datos de WifiManager
    emonCMSserverAddress = wifiManager.getEmonCMSserverAddress ();
    emonCMSwriteApiKey = wifiManager.getEmonCMSwriteApiKey ();
    mainsVoltage = wifiManager.getMainsVoltage ();

#ifdef DEBUG_ENABLED
    Serial.printf ("Servidor: %s\n", emonCMSserverAddress.c_str ());
    Serial.printf ("API Key: %s\n", emonCMSwriteApiKey.c_str ());
    Serial.printf ("Tensión: %d\n", mainsVoltage);
#endif // DEBUG_ENABLED

    if (mainsVoltage == 0) {
#ifdef DEBUG_ENABLED
        Serial.println ("ERROR. Tensión debe ser un número");
#endif // DEBUG_ENABLED
        wifiManager.resetSettings ();
        delay (1000);
        ESP.reset ();
    }

}

void loadConfigData () {
    //clean FS, for testing
    //SPIFFS.format();

    //read configuration from FS json
    Serial.println ("Montando sistema de archivos...");

    if (SPIFFS.begin ()) {
        Serial.printf ("Sistema de archivos montado. Abriendo %s\n", configFileName);
        if (SPIFFS.exists (configFileName)) {
            //file exists, reading and loading
            Serial.printf ("Leyendo el archivo de configuración: %s\n", configFileName);
            File configFile = SPIFFS.open (configFileName, "r");
            if (configFile) {
                Serial.println ("Archivo de configuración abierto");
                size_t size = configFile.size ();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr<char[]> buf (new char[size]);

                configFile.readBytes (buf.get (), size);
                DynamicJsonBuffer jsonBuffer;
                JsonObject& json = jsonBuffer.parseObject (buf.get ());
                json.printTo (Serial);
                if (json.success ()) {
                    Serial.println ("\nparsed json");

                    //strcpy (mqtt_server, json["mqtt_server"]);
                    emonCMSserverAddress = json.get<String> ("emonCMSserver");
                    emonCMSserverPath = json.get<String> ("emonCMSpath");
                    emonCMSwriteApiKey = json.get<String> ("emonCMSapiKey");
                    mainsVoltage = json.get<int> ("mainsVoltage");

#ifdef DEBUG_ENABLED
                    Serial.printf ("emonCMSserverAddress: %s\n", emonCMSserverAddress.c_str ());
                    Serial.printf ("emonCMSserverPath: %s\n", emonCMSserverPath.c_str ());
                    Serial.printf ("emonCMSwriteApiKey: %s\n", emonCMSwriteApiKey.c_str ());
                    Serial.printf ("mainsVoltage: %d\n", mainsVoltage);
#endif // DEBUG_ENABLED

                    //strcpy (mqtt_port, json["mqtt_port"]);
                    //strcpy (blynk_token, json["blynk_token"]);
                    configLoaded = true;
                } else {
                    Serial.println ("Error al leer el archivo de configuración");
                }
            } else {
                Serial.println ("Error al abrir el archivo de configuración");
            }
        } else {
            Serial.println ("El archivo de configuración no existe");
        }

        SPIFFS.end ();
    } else {
        Serial.println ("Error al abrir el sistema de archivos. Formateando");
        SPIFFS.format ();
        SPIFFS.end ();
        ESP.reset ();
    }
    //end read

}

void saveConfigData () {
    Serial.println ("saving config");
    if (SPIFFS.begin ()) {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.createObject ();
        json["emonCMSserver"] = emonCMSserverAddress;
        json["emonCMSpath"] = emonCMSserverPath;
        json["emonCMSapiKey"] = emonCMSwriteApiKey;
        json["mainsVoltage"] = mainsVoltage;

        File configFile = SPIFFS.open (configFileName, "w");
        if (!configFile) {
            Serial.println ("Error al abrir el archivo de configuración");
        }

        json.prettyPrintTo (Serial);
        Serial.println ();
        json.printTo (configFile);
        configFile.close ();
    } else {
        Serial.printf ("Error al montar el sistema de archivos");
    }
    SPIFFS.end ();
    //end save
}

void startWifiManager (MyWiFiManager &wifiManager) {

    wifiManager.setSaveConfigCallback (configModeCallback);
    wifiManager.init ();
}

/********************************************//**
*  Setup
***********************************************/
void setup () {
    MyWiFiManager wifiManager;              ///< WiFi Manager. Configura los datos WiFi y otras configuraciones

    Serial.begin (115200);
    // Control del ventilador
    pinMode (D3, INPUT_PULLUP); // D3 = GPIO0. Botón para controlar la activación del ventilador
    analogWrite (D1, 0); // D1 = GPIO5. Pin PWM para controlar el ventilador

#ifdef WIFI_MANAGER
    //leer datos de la flash
    configFileName = "\config.json";
    loadConfigData ();

    // Si no se han configurado los datos del servidor borrar la configuración
    if (emonCMSserverAddress == "" || emonCMSserverPath == "" || emonCMSwriteApiKey == "") {
        wifiManager.resetSettings ();
    }

    startWifiManager (wifiManager);

    //Si hay que guardar
    if (shouldSaveConfig) {
        getCustomData (wifiManager);

        //	guardar datos en flash
        saveConfigData ();
    }

#else // WIFI_MANAGER
    //--------------------- Conectar a la red WiFi -------------------------
    WiFi.begin (WIFI_SSID, WIFI_PASS);

    Serial.printf ("Conectando a la red %s ", WIFI_SSID);

    while (!WiFi.isConnected ()) {
        Serial.print (".");
        delay (500);
    }
    //----------------------------------------------------------------------
#endif // WIFI_MANAGER
    Serial.printf ("Conectado!!! IP: %s\n", WiFi.localIP ().toString ().c_str ());

#ifdef DEBUG_ENABLED
    Serial.printf ("emonCMSserverAddress: %s\n", emonCMSserverAddress.c_str ());
    Serial.printf ("emonCMSserverPath: %s\n", emonCMSserverPath.c_str ());
    Serial.printf ("emonCMSwriteApiKey: %s\n", emonCMSwriteApiKey.c_str ());
    Serial.printf ("mainsVoltage: %d\n", mainsVoltage);
#endif // DEBUG_ENABLED

    MDNS.begin ("FridgeSaverMonitor"); //Iniciar servidor MDNS para llamar al dispositivo como FridgeSaverMonitor.local

    OTASetup ();

    //Iniciar RemoteDebug

    //Iniciar NTP

#ifndef TEST
   // Iniciar el sensor de corriente
#ifdef EMONLIB
    emon1.current (A0, 3.05);             // Current: input pin, calibration.
#endif

    // Iniciar sensores de temperatura comprobando que el número de sensores es el necesario
    while (numberOfDevices != NUMBER_OF_SENSORS) {
        numberOfDevices = initTempSensors ();
        if (numberOfDevices != NUMBER_OF_SENSORS) {
            Serial.printf ("Error en el numero de sensores: %d\n", numberOfDevices);
            delay (1000);
        }
    }

#endif
    sortSensors (); // Asignar los sensores automáticamente
}

/********************************************//**
*  Función para obtener la medida de consumo en Vatios
***********************************************/
double getPower () {
    double watts;

#ifdef EMONLIB
    watts = emon1.calcIrms (1480) * mainsVoltage;  // Calculate Irms * V. Aparent power
#endif

#ifdef EMONLIB
    if (digitalRead (D3) && watts > fanThreshold) { // Si el consumo > 60W
#else
    if (digitalRead (D3)) {
#endif
        analogWrite (D1, 850); // Encender ventilador
        fanOn = 850;
    } else {
        analogWrite (D1, 0);
        fanOn = 0;
    }

    return watts;
}

/********************************************//**
*  Envía los datos a la plataforma de EmonCMS
***********************************************/
int8_t sendDataEmonCMS (float tempRadiator,
                        float tempAmbient, 
                        float tempFridge, 
                        float tempFreezer, 
                        double watts,
                        int fanOn) {

    WiFiClientSecure client; ///< Cliente TCP con SSL
    const unsigned int maxTimeout = 5000; ///< Tiempo maximo de espera a la respuesta del servidor
    unsigned long timeout; ///< Contador para acumilar el tiempo de espera
    char* tempStr; ///< Cadena temporal para almacenar los numeros como texto

    // Conecta al servidor
    if (!client.connect (emonCMSserverAddress.c_str (), 443)) {
#ifdef DEBUG_ENABLED
        Serial.printf ("Error al conectar al servidor EmonCMS en %s\n", emonCMSserverAddress.c_str ());
#endif
        return -1; // Error de conexión
    }

    // Compone la peticion HTTP
    String httpRequest = "GET "+ emonCMSserverPath +"/input/post.json?node=IoTFridgeSaver&fulljson={";
    dtostrf (tempRadiator, 3, 3, tempStr);
    httpRequest += "\"tempRadiator\":" + String (tempStr) + ",";
    dtostrf (tempAmbient, 3, 3, tempStr);
    httpRequest += "\"tempAmbient\":" + String (tempStr) + ",";
    dtostrf (tempFridge, 3, 3, tempStr);
    httpRequest += "\"tempFridge\":" + String (tempStr) + ",";
    dtostrf (tempFreezer, 3, 3, tempStr);
    httpRequest += "\"tempFreezer\":" + String (tempStr) + ",";
    dtostrf (watts, 3, 3, tempStr);
    httpRequest += "\"watts\":" + String (tempStr) + ",";
    httpRequest += "\"watts\":" + fanOn;
    httpRequest += ",}&apikey=" + emonCMSwriteApiKey + " HTTP/1.1\r\n";
    httpRequest += "Host: " + emonCMSserverAddress + "\r\n\r\n";

#ifdef DEBUG_ENABLED
    Serial.printf ("%s: Request; ->\n %s\n", __FUNCTION__, httpRequest.c_str ());
#endif \\ DEBUG_ENABLED

    // Envia la peticion
    client.print (httpRequest);

    // Espera la respuesta
    timeout = millis ();
    while (!client.available ()) {
        if (millis () - timeout > maxTimeout) {
#ifdef DEBUG_ENABLED
            Serial.printf ("%s: Firebase client Timeout !", __FUNCTION__);
#endif
            client.stop ();
            return -2; // Timeout
        }
    }

    // Recupera la respuesta
    while (client.available ()) {
        String line = client.readStringUntil ('\n');
#ifdef DEBUG_ENABLED
        Serial.printf ("%s: Response: %s\n", __FUNCTION__, line.c_str ());
#endif
    }

    client.stop (); // Desconecta el cliente

#ifdef DEBUG_ENABLED
    Serial.printf ("%s: Data sent !!!\n", __FUNCTION__);
#endif // DEBUG

    return 0; // OK
}

/********************************************//**
*  Bucle principal
***********************************************/
void loop () {
    static long lastRun = 0;
    double watts;

    ArduinoOTA.handle ();

    if (millis () - lastRun > MEASURE_PERIOD) {
        lastRun = millis ();
#ifndef TEST
        sensors.requestTemperatures ();
        temperatures[tempRadiator_idx] = sensors.getTempCByIndex (tempRadiator_idx);
        temperatures[tempAmbient_idx] = sensors.getTempCByIndex (tempAmbient_idx);
        temperatures[tempFridge_idx] = sensors.getTempCByIndex (tempFridge_idx);
        temperatures[tempFreezer_idx] = sensors.getTempCByIndex (tempFreezer_idx);

        watts = getPower ();
#else
        temperatures[tempRadiator_idx] = random (35, 40);
        temperatures[tempAmbient_idx] = random (25, 30);
        temperatures[tempFridge_idx] = random (0, 10);
        temperatures[tempFreezer_idx] = random (-20, -15);
        watts = random (0,100);
#endif

#ifdef DEBUG_ENABLED
        Serial.printf ("Temperatura radiador: %f\n", temperatures[tempRadiator_idx]);
        Serial.printf ("Temperatura ambiente: %f\n", temperatures[tempAmbient_idx]);
        Serial.printf ("Temperatura frigorifico: %f\n", temperatures[tempFridge_idx]);
        Serial.printf ("Temperatura congelador: %f\n", temperatures[tempFreezer_idx]);
        Serial.printf ("Consumo: %f\n", watts);
#endif // DEBUG_ENABLED

        sendDataEmonCMS (temperatures[tempRadiator_idx], temperatures[tempAmbient_idx], temperatures[tempFridge_idx], temperatures[tempFreezer_idx], watts, fanOn);

    }


}