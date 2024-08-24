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

#include "clockson/ui.h"

#include <esp_timer.h>
#include <led_strip.h>

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
}

void UserInterface::main_loop() {
	while (true) {
		uint64_t now_us = esp_timer_get_time();
		uint64_t last_sync_us{0};

		if (!network_.time_ok(&last_sync_us)) {
			set_led(colour::YELLOW);
		} else if (now_us - transmit_.last_us() > microseconds(1s).count()) {
			set_led(colour::RED);
		} else if (now_us - last_sync_us > 2 * duration_cast<microseconds>(milliseconds(CONFIG_LWIP_SNTP_UPDATE_DELAY)).count()) {
			set_led(colour::BLUE);
		} else {
			set_led(colour::GREEN);
		}

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

} // namespace clockson
