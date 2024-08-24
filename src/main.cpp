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

#include "clockson/main.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <unistd.h>

#include <chrono>
#include <cinttypes>

#include "clockson/calendar.h"
#include "clockson/network.h"

using namespace clockson;

extern "C" void app_main() {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	new Network{};

	gpio_num_t pin = GPIO_NUM_1;
	gpio_config_t config{};

	config.pin_bit_mask = 1ULL << pin,
	config.mode = GPIO_MODE_INPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_ENABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	ESP_ERROR_CHECK(gpio_config(&config));

	TaskStatus_t status;

	vTaskGetInfo(nullptr, &status, pdTRUE, eRunning);
	ESP_LOGI(TAG, "Free stack: %lu", status.usStackHighWaterMark);

	while (true) {
		auto now = std::chrono::system_clock::now();
		uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		ESP_LOGI(TAG, "Time: %" PRIu64 "%s %s", now_ms,
			Network::time_ok() ? "+" : "", Calendar{now}.to_string().c_str());
		sleep(1);
	}
}
