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
		network_.syslog(TAG, "Power on radio clock");
		ESP_ERROR_CHECK(gpio_set_level(power_pin_, 0));

		state_ = State::POWER_ON_WAIT;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, POWER_ON_WAIT_US));
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
		uint64_t now_us = esp_timer_get_time();

		enable_level_ = new_enable_level;
		time_signal_enabled_ = !enable_level_;

		if (time_signal_enabled_) {
			std::string message = "Time signal requested";
			uint64_t duration_us = now_us - time_signal_change_us_;

			if (time_signal_change_us_) {
				message += " (" + std::to_string(duration_us) + "us)";
			}
			time_signal_change_us_ = now_us;

			network_.syslog(TAG, message);

			if (state_ == State::POWER_ON) {
				turn_off_radio_control();
			} else if (state_ == State::RADIO_CONTROL_ON) {
				ready();
			} else if (state_ == State::RUNNING) {
				cancel_idle_timeout();
			}
		} else {
			std::string message = "Time signal ignored";
			uint64_t duration_us = now_us - time_signal_change_us_;

			if (time_signal_change_us_) {
				message += " (" + std::to_string(duration_us) + "us)";
			}
			time_signal_change_us_ = now_us;

			network_.syslog(TAG, message);

			if (state_ == State::RADIO_CONTROL_OFF) {
				set_time_format_24h();
			}

			if (state_ == State::RUNNING) {
				if (duration_us >= FAILURE_MIN_US && duration_us <= FAILURE_MAX_US) {
					recover();
				} else {
					start_idle_timeout();
				}
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
		state_ = State::POWER_ON;
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
		release_button(toggle_12h_24h_pin_);
		state_ = State::RELEASE_TOGGLE_12H_24H;
		ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_RELEASE_US));
		break;

	case State::RUNNING:
		network_.syslog(TAG, "Radio clock has stopped requesting the time signal");
		[[fallthrough]];
	case State::RELEASE_TOGGLE_12H_24H:
	case State::RADIO_CONTROL_ON:
		turn_on_radio_control();
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
		} else {
			ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, RETRY_US));
		}
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
	network_.syslog(TAG, "Turning off radio control");

	press_button(toggle_radio_control_pin_);
	state_ = State::PRESS_RADIO_CONTROL_OFF;
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
}

void RadioClock::turn_on_radio_control() {
	network_.syslog(TAG, "Turning on radio control");

	press_button(toggle_radio_control_pin_);
	state_ = State::PRESS_RADIO_CONTROL_ON;
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
}

void RadioClock::set_time_format_24h() {
	network_.syslog(TAG, "Setting time format to 24 hours");

	press_button(toggle_12h_24h_pin_);
	state_ = State::PRESS_TOGGLE_12H_24H;
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, BUTTON_PRESS_US));
}

void RadioClock::ready() {
	network_.syslog(TAG, "Radio clock ready");

	state_ = State::RUNNING;
	esp_timer_stop(control_timer_);
}

void RadioClock::recover() {
	network_.syslog(TAG, "Radio clock has failed to use the time signal");

	state_ = State::RADIO_CONTROL_ON;
	esp_timer_stop(control_timer_);
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, RETRY_US));
}

void RadioClock::start_idle_timeout() {
	esp_timer_stop(control_timer_);
	ESP_ERROR_CHECK(esp_timer_start_once(control_timer_, IDLE_TIMEOUT_US));
}

void RadioClock::cancel_idle_timeout() {
	esp_timer_stop(control_timer_);
}

} // namespace clockson
