/**
 * Copyright 2021 Johannes Marbach
 * Copyright 2024 Bardia Moshiri
 * Copyright 2024 David Badiei
 *
 * This file is part of lvglcharger, hereafter referred to as the program.
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
#include "lvglcharger.h"
#include "terminal.h"
#include "theme.h"
#include "themes.h"
#include "config.h"

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

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include <sys/reboot.h>
#include <sys/time.h>

/**
 * Static variables
 */

#define CMDLINE_FILE "/proc/cmdline"
#define CHARGER_STRING "androidboot.bootreason=usb"
#define BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define CHARGER_ONLINE "/sys/class/power_supply/charger/online"
#define MAX_BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/max_brightness"
#define BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/brightness"

cli_opts cli_options;
config_opts conf_opts;

bool is_alternate_theme = true;

lv_obj_t *battery_fill;
lv_obj_t *battery_label;

/**
 * Static prototypes
 */

/**
 * Set the UI theme.
 *
 * @param is_dark true if the dark theme should be applied, false if the light theme should be applied
 */
static void set_theme(bool is_dark);

/**
 * Handle termination signals sent to the process.
 *
 * @param signum the signal's number
 */
static void sigaction_handler(int signum);

/**
 * Return current battery capacity
 */
static int read_battery_capacity(void);

/**
 * Update the battery level
 *
 * @param *arg is unused
 */
static void *update_battery_level(void *arg);

/**
 * Check charger status, if device stopped charging, exit out
 */
static void check_charger_status();

/**
 * Check charger status repeatedly
 *
 * @param *arg is unused
 */
static void *check_charger(void* arg);

/**
 * Returns 0 if device is in charger mode
 */
static int bootreason_charger();

/**
 * Lowers the brightness to 1/4th of max_brightness
 */
static void adjust_backlight();

/**
 * Static functions
 */

static void set_theme(bool is_alternate) {
    theme_apply(&(themes_themes[is_alternate ? conf_opts.theme.alternate_id : conf_opts.theme.default_id]));
}

static void sigaction_handler(int signum) {
    LV_UNUSED(signum);
    terminal_reset_current_terminal();
    exit(0);
}

static int read_battery_capacity(void) {
    FILE *file = fopen(BATTERY_CAPACITY, "r");
    if (file == NULL) {
        perror("Failed to open capacity file");
        return -1;
    }

    int capacity;
    fscanf(file, "%d", &capacity);
    fclose(file);

    return capacity;
}

static void *update_battery_level(void *arg) {
    (void)arg; // unused, don't throw a warning

    while (1) {
        int capacity = read_battery_capacity();
        if (capacity >= 0 && capacity <= 100) {
            if (capacity == 100) {
                lv_obj_set_size(battery_fill, LV_PCT(100), 99 * 8); // on 100, it goes out of the border radius, because of rounded corners, don't go above 99
            // levels 1 to 12 are a little different as we have rounded corners and need to take care of it
            } else if (capacity == 1) {
                lv_obj_set_size(battery_fill, LV_PCT(80), capacity * 8);
            } else if (capacity == 2) {
                lv_obj_set_size(battery_fill, LV_PCT(82), capacity * 8);
            } else if (capacity == 3) {
                lv_obj_set_size(battery_fill, LV_PCT(83), capacity * 8);
            } else if (capacity == 4) {
                lv_obj_set_size(battery_fill, LV_PCT(84), capacity * 8);
            } else if (capacity == 5) {
                lv_obj_set_size(battery_fill, LV_PCT(86), capacity * 8);
            } else if (capacity == 6) {
                lv_obj_set_size(battery_fill, LV_PCT(88), capacity * 8);
            } else if (capacity == 7) {
                lv_obj_set_size(battery_fill, LV_PCT(89), capacity * 8);
            } else if (capacity == 8) {
                lv_obj_set_size(battery_fill, LV_PCT(91), capacity * 8);
            } else if (capacity == 9) {
                lv_obj_set_size(battery_fill, LV_PCT(92), capacity * 8);
            } else if (capacity == 10) {
                lv_obj_set_size(battery_fill, LV_PCT(94), capacity * 8);
            } else if (capacity == 11) {
                lv_obj_set_size(battery_fill, LV_PCT(96), capacity * 8);
            } else if (capacity == 12) {
                lv_obj_set_size(battery_fill, LV_PCT(98), capacity * 8);
            } else {
                lv_obj_set_size(battery_fill, LV_PCT(100), capacity * 8);
            }

            lv_label_set_text_fmt(battery_label, "%d%%", capacity);
            lv_obj_align(battery_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
        }

        sleep(1);
    }

    return NULL;
}

static void check_charger_status() {
    FILE* file = fopen(CHARGER_ONLINE, "r");
    if (file == NULL) {
        perror("Failed to open file");
        exit(1);
    }

    int status;
    fscanf(file, "%d", &status);
    fclose(file);

    if (status == 0) {
        printf("Charger is offline. exiting\n");
        exit(0);
    }
}

static void *check_charger(void* arg) {
    (void)arg; // unused, don't throw a warning

    while (1) {
        check_charger_status();
        sleep(1);
    }
    return NULL;
}

static void adjust_backlight() {
    FILE* file = fopen(MAX_BRIGHTNESS_PATH, "r");
    if (file == NULL) {
        printf("Failed to open max brightness file\n");
        return;
    }

    int max_brightness;
    if (fscanf(file, "%d", &max_brightness) != 1) {
        printf("Failed to read max brightness\n");
        fclose(file);
        return;
    }
    fclose(file);

    int new_brightness = max_brightness / 4;

    file = fopen(BRIGHTNESS_PATH, "w");
    if (file == NULL) {
        printf("Failed to open brightness file\n");
        return;
    }

    if (fprintf(file, "%d", new_brightness) < 0) {
        printf("Failed to write new brightness\n");
        fclose(file);
        return;
    }
    fclose(file);

    printf("Backlight adjusted to %d\n", new_brightness);
}

static int bootreason_charger() {
    FILE *file;
    char *buffer = NULL;
    size_t size = 0;

    file = fopen(CMDLINE_FILE, "r");
    if (file == NULL) {
        perror("Failed to open file");
        return 0;
    }

    if (getline(&buffer, &size, file) == -1) {
        perror("Failed to read file");
        fclose(file);
        return 0;
    }

    fclose(file);

    if (strstr(buffer, CHARGER_STRING) != NULL) {
        free(buffer);
        return 1;
    }

    free(buffer);
    return 0;
}


/**
 * Main
 */

int main(int argc, char *argv[]) {
    if (!bootreason_charger()) {
        printf("Device is not in charger mode\n");
        exit(0);
    }

    check_charger_status();

    struct stat buffer;
    if (stat("/usr/bin/plymouth", &buffer) == 0) { // plymouth will block minui
        system("plymouth quit");
    }

    /* Parse command line options */
    cli_parse_opts(argc, argv, &cli_options);

    /* Parse config files */
    config_parse(cli_options.config_files, cli_options.num_config_files, &conf_opts);

    /* Prepare current TTY and clean up on termination */
    terminal_prepare_current_terminal();
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigaction_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    /* Initialise LVGL and set up logging callback */
    lv_init();

    /* Initialise display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    /* Initialise framebuffer driver and query display size */
    uint32_t hor_res = 0;
    uint32_t ver_res = 0;
    uint32_t dpi = 0;

    switch (conf_opts.general.backend) {
#if USE_FBDEV
    case BACKENDS_BACKEND_FBDEV:
        fbdev_init();
        fbdev_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = fbdev_flush;
        break;
#endif /* USE_FBDEV */
#if USE_DRM
    case BACKENDS_BACKEND_DRM:
        drm_init();
        drm_get_sizes((lv_coord_t *)&hor_res, (lv_coord_t *)&ver_res, &dpi);
        disp_drv.flush_cb = drm_flush;
        break;
#endif /* USE_DRM */
#if USE_MINUI
    case BACKENDS_BACKEND_MINUI:
        minui_init();
        minui_get_sizes(&hor_res, &ver_res, &dpi);
        disp_drv.flush_cb = minui_flush;
        break;
#endif /* USE_MINUI */
    default:
        printf("Unable to find suitable backend\n");
        exit(EXIT_FAILURE);
    }

    /* Override display parameters with command line options if necessary */
    if (cli_options.hor_res > 0) {
        hor_res = cli_options.hor_res;
    }
    if (cli_options.ver_res > 0) {
        ver_res = cli_options.ver_res;
    }
    if (cli_options.dpi > 0) {
        dpi = cli_options.dpi;
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
    disp_drv.offset_x = cli_options.x_offset;
    disp_drv.offset_y = cli_options.y_offset;
    disp_drv.dpi = dpi;
    lv_disp_drv_register(&disp_drv);

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);

    set_theme(0);

    /* Battery */
    lv_obj_t *battery = lv_obj_create(lv_scr_act());
    lv_obj_set_size(battery, 400, 800);
    lv_obj_align(battery, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_border_width(battery, 5, LV_PART_MAIN);
    lv_obj_set_style_border_color(battery, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_radius(battery, 60, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery, 0, LV_PART_MAIN);

    /* Green fill */
    battery_fill = lv_obj_create(battery);
    lv_obj_set_size(battery_fill, LV_PCT(100), LV_PCT(0));
    lv_obj_align(battery_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(battery_fill, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(battery_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_fill, 55, LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_fill, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_fill, 5, LV_PART_MAIN);

    /* Battery's tip */
    lv_obj_t *battery_tip = lv_obj_create(lv_scr_act());
    lv_obj_set_size(battery_tip, 140, 45);
    lv_obj_align(battery_tip, LV_ALIGN_TOP_MID, 0, 745);
    lv_obj_set_style_border_width(battery_tip, 5, LV_PART_MAIN);
    lv_obj_set_style_border_color(battery_tip, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_radius(battery_tip, 30, LV_PART_MAIN);

    /* Battery text label */
    battery_label = lv_label_create(battery);
    lv_label_set_text(battery_label, "");
    lv_obj_align(battery_label, LV_ALIGN_CENTER, 0, 0);
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &lv_font_montserrat_48);
    lv_obj_add_style(battery_label, &style, 0);

    adjust_backlight();

    pthread_t battery_thread;
    pthread_create(&battery_thread, NULL, update_battery_level, NULL);

    pthread_t charger_thread;
    pthread_create(&charger_thread, NULL, check_charger, NULL);

    /* Run lvgl in "tickless" mode */
    while(1) {
        lv_task_handler();
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
uint32_t get_tick(void) {
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
