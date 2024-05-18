/**
 * Copyright 2021 Johannes Marbach
 *
 * This file is part of furios-recovery, hereafter referred to as the program.
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


#include "theme.h"

#include "log.h"
#include "lvglcharger.h"

#include "lvgl/lvgl.h"


/**
 * Static variables
 */

static ul_theme current_theme;
static lv_theme_t lv_theme;

static struct {
    lv_style_t widget;
    lv_style_t window;
    lv_style_t header;
    lv_style_t key;
    lv_style_t button;
    lv_style_t button_pressed;
    lv_style_t textarea;
    lv_style_t textarea_placeholder;
    lv_style_t textarea_cursor;
    lv_style_t dropdown;
    lv_style_t dropdown_pressed;
    lv_style_t dropdown_list;
    lv_style_t dropdown_list_selected;
    lv_style_t label;
    lv_style_t msgbox;
    lv_style_t msgbox_label;
    lv_style_t msgbox_btnmatrix;
    lv_style_t msgbox_background;
    lv_style_t bar;
    lv_style_t bar_indicator;
} styles; 

static bool are_styles_initialised = false;


/**
 * Static prototypes
 */

/**
 * Set up the static styles for a specific theme.
 *
 * @param theme theme to derive the styles from
 */
static void init_styles(const ul_theme *theme);

/**
 * Initialise or reset a style.
 *
 * @param style style to reset
 */
static void reset_style(lv_style_t *style);

/**
 * Apply a theme to an object.
 *
 * @param theme theme to apply
 * @param obj object to style
 */
static void apply_theme_cb(lv_theme_t *theme, lv_obj_t *obj);


/**
 * Static functions
 */

static void init_styles(const ul_theme *theme) {
    reset_style(&(styles.widget));

    reset_style(&(styles.window));
    lv_style_set_bg_opa(&(styles.window), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.window), lv_color_hex(theme->window.bg_color));

    reset_style(&(styles.header));
    lv_style_set_bg_opa(&(styles.header), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.header), lv_color_hex(theme->header.bg_color));
    lv_style_set_border_side(&(styles.header), LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&(styles.header), lv_dpx(theme->header.border_width));
    lv_style_set_border_color(&(styles.header), lv_color_hex(theme->header.border_color));
    lv_style_set_pad_all(&(styles.header), lv_dpx(theme->header.pad));
    lv_style_set_pad_gap(&(styles.header), lv_dpx(theme->header.gap));

    reset_style(&(styles.button));
    lv_style_set_text_color(&(styles.button), lv_color_hex(theme->button.normal.fg_color));
    lv_style_set_bg_opa(&(styles.button), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.button), lv_color_hex(theme->button.normal.bg_color));
    lv_style_set_border_side(&(styles.button), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.button), lv_dpx(theme->button.border_width));
    lv_style_set_border_color(&(styles.button), lv_color_hex(theme->button.normal.border_color));
    lv_style_set_radius(&(styles.button), lv_dpx(theme->button.corner_radius));
    lv_style_set_pad_all(&(styles.button), lv_dpx(theme->button.pad));

    reset_style(&(styles.button_pressed));
    lv_style_set_text_color(&(styles.button_pressed), lv_color_hex(theme->button.pressed.fg_color));
    lv_style_set_bg_color(&(styles.button_pressed), lv_color_hex(theme->button.pressed.bg_color));
    lv_style_set_border_color(&(styles.button_pressed), lv_color_hex(theme->button.pressed.border_color));

    reset_style(&(styles.textarea));
    lv_style_set_text_color(&(styles.textarea), lv_color_hex(theme->textarea.fg_color));
    lv_style_set_bg_opa(&(styles.textarea), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.textarea), lv_color_hex(theme->textarea.bg_color));  
    lv_style_set_border_side(&(styles.textarea), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.textarea), lv_dpx(theme->textarea.border_width));
    lv_style_set_border_color(&(styles.textarea), lv_color_hex(theme->textarea.border_color));
    lv_style_set_radius(&(styles.textarea), lv_dpx(theme->textarea.corner_radius));
    lv_style_set_pad_all(&(styles.textarea), lv_dpx(theme->textarea.pad));

    reset_style(&(styles.textarea_placeholder));
    lv_style_set_text_color(&(styles.textarea_placeholder), lv_color_hex(theme->textarea.placeholder_color));

    reset_style(&(styles.textarea_cursor));
    lv_style_set_border_side(&(styles.textarea_cursor), LV_BORDER_SIDE_LEFT);
    lv_style_set_border_width(&(styles.textarea_cursor), lv_dpx(theme->textarea.cursor.width));
    lv_style_set_border_color(&(styles.textarea_cursor), lv_color_hex(theme->textarea.cursor.color));
    lv_style_set_anim_time(&(styles.textarea_cursor), theme->textarea.cursor.period);

    reset_style(&(styles.dropdown));
    lv_style_set_text_color(&(styles.dropdown), lv_color_hex(theme->dropdown.button.normal.fg_color));
    lv_style_set_bg_opa(&(styles.dropdown), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.dropdown), lv_color_hex(theme->dropdown.button.normal.bg_color));
    lv_style_set_border_side(&(styles.dropdown), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.dropdown), lv_dpx(theme->dropdown.button.border_width));
    lv_style_set_border_color(&(styles.dropdown), lv_color_hex(theme->dropdown.button.normal.border_color));
    lv_style_set_radius(&(styles.dropdown), lv_dpx(theme->dropdown.button.corner_radius));
    lv_style_set_pad_all(&(styles.dropdown), lv_dpx(theme->dropdown.button.pad));

    reset_style(&(styles.dropdown_pressed));
    lv_style_set_text_color(&(styles.dropdown_pressed), lv_color_hex(theme->dropdown.button.pressed.fg_color));
    lv_style_set_bg_color(&(styles.dropdown_pressed), lv_color_hex(theme->dropdown.button.pressed.bg_color));
    lv_style_set_border_color(&(styles.dropdown_pressed), lv_color_hex(theme->dropdown.button.pressed.border_color));

    reset_style(&(styles.dropdown_list));
    lv_style_set_text_color(&(styles.dropdown_list), lv_color_hex(theme->dropdown.list.fg_color));
    lv_style_set_bg_opa(&(styles.dropdown_list), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.dropdown_list), lv_color_hex(theme->dropdown.list.bg_color));
    lv_style_set_border_side(&(styles.dropdown_list), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.dropdown_list), lv_dpx(theme->dropdown.list.border_width));
    lv_style_set_border_color(&(styles.dropdown_list), lv_color_hex(theme->dropdown.list.border_color));
    lv_style_set_radius(&(styles.dropdown_list), lv_dpx(theme->dropdown.list.corner_radius));
    lv_style_set_pad_all(&(styles.dropdown_list), lv_dpx(theme->dropdown.list.pad));

    reset_style(&(styles.dropdown_list_selected));
    lv_style_set_text_color(&(styles.dropdown_list_selected), lv_color_hex(theme->dropdown.list.selection_fg_color));
    lv_style_set_bg_opa(&(styles.dropdown_list_selected), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.dropdown_list_selected), lv_color_hex(theme->dropdown.list.selection_bg_color));

    reset_style(&(styles.label));
    lv_style_set_text_color(&(styles.label), lv_color_hex(theme->label.fg_color));

    reset_style(&(styles.msgbox));
    lv_style_set_text_color(&(styles.msgbox), lv_color_hex(theme->msgbox.fg_color));
    lv_style_set_bg_opa(&(styles.msgbox), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.msgbox), lv_color_hex(theme->msgbox.bg_color));
    lv_style_set_border_side(&(styles.msgbox), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.msgbox), lv_dpx(theme->msgbox.border_width));
    lv_style_set_border_color(&(styles.msgbox), lv_color_hex(theme->msgbox.border_color));
    lv_style_set_radius(&(styles.msgbox), lv_dpx(theme->msgbox.corner_radius));
    lv_style_set_pad_all(&(styles.msgbox), lv_dpx(theme->msgbox.pad));

    reset_style(&(styles.msgbox_label));
    lv_style_set_text_align(&(styles.msgbox_label), LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_bottom(&(styles.msgbox_label), lv_dpx(theme->msgbox.gap));

    reset_style(&(styles.msgbox_btnmatrix));
    lv_style_set_pad_gap(&(styles.msgbox_btnmatrix), lv_dpx(theme->msgbox.buttons.gap));
    lv_style_set_min_width(&(styles.msgbox_btnmatrix), LV_PCT(100));

    reset_style(&(styles.msgbox_background));
    lv_style_set_bg_color(&(styles.msgbox_background), lv_color_hex(theme->msgbox.dimming.color));
    lv_style_set_bg_opa(&(styles.msgbox_background), theme->msgbox.dimming.opacity);

    reset_style(&(styles.bar));
    lv_style_set_border_side(&(styles.bar), LV_BORDER_SIDE_FULL);
    lv_style_set_border_width(&(styles.bar), lv_dpx(theme->bar.border_width));
    lv_style_set_border_color(&(styles.bar), lv_color_hex(theme->bar.border_color));
    lv_style_set_radius(&(styles.bar), lv_dpx(theme->bar.corner_radius));

    reset_style(&(styles.bar_indicator));
    lv_style_set_bg_opa(&(styles.bar_indicator), LV_OPA_COVER);
    lv_style_set_bg_color(&(styles.bar_indicator), lv_color_hex(theme->bar.indicator.bg_color));

    are_styles_initialised = true;
}

static void reset_style(lv_style_t *style) {
    if (are_styles_initialised) {
        lv_style_reset(style);
    } else {
        lv_style_init(style);
    }
}

static void apply_theme_cb(lv_theme_t *theme, lv_obj_t *obj) {
    LV_UNUSED(theme);

    lv_obj_add_style(obj, &(styles.widget), 0);

    if (lv_obj_get_parent(obj) == NULL) {
        lv_obj_add_style(obj, &(styles.window), 0);
        return;
    }

    if (lv_obj_has_flag(obj, UL_WIDGET_HEADER)) {
        lv_obj_add_style(obj, &(styles.header), 0);
        return;
    }

    if (lv_obj_check_type(obj, &lv_btn_class)) {
        lv_obj_add_style(obj, &(styles.button), 0);
        lv_obj_add_style(obj, &(styles.button_pressed), LV_STATE_PRESSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class) && lv_obj_check_type(lv_obj_get_parent(obj), &lv_btn_class)) {
        return; /* Inherit styling from button */
    }

    if (lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_add_style(obj, &(styles.textarea), 0);
        lv_obj_add_style(obj, &(styles.textarea_placeholder), LV_PART_TEXTAREA_PLACEHOLDER);
        lv_obj_add_style(obj, &(styles.textarea_cursor), LV_PART_CURSOR | LV_STATE_FOCUSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class) && lv_obj_check_type(lv_obj_get_parent(obj), &lv_textarea_class)) {
        return; /* Inherit styling from textarea */
    }

    if (lv_obj_check_type(obj, &lv_dropdown_class)) {
        lv_obj_add_style(obj, &(styles.dropdown), 0);
        lv_obj_add_style(obj, &(styles.dropdown_pressed), LV_STATE_PRESSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_dropdownlist_class)) {
        lv_obj_add_style(obj, &(styles.dropdown_list), 0);
        lv_obj_add_style(obj, &(styles.dropdown_list_selected), LV_PART_SELECTED | LV_STATE_CHECKED);
        lv_obj_add_style(obj, &(styles.dropdown_list_selected), LV_PART_SELECTED | LV_STATE_PRESSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class) && lv_obj_check_type(lv_obj_get_parent(obj), &lv_dropdownlist_class)) {
        return; /* Inherit styling from dropdown list */
    }

    if (lv_obj_check_type(obj, &lv_msgbox_class)) {
        lv_obj_add_style(obj, &(styles.msgbox), 0);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class) && (lv_obj_check_type(lv_obj_get_parent(obj), &lv_msgbox_class) || lv_obj_check_type(lv_obj_get_parent(obj), &lv_msgbox_content_class))) {
        lv_obj_add_style(obj, &(styles.msgbox_label), 0);
        return; /* Inherit styling from message box */
    }

    if (lv_obj_check_type(obj, &lv_btnmatrix_class) && lv_obj_check_type(lv_obj_get_parent(obj), &lv_msgbox_class)) {
        lv_obj_add_style(obj, &(styles.msgbox_btnmatrix), 0);
        lv_obj_add_style(obj, &(styles.button), LV_PART_ITEMS);
        lv_obj_add_style(obj, &(styles.button_pressed), LV_PART_ITEMS | LV_STATE_PRESSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_msgbox_backdrop_class)) {
        lv_obj_add_style(obj, &(styles.msgbox_background), 0);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class) || lv_obj_check_type(obj, &lv_spangroup_class)) {
        lv_obj_add_style(obj, &(styles.label), 0);
        return;
    }

    if (lv_obj_check_type(obj, &lv_bar_class)) {
        lv_obj_add_style(obj, &(styles.bar), 0);
        lv_obj_add_style(obj, &(styles.bar_indicator), LV_PART_INDICATOR);
        return;
    }
}


/**
 * Public functions
 */

void ul_theme_apply(const ul_theme *theme) {
    if (!theme) {
        ul_log(UL_LOG_LEVEL_ERROR, "Could not apply theme from NULL pointer");
        return;
    }

    lv_theme.disp = NULL;
    lv_theme.apply_cb = apply_theme_cb;

    current_theme = *theme;
    init_styles(theme);

    lv_obj_report_style_change(NULL);
    lv_disp_set_theme(NULL, &lv_theme);
    lv_theme_apply(lv_scr_act());
}
