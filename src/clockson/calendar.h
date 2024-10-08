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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

namespace clockson {

class Calendar {
public:
	explicit Calendar(time_t t);
	explicit Calendar(std::chrono::system_clock::time_point tp);
	~Calendar() = default;

	inline uint64_t utc_time() const { return utc_time_; }
	inline uint16_t year() const { return year_; }
	inline uint8_t month() const { return month_; }
	inline uint8_t day() const { return day_; }
	inline uint8_t weekday() const { return weekday_; } /* 0 = Sunday */
	inline uint8_t hour() const { return hour_; }
	inline uint8_t minute() const { return minute_; }
	inline bool summer() const { return summer_; }
	inline bool summer_change_soon() const { return summer_change_soon_; }

	std::string to_string() const;

private:
	Calendar(time_t t, bool next);

	uint64_t utc_time_{0};
	uint16_t year_{0};
	uint8_t month_{0};
	uint8_t day_{0};
	uint8_t weekday_{0}; /* 0 = Sunday */
	uint8_t hour_{0};
	uint8_t minute_{0};
	bool summer_{false};
	bool summer_change_soon_{false};
};

} // namespace clockson
