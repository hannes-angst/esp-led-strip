#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_OPTIMIZE_PRINTF

#define LOCAL_CONFIG_AVAILABLE


#define ERROR_LEVEL
#define WARN_LEVEL
#define INFO_LEVEL
#define DEBUG_LEVEL

//Disable SSL
//#define MQTT_SSL_ENABLE


#define APP_NAME        "Remote LED Stripe"
#define APP_VER_MAJ		1
#define APP_VER_MIN		1
#define APP_VER_REV		1

//#define DEBUG_ON		//Enable debug
#define MQTT_HOST     	"192.168.13.100" //or "mqtt.yourdomain.com"
#define MQTT_PORT     	1883
#define MQTT_BUF_SIZE   1024
#define MQTT_KEEPALIVE  60  /*second*/
#define MQTT_CLEAN_SESSION 		1

#define MQTT_DISCOVER		"/angst/devices/discovery/"

#define MQTT_TOPIC_BASE		"/angst/devices/"
#define MQTT_CLIENT_TYPE    "led"

#define MQTT_STATUS    		"status"

#define MQTT_STATUS_ONLINE  "online"
#define MQTT_STATUS_OFFLINE  "offline"

#define MQTT_CLIENT_ID    	"LED"

//Amount of PWM channels (R, G B)
#define PWM_CHANNELS 		3
#define PWM_MAX_CHANNELS 	3

// PWM channel to color association
#define CHANNEL_BLUE   	1
#define CHANNEL_GREEN  	2
#define CHANNEL_RED   	0

//PWM frequency
// * 200ns ^= 1 kHz
#define MAX_PERIOD 		5000

#ifndef LOCAL_CONFIG_AVAILABLE
#error Please copy user_config.sample.h to user_config.local.h and modify your configurations
#else
#include "user_config.local.h"
#endif


#ifndef TOPIC_GROUP

#define TOPIC_GROUP	"%sled/group"

#endif

#define MQTT_RECONNECT_TIMEOUT  5 /*second*/

#define DEFAULT_SECURITY  		0
#define QUEUE_BUFFER_SIZE       2048

#define PROTOCOL_NAMEv31
//PROTOCOL_NAMEv31      /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/





#define LOCAL_CONFIG_AVAILABLE

#ifndef LOCAL_CONFIG_AVAILABLE
#error Please copy user_config.sample.h to user_config.local.h and modify your configurations
#else
#include "user_config.local.h"
#endif


#ifdef ERROR_LEVEL
#define ERROR( format, ... ) os_printf( "[ERROR] " format, ## __VA_ARGS__ )
#else
#define ERROR( format, ... )
#endif

#ifdef WARN_LEVEL
#define WARN( format, ... ) os_printf( "[WARN] " format, ## __VA_ARGS__ )
#else
#define WARN( format, ... )
#endif

#ifdef INFO_LEVEL

#define INFO( format, ... ) os_printf( "[INFO] " format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif

#ifdef DEBUG_LEVEL
#define MQTT_DEBUG_ON 1
#define DEBUG( format, ... ) os_printf( "[DEBUG] " format, ## __VA_ARGS__ )
#else
#define DEBUG( format, ... )
#endif
#endif

