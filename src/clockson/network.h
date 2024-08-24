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

#include <cstddef>
#include <esp_wifi.h>
#include <sdkconfig.h>

namespace clockson {

class Network {
public:
	Network();
	~Network() = delete;

	void event_handler(esp_event_base_t event_base, int32_t event_id,
		void *event_data);

private:
	static constexpr const char *TAG = "clockson.Network";
};

} // namespace clockson
