#include <user_interface.h>
#include <ets_sys.h>
#include <osapi.h>
#include <mem.h>

#include "user_config.h"
#include "debug.h"
#include "mqtt.h"
#include "wifi.h"
#include "info.h"
#include "pwm.c"
#include "parse.h"

static color_slots slots;
static uint8_t slotPos = 0;

static ETSTimer fire_timer;

static MQTT_Client mqttClient;

// PWM setup
static uint32 io_info[PWM_CHANNELS][3] = {
		// MUX, FUNC, PIN
		{ PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12, 12 },
		{ PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13, 13 },
		{ PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14, 14 }
};

static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
	if (status == STATION_GOT_IP) {
		INFO("Connected to AP.\r\n");
		MQTT_Connect(&mqttClient);
	} else if (status == STATION_WRONG_PASSWORD || status == STATION_NO_AP_FOUND || status == STATION_CONNECT_FAIL) {
		INFO("Error connecting to AP.\r\n");
	} else {
		INFO("Connecting to AP.\r\n");
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
	len += os_sprintf(dataBuf + len, ",\"group\":\""TOPIC_GROUP"\"}", MQTT_TOPIC_BASE);


	MQTT_Publish(client, topicBuf, dataBuf, len, 0, 0);

	os_free(topicBuf);
	//dataBuf freed by MQTT_Publish
}

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Connected\r\n");

	char *topicBuf = (char*) os_zalloc(256);

	//C&C
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_CLIENT_TYPE);
	MQTT_Subscribe(client, topicBuf, 0);

	os_sprintf(topicBuf, TOPIC_GROUP, MQTT_TOPIC_BASE);
	MQTT_Subscribe(client, topicBuf, 0);

	//participate in discovery requests
	MQTT_Subscribe(client, MQTT_DISCOVER, 0);

	//Tell Online status
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_STATUS);
	MQTT_Publish(client, topicBuf, MQTT_STATUS_ONLINE, strlen(MQTT_STATUS_ONLINE), 1, 1);

	os_free(topicBuf);
}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args) {
	INFO("MQTT: Disconnected\r\n");
	WIFI_Reconnect(STA_SSID, STA_PASS, wifiConnectCb);
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args) {
	INFO("MQTT: Published\r\n");
}

static void ICACHE_FLASH_ATTR setColor(uint8_t r, uint8_t g, uint8_t b) {
	pwm_set_duty(r*MAX_PERIOD/255, CHANNEL_RED);
	pwm_set_duty(g*MAX_PERIOD/255, CHANNEL_GREEN);
	pwm_set_duty(b*MAX_PERIOD/255, CHANNEL_BLUE);
	pwm_start();
}


static void ICACHE_FLASH_ATTR fire() {
	os_timer_disarm(&fire_timer);
	slotPos = (slotPos + 1) % slots.length;
	setColor(
			slots.slot[slotPos].r,
			slots.slot[slotPos].g,
			slots.slot[slotPos].b
	);

	os_timer_setfn(&fire_timer, (os_timer_func_t *) fire, NULL);
	os_timer_arm(&fire_timer, 100, 0);
}

static void ICACHE_FLASH_ATTR startFire() {
	if (slots.length > 1) {
		DEBUG("Starting timer (sz: %d).\r\n", slots.length);
		slotPos = slots.length - 1;
		fire();
	} else {
		DEBUG("Setting color %d, %d, %d (%d).\r\n", slots.slot[slotPos].r, slots.slot[slotPos].g, slots.slot[slotPos].b, slots.length);
		slotPos = 0;
		setColor(
				slots.slot[slotPos].r,
				slots.slot[slotPos].g,
				slots.slot[slotPos].b
		);
	}
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
	char *topicBuf = (char*) os_zalloc(topic_len + 1);
	char *dataBuf = (char*) os_zalloc(data_len + 1);

	MQTT_Client* client = (MQTT_Client*) args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if (os_strcmp(topicBuf, MQTT_DISCOVER) == 0) {
		sendDeviceInfo(args);
	} else {
		os_timer_disarm(&fire_timer);
		readColors(dataBuf, data_len, &slots);
		startFire();
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

	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}


void ICACHE_FLASH_ATTR user_light_init(void) {
	// initial duty: all off
	uint32 pwm_duty_init[PWM_CHANNELS] = { 0, 0, 0 };
	pwm_init(MAX_PERIOD, pwm_duty_init, PWM_CHANNELS, io_info);
	pwm_start();
}

static void ICACHE_FLASH_ATTR app_init(void) {
	slotPos = 0;
	slots.length = 0;

	user_light_init();
	print_info();
	mqtt_init();
	WIFI_Connect(STA_SSID, STA_PASS, wifiConnectCb);
}

void user_init(void) {
	system_init_done_cb(app_init);
}
