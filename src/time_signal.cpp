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

#include "clockson/time_signal.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <ctime>

#include "clockson/calendar.h"

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::seconds;

namespace clockson {

TimeSignal::TimeSignal() : time_(0) {
}

TimeSignal::TimeSignal(time_t t, uint64_t offset_us) : time_(t) {
	data_t a, b;

	set_bcd(a, 17, 24, time_.year() % 100U);
	set_bcd(a, 25, 29, time_.month());
	set_bcd(a, 30, 35, time_.day());
	set_bcd(a, 36, 38, time_.weekday());
	set_bcd(a, 39, 44, time_.hour());
	set_bcd(a, 45, 51, time_.minute());

	/* Minute identifier */
	a[53] = true;
	a[54] = true;
	a[55] = true;
	a[56] = true;
	a[57] = true;
	a[58] = true;

	b[53] = time_.summer_change_soon();
	b[58] = time_.summer();

	b[54] = odd_parity(a, 17, 24);
	b[55] = odd_parity(a, 25, 35);
	b[56] = odd_parity(a, 36, 38);
	b[57] = odd_parity(a, 39, 51);

	auto ts = duration_cast<microseconds>(seconds{time_.utc_time()});

	ts -= microseconds{offset_us};

	/* Transmit time one minute before */
	ts -= minutes{1};

	/* Minute marker */
	values_.emplace_back(ts.count(), false);
	ts += milliseconds{500};
	values_.emplace_back(ts.count(), true);
	ts += milliseconds{500};

	/* Second markers */
	for (size_t i = 1; i <= 59; i++) {
		values_.emplace_back(ts.count(), false);
		ts += milliseconds{100};

		if (!a[i]) {
			values_.emplace_back(ts.count(), true);
		}
		ts += milliseconds{100};

		if (b[i] != a[i]) {
			values_.emplace_back(ts.count(), !b[i]);
		}
		ts += milliseconds{100};

		if (b[i]) {
			values_.emplace_back(ts.count(), true);
		}
		ts += milliseconds{700};
	}
}

inline void TimeSignal::set_bcd(data_t &data, size_t begin, size_t end,
		unsigned int value) {
	size_t i = end;

	while (i >= begin) {
		std::bitset<4> bits{value % 10U};

		value /= 10U;

		for (size_t j = 0; j < 4 && i >= begin; j++) {
			data[i] = bits[j];

			if (i == begin) {
				return;
			}

			i--;
		}
	}
}

inline bool TimeSignal::odd_parity(const data_t &data, size_t begin, size_t end) {
	bool parity = true;

	for (size_t i = begin; i <= end; i++) {
		parity ^= data[i];
	}

	return parity;
}

} // namespace clockson
