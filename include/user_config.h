#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_OPTIMIZE_PRINTF

#define LOCAL_CONFIG_AVAILABLE


#define DEBUG_ON 1

//Disable SSL
//#define MQTT_SSL_ENABLE


#define APP_NAME        "Remote LED Stripe"
#define APP_VERSION     "1.0.0-SNAPSHOT"

//#define DEBUG_ON		//Enable debug
#define MQTT_HOST     	"192.168.13.100" //or "mqtt.yourdomain.com"
#define MQTT_PORT     	1883
#define MQTT_BUF_SIZE   1024
#define MQTT_KEEPALIVE  120  /*second*/

#define MQTT_DISCOVER		"/angst/devices/discovery/"

#define MQTT_TOPIC_BASE		"/angst/devices/"
#define MQTT_CLIENT_TYPE    "led"

#define MQTT_STATUS    		"status"

#define MQTT_STATUS_ONLINE  "online"
#define MQTT_STATUS_OFFLINE  "offline"

#define MQTT_CLIENT_ID    	"LED"
#define MQTT_USER     		""
#define MQTT_PASS     		""

#define MQTT_CLEAN_SESSION 1
#define MQTT_KEEPALIVE 120


#ifndef LOCAL_CONFIG_AVAILABLE
#error Please copy user_config.sample.h to user_config.local.h and modify your configurations
#else
#include "user_config.local.h"
#endif

#define MQTT_RECONNECT_TIMEOUT  5 /*second*/

#define DEFAULT_SECURITY  0
#define QUEUE_BUFFER_SIZE       2048

#define PROTOCOL_NAMEv311
//PROTOCOL_NAMEv31      /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/





#if defined(DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif




#endif

