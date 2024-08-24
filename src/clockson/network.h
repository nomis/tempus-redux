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
#include <sys/time.h>

namespace clockson {

namespace network {

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
	void *event_data);

void time_synced(struct timeval *tv);

} // namespace network

class Network {
public:
	Network();
	~Network() = delete;

	static bool time_ok();
	static bool time_ok(uint64_t *time_sync_us_out);

private:
	static constexpr const char *TAG = "clockson.Network";

	friend void network::event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data);
	friend void network::time_synced(struct timeval *tv);

	static void time_synced(struct timeval *tv);

	void event_handler(esp_event_base_t event_base, int32_t event_id,
		void *event_data);

	static uint64_t time_sync_us_;
};

} // namespace clockson
