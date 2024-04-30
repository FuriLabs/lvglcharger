/**
 * Copyright 2021 Johannes Marbach
 * Copyright 2024 Bardia Moshiri
 * Copyright 2024 David Badiei
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


#include "backends.h"
#include "command_line.h"
#include "config.h"
#include "indev.h"
#include "log.h"
#include "furios-recovery.h"
#include "terminal.h"
#include "theme.h"
#include "themes.h"
#include "lvm.h"

#include "lv_drv_conf.h"

#if USE_FBDEV
#include "lv_drivers/display/fbdev.h"
#endif /* USE_FBDEV */
#if USE_DRM
#include "lv_drivers/display/drm.h"
#endif /* USE_DRM */
#if USE_MINUI
#include "lv_drivers/display/minui.h"
#endif /* USE_MINUI */

#include "lvgl/lvgl.h"

#include "squeek2lvgl/sq2lv.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/reboot.h>
#include <sys/time.h>

#define NUM_IMAGES 1

/**
 * Static variables
 */

ul_cli_opts cli_opts;
ul_config_opts conf_opts;

bool is_alternate_theme = true;
bool is_password_obscured = true;
bool is_keyboard_hidden = true;

lv_obj_t *keyboard = NULL;

LV_IMG_DECLARE(furilabs_white)
LV_IMG_DECLARE(furilabs_black)

const void *darkmode_imgs[] = {&furilabs_white};
const void *lightmode_imgs[] = {&furilabs_black};

/*
   0: FuriLabs logo
*/
lv_obj_t* images[1];

/**
 * Static prototypes
 */

/**
 * Handle LV_EVENT_CLICKED events from the theme toggle button.
 *
 * @param event the event object
 */
static void toggle_theme_btn_clicked_cb(lv_event_t *event);

/**
 * Toggle between the light and dark theme.
 */
static void toggle_theme(void);

/**
 * Set the UI theme.
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void set_theme(bool is_dark);

/**
 * Set the image mode
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void update_image_mode(bool is_dark);

/**
 * Handle LV_EVENT_CLICKED events from the show/hide password toggle button.
 *
 * @param event the event object
 */
static void toggle_pw_btn_clicked_cb(lv_event_t *event);

/**
 * Toggle between showing and hiding the password.
 */
static void toggle_password_obscured(void);

/**
 * Show / hide the password.
 *
 * @param is_hidden true if the password should be hidden, false if it should be shown
 */
static void set_password_obscured(bool is_obscured);

/**
 * Handle LV_EVENT_CLICKED events from the show/hide keyboard toggle button.
 *
 * @param event the event object
 */
static void toggle_kb_btn_clicked_cb(lv_event_t *event);

/**
 * Toggle between showing and hiding the keyboard.
 */
static void toggle_keyboard_hidden(void);

/**
 * Show / hide the keyboard
 *
 * @param is_hidden true if the keyboard should be hidden, false if it should be shown
 */
static void set_keyboard_hidden(bool is_hidden);

/**
 * Callback for the keyboard's vertical slide in / out animation.
 *
 * @param obj keyboard widget
 * @param value y position
 */
static void keyboard_anim_y_cb(void *obj, int32_t value);

/**
 * Handle LV_EVENT_CLICKED events from the shutdown button.
 *
 * @param event the event object
 */
static void shutdown_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the shutdown message box.
 *
 * @param event the event object
 */
static void shutdown_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the reboot button.
 *
 * @param event the event object
 */
static void reboot_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the reboot message box.
 *
 * @param event the event object
 */
static void reboot_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the factory reset button.
 */
static void factory_reset_btn_clicked_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the factory reset confirmation button.
 */
static void perform_factory_reset(lv_timer_t *timer);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the factory reset message box.
 *
 * @param event the event object
 */
static void factory_reset_mbox_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_CLICKED events from the factory reset failed messsage box
 */
static void close_mbox_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_VALUE_CHANGED events from the keyboard widget.
 *
 * @param event the event object
 */
static void keyboard_value_changed_cb(lv_event_t *event);

/**
 * Handle LV_EVENT_READY events from the textarea widget.
 *
 * @param event the event object
 */
static void textarea_ready_cb(lv_event_t *event);

/**
 * Check password against LVM
 *
 * @param textarea the textarea widget
 */
static void check_password(lv_obj_t *textarea);

/**
 * Handle LV_EVENT_CLICKED events from the password check
 */
static void factory_reset_password(lv_timer_t *timer);

/**
 * Factory resets the device
 */
static int factory_reset(void);

/**
 * Decrypts the device
 */
static void decrypt(void);

/**
 * Reboots the device.
 */
static void reboot_device(void);

/**
 * Shuts down the device.
 */
static void shutdown(void);

/**
 * Handle termination signals sent to the process.
 *
 * @param signum the signal's number
 */
static void sigaction_handler(int signum);


/**
 * Static functions
 */

static void toggle_theme_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_theme();
}

static void toggle_theme(void) {
    is_alternate_theme = !is_alternate_theme;
 
    update_image_mode(is_alternate_theme);
    set_theme(is_alternate_theme);
}

static void update_image_mode(bool is_alternate) {
    for (int i = 0; i < NUM_IMAGES; i++)
        lv_img_set_src(images[i], is_alternate ? lightmode_imgs[i] : darkmode_imgs[i]);
}

static void set_theme(bool is_alternate) {
    ul_theme_apply(&(ul_themes_themes[is_alternate ? conf_opts.theme.alternate_id : conf_opts.theme.default_id]));
}

static void toggle_pw_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_password_obscured();
}

static void toggle_password_obscured(void) {
    is_password_obscured = !is_password_obscured;
    set_password_obscured(is_password_obscured);
}

static void set_password_obscured(bool is_obscured) {
    lv_obj_t *textarea = lv_keyboard_get_textarea(keyboard);
    lv_textarea_set_password_mode(textarea, is_obscured);
}

static void toggle_kb_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    toggle_keyboard_hidden();
}

static void toggle_keyboard_hidden(void) {
    is_keyboard_hidden = !is_keyboard_hidden;
    set_keyboard_hidden(is_keyboard_hidden);
}

static void set_keyboard_hidden(bool is_hidden) {
    if (!conf_opts.general.animations) {
        lv_obj_set_y(keyboard, is_hidden ? lv_obj_get_height(keyboard) : 0);
        return;
    }

    lv_anim_t keyboard_anim;
    lv_anim_init(&keyboard_anim);
    lv_anim_set_var(&keyboard_anim, keyboard);
    lv_anim_set_values(&keyboard_anim, is_hidden ? 0 : lv_obj_get_height(keyboard), is_hidden ? lv_obj_get_y(keyboard) : 0);
    lv_anim_set_path_cb(&keyboard_anim, lv_anim_path_ease_out);
    lv_anim_set_time(&keyboard_anim, 500);
    lv_anim_set_exec_cb(&keyboard_anim, keyboard_anim_y_cb);
    lv_anim_start(&keyboard_anim);
}

static void keyboard_anim_y_cb(void *obj, int32_t value) {
    lv_obj_set_y(obj, value);
}

static void shutdown_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = { "Yes", "No", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Shutdown device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, shutdown_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void shutdown_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        shutdown();
    }
    lv_msgbox_close(mbox);
}

static void reboot_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = { "Yes", "No", "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Reboot device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, reboot_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void reboot_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        reboot_device();
    }
    lv_msgbox_close(mbox);
}

static void factory_reset_btn_clicked_cb(lv_event_t *event) {
    LV_UNUSED(event);
    static const char *btns[] = {"Yes", "No", ""};
    lv_obj_t *mbox = lv_msgbox_create(NULL, NULL, "Factory reset device?", btns, false);
    lv_obj_set_size(mbox, 400, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(mbox, factory_reset_mbox_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
}

static void factory_reset_mbox_value_changed_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
        lv_msgbox_close(mbox);

        lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Resetting device...", NULL, false);
        lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_center(resetting_mbox);

        lv_timer_t *timer = lv_timer_create(perform_factory_reset, 500, resetting_mbox);
        lv_timer_set_repeat_count(timer, 1);
    } else {
        lv_msgbox_close(mbox);
    }
}

static void perform_factory_reset(lv_timer_t *timer) {
    lv_obj_t *resetting_mbox = (lv_obj_t *)timer->user_data;
    const char *lvm_device_path = "/dev/droidian/droidian-persistent";
    size_t print_bytes = 64;
    int result = is_lv_encrypted_with_luks(lvm_device_path, print_bytes);

    result = 1;
    if (result == -1) {
        // rootfs.img in data? well we can't reset that for now
        lv_msgbox_close(resetting_mbox);
        static const char *btns[] = {"OK", ""};
        lv_obj_t *fail_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
        lv_obj_set_size(fail_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(fail_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(fail_mbox);
    } else {
        if (result == 1) {
            decrypt(); // Decrypt LVM if necessary
            lv_msgbox_close(resetting_mbox);
            return;
        }

        // LVM is not encrypted or unlocked, we can continue
        int factory_reset_result = factory_reset();

        lv_msgbox_close(resetting_mbox);

        if (factory_reset_result == 0) {
            static const char *btns[] = {"OK", ""};
            lv_obj_t *success_mbox = lv_msgbox_create(NULL, NULL, "Successfully reset to factory settings", btns, false);
            lv_obj_set_size(success_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_add_event_cb(success_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(success_mbox);
        } else {
            static const char *btns[] = {"OK", ""};
            lv_obj_t *error_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
            lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_add_event_cb(error_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(error_mbox);
        }
    }
}

static void close_mbox_cb(lv_event_t *event) {
    lv_obj_t *mbox = lv_event_get_current_target(event);

    // maybe do something instead of sleep?
    sleep(3);
    reboot_device();
    lv_msgbox_close(mbox);
}

static void keyboard_value_changed_cb(lv_event_t *event) {
    lv_obj_t *kb = lv_event_get_target(event);

    uint16_t btn_id = lv_btnmatrix_get_selected_btn(kb);
    if (btn_id == LV_BTNMATRIX_BTN_NONE) {
        return;
    }

    if (sq2lv_is_layer_switcher(kb, btn_id)) {
        sq2lv_switch_layer(kb, btn_id);
        return;
    }

    lv_keyboard_def_event_cb(event);
}

static void textarea_ready_cb(lv_event_t *event) {
    check_password(lv_event_get_target(event));
}

static void check_password(lv_obj_t *textarea) {
    const char *password = lv_textarea_get_text(textarea);

    static int attempt_count = 0;

    int result = mount_luks_lvm(password);
    if (result == EXIT_SUCCESS) {
        lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Resetting device...", NULL, false);
        lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_center(resetting_mbox);
        lv_timer_t *timer = lv_timer_create(factory_reset_password, 500, resetting_mbox);
        lv_timer_set_repeat_count(timer, 1);
    } else if (result == 2) {
        attempt_count++;
        if (attempt_count >= 3) {
            lv_obj_t *resetting_mbox = lv_msgbox_create(NULL, NULL, "Maximum password attempt reached.", NULL, false);
            lv_obj_set_size(resetting_mbox, 400, LV_SIZE_CONTENT);
            lv_obj_center(resetting_mbox);
        }
    }
}

static void factory_reset_password(lv_timer_t *timer) {
    lv_obj_t *resetting_mbox = (lv_obj_t *)timer->user_data;

    int factory_reset_result = factory_reset();

    lv_msgbox_close(resetting_mbox);

    if (factory_reset_result == 0) {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *success_mbox = lv_msgbox_create(NULL, NULL, "Successfully reset to factory settings", btns, false);
        lv_obj_set_size(success_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(success_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(success_mbox);
    } else {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *error_mbox = lv_msgbox_create(NULL, NULL, "Failed to factory reset", btns, false);
        lv_obj_set_size(error_mbox, 400, LV_SIZE_CONTENT);
        lv_obj_add_event_cb(error_mbox, close_mbox_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(error_mbox);
    }
}

static int factory_reset(void) {
    // the reason most things here are system calls is because our ramdisk must be small and more libraries we link against the bigger the binary will get
    // here, we're using pre existing binaries in the ramdisk to not take too much storage in the ramdisk
    char cmd[256];
    int result;

    snprintf(cmd, sizeof(cmd), "dmsetup create --concise \"$(parse-android-dynparts /dev/disk/by-partlabel/super)\"");
    system(cmd);

    mkdir("/system_mnt", 0755);
    result = mount("/dev/mapper/dynpart-system_a", "/system_mnt", "ext4", 0, NULL);
    if (result != 0) {
        printf("Failed to mount dynpart-system_a\n");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "zstd -f -d /system_mnt/userdata-raw.img.zst -o /userdata-raw.img");
    result = system(cmd);
    if (result != 0) {
        printf("Failed to unpack userdata-raw.img.zst\n");
        umount("/system_mnt");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "dd if=/userdata-raw.img of=/dev/disk/by-partlabel/userdata");
    result = system(cmd);
    if (result != 0) {
        printf("Failed to write userdata\n");
        umount("/system_mnt");
        return -1;
    }

    umount("/system_mnt");

    return 0;
}

static void decrypt(void) {
    uint32_t hor_res = 0;
    uint32_t ver_res = 0;
    uint32_t dpi = 0;

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case UL_BACKENDS_BACKEND_FBDEV:
        fbdev_get_sizes(&hor_res, &ver_res, &dpi);
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case UL_BACKENDS_BACKEND_DRM:
        drm_get_sizes((lv_coord_t *)&hor_res, (lv_coord_t *)&ver_res, &dpi);
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case UL_BACKENDS_BACKEND_MINUI:
        minui_get_sizes(&hor_res, &ver_res, &dpi);
        break;
#endif /* USE_MINUI */
    default:
        ul_log(UL_LOG_LEVEL_ERROR, "Unable to find suitable backend");
        exit(EXIT_FAILURE);
    }

    is_keyboard_hidden = false;

    /* Prevent scrolling when keyboard is off-screen */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* Figure out a few numbers for sizing and positioning */
    const int keyboard_height = ver_res > hor_res ? ver_res / 3 : ver_res / 2;
    const int padding = keyboard_height / 8;
    const int label_width = hor_res - 2 * padding;
    const int textarea_container_max_width = LV_MIN(hor_res, ver_res);

    /* Main flexbox */
    lv_obj_t *container = lv_obj_create(lv_scr_act());
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(container, LV_PCT(100), ver_res - keyboard_height);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_set_style_pad_row(container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(container, padding, LV_PART_MAIN);

    /* Label container */
    lv_obj_t *label_container = lv_obj_create(container);
    lv_obj_set_size(label_container, label_width, LV_PCT(100));
    lv_obj_set_flex_grow(label_container, 1);

    /* Label */
    lv_obj_t *spangroup = lv_spangroup_create(label_container);
    lv_spangroup_set_align(spangroup, LV_TEXT_ALIGN_CENTER);
    lv_spangroup_set_mode(spangroup, LV_SPAN_MODE_BREAK);
    lv_spangroup_set_overflow(spangroup, LV_SPAN_OVERFLOW_ELLIPSIS);
    lv_span_t *span1 = lv_spangroup_new_span(spangroup);

    /* Label text */
    lv_span_set_text(span1, "Password required for factory reset");

    /* Size label to content */
    const lv_coord_t label_height = lv_spangroup_get_expand_height(spangroup, label_width);
    lv_obj_set_style_max_height(spangroup, LV_PCT(100), LV_PART_MAIN);
    lv_obj_set_size(spangroup, label_width, label_height);
    lv_obj_set_align(spangroup, LV_ALIGN_BOTTOM_MID);

    /* Textarea flexbox */
    lv_obj_t *textarea_container = lv_obj_create(container);
    lv_obj_set_size(textarea_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(textarea_container, textarea_container_max_width, LV_PART_MAIN);
    lv_obj_set_flex_flow(textarea_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(textarea_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(textarea_container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_right(textarea_container, padding, LV_PART_MAIN);

    /* Textarea */
    lv_obj_t *textarea = lv_textarea_create(textarea_container);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_password_mode(textarea, true);
    lv_textarea_set_password_bullet(textarea, conf_opts.textarea.bullet);
    lv_textarea_set_placeholder_text(textarea, "Enter password...");
    lv_obj_add_event_cb(textarea, textarea_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_set_flex_grow(textarea, 1);
    lv_obj_add_state(textarea, LV_STATE_FOCUSED);

    /* Route physical keyboard input into textarea */
    ul_indev_set_up_textarea_for_keyboard_input(textarea);

    /* Reveal / obscure password button */
    lv_obj_t *toggle_pw_btn = lv_btn_create(textarea_container);
    const int textarea_height = lv_obj_get_height(textarea);
    lv_obj_set_size(toggle_pw_btn, textarea_height, textarea_height);
    lv_obj_t *toggle_pw_btn_label = lv_label_create(toggle_pw_btn);
    lv_obj_center(toggle_pw_btn_label);
    lv_label_set_text(toggle_pw_btn_label, LV_SYMBOL_EYE_OPEN);
    lv_obj_add_event_cb(toggle_pw_btn, toggle_pw_btn_clicked_cb, LV_EVENT_CLICKED, NULL);

    /* Show / hide keyboard button */
    lv_obj_t *toggle_kb_btn = lv_btn_create(textarea_container);
    lv_obj_set_size(toggle_kb_btn, textarea_height, textarea_height);
    lv_obj_add_event_cb(toggle_kb_btn, toggle_kb_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *toggle_kb_btn_label = lv_label_create(toggle_kb_btn);
    lv_label_set_text(toggle_kb_btn_label, LV_SYMBOL_KEYBOARD);
    lv_obj_center(toggle_kb_btn_label);

    /* Hide label if it clips veritcally */
    if (label_height > lv_obj_get_height(label_container)) {
        lv_obj_set_height(spangroup, 0);
    }

    /* Keyboard (after textarea / label so that key popovers are not drawn over) */
    keyboard = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keyboard, textarea);
    lv_obj_remove_event_cb(keyboard, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(keyboard, keyboard_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_pos(keyboard, 0, is_keyboard_hidden ? keyboard_height : 0);
    lv_obj_set_size(keyboard, hor_res, keyboard_height);
    ul_theme_prepare_keyboard(keyboard);

    /* Apply textarea options */
    set_password_obscured(conf_opts.textarea.obscured);
}

static void reboot_device(void) {
    sync();
    reboot(RB_AUTOBOOT);
}

static void shutdown(void) {
    sync();
    reboot(RB_POWER_OFF);
}

static void sigaction_handler(int signum) {
    LV_UNUSED(signum);
    ul_terminal_reset_current_terminal();
    exit(0);
}

/**
 * Main
 */

int main(int argc, char *argv[]) {
    /* Parse command line options */
    ul_cli_parse_opts(argc, argv, &cli_opts);

    /* Set up log level */
    if (cli_opts.verbose) {
        ul_log_set_level(UL_LOG_LEVEL_VERBOSE);
    }

    /* Announce ourselves */
    ul_log(UL_LOG_LEVEL_VERBOSE, "furios-recovery %s", UL_VERSION);

    /* Parse config files */
    ul_config_parse(cli_opts.config_files, cli_opts.num_config_files, &conf_opts);

    /* Prepare current TTY and clean up on termination */
    ul_terminal_prepare_current_terminal();
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigaction_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    /* Initialise LVGL and set up logging callback */
    lv_init();

    lv_log_register_print_cb(ul_log_print_cb);

    /* Initialise display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Initialise framebuffer driver and query display size */
    uint32_t hor_res = 0;
    uint32_t ver_res = 0;
    uint32_t dpi = 0;

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case UL_BACKENDS_BACKEND_FBDEV:
        fbdev_init();
        fbdev_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = fbdev_flush;
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case UL_BACKENDS_BACKEND_DRM:
        drm_init();
        drm_get_sizes((lv_coord_t *)&hor_res, (lv_coord_t *)&ver_res, &dpi);
        disp_drv.flush_cb = drm_flush;
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case UL_BACKENDS_BACKEND_MINUI:
        minui_init();
        minui_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = minui_flush;
        break;
#endif /* USE_MINUI */
    default:
        ul_log(UL_LOG_LEVEL_ERROR, "Unable to find suitable backend");
        exit(EXIT_FAILURE);
    }

    /* Override display parameters with command line options if necessary */
    if (cli_opts.hor_res > 0) {
        hor_res = cli_opts.hor_res;
    }
    if (cli_opts.ver_res > 0) {
        ver_res = cli_opts.ver_res;
    }
    if (cli_opts.dpi > 0) {
        dpi = cli_opts.dpi;
    }

    /* Prepare display buffer */
    const size_t buf_size = hor_res * ver_res / 10; /* At least 1/10 of the display size is recommended */
    lv_disp_draw_buf_t disp_buf;
    lv_color_t *buf = (lv_color_t *)malloc(buf_size * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, buf_size);

    /* Register display driver */
    disp_drv.draw_buf = &disp_buf;
    disp_drv.hor_res = hor_res;
    disp_drv.ver_res = ver_res;
    disp_drv.offset_x = cli_opts.x_offset;
    disp_drv.offset_y = cli_opts.y_offset;
    disp_drv.dpi = dpi;
    lv_disp_drv_register(&disp_drv);

    /* Connect input devices */
    ul_indev_auto_connect(conf_opts.input.keyboard, conf_opts.input.pointer, conf_opts.input.touchscreen);
    ul_indev_set_up_mouse_cursor();

    /* Initialise theme */
    set_theme(is_alternate_theme);

    /* Prevent scrolling when keyboard is off-screen */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* Figure out a few numbers for sizing and positioning */
    const int keyboard_height = ver_res > hor_res ? ver_res / 3 : ver_res / 2;
    const int padding = keyboard_height / 8;
    const int label_width = hor_res - 2 * padding;

    /* Main flexbox */
    lv_obj_t *container = lv_obj_create(lv_scr_act());
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_size(container, LV_PCT(100), ver_res - keyboard_height);
    lv_obj_set_pos(container, 0, 0);
    lv_obj_set_style_pad_row(container, padding, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(container, padding, LV_PART_MAIN);

    /* Label container */
    lv_obj_t *label_container = lv_obj_create(container);
    lv_obj_set_size(label_container, label_width, LV_PCT(100));
    lv_obj_set_flex_grow(label_container, 1);

    /* Top label */
    lv_obj_t *top_label_container = lv_obj_create(lv_scr_act());
    lv_obj_set_width(top_label_container, LV_PCT(100));
    lv_obj_set_height(top_label_container, LV_SIZE_CONTENT);
    lv_obj_set_align(top_label_container, LV_ALIGN_BOTTOM_MID);

    /* Top label text */
    lv_obj_t *furios_label = lv_label_create(top_label_container);
    lv_label_set_text(furios_label, "FuriOS Recovery");
    lv_obj_align(furios_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Initialize images */
    for (int i = 0; i < NUM_IMAGES; i++)
        images[i] = lv_img_create(lv_scr_act());

    /* Furilabs logo */
    lv_obj_align(images[0], LV_ALIGN_TOP_MID, 0, 100);
    
    /* Set image mode */
    update_image_mode(is_alternate_theme);

    /* Reboot button */
    lv_obj_t *reboot_btn = lv_btn_create(label_container);
    lv_obj_set_width(reboot_btn, LV_PCT(100));
    lv_obj_set_height(reboot_btn, 100);
    lv_obj_t *reboot_btn_label = lv_label_create(reboot_btn);
    lv_label_set_text(reboot_btn_label, "Reboot");
    lv_obj_add_event_cb(reboot_btn, reboot_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(reboot_btn, LV_ALIGN_TOP_MID, 0, 500);
    lv_obj_set_flex_flow(reboot_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(reboot_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Shutdown middle button */
    lv_obj_t *shutdown_middle_btn = lv_btn_create(label_container);
    lv_obj_set_width(shutdown_middle_btn, LV_PCT(100));
    lv_obj_set_height(shutdown_middle_btn, 100);
    lv_obj_t *shutdown_middle_btn_label = lv_label_create(shutdown_middle_btn);
    lv_label_set_text(shutdown_middle_btn_label, "Shutdown");
    lv_obj_add_event_cb(shutdown_middle_btn, shutdown_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(shutdown_middle_btn, LV_ALIGN_TOP_MID, 0, 600);
    lv_obj_set_flex_flow(shutdown_middle_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(shutdown_middle_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Factory reset button */
    lv_obj_t *factory_reset_btn = lv_btn_create(label_container);
    lv_obj_set_width(factory_reset_btn, LV_PCT(100));
    lv_obj_set_height(factory_reset_btn, 100);
    lv_obj_t *factory_reset_btn_label = lv_label_create(factory_reset_btn);
    lv_label_set_text(factory_reset_btn_label, "Factory Reset");
    lv_obj_add_event_cb(factory_reset_btn, factory_reset_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(factory_reset_btn, LV_ALIGN_TOP_MID, 0, 700);
    lv_obj_set_flex_flow(factory_reset_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(factory_reset_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* theme button */
    lv_obj_t *theme_btn = lv_btn_create(label_container);
    lv_obj_set_width(theme_btn, LV_PCT(100));
    lv_obj_set_height(theme_btn, 100);
    lv_obj_t *theme_btn_label = lv_label_create(theme_btn);
    lv_label_set_text(theme_btn_label, "Toggle Theme");
    lv_obj_add_event_cb(theme_btn, toggle_theme_btn_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(theme_btn, LV_ALIGN_TOP_MID, 0, 800);
    lv_obj_set_flex_flow(theme_btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(theme_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Run lvgl in "tickless" mode */
    uint32_t timeout = conf_opts.general.timeout * 1000; /* ms */
    while(1) {
        if (!timeout || lv_disp_get_inactive_time(NULL) < timeout) {
            lv_task_handler();
        } else if (timeout) {
            shutdown();
        }
        usleep(5000);
    }

    return 0;
}


/**
 * Tick generation
 */

/**
 * Generate tick for LVGL.
 * 
 * @return tick in ms
 */
uint32_t ul_get_tick(void) {
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
