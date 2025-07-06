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

#pragma once

#include <esp_timer.h>
#include <driver/gpio.h>

#include <atomic>
#include <cstddef>

namespace clockson {

class Network;

enum State {
	POWER_OFF,
	POWER_ON,
	RADIO_CONTROL_OFF,
	RADIO_CONTROL_ON,
	RUNNING,
};

class RadioClock {
public:
	RadioClock(Network &network, bool present, gpio_num_t power_pin,
		gpio_num_t enable_pin, gpio_num_t toggle_radio_control_pin,
		gpio_num_t toggle_12h_24h_pin);
	~RadioClock() = delete;

	void power_on();

private:
	static constexpr const char *TAG = "clockson.RadioClock";

	static void enable_interrupt_handler(void *arg);
	static void event(void *arg);

	void enable_interrupt_handler();
	void event();
	void release_button(gpio_num_t button);
	void press_button(gpio_num_t butotn);

	Network &network_;
	const bool present_;
	const gpio_num_t power_pin_;
	const gpio_num_t enable_pin_;
	const gpio_num_t toggle_radio_control_pin_;
	const gpio_num_t toggle_12h_24h_pin_;
	State state_{State::POWER_OFF};
	esp_timer_handle_t timer_{nullptr};
	int enable_level_{-1};
	std::atomic<int> isr_enable_level_{-1};
};

} // namespace clockson
