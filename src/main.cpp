/*
 * tempus-redux - ESP32 Radio clock signal generator
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

#include "clockson/main.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>

using namespace clockson;

extern "C" void app_main() {
	TaskStatus_t status;

	vTaskGetInfo(nullptr, &status, pdTRUE, eRunning);
	ESP_LOGD(TAG, "Free stack: %lu", status.usStackHighWaterMark);

	gpio_num_t pin = GPIO_NUM_1;
	gpio_config_t config{};

	config.pin_bit_mask = 1ULL << pin,
	config.mode = GPIO_MODE_INPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	ESP_ERROR_CHECK(gpio_config(&config));

	bool last_state = gpio_get_level(pin);
	uint64_t start = esp_timer_get_time();

	while (true) {
		bool state = gpio_get_level(pin);

		if (state != last_state) {
			uint64_t stop = esp_timer_get_time();

			ESP_LOGI(TAG, "%d: %" PRIu64, last_state, (stop - start) / 1000U);
			start = stop;
			last_state = state;
		}
	}

}
