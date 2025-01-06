/**
 * Copyright 2021 Johannes Marbach
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


#include "command_line.h"

#include "lvglcharger.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>


/**
 * Static prototypes
 */

/**
 * Initialise a command line options struct with default values and exit on failure.
 * 
 * @param opts pointer to the options struct
 */
static void init_opts(cli_opts *opts);

/**
 * Output usage instructions.
 */
static void print_usage();


/**
 * Static functions
 */

static void init_opts(cli_opts *opts) {
    opts->num_config_files = 1;

    opts->config_files = malloc(sizeof(char *));
    if (!opts->config_files) {
        printf("Could not allocate memory for config file paths\n");
        exit(EXIT_FAILURE);
    }
    opts->config_files[0] = "/etc/lvglcharger.conf";

    opts->hor_res = -1;
    opts->ver_res = -1;
    opts->x_offset = 0;
    opts->y_offset = 0;
    opts->verbose = false;
}

static void print_usage() {
    fprintf(stderr,
        /*-------------------------------- 78 CHARS --------------------------------*/
        "Usage: lvglcharger [OPTION]\n"
        "Mandatory arguments to long options are mandatory for short options too.\n"
        "  -c, --config=PATH         Locaton of the main config file. Defaults to\n"
        "                            /etc/lvglcharger.conf.\n"
        "  -C, --config-override     Location of the config override file. Values in\n"
        "                            this file override values for the same keys in\n"
        "                            the main config file. If specified multiple\n"
        "                            times, the values from consecutive files will be\n"
        "                            merged in order.\n"
        "  -g, --geometry=NxM[@X,Y]  Force a display size of N horizontal times M\n"
        "                            vertical pixels, offset horizontally by X\n"
        "                            pixels and vertically by Y pixels\n"
        "  -d  --dpi=N               Override the display's DPI value\n"
        "  -h, --help                Print this message and exit\n"
        "  -V, --version             Print the lvglcharger version and exit\n");
        /*-------------------------------- 78 CHARS --------------------------------*/
}


/**
 * Public functions
 */

void cli_parse_opts(int argc, char *argv[], cli_opts *opts) {
    init_opts(opts);

    struct option long_opts[] = {
        { "config",          required_argument, NULL, 'c' },
        { "config-override", required_argument, NULL, 'C' },
        { "geometry",        required_argument, NULL, 'g' },
        { "dpi",             required_argument, NULL, 'd' },
        { "help",            no_argument,       NULL, 'h' },
        { "verbose",         no_argument,       NULL, 'v' },
        { "version",         no_argument,       NULL, 'V' },
        { NULL, 0, NULL, 0 }
    };

    int opt, index = 0;

    while ((opt = getopt_long(argc, argv, "c:C:g:d:hvV", long_opts, &index)) != -1) {
        switch (opt) {
        case 'c':
            opts->config_files[0] = optarg;
            break;
        case 'C':
            opts->config_files = realloc(opts->config_files, (opts->num_config_files + 1) * sizeof(char *));
            if (!opts->config_files) {
                printf("Could not allocate memory for config file paths\n");
                exit(EXIT_FAILURE);
            }
            opts->config_files[opts->num_config_files] = optarg;
            opts->num_config_files++;
            break;
        case 'g':
            if (sscanf(optarg, "%ix%i@%i,%i", &(opts->hor_res), &(opts->ver_res), &(opts->x_offset), &(opts->y_offset)) != 4) {
                if (sscanf(optarg, "%ix%i", &(opts->hor_res), &(opts->ver_res)) != 2) {
                    printf("Invalid geometry argument \"%s\"\n", optarg);
                    exit(EXIT_FAILURE);
                }
            }
            break;
        case 'd':
            if (sscanf(optarg, "%i", &(opts->dpi)) != 1) {
                printf("Invalid dpi argument \"%s\"\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            print_usage();
            exit(EXIT_SUCCESS);
        case 'v':
            opts->verbose = true;
            break;
        case 'V':
            fprintf(stderr, "lvglcharger %s\n", VERSION);
            exit(0);
        default:
            print_usage();
            exit(EXIT_FAILURE);
        }
    }
}
