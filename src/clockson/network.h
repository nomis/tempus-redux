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

#include "freertos.h"

#include <cstddef>
#include <esp_wifi.h>
#include <sdkconfig.h>
#include <sys/time.h>

#include <atomic>
#include <string>

extern "C" int __wrap_adjtime(const struct timeval *delta,
	struct timeval *outdelta);

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
	static void time_slew_next();

	void syslog(std::string_view message);

private:
	static constexpr const char *TAG = "clockson.Network";
	/*
	 * Maximum smooth time adjustment is 750ms, which will take 30 minutes when
	 * adjusting by 25ms every minute
	 */
	static constexpr suseconds_t UPPER_TIME_STEP_US = 750000;
	static constexpr suseconds_t LOWER_TIME_STEP_US = -UPPER_TIME_STEP_US;
	/*
	 * Maximum smooth time adjustment per transmission, to avoid tearing the
	 * signal or going beyond receiver timing tolerances
	 */
	static constexpr suseconds_t UPPER_TIME_SLEW_US = 25000;
	static constexpr suseconds_t LOWER_TIME_SLEW_US = -UPPER_TIME_SLEW_US;
	static constexpr suseconds_t ONE_SECOND_US = 1000000;

	friend void network::event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data);
	friend void network::time_synced(struct timeval *tv);
	friend int ::__wrap_adjtime(const struct timeval *delta,
		struct timeval *outdelta);

	static void time_synced(struct timeval *tv);
	static int adjtime(const struct timeval *delta, struct timeval *outdelta);

	void event_handler(esp_event_base_t event_base, int32_t event_id,
		void *event_data);

	static uint64_t time_sync_us_;
	static bool time_step_first_;
	static std::atomic<int> time_slew_allowed_;
	static int time_slew_current_;

	int syslog_{-1};
};

} // namespace clockson
