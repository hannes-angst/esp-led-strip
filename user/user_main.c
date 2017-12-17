/* main.c -- MQTT client example
 *
 * Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Redis nor the names of its contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <user_interface.h>
#include <ets_sys.h>
#include <osapi.h>
#include <mem.h>
#include "debug.h"
#include "mqtt.h"
#include "wifi.h"
#include "pwm.c"

#define PWM_CHANNELS 3

#define CHANNEL_BLUE   	2
#define CHANNEL_GREEN  	1
#define CHANNEL_RED   	0

#define PWM_CHANNELS 3
const uint32_t period = 5000; // * 200ns ^= 1 kHz

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
	len += os_sprintf(dataBuf + len, "{\"name\":\"%s\"", APP_NAME);
	len += os_sprintf(dataBuf + len, ",\"version\":\"%s\"", APP_VERSION);

	uint8 mac_addr[6];
	if (wifi_get_macaddr(0, mac_addr)) {
		len += os_sprintf(dataBuf + len,
				",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"", mac_addr[0],
				mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
				mac_addr[5]);
	}

	struct ip_info info;
	if (wifi_get_ip_info(0, &info)) {
		len += os_sprintf(dataBuf + len, ",\"ip\":\"%d.%d.%d.%d\"",
				IP2STR(&info.ip.addr));
	}

	len += os_sprintf(dataBuf + len, ",\"type\":\"%s\"", MQTT_CLIENT_TYPE);
	len += os_sprintf(dataBuf + len, ",\"base\":\"%s%08X/\"}", MQTT_TOPIC_BASE,
			system_get_chip_id());

	MQTT_Publish(client, topicBuf, dataBuf, len, 1, 0);

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

	//participate in discovery requests
	MQTT_Subscribe(client, MQTT_DISCOVER, 2);

	//Tell Online status
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(),
	MQTT_STATUS);
	MQTT_Publish(client, topicBuf, MQTT_STATUS_ONLINE,
			strlen(MQTT_STATUS_ONLINE), 1, 1);

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

	pwm_set_duty(r * period / 255, CHANNEL_RED);
	pwm_set_duty(g * period / 255, CHANNEL_GREEN);
	pwm_set_duty(b * period / 255, CHANNEL_BLUE);
	pwm_start();

	INFO("Red   to %02X.\r\n", r);
	INFO("Green to %02X.\r\n", g);
	INFO("Blue  to %02X.\r\n", b);

}

void ICACHE_FLASH_ATTR user_light_init(void) {
	// initial duty: all off
	uint32 pwm_duty_init[PWM_CHANNELS] = { 0, 0, 0 };
	pwm_init(period, pwm_duty_init, PWM_CHANNELS, io_info);
	pwm_start();
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
	} else {
		setColor(dataBuf);
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR print_info() {
	INFO("\r\n\r\n[INFO] BOOTUP...\r\n");
	INFO("[INFO] SDK: %s\r\n", system_get_sdk_version());
	INFO("[INFO] Chip ID: %08X\r\n", system_get_chip_id());
	INFO("[INFO] Memory info:\r\n");
	system_print_meminfo();

	INFO("[INFO] -------------------------------------------\n");
	INFO("[INFO] %s v.%s \n", APP_NAME, APP_VERSION);
	INFO("[INFO] -------------------------------------------\n");

}


static void ICACHE_FLASH_ATTR app_init(void) {
	print_info();

	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS,
	MQTT_KEEPALIVE, MQTT_CLEAN_SESSION);

	char *topicBuf = (char*) os_zalloc(256);
	os_sprintf(topicBuf, "%s%08X/%s", MQTT_TOPIC_BASE, system_get_chip_id(), MQTT_STATUS);
	MQTT_InitLWT(&mqttClient, topicBuf, MQTT_STATUS_OFFLINE, 2, 1);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	WIFI_Connect(STA_SSID, STA_PASS, wifiConnectCb);
	os_free(topicBuf);

	user_light_init();


}

void user_init(void) {
	system_init_done_cb(app_init);
}
