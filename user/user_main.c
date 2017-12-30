#include <user_interface.h>
#include <ets_sys.h>
#include <osapi.h>

#include <mem.h>
#include "debug.h"
#include "mqtt.h"
#include "wifi.h"
#include "info.h"
#include "pwm.c"


#define PWM_CHANNELS 3

#define CHANNEL_BLUE   	2
#define CHANNEL_GREEN  	1
#define CHANNEL_RED   	0

const uint32_t MAX_PERIOD = 5000; // * 200ns ^= 1 kHz

// PWM setup
uint32 io_info[PWM_CHANNELS][3] = {
    // MUX, FUNC, PIN
    {PERIPHS_IO_MUX_MTDI_U,  FUNC_GPIO12, 12},
    {PERIPHS_IO_MUX_MTCK_U,  FUNC_GPIO13, 13},
    {PERIPHS_IO_MUX_MTMS_U,  FUNC_GPIO14, 14},
};

MQTT_Client mqttClient;
static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
	if (status == STATION_GOT_IP) {
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

static void ICACHE_FLASH_ATTR sendDeviceInfo(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;

	char *topicBuf = (char*) os_zalloc(128);
	char *dataBuf = (char*) os_zalloc(1024);
	//Tell Online status
	os_sprintf(topicBuf, "%s%08X/info", MQTT_TOPIC_BASE, system_get_chip_id());

	int len = 0;
	len += os_sprintf(dataBuf + len, "{\"name\":\"led\"");
	len += os_sprintf(dataBuf + len, ",\"app\":\"%s\"", APP_NAME);
	len += os_sprintf(dataBuf + len, ",\"version\":\"%d.%d.%d\"", APP_VER_MAJ, APP_VER_MIN, APP_VER_REV);

	uint8 mac_addr[6];
	if (wifi_get_macaddr(0, mac_addr)) {
		len += os_sprintf(dataBuf + len, ",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	}
	struct ip_info info;
	if (wifi_get_ip_info(0, &info)) {
		len += os_sprintf(dataBuf + len, ",\"ip\":\"%d.%d.%d.%d\"", IP2STR(&info.ip.addr));
	}
	len += os_sprintf(dataBuf + len, ",\"type\":\"%s\"", MQTT_CLIENT_TYPE);
	len += os_sprintf(dataBuf + len, ",\"base\":\"%s%08X/\"", MQTT_TOPIC_BASE, system_get_chip_id());
	len += os_sprintf(dataBuf + len, ",\"group\":\"%sled/wohnzimmer\"}", MQTT_TOPIC_BASE);
	MQTT_Publish(client, topicBuf, dataBuf, len, 0, 0);

	os_free(topicBuf);
	os_free(dataBuf);
}

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Connected\r\n");

	char *topicBuf = (char*) os_zalloc(256);

	//C&C
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_CLIENT_TYPE);
	MQTT_Subscribe(client, topicBuf, 2);

	os_sprintf(topicBuf, "%sled/wohnzimmer", MQTT_TOPIC_BASE);
	MQTT_Subscribe(client, topicBuf, 2);

	//participate in discovery requests
	MQTT_Subscribe(client, MQTT_DISCOVER, 2);

	//Tell Online status
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_STATUS);
	MQTT_Publish(client, topicBuf, MQTT_STATUS_ONLINE, strlen(MQTT_STATUS_ONLINE), 1, 1);

	os_free(topicBuf);
}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Disconnected\r\n");
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Published\r\n");
}

static u32 readDigit(char value) {
	if (value < 48) {
		return 0;
	}

	// 0...9
	if (value <= 57) {
		return value - 48;
	}

	// between digits
	if (value > 57 && value < 64) {
		return 9;
	}

	if (value > 70) {
		return 15;
	}

	return value - 55;
}


void ICACHE_FLASH_ATTR setColor(char* dataBuf) {
	uint32 r = readDigit(dataBuf[0]) * 16 + readDigit(dataBuf[1]);
	uint32 g = readDigit(dataBuf[2]) * 16 + readDigit(dataBuf[3]);
	uint32 b = readDigit(dataBuf[4]) * 16 + readDigit(dataBuf[5]);

	r = ( r > 255 ? 255 : r < 0 ? 0 : r);
	g = ( g > 255 ? 255 : g < 0 ? 0 : g);
	b = ( b > 255 ? 255 : b < 0 ? 0 : b);

	pwm_set_duty(r * MAX_PERIOD / 255, CHANNEL_RED);
	pwm_set_duty(g * MAX_PERIOD / 255, CHANNEL_GREEN);
	pwm_set_duty(b * MAX_PERIOD / 255, CHANNEL_BLUE);
	pwm_start();

	INFO("Red   to %02X.\r\n", r);
	INFO("Green to %02X.\r\n", g);
	INFO("Blue  to %02X.\r\n", b);

}

void ICACHE_FLASH_ATTR user_light_init(void) {
	// initial duty: all off
	uint32 pwm_duty_init[PWM_CHANNELS] = { 0, 0, 0 };
	pwm_init(MAX_PERIOD, pwm_duty_init, PWM_CHANNELS, io_info);
	pwm_start();
}

static ETSTimer fire_timer;
static uint8_t fireCounter=0;

static void ICACHE_FLASH_ATTR startFire() {
	os_timer_disarm(&fire_timer);
	fireCounter+=7;
	pwm_set_duty(MAX_PERIOD, CHANNEL_RED);
	pwm_set_duty((4 + (fireCounter % 10)) * MAX_PERIOD / 255, CHANNEL_GREEN);
	pwm_set_duty(0, CHANNEL_BLUE);
	pwm_start();

	os_timer_setfn(&fire_timer, (os_timer_func_t *) startFire, NULL);
	os_timer_arm(&fire_timer, 100, 0);
}
static void ICACHE_FLASH_ATTR stopFire() {
	os_timer_disarm(&fire_timer);
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic,
		uint32_t topic_len, const char *data, uint32_t data_len) {

	char *topicBuf = (char*) os_zalloc(topic_len + 1), *dataBuf =
			(char*) os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*) args;
	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;
	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if (os_strcmp(topicBuf, MQTT_DISCOVER) == 0) {
		sendDeviceInfo(args);
	} else if(os_strncmp(data, "fire", 4) == 0) {
		INFO("start fire\r\n");
		startFire();
	} else {
		stopFire();
		setColor(dataBuf);
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

static void ICACHE_FLASH_ATTR mqtt_init(void) {
	DEBUG("INIT MQTT\r\n");
	//If WIFI is connected, MQTT gets connected (see wifiConnectCb)
	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, SEC_NONSSL);

	char *clientId = (char*) os_zalloc(64);
	os_sprintf(clientId, "%s%08X", MQTT_CLIENT_ID, system_get_chip_id());

	char *id = (char*) os_zalloc(32);
	os_sprintf(id, "%08X", system_get_chip_id());

	//id will be copied by MQTT_InitLWT
	if (!MQTT_InitClient(&mqttClient, clientId, id, id, MQTT_KEEPALIVE, MQTT_CLEAN_SESSION)) {
		ERROR("Could not initialize MQTT client");
	}
	os_free(id);
	os_free(clientId);


	char *topicBuf = (char*) os_zalloc(256);
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_STATUS);
	MQTT_InitLWT(&mqttClient, topicBuf, MQTT_STATUS_OFFLINE, 2, 1);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	os_free(topicBuf);

}

static void ICACHE_FLASH_ATTR app_init(void) {
	print_info();
	mqtt_init();


	WIFI_Connect(STA_SSID, STA_PASS, wifiConnectCb);

	user_light_init();


}

void user_init(void) {
	system_init_done_cb(app_init);
}
