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

#include "clockson/transmit.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include <chrono>

#include "clockson/network.h"
#include "clockson/time_signal.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::system_clock;
using namespace std::chrono_literals;

namespace clockson {

Transmit::Transmit(gpio_num_t pin, bool active_low) : pin_(pin),
		active_low_(active_low) {
	esp_timer_create_args_t timer_config{};
	timer_config.callback = event;
	timer_config.arg = this;
	timer_config.dispatch_method = ESP_TIMER_TASK;
	timer_config.name = "transmit";

	ESP_ERROR_CHECK(esp_timer_create(&timer_config, &timer_));

	gpio_config_t config{};

	config.pin_bit_mask = 1ULL << pin,
	config.mode = GPIO_MODE_OUTPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
	ESP_ERROR_CHECK(gpio_config(&config));

	ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
}

void Transmit::event(void *arg) {
	reinterpret_cast<Transmit*>(arg)->event();
}

void Transmit::event() {
	while (true) {
		uint64_t uptime_us = esp_timer_get_time();
		auto now = system_clock::now();

		if (!current_.available()) {
			uint64_t last_sync_us{0};
			if (!Network::time_ok(&last_sync_us)) {
				if (last_sync_us) {
					ESP_LOGW(TAG, "Waiting for time sync (last sync %" PRIu64 "us ago)",
						uptime_us - last_sync_us);
				} else {
					ESP_LOGI(TAG, "Waiting for first time sync");
				}
				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
				return;
			}

			uint64_t now_us = duration_cast<microseconds>(now.time_since_epoch()).count();
			uint32_t now_s = system_clock::to_time_t(now);

			if (now_us < uptime_us) {
				ESP_LOGE(TAG, "Invalid: now_us=%" PRIu64 " < uptime_us=%" PRIu64, now_us, uptime_us);
				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
				return;
			}

			uint64_t offset_us = now_us - uptime_us;

			now_s++;
			now_s /= 60U;
			now_s++;
			now_s *= 60U;

			if (time_s_ == now_s) {
				/*
				* If the clock has gone backwards slightly, don't try to repeat the
				* same time signal twice.
				*/

				uint64_t remaining_us = (now_us / microseconds(1min).count() + 1) * microseconds(1min).count()
					- microseconds(700ms).count() - offset_us - uptime_us;

				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, remaining_us));
				return;
			}

			current_ = TimeSignal{now_s, offset_us};
			time_s_ = now_s;

			ESP_LOGI(TAG, "%s (offset %" PRIu64 "us)", current_.time().to_string().c_str(), offset_us);

			while (current_.available() && current_.next().ts < uptime_us) {
				current_.pop();
			}

			continue;
		}

		auto signal = current_.next();
		uint64_t signal_us = signal.ts < 0 ? 0 : signal.ts;

		if (uptime_us < signal_us) {
			ESP_ERROR_CHECK(esp_timer_start_once(timer_, signal_us - uptime_us));
			return;
		}

		ESP_ERROR_CHECK(gpio_set_level(pin_, signal.carrier ? active() : inactive()));
		current_.pop();
	}
}

} // namespace clockson
