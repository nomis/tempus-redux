/*
 * tempus-redux - ESP32 "Time from NPL" (MSF) Radio clock signal generator
 * Copyright 2025  Simon Arlott
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

#include "clockson/radio_clock.h"

#include "clockson/freertos.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <driver/gpio.h>

#include <chrono>

#include "clockson/network.h"

using std::chrono::microseconds;
using std::chrono::system_clock;
using namespace std::chrono_literals;

namespace clockson {

RadioClock::RadioClock(Network &network, bool present, gpio_num_t power_pin,
		gpio_num_t enable_pin, gpio_num_t toggle_radio_control_pin,
		gpio_num_t toggle_12h_24h_pin) : network_(network), present_(present),
		power_pin_(power_pin), enable_pin_(enable_pin),
		toggle_radio_control_pin_(toggle_radio_control_pin),
		toggle_12h_24h_pin_(toggle_12h_24h_pin) {
	if (!present_) {
		return;
	}

	esp_timer_create_args_t timer_config{};
	timer_config.callback = event;
	timer_config.arg = this;
	timer_config.dispatch_method = ESP_TIMER_TASK;
	timer_config.name = "radio_clock";

	ESP_ERROR_CHECK(esp_timer_create(&timer_config, &timer_));
	ESP_ERROR_CHECK(esp_timer_start_once(timer_, microseconds(1s).count()));

	{
		gpio_config_t config{};

		config.pin_bit_mask = 1ULL << power_pin_;
		config.mode = GPIO_MODE_OUTPUT;
		config.pull_up_en = GPIO_PULLUP_DISABLE;
		config.pull_down_en = GPIO_PULLDOWN_DISABLE;
		config.intr_type = GPIO_INTR_DISABLE;

		ESP_LOGI(TAG, "Power off radio clock");
		ESP_ERROR_CHECK(gpio_set_level(power_pin_, 1));
		ESP_ERROR_CHECK(gpio_config(&config));
	}

	{
		gpio_config_t config{};

		config.pin_bit_mask = 1ULL << enable_pin_;
		config.mode = GPIO_MODE_INPUT;
		config.pull_up_en = GPIO_PULLUP_ENABLE;
		config.pull_down_en = GPIO_PULLDOWN_DISABLE;
		config.intr_type = GPIO_INTR_ANYEDGE;

		ESP_ERROR_CHECK(gpio_config(&config));
	}

	release_button(toggle_radio_control_pin_);
	release_button(toggle_12h_24h_pin_);
}

void RadioClock::power_on() {
	if (present_ && state_ == State::POWER_OFF) {
		ESP_LOGI(TAG, "Power on radio clock");
		network_.syslog("Power on radio clock");
		ESP_ERROR_CHECK(gpio_set_level(power_pin_, 0));

		state_ = State::POWER_ON;

		ESP_ERROR_CHECK(gpio_isr_handler_add(enable_pin_, enable_interrupt_handler, this));
		ESP_ERROR_CHECK(gpio_intr_enable(enable_pin_));
	}
}

void RadioClock::enable_interrupt_handler(void *arg) {
	reinterpret_cast<RadioClock*>(arg)->enable_interrupt_handler();
}

void RadioClock::enable_interrupt_handler() {
	isr_enable_level_ = gpio_get_level(enable_pin_);
	esp_timer_stop(timer_);
	esp_timer_start_once(timer_, microseconds(1us).count());
}

void RadioClock::event(void *arg) {
	reinterpret_cast<RadioClock*>(arg)->event();
}

void RadioClock::event() {
	if (isr_enable_level_ != enable_level_) {
		enable_level_ = isr_enable_level_;

		if (enable_level_) {
			ESP_LOGI(TAG, "Time signal ignored");
			network_.syslog("Time signal ignored");

			if (state_ == State::RADIO_CONTROL_OFF) {
				state_ = State::RADIO_CONTROL_ON;

				ESP_LOGI(TAG, "Setting time format to 24 hours");
				network_.syslog("Setting time format to 24 hours");

				press_button(toggle_12h_24h_pin_);
				vTaskDelay(200 / portTICK_PERIOD_MS);

				ESP_LOGI(TAG, "Turning on radio control");
				network_.syslog("Turning on radio control");

				press_button(toggle_radio_control_pin_);
			}
		} else {
			ESP_LOGI(TAG, "Time signal requested");
			network_.syslog("Time signal requested");

			if (state_ == State::POWER_ON) {
				ESP_LOGI(TAG, "Turning off radio control");
				network_.syslog("Turning off radio control");

				press_button(toggle_radio_control_pin_);

				state_ = State::RADIO_CONTROL_OFF;
			} else if (state_ == State::RADIO_CONTROL_ON) {
				ESP_LOGI(TAG, "Radio clock ready");
				network_.syslog("Radio clock ready");

				state_ = State::RUNNING;
			}
		}
	}

	esp_timer_start_once(timer_, microseconds(1s).count());
}

void RadioClock::release_button(gpio_num_t button) {
	gpio_config_t config{};

	config.pin_bit_mask = 1ULL << button;
	config.mode = GPIO_MODE_INPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	ESP_ERROR_CHECK(gpio_config(&config));
}

void RadioClock::press_button(gpio_num_t button) {
	gpio_config_t config{};

	config.pin_bit_mask = 1ULL << button;
	config.mode = GPIO_MODE_OUTPUT;
	config.pull_up_en = GPIO_PULLUP_DISABLE;
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.intr_type = GPIO_INTR_DISABLE;

	ESP_ERROR_CHECK(gpio_set_level(button, 0));
	ESP_ERROR_CHECK(gpio_config(&config));

	vTaskDelay(200 / portTICK_PERIOD_MS);
	release_button(button);
}

} // namespace clockson
