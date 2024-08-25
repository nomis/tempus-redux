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

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <deque>

#include "calendar.h"

namespace clockson {

struct Signal {
	int64_t ts;
	bool carrier;

	inline uint64_t unsigned_ts() const { return ts < 0 ? 0 : ts; }
};

class TimeSignal {
public:
	TimeSignal();
	explicit TimeSignal(time_t t, uint64_t offset_us);
	~TimeSignal() = default;

	inline bool available() { return !values_.empty(); }
	inline Signal next() { return values_.front(); }
	inline void pop() { values_.pop_front(); }

	inline const Calendar& time() const { return time_; }

private:
	using data_t = std::bitset<60>;

	static void set_bcd(data_t &data, size_t begin, size_t end, unsigned int value);
	static bool odd_parity(const data_t &data, size_t begin, size_t end);

	Calendar time_;
	std::deque<Signal> values_;
};

} // namespace clockson
