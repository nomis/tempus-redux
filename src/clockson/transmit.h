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

#pragma once

#include <esp_timer.h>
#include <driver/gpio.h>

#include <atomic>
#include <cstddef>

#include "time_signal.h"

namespace clockson {

class Network;

class Transmit {
public:
	Transmit(Network &network, gpio_num_t pin, bool active_low);
	~Transmit() = delete;

	inline uint64_t last_us() const { return last_us_; }

private:
	static constexpr const char *TAG = "clockson.Transmit";

	static void event(void *arg);

	inline int active() const { return active_low_ ? 0 : 1; }
	inline int inactive() const { return active_low_ ? 1 : 0; }

	void event();

	Network &network_;
	const gpio_num_t pin_;
	const bool active_low_;
	esp_timer_handle_t timer_{nullptr};
	uint64_t offset_us_{0};
	uint32_t time_s_{0};
	TimeSignal current_;
	std::atomic<uint64_t> last_us_{0};
};

} // namespace clockson
