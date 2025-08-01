/*
 * tempus-redux - ESP32 "Time from NPL" (MSF) Radio clock signal generator
 * Copyright 2024-2025  Simon Arlott
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

#include "clockson/ui.h"

#include <esp_crt_bundle.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <led_strip.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <chrono>

#include "clockson/network.h"
#include "clockson/transmit.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

namespace clockson {

namespace colour = ui::colour;
using ui::RGBColour;

UserInterface::UserInterface(Network &network, Transmit &transmit)
		: network_(network), transmit_(transmit) {
	led_strip_config_t led_strip_config{};
	led_strip_rmt_config_t rmt_config{};

	led_strip_config.max_leds = 1;
	led_strip_config.strip_gpio_num = 38;
	led_strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
	led_strip_config.led_model = LED_MODEL_WS2812;
	rmt_config.resolution_hz = 10 * 1000 * 1000;

	ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_config, &rmt_config, &led_strip_));
	set_led(colour::OFF);

	if (OTA_URL[0] && COMMAND_PASSWORD[0]) {
		socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		assert(socket_ != -1);

		int flags = fcntl(socket_, F_GETFL, 0);

		assert(flags != -1);
		assert(fcntl(socket_, F_SETFL, flags | O_NONBLOCK) == 0);

		struct sockaddr_in addr {};

		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = IPADDR_ANY;
		addr.sin_port = htons(32000);

		assert(bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) == 0);
	}
}

void UserInterface::main_loop() {
	while (true) {
		uint64_t now_us = esp_timer_get_time();
		uint64_t last_sync_us{0};

		if (!network_.time_ok(&last_sync_us)) {
			set_led(colour::ORANGE);
		} else if (now_us - transmit_.last_us() > (uint64_t)microseconds(1s).count()) {
			set_led(colour::RED);
		} else if (now_us - last_sync_us > 2 * duration_cast<microseconds>(milliseconds(CONFIG_LWIP_SNTP_UPDATE_DELAY)).count()) {
			set_led(colour::BLUE);
		} else {
			set_led(colour::GREEN);
		}

		if (socket_ != -1)
			handle_command();

		sleep(1);
	}
}

void UserInterface::set_led(RGBColour colour) {
	ESP_ERROR_CHECK(led_strip_set_pixel(led_strip_, 0,
		colour.red * LED_LEVEL / 255,
		colour.green * LED_LEVEL / 255,
		colour.blue * LED_LEVEL / 255));
	ESP_ERROR_CHECK(led_strip_refresh(led_strip_));
}

void UserInterface::handle_command() {
	std::vector<char> buf(512);
	ssize_t len;

	len = recv(socket_, buf.data(), buf.size(), 0);
	if (len < 0)
		return;

	auto command = std::string{buf.data(), (size_t)len};

	if (command.rfind(COMMAND_PASSWORD, 0) != 0)
		return;

	command = command.substr(COMMAND_PASSWORD.length());

	if (command == " restart") {
		esp_restart();
	} else if (command == " status") {
		network_.ota_status();
	} else if (command == " good") {
		esp_ota_mark_app_valid_cancel_rollback();
	} else if (command == " bad") {
		esp_ota_mark_app_invalid_rollback_and_reboot();
	} else if (command == " ota") {
		esp_http_client_config_t http_config{};
		esp_https_ota_config_t ota_config{};
		esp_https_ota_handle_t handle{};

		http_config.crt_bundle_attach = esp_crt_bundle_attach;
		http_config.keep_alive_enable = true;
		http_config.disable_auto_redirect = true;
		http_config.url = OTA_URL;
		ota_config.http_config = &http_config;

		esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
		if (err) {
			network_.syslog(std::string{"OTA begin failed: "} + std::to_string(err));
			return;
		}

		const int size = esp_https_ota_get_image_size(handle);
		uint64_t last_report_us = 0;
		int last_progress = -1;
		network_.syslog(std::string{"OTA size: "} + std::to_string(size));

		while (true) {
			err = esp_https_ota_perform(handle);
			int count = esp_https_ota_get_image_len_read(handle);
			int progress = (count * 100) / size;
			uint64_t now_us = esp_timer_get_time();

			if (err == ESP_OK || (progress != last_progress && now_us - last_report_us >= 500000ULL)) {
				network_.syslog(std::string{"OTA progress: "} + std::to_string(progress) + "%");
				last_progress = progress;
				last_report_us = now_us;
			}

			if (err == ESP_OK) {
				err = esp_https_ota_finish(handle);
				if (err) {
					network_.syslog(std::string{"OTA finish failed: "} + std::to_string(err));
				} else {
					network_.syslog(std::string{"OTA finished"});
					esp_restart();
				}
				return;
			} else if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
				network_.syslog(std::string{"OTA perform failed: "} + std::to_string(err));
				esp_https_ota_abort(handle);
				return;
			}
		}
	}
}

} // namespace clockson
