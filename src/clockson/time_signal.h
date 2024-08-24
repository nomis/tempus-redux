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

namespace clockson {

struct Signal {
	uint64_t ts;
	bool carrier;
};

class TimeSignal {
public:
	TimeSignal(time_t t);
	~TimeSignal() = default;

	bool empty() { return values_.empty(); }
	Signal next() { return values_.front(); }
	void pop() { values_.pop_front(); }

private:
	using data_t = std::bitset<60>;

	static void set_bcd(data_t &data, size_t begin, size_t end, unsigned int value);
	static bool odd_parity(data_t &data, size_t begin, size_t end);

	std::deque<Signal> values_;
};

} // namespace clockson
