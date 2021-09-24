/**
 * Copyright 2021 Johannes Marbach
 *
 * This file is part of unl0kr, hereafter referred to as the program.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef UL_INDEV_H
#define UL_INDEV_H

#include "lvgl/lvgl.h"

#include <stdbool.h>

/**
 * Auto-connect currently available keyboard, pointer and touchscreen input devices.
 */
void ul_indev_auto_connect();

/**
 * Check if any keyboard devices are connected.
 *
 * @return true if at least one keyboard device is connected, false otherwise
 */
bool ul_indev_is_keyboard_connected();

/**
 * Set up an LVGL text area to receive input from currently connected keyboard devices.
 * 
 * @param textarea textarea widget
 */
void ul_indev_set_up_textarea_for_keyboard_input(lv_obj_t *textarea);

/**
 * Set up the mouse cursor image for currently connected pointer devices.
 */
void ul_indev_set_up_mouse_cursor();

#endif /* UL_INDEV_H */
