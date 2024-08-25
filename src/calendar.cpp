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

#include "clockson/calendar.h"

#include <assert.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace clockson {

Calendar::Calendar(time_t t) : Calendar(t, false) {
	summer_change_soon_ = Calendar{t, true}.summer() != summer_;
}

Calendar::Calendar(std::chrono::system_clock::time_point tp)
		: Calendar(std::chrono::system_clock::to_time_t(tp)) {
}

Calendar::Calendar(time_t t, bool next) {
	struct tm tm{};
	uint64_t ts = t;

	ts /= 60U; /* current minute */
	if (next) {
		ts += 61U; /* 61 minutes in the future */
	}
	ts *= 60U;
	t = ts;

	assert(gmtime_r(&t, &tm) == &tm);

	switch (tm.tm_mon + 1) {
	case 1: /* January */
	case 2: /* February */
	case 11: /* November */
	case 12: /* December */
		summer_ = false;
		break;

	case 4: /* April */
	case 5: /* May */
	case 6: /* June */
	case 7: /* July */
	case 8: /* August */
	case 9: /* September */
		summer_ = true;
		break;

	default: /* March/October */
		int last_sunday = tm.tm_mday + (tm.tm_wday ? (7 - tm.tm_wday) : 0);

		while (last_sunday <= 31) {
			last_sunday += 7;
		}
		last_sunday -= 7;

		summer_ = (
				(tm.tm_mday == last_sunday && tm.tm_hour >= 1)
					|| tm.tm_mday > last_sunday
			) ^ ((tm.tm_mon + 1) == 10 /* October */);
		break;
	}

	utc_time_ = t;

	if (summer_) {
		ts += 3600U;
		t = ts;

		assert(gmtime_r(&t, &tm) == &tm);
	}

	year_ = tm.tm_year + 1900;
	month_ = tm.tm_mon + 1;
	day_ = tm.tm_mday;
	weekday_ = tm.tm_wday;
	hour_ = tm.tm_hour;
	minute_ = tm.tm_min;
}

std::string Calendar::to_string() const {
	std::vector<char> text(32);

	std::snprintf(text.data(), text.size(),
		"%04u-%02u-%02uT%02u:%02u+0%u:00%s",
		year_, month_, day_, hour_, minute_,
		summer_ ? 1U : 0U,
		summer_change_soon_ ? "#" : "");

	return {text.data(), text.size()};
}

} // namespace clockson
