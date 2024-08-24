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

#include <led_strip.h>

namespace clockson {

namespace ui {

struct RGBColour {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} __attribute__((packed));

namespace colour {

static constexpr const RGBColour OFF = {0, 0, 0};
static constexpr const RGBColour RED = {255, 0, 0};
static constexpr const RGBColour ORANGE = {255, 96, 0};
static constexpr const RGBColour YELLOW = {255, 255, 0};
static constexpr const RGBColour GREEN = {0, 255, 0};
static constexpr const RGBColour CYAN = {0, 255, 255};
static constexpr const RGBColour BLUE = {0, 0, 255};
static constexpr const RGBColour MAGENTA = {255, 0, 255};
static constexpr const RGBColour WHITE = {255, 255, 255};

} // namespace colour

} // namespace ui

class Network;
class Transmit;

class UserInterface {
public:
	UserInterface(Network &network, Transmit &transmit);
	~UserInterface() = delete;

	void main_loop();

private:
	static constexpr uint8_t LED_LEVEL = CONFIG_CLOCKSON_UI_LED_BRIGHTNESS;

	void set_led(ui::RGBColour colour);

	Network &network_;
	Transmit &transmit_;
	led_strip_handle_t led_strip_{nullptr};
};

} // namespace clockson
