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


#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>

#include "clockson/network.h"

namespace clockson {

RadioClock::RadioClock(Network &network, gpio_num_t power_pin,
		gpio_num_t enable_pin, gpio_num_t toggle_radio_control_pin,
		gpio_num_t toggle_12h_24h_pin) : network_(network),
		power_pin_(power_pin), enable_pin_(enable_pin),
		toggle_radio_control_pin_(toggle_radio_control_pin),
		toggle_12h_24h_pin_(toggle_12h_24h_pin) {
	{
		esp_timer_create_args_t timer_config{};
		timer_config.callback = enable_event;
		timer_config.arg = this;
		timer_config.dispatch_method = ESP_TIMER_TASK;
		timer_config.name = "radio_clock_enable";

		ESP_ERROR_CHECK(esp_timer_create(&timer_config, &enable_timer_));
	}

	{
		esp_timer_create_args_t timer_config{};
		timer_config.callback = control_event;
		timer_config.arg = this;
		timer_config.dispatch_method = ESP_TIMER_TASK;
		timer_config.name = "radio_clock_button";

		ESP_ERROR_CHECK(esp_timer_create(&timer_config, &control_timer_));
	}

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
	if (state_ == State::POWER_OFF) {
		ESP_LOGI(TAG, "Power on radio clock");
		network_.syslog("Power on radio clock");
		ESP_ERROR_CHECK(gpio_set_level(power_pin_, 0));

		state_ = State::POWER_ON_WAIT;
	}
}

void RadioClock::enable_interrupt_handler(void *arg) {
	reinterpret_cast<RadioClock*>(arg)->enable_interrupt_handler();
}

void RadioClock::enable_interrupt_handler() {
	isr_enable_level_ = gpio_get_level(enable_pin_);
	esp_timer_stop(enable_timer_);
	ESP_ERROR_CHECK(esp_timer_start_once(enable_timer_, 1));
}

void RadioClock::enable_event(void *arg) {
	reinterpret_cast<RadioClock*>(arg)->enable_event();
}

void RadioClock::enable_event() {
	int new_enable_level = isr_enable_level_;

	if (new_enable_level != enable_level_) {
		enable_level_ = new_enable_level;
		time_signal_enabled_ = !enable_level_;

		if (time_signal_enabled_) {
			ESP_LOGI(TAG, "Time signal requested");
			network_.syslog("Time signal requested");

			if (state_ == State::POWER_ON) {
				turn_off_radio_control();
			} else if (state_ == State::RADIO_CONTROL_ON) {
				ready();
			}
		} else {
			ESP_LOGI(TAG, "Time signal ignored");
			network_.syslog("Time signal ignored");

			if (state_ == State::RADIO_CONTROL_OFF) {
				set_time_format_24h();
			}
		}
	}
}

void RadioClock::control_event(void *arg) {
	reinterpret_cast<RadioClock*>(arg)->control_event();
}

void RadioClock::control_event() {
	switch (state_) {
	case State::POWER_OFF:
	case State::POWER_ON_WAIT:
		state_ = State::POWER_ON_WAIT;
		enable_interrupt_handler();
		ESP_ERROR_CHECK(gpio_isr_handler_add(enable_pin_, enable_interrupt_handler, this));
		ESP_ERROR_CHECK(gpio_intr_enable(enable_pin_));
		break;

	case State::POWER_ON:
		break;

	case State::PRESS_RADIO_CONTROL_OFF:
		release_button(toggle_radio_control_pin_);
		state_ = State::RELEASE_RADIO_CONTROL_OFF;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_RELEASE_US));
		break;

	case State::RELEASE_RADIO_CONTROL_OFF:
		state_ = State::RADIO_CONTROL_OFF;
		if (!time_signal_enabled_) {
			set_time_format_24h();
		}
		break;

	case State::RADIO_CONTROL_OFF:
		break;

	case State::PRESS_TOGGLE_12H_24H:
		release_button(toggle_radio_control_pin_);
		state_ = State::RELEASE_TOGGLE_12H_24H;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_RELEASE_US));
		break;

	case State::RELEASE_TOGGLE_12H_24H:
		ESP_LOGI(TAG, "Turning on radio control");
		network_.syslog("Turning on radio control");

		press_button(toggle_radio_control_pin_);
		state_ = State::PRESS_RADIO_CONTROL_ON;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
		break;

	case State::PRESS_RADIO_CONTROL_ON:
		release_button(toggle_radio_control_pin_);
		state_ = State::RELEASE_RADIO_CONTROL_ON;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_RELEASE_US));
		break;

	case State::RELEASE_RADIO_CONTROL_ON:
		state_ = State::RADIO_CONTROL_ON;
		if (time_signal_enabled_) {
			ready();
		}
		break;

	case State::RADIO_CONTROL_ON:
	case State::RUNNING:
		break;
	}
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
}

void RadioClock::turn_off_radio_control() {
	ESP_LOGI(TAG, "Turning off radio control");
	network_.syslog("Turning off radio control");

	press_button(toggle_radio_control_pin_);
	state_ = State::PRESS_RADIO_CONTROL_OFF;
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
}

void RadioClock::set_time_format_24h() {
	ESP_LOGI(TAG, "Setting time format to 24 hours");
	network_.syslog("Setting time format to 24 hours");

	press_button(toggle_12h_24h_pin_);
	state_ = State::PRESS_TOGGLE_12H_24H;
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
}

void RadioClock::ready() {
	ESP_LOGI(TAG, "Radio clock ready");
	network_.syslog("Radio clock ready");

	state_ = State::RUNNING;
}

} // namespace clockson
