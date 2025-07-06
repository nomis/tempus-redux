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

	POWER_ON_WAIT,
	POWER_ON,

	PRESS_RADIO_CONTROL_OFF,
	RELEASE_RADIO_CONTROL_OFF,
	RADIO_CONTROL_OFF,

	PRESS_TOGGLE_12H_24H,
	RELEASE_TOGGLE_12H_24H,
	PRESS_RADIO_CONTROL_ON,
	RELEASE_RADIO_CONTROL_ON,
	RADIO_CONTROL_ON,

	RUNNING,
};

class RadioClock {
public:
	RadioClock(Network &network, gpio_num_t power_pin,
		gpio_num_t enable_pin, gpio_num_t toggle_radio_control_pin,
		gpio_num_t toggle_12h_24h_pin);
	~RadioClock() = delete;

	void power_on();

private:
	static constexpr const char *TAG = "clockson.RadioClock";
	static constexpr uint64_t POWER_ON_WAIT_US = 1 * 1000 * 1000;
	static constexpr uint64_t BUTTON_PRESS_US = 200 * 1000;
	static constexpr uint64_t BUTTON_RELEASE_US = 200 * 1000;

	static void enable_interrupt_handler(void *arg);
	static void enable_event(void *arg);
	static void control_event(void *arg);

	void enable_interrupt_handler();
	void enable_event();
	void control_event();
	void release_button(gpio_num_t button);
	void press_button(gpio_num_t button);

	void turn_off_radio_control();
	void set_time_format_24h();
	void ready();

	Network &network_;
	const gpio_num_t power_pin_;
	const gpio_num_t enable_pin_;
	const gpio_num_t toggle_radio_control_pin_;
	const gpio_num_t toggle_12h_24h_pin_;
	State state_{State::POWER_OFF};
	esp_timer_handle_t enable_timer_{nullptr};
	int enable_level_{-1};
	std::atomic<int> isr_enable_level_{-1};
	bool time_signal_enabled_{false};
	esp_timer_handle_t control_timer_{nullptr};
};

} // namespace clockson
