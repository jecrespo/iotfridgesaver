/**
* @file ConfigData.h
* \~English
* @brief General configuration settings
*
* \~Spanish
* @brief Parámetros generales de configuración
*/
#pragma once
#ifndef _CONFIGDATA_h
#define _CONFIGDATA_h

//#define TEST          ///<\~English Enables test mode to use simulated data without using real sensors \~Spanish Habilita el modo test para usar datos simulados si no tengo sensores
#define DEBUG_ENABLED   ///<\~Spanish Activa la salida de debug por Telnet
#define DEBUG_SERIAL    ///<\~Spanish Activa la salida de debug por puerto serie
#define WIFI_MANAGER    ///<\~Spanish Activa la configuración con WiFiManager
#define MQTT_POWER_INPUT            ///<\~Spanish Activa la recepción de la medida de potencia por MQTT_POWER_INPUT
//#define EMONLIB       ///<\~English If enabled it uses Emonlib library to measure power using a clamp sensor \~Spanish Debe estar activado si se usa una pinza CT030 con EmonLib

#ifndef WIFI_MANAGER
const char* WIFI_SSID = "NOMBRE_DE_MI_RED"; ///<\~Spanish Nombre de la red WiFi. Solo si no se usa WiFiManager
const char* WIFI_PASS = "CONTRASEÑA";       ///<\~Spanish Contraseña de la red WiFi. Solo si no se usa WiFiManager
#endif // WIFI_MANAGER


#define FAN_ENABLE_BUTTON   D3      ///<\~Spanish Pin donde conectar el botón para activar el ventilador
#define FAN_PWM_PIN         D1      ///<\~Spanish Pin para el control de la velocidad del ventilador
#define MEASURE_PERIOD      60000   ///<\~Spanish Periodo de medida de temperatura y consumo en milisegundos, y envío de los datos al servicio en la nube

const int t_longPress =     10000;   ///<\~Spanish Tiempo en milisegundos para identificar pulsación larga

const int fanThreshold =    60;     ///<\~Spanish Valor en vatios por encima del cual se enciende el ventilador



#endif