/*
 * tempus-redux - ESP32 "Time from NPL" (MSF) Radio clock signal generator
 * Copyright 2024  Simon Arlott
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clockson/network.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <sys/time.h>

#include <cstdio>
#include <cstring>
#include <chrono>

using namespace std::chrono_literals;

namespace clockson {

uint64_t Network::time_sync_us_{0};

Network::Network() {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	ESP_ERROR_CHECK(esp_netif_set_hostname(netif, CONFIG_CLOCKSON_DHCP_HOSTNAME));

	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();

	init_cfg.nvs_enable = false;

	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

	esp_sntp_config_t sntp_cfg{};

	sntp_cfg.server_from_dhcp = true;
	sntp_cfg.start = true;
	sntp_cfg.sync_cb = &network::time_synced;

	ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_cfg));

	wifi_config_t wifi_cfg{};

	wifi_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
	std::strncpy(reinterpret_cast<char*>(&wifi_cfg.sta.ssid),
		CONFIG_CLOCKSON_WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
	std::snprintf(reinterpret_cast<char*>(&wifi_cfg.sta.password),
		sizeof(wifi_cfg.sta.password), "%s", CONFIG_CLOCKSON_WIFI_PASSWORD);
	wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
	wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
	std::snprintf(reinterpret_cast<char*>(&wifi_cfg.sta.sae_h2e_identifier),
		sizeof(wifi_cfg.sta.sae_h2e_identifier), "%s", CONFIG_CLOCKSON_WIFI_PASSWORD);

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	if (CONFIG_CLOCKSON_WIFI_SSID[0]) {
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
		ESP_LOGI(TAG, "WiFi configured: %s", CONFIG_CLOCKSON_WIFI_SSID);

		esp_event_handler_instance_t instance_any_id;
		esp_event_handler_instance_t instance_got_ip;
		ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
			ESP_EVENT_ANY_ID, &network::event_handler, this, &instance_any_id));
		ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
			IP_EVENT_STA_GOT_IP, &network::event_handler, this, &instance_got_ip));

		ESP_ERROR_CHECK(esp_wifi_start());
	} else {
		ESP_LOGI(TAG, "WiFi unconfigured");
	}
}

namespace network {

void event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {
	reinterpret_cast<Network*>(arg)->event_handler(event_base, event_id, event_data);
}

} // namespace network

void Network::event_handler(esp_event_base_t event_base, int32_t event_id,
		void *event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG, "WiFi start");
		ESP_ERROR_CHECK(esp_wifi_connect());
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		ESP_LOGI(TAG, "WiFi disconnected");
		ESP_ERROR_CHECK(esp_wifi_connect());
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "WiFi IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
		sntp_restart();
	}
}

namespace network {

void time_synced(struct timeval *tv) {
	Network::time_synced(tv);
}

} // namespace network

void Network::time_synced(struct timeval *tv) {
	time_sync_us_ = esp_timer_get_time();
	ESP_LOGI(TAG, "SNTP sync: %llu.%06lu", (unsigned long long)tv->tv_sec,
		(unsigned long)tv->tv_usec);
}

bool Network::time_ok() {
	return time_ok(nullptr);
}

bool Network::time_ok(uint64_t *time_sync_us_out) {
	uint64_t now = esp_timer_get_time();
	uint64_t time_sync_us = time_sync_us_;

	if (time_sync_us_out) {
		*time_sync_us_out = time_sync_us;
	}

	return time_sync_us > 0
		&& (now - time_sync_us < (uint64_t)std::chrono::microseconds(3h).count());
}

} // namespace clockson
