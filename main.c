/**
 * Copyright 2021 Johannes Marbach
 * Copyright 2024 David Badiei
 * Copyright 2025 Bardia Moshiri
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
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#include <sys/reboot.h>
#include <sys/socket.h>

#include <linux/netlink.h>
#include <libinput.h>
#include <linux/input.h>

/**
 * Static variables
 */

#define CMDLINE_FILE "/proc/cmdline"
#define CHARGER_STRING "androidboot.bootreason=usb"
#define BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define AC_ONLINE_PATH  "/sys/class/power_supply/ac/online"
#define USB_ONLINE_PATH "/sys/class/power_supply/usb/online"
#define MAX_BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/max_brightness"
#define BRIGHTNESS_PATH "/sys/class/leds/lcd-backlight/brightness"

cli_opts cli_options;
config_opts conf_opts;

bool is_alternate_theme = true;
bool screen_is_on = true;
int max_brightness = 0;

lv_obj_t *battery_fill;
lv_obj_t *battery_label;

static int current_capacity = -1;
static int current_charger_online = 1;

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
 * Return current battery capacity (0-100), or -1 on error
 */
static int read_battery_capacity(void);

/**
 * Return current charger status (1 online / 0 offline)
 */
static int read_charger_online(void);

/**
 * Apply capacity value to UI widgets
 *
 * @param capacity Percentage 0-100
 */
static void ui_update_capacity(int capacity);

/**
 * Update charger state
 *
 * @param online 1 if charger online, 0 if offline
 */
static void handle_charger_state(int online);

/**
 * libinput and screen power thread
 *
 * @param *arg is unused
 */
static void* monitor_power_key(void *arg);

/**
 * Set initial brightness and toggle brightness later.
 *
 * @param brightness The brightness value to set
 */
static void set_brightness(int brightness);

/**
 * Toggle screen state (on/off) by adjusting brightness
 */
static void toggle_screen(void);

/**
 * Initialize libinput and monitor for power key events
 *
 * @param path Device path to open
 * @param flags Open flags
 * @param user_data User data pointer (user_data is unused)
 * @return File descriptor or negative error code
 */
static int open_restricted(const char *path, int flags, void *user_data);

/**
 * Close callback for libinput
 *
 * @param fd File descriptor to close
 * @param user_data User data pointer (user_data is unused)
 */
static void close_restricted(int fd, void *user_data);

/**
 * Check if a file is an input device
 *
 * @param path Path to the input device
 * @return 1 if it's an input device, 0 otherwise
 */
static int is_input_device(const char *path);

/**
 * Returns 1 if bootreason == charger mode, 0 otherwise
 */
static int bootreason_charger(void);

/**
 * Listen to kernel uevents via netlink
 *
 * @param arg unused
 */
static void* monitor_power_supply_uevent(void *arg);

/**
 * Parse one uevent message for POWER_SUPPLY_ keys
 *
 * @param msg nul-separated env block from netlink
 * @param len length of block
 */
static void parse_power_supply_event(const char *msg, ssize_t len);

/**
 * Static data / structs
 */

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

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
    if (fscanf(file, "%d", &capacity) != 1) {
        fclose(file);
        return -1;
    }
    fclose(file);

    return capacity;
}

static int read_charger_online(void) {
    int ac_online  = -1;
    int usb_online = -1;

    FILE *file;

    file = fopen(AC_ONLINE_PATH, "r");
    if (file) {
        if (fscanf(file, "%d", &ac_online) != 1)
            ac_online = -1;
        fclose(file);
    }

    file = fopen(USB_ONLINE_PATH, "r");
    if (file) {
        if (fscanf(file, "%d", &usb_online) != 1)
            usb_online = -1;
        fclose(file);
    }

    /* If either one is online, treat as charger connected */
    if (ac_online == 1 || usb_online == 1)
        return 1;

    if (ac_online == 0 && usb_online == 0)
        return 0;

    return 1;
}

static void ui_update_capacity(int capacity) {
    if (capacity < 0 || capacity > 100)
        return;

    current_capacity = capacity;

    if (capacity == 100)
        lv_obj_set_size(battery_fill, LV_PCT(100), 99 * 8); /* on 100, it goes out of the border radius because of rounded corners, don't go above 99 */
    /* on 100, it goes out of the border radius because of rounded corners, don't go above 99 */
    else if (capacity == 1)
        lv_obj_set_size(battery_fill, LV_PCT(80), capacity * 8);
    else if (capacity == 2)
        lv_obj_set_size(battery_fill, LV_PCT(82), capacity * 8);
    else if (capacity == 3)
        lv_obj_set_size(battery_fill, LV_PCT(83), capacity * 8);
    else if (capacity == 4)
        lv_obj_set_size(battery_fill, LV_PCT(84), capacity * 8);
    else if (capacity == 5)
        lv_obj_set_size(battery_fill, LV_PCT(86), capacity * 8);
    else if (capacity == 6)
        lv_obj_set_size(battery_fill, LV_PCT(88), capacity * 8);
    else if (capacity == 7)
        lv_obj_set_size(battery_fill, LV_PCT(89), capacity * 8);
    else if (capacity == 8)
        lv_obj_set_size(battery_fill, LV_PCT(91), capacity * 8);
    else if (capacity == 9)
        lv_obj_set_size(battery_fill, LV_PCT(92), capacity * 8);
    else if (capacity == 10)
        lv_obj_set_size(battery_fill, LV_PCT(94), capacity * 8);
    else if (capacity == 11)
        lv_obj_set_size(battery_fill, LV_PCT(96), capacity * 8);
    else if (capacity == 12)
        lv_obj_set_size(battery_fill, LV_PCT(98), capacity * 8);
    else
        lv_obj_set_size(battery_fill, LV_PCT(100), capacity * 8);

    lv_label_set_text_fmt(battery_label, "%d%%", capacity);
    lv_obj_align(battery_fill, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void handle_charger_state(int online) {
    current_charger_online = online;

    if (online == 0) {
        printf("Charger is offline. exiting\n");
        exit(0);
    }
}

static void set_brightness(int brightness) {
    FILE* file = fopen(BRIGHTNESS_PATH, "w");
    if (file == NULL) {
        printf("Failed to open brightness file\n");
        return;
    }

    if (fprintf(file, "%d", brightness) < 0) {
        printf("Failed to write new brightness\n");
        fclose(file);
        return;
    }

    fclose(file);

    printf("Brightness set to %d\n", brightness);

    screen_is_on = (brightness > 0);
}

static void toggle_screen(void) {
    if (screen_is_on)
        set_brightness(0);
    else
        set_brightness(max_brightness / 4);
}

static int bootreason_charger(void) {
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

static int open_restricted(const char *path, int flags, void *user_data) {
    (void)user_data;
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data) {
    (void)user_data;
    close(fd);
}

static int is_input_device(const char *path) {
    int fd;
    char name[256];

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

static void* monitor_power_key(void *arg) {
    (void)arg;

    struct libinput *li;
    struct libinput_event *event;
    int rc;

    li = libinput_path_create_context(&interface, NULL);
    if (!li) {
        fprintf(stderr, "Failed to initialize libinput context\n");
        return NULL;
    }

    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];

    dir = opendir("/dev/input");
    if (!dir) {
        fprintf(stderr, "Failed to open /dev/input directory\n");
        libinput_unref(li);
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) == 0) {
            snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

            if (is_input_device(path)) {
                struct libinput_device *device;

                device = libinput_path_add_device(li, path);
                if (!device)
                    fprintf(stderr, "Failed to add device: %s\n", path);
                else
                    printf("Added input device: %s\n", path);
            }
        }
    }

    closedir(dir);

    printf("Monitoring all input devices for KEY_POWER events...\n");

    libinput_dispatch(li);

    while (1) {
        fd_set fds;
        int fd = libinput_get_fd(li);

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        rc = select(fd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0) {
            fprintf(stderr, "select() failed: %s\n", strerror(errno));
            break;
        }

        libinput_dispatch(li);

        while ((event = libinput_get_event(li)) != NULL) {
            if (libinput_event_get_type(event) == LIBINPUT_EVENT_KEYBOARD_KEY) {
                struct libinput_event_keyboard *key_event;
                enum libinput_key_state state;
                uint32_t key;

                key_event = libinput_event_get_keyboard_event(event);
                key = libinput_event_keyboard_get_key(key_event);
                state = libinput_event_keyboard_get_key_state(key_event);

                if (key == KEY_POWER && state == LIBINPUT_KEY_STATE_RELEASED) {
                    struct libinput_device *device = libinput_event_get_device(event);
                    const char *device_name = libinput_device_get_name(device);

                    printf("KEY_POWER released on device '%s'. Toggling screen state.\n", device_name);
                    toggle_screen();
                }
            }

            libinput_event_destroy(event);
        }
    }

    libinput_unref(li);
    return NULL;
}

static void* monitor_power_supply_uevent(void *arg) {
    (void)arg;

    int sockfd;
    struct sockaddr_nl sa;
    int bufsize = 1024 * 4;
    char *buf = NULL;

    sockfd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (sockfd < 0) {
        perror("socket(AF_NETLINK)");
        return NULL;
    }

    memset(&sa, 0, sizeof(sa));
    sa.nl_family = AF_NETLINK;
    sa.nl_pid = getpid();
    sa.nl_groups = 1;

    if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind(netlink)");
        close(sockfd);
        return NULL;
    }

    buf = (char *)malloc(bufsize);
    if (!buf) {
        fprintf(stderr, "malloc() failed for uevent buffer\n");
        close(sockfd);
        return NULL;
    }

    printf("Listening for power_supply uevents via netlink...\n");

    while (1) {
        ssize_t len;
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        if (select(sockfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR)
                continue;
            perror("select(netlink)");
            break;
        }

        if (!FD_ISSET(sockfd, &fds))
            continue;

        len = recv(sockfd, buf, bufsize - 1, 0);
        if (len < 0) {
            if (errno == EINTR)
                continue;
            perror("recv(netlink)");
            break;
        }

        buf[len] = '\0';

        parse_power_supply_event(buf, len);
    }

    free(buf);
    close(sockfd);
    return NULL;
}

static int safe_atoi(const char *s, int fallback) {
    char *endp = NULL;
    long v = strtol(s, &endp, 10);
    if (endp == s || *endp != '\0')
        return fallback;
    if (v < INT_MIN || v > INT_MAX)
        return fallback;
    return (int)v;
}

static void parse_power_supply_event(const char *msg, ssize_t len) {
    const char *p = msg;
    const char *end = msg + len;

    const char *devpath = NULL;
    const char *capacity_str = NULL;
    const char *online_str = NULL;

    static int ac_online  = -1;
    static int usb_online = -1;

    while (p < end && *p) {
        if (strncmp(p, "DEVPATH=", 8) == 0)
            devpath = p + 8;
        else if (strncmp(p, "POWER_SUPPLY_CAPACITY=", 22) == 0)
            capacity_str = p + 22;
        else if (strncmp(p, "POWER_SUPPLY_ONLINE=", 20) == 0)
            online_str = p + 20;

        p += strlen(p) + 1;
    }

    if (!devpath || strstr(devpath, "/power_supply/") == NULL)
        return;

    /* Battery capacity event */
    if (strstr(devpath, "/power_supply/battery") && capacity_str) {
        int cap = safe_atoi(capacity_str, -1);
        if (cap >= 0 && cap <= 100) {
            printf("uevent: battery capacity %d%%\n", cap);
            ui_update_capacity(cap);
        }
    }

    /* Charger state event */
    if (online_str) {
        int online_val = safe_atoi(online_str, -1);

        if (strstr(devpath, "/power_supply/ac")) {
            if (online_val == 0 || online_val == 1) {
                ac_online = online_val;
                printf("uevent: ac online=%d\n", ac_online);
            }
        } else if (strstr(devpath, "/power_supply/usb")) {
            if (online_val == 0 || online_val == 1) {
                usb_online = online_val;
                printf("uevent: usb online=%d\n", usb_online);
            }
        }

        if (ac_online != -1 || usb_online != -1) {
            int aggregated_online = (ac_online == 1 || usb_online == 1) ? 1 : 0;
            handle_charger_state(aggregated_online);
        }
    }
}

int main(int argc, char *argv[]) {
    if (!bootreason_charger()) {
        printf("Device is not in charger mode\n");
        exit(0);
    }

    current_charger_online = read_charger_online();
    if (current_charger_online == 0) {
        printf("Charger is offline at startup. exiting\n");
        exit(0);
    }

    /* Plymouth blocks minui */
    struct stat buffer;
    if (stat("/usr/bin/plymouth", &buffer) == 0)
        system("plymouth quit");

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

    /* Read max brightness */
    FILE* file = fopen(MAX_BRIGHTNESS_PATH, "r");
    if (file == NULL) {
        printf("Failed to open max brightness file\n");
    } else {
        if (fscanf(file, "%d", &max_brightness) != 1) {
            printf("Failed to read max brightness\n");
            max_brightness = 255; /* fallback default */
        }
        fclose(file);
    }

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
    if (cli_options.hor_res > 0)
        hor_res = cli_options.hor_res;
    if (cli_options.ver_res > 0)
        ver_res = cli_options.ver_res;
    if (cli_options.dpi > 0)
        dpi = cli_options.dpi;

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

    /* Battery fill with blue-white gradient */
    battery_fill = lv_obj_create(battery);
    lv_obj_set_size(battery_fill, LV_PCT(100), LV_PCT(0));
    lv_obj_align(battery_fill, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* Create gradient style */
    static lv_style_t style_gradient;
    lv_style_init(&style_gradient);

    /* Create the gradient descriptor */
    static lv_grad_dsc_t grad;
    grad.dir = LV_GRAD_DIR_VER;            /* Vertical gradient */
    grad.stops_count = 2;                  /* 2 color stops */
    grad.stops[0].color = lv_color_hex(0x093E94); /* Dark blue color */
    grad.stops[1].color = lv_color_hex(0xCCCCCC); /* Light grayish color */
    grad.stops[0].frac = 0;                /* Position of the first stop (0%) */
    grad.stops[1].frac = 255;              /* Position of the second stop (100%) */

    /* Apply style settings */
    lv_style_set_bg_grad(&style_gradient, &grad);
    lv_style_set_bg_opa(&style_gradient, LV_OPA_COVER);
    lv_style_set_radius(&style_gradient, 55);
    lv_style_set_border_width(&style_gradient, 0);
    lv_style_set_pad_all(&style_gradient, 5);

    /* Apply the style to the battery fill object */
    lv_obj_add_style(battery_fill, &style_gradient, 0);

    /* Battery "tip" */
    lv_obj_t *battery_tip = lv_obj_create(lv_scr_act());
    lv_obj_set_size(battery_tip, 140, 45);
    lv_obj_align_to(battery_tip, battery, LV_ALIGN_OUT_TOP_MID, 0, -5);
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

    current_capacity = read_battery_capacity();
    if (current_capacity >= 0 && current_capacity <= 100)
        ui_update_capacity(current_capacity);
    else
        ui_update_capacity(0);

    handle_charger_state(current_charger_online);

    /* Set initial brightness to 1/4 of max_brightness */
    set_brightness(max_brightness / 4);

    /* Create threads */
    pthread_t power_key_thread;
    pthread_t uevent_thread;

    pthread_create(&power_key_thread, NULL, monitor_power_key, NULL);
    pthread_create(&uevent_thread, NULL, monitor_power_supply_uevent, NULL);

    /* Run lvgl in "tickless" mode */
    while (1) {
        lv_task_handler();
        usleep(5000);
    }

    return 0;
}

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
