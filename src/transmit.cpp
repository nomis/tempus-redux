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
#include <cstdio>
#include <vector>

#include "clockson/network.h"
#include "clockson/time_signal.h"

using std::chrono::duration_cast;
using std::chrono::time_point_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using namespace std::chrono_literals;

namespace clockson {

Transmit::Transmit(Network &network, gpio_num_t pin, bool active_low)
		: network_(network), pin_(pin), active_low_(active_low) {
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

		if (!current_.available()) {
			/*
			 * Convert this to microseconds before applying a test offset
			 * because the default precision of nanoseconds doesn't support
			 * times beyond ~2554 in a uint64_t value.
			 */
			auto now = time_point_cast<microseconds>(system_clock::now()).time_since_epoch();
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

#ifdef CONFIG_CLOCKSON_TEST_TIME_S
# define CLOCKSON_CONCAT_(x,y) x##y
# define CLOCKSON_CONCAT(x,y) CLOCKSON_CONCAT_(x,y)
# define CLOCKSON_TEST_TIME_US (uint64_t)(CLOCKSON_CONCAT(CONFIG_CLOCKSON_TEST_TIME_S, 000000ULL))
			static auto test_offset = microseconds{CLOCKSON_TEST_TIME_US} - now;
			now += test_offset;
# undef CLOCKSON_TEST_TIME_US
# undef CLOCKSON_CONCAT
# undef CLOCKSON_CONCAT_
#endif

			uint64_t now_us = now.count();
			uint64_t now_s = duration_cast<seconds>(now).count();

			if (now_us < uptime_us) {
				ESP_LOGE(TAG, "Invalid: now_us=%" PRIu64 " < uptime_us=%" PRIu64, now_us, uptime_us);
				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
				return;
			}

			uint64_t offset_us = now_us - uptime_us;

			/*
			 * Generate time signal for the next minute (as required by the
			 * protocol), looking ahead 1 second further because preparation for
			 * the next transmission happens up to 900ms before the current
			 * minute finishes.
			 */
			now_s++;
			now_s /= 60U;
			now_s++;
			now_s *= 60U;

			if (last_signal_s_ == now_s) {
				/*
				 * If the clock has gone backwards slightly, don't try to repeat the
				 * same time signal twice.
				 */

				uint64_t next_minute_us = (now_us / (uint64_t)microseconds(1min).count() + 1U)
					* (uint64_t)microseconds(1min).count()
					- (uint64_t)microseconds(700ms).count();

				if (next_minute_us < now_us) {
					ESP_LOGE(TAG, "Invalid: next_minute_us=%" PRIu64 " < now_us=%" PRIu64, next_minute_us, now_us);
					ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
					ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
					return;
				}

				uint64_t remaining_us = next_minute_us - offset_us - uptime_us;

				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, remaining_us));
				return;
			}

			current_ = TimeSignal{(time_t)now_s, offset_us};
			last_signal_s_ = now_s;

			std::vector<char> message(64);

			std::snprintf(message.data(), message.size(), "%s (offset %" PRIu64 "us)",
				current_.time().to_string().c_str(), offset_us);
			ESP_LOGI(TAG, "%s", message.data());
			network_.syslog(message.data());

			/*
			 * Skip everything that would have happened in the past if we start
			 * in the middle of a minute.
			 */
			while (current_.available() && current_.next().unsigned_ts() < uptime_us) {
				current_.pop();
			}

			/*
			 * If the transmission times somehow overflow uint64_t, avoid going
			 * into a fast infinite loop.
			 */
			if (!current_.available()) {
				ESP_LOGW(TAG, "Nothing left to transmit");
				ESP_ERROR_CHECK(gpio_set_level(pin_, active()));
				ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));
				return;
			}

			network_.time_slew_next();
			continue;
		}

		auto signal = current_.next();
		uint64_t signal_us = signal.unsigned_ts();

		if (uptime_us < signal_us) {
			ESP_ERROR_CHECK(esp_timer_start_once(timer_, signal_us - uptime_us));
			return;
		}

		ESP_ERROR_CHECK(gpio_set_level(pin_, signal.carrier ? active() : inactive()));
		last_us_ = uptime_us;
		current_.pop();
	}
}

} // namespace clockson
