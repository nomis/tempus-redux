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
#include <netdb.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <vector>

using namespace std::chrono_literals;
using std::chrono::microseconds;

namespace clockson {

uint64_t Network::time_sync_us_{0};
bool Network::time_step_first_{true};
std::atomic<int> Network::time_slew_allowed_{0};
int Network::time_slew_current_{0};

Network::Network() {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());


	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	ESP_ERROR_CHECK(esp_netif_set_hostname(netif, CONFIG_CLOCKSON_DHCP_HOSTNAME));


	struct addrinfo hints{};

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
	hints.ai_protocol = IPPROTO_UDP;

	if (CONFIG_CLOCKSON_SYSLOG_IP_ADDRESS[0]) {
		struct addrinfo *res;

		int gai_ret = ::getaddrinfo(CONFIG_CLOCKSON_SYSLOG_IP_ADDRESS, "514", &hints, &res);

		if (gai_ret) {
			ESP_LOGE(TAG, "syslog getaddrinfo(): %d", gai_ret);
		} else {
			syslog_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

			if (syslog_ == -1) {
				ESP_LOGE(TAG, "syslog socket(): %d", syslog_);
			} else {
				int ret = ::connect(syslog_, res->ai_addr, res->ai_addrlen);

				if (ret) {
					ESP_LOGE(TAG, "syslog connect(): %d", ret);
					::close(syslog_);
					syslog_ = -1;
				}
			}

			::freeaddrinfo(res);
		}
	}


	wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();

	init_cfg.nvs_enable = false;

	ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
	ESP_ERROR_CHECK(esp_wifi_set_country_code("GB", true));


	esp_sntp_config_t sntp_cfg{};

	sntp_cfg.smooth_sync = true;
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
	wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
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
		wifi_event_sta_disconnected_t *data = reinterpret_cast<wifi_event_sta_disconnected_t*>(event_data);
		ESP_LOGI(TAG, "WiFi disconnected: %02x:%02x:%02x:%02x:%02x:%02x %u",
			data->bssid[0], data->bssid[1], data->bssid[2],
			data->bssid[3], data->bssid[4], data->bssid[5],
			data->reason);
		ESP_ERROR_CHECK(esp_wifi_connect());
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
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
		&& (now - time_sync_us < (uint64_t)microseconds(3h).count());
}

void Network::time_slew_next() {
	time_slew_allowed_++;
}

int Network::adjtime(const struct timeval *delta, struct timeval *outdelta) {
	if (delta != nullptr) {
		bool allowed = false;
		bool applied = false;
		int time_slew_copy = time_slew_allowed_;

		if (time_slew_copy != time_slew_current_) {
			time_slew_current_ = time_slew_copy;
			allowed = true;
		}

		if (delta->tv_sec != 0 || delta->tv_usec < LOWER_TIME_STEP_US
				|| delta->tv_usec >= UPPER_TIME_STEP_US || time_step_first_) {
			/* Outside permitted adjustment range */
			time_step_first_ = false;
			errno = EINVAL;
			return -1;
		} else if (delta->tv_usec != 0 && allowed) {
			struct timeval now{};

			if (::gettimeofday(&now, nullptr)) {
				return -1;
			}

			/*
			 * Limit maximum slew amount per transmission, which relies on the
			 * next time sync to continue the adjustment
			 */
			if (delta->tv_usec < 0) {
				now.tv_usec += std::max(delta->tv_usec, LOWER_TIME_SLEW_US);

				if (now.tv_usec < 0) {
					now.tv_sec--;
					now.tv_usec += ONE_SECOND_US;
				}
			} else if (delta->tv_usec > 0) {
				now.tv_usec += std::min(delta->tv_usec, UPPER_TIME_SLEW_US);

				if (now.tv_usec >= ONE_SECOND_US) {
					now.tv_sec++;
					now.tv_usec -= ONE_SECOND_US;
				}
			}

			if (::settimeofday(&now, nullptr)) {
				return -1;
			}

			applied = true;
		}

		ESP_LOGI(TAG, "SNTP adjtime: tv_sec=%lld tv_usec=%ld (%s)",
			(unsigned long long)delta->tv_sec, (unsigned long)delta->tv_usec,
			applied ? "applied" : "skipped");
	}

	if (outdelta) {
		outdelta->tv_sec = 0;
		outdelta->tv_usec = 0;
	}

	return 0;
}

void Network::syslog(std::string_view message) {
	if (syslog_ == -1) {
		return;
	}

	std::vector<char> buffer(64 + message.length());

	uint64_t timestamp_ms = esp_timer_get_time() / 1000U;
	unsigned long days;
	unsigned int hours, minutes, seconds, milliseconds;

	days = timestamp_ms / 86400000UL;
	timestamp_ms %= 86400000UL;

	hours = timestamp_ms / 3600000UL;
	timestamp_ms %= 3600000UL;

	minutes = timestamp_ms / 60000UL;
	timestamp_ms %= 60000UL;

	seconds = timestamp_ms / 1000UL;
	timestamp_ms %= 1000UL;

	milliseconds = timestamp_ms;

	std::snprintf(buffer.data(), buffer.size(),
		"<14>1 - - - - - - \xEF\xBB\xBF%03lu+%02u:%02u:%02u.%03u %*s",
			days, hours, minutes, seconds, milliseconds,
			message.length(), message.data());

	::send(syslog_, buffer.data(), std::strlen(buffer.data()), 0);
}

} // namespace clockson

extern "C" int __real_adjtime();

extern "C" int __wrap_adjtime(const struct timeval *delta,
		struct timeval *outdelta) {
	return clockson::Network::adjtime(delta, outdelta);
}
