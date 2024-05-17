LVGL charger
===============

Recovery project for the initramfs based on [LVGL].

# Usage

```
$ lvglcharger --help
Usage: lvglcharger [OPTION]

Mandatory arguments to long options are mandatory for short options too.
  -c, --config=PATH      Locaton of the main config file. Defaults to
                         /etc/lvglcharger.conf.
  -C, --config-override  Location of the config override file. Values in
                         this file override values for the same keys in the
                         main config file. If specified multiple times, the
                         values from consecutive files will be merged in
                         order.
  -g, --geometry=NxM     Force a display size of N horizontal times M
                         vertical pixels
  -d  --dpi=N            Overrides the DPI
  -h, --help             Print this message and exit
  -v, --verbose          Enable more detailed logging output on STDERR
  -V, --version          Print the lvglcharger version and exit
```

For an example configuration file, see [lvglcharger].

# Development

## Dependencies

- [inih]
- [lvgl] (git submodule / linked statically)
- [lv_drivers] (git submodule / linked statically)
- [libinput]
- [libxkbcommon]
- [libdrm] (optional, required for the DRM backend)
- evdev kernel module

## Building & running

Some of LVGL charger's dependencies are included as git submodules in this repository. You can clone the repository and initialise the submodules with

```
$ git clone https://github.com/furilabs/lvglcharger.git
$ cd lvglcharger
$ git submodule init
$ git submodule update
```

When pulling changes from the remote later, either use `git pull --recurse-submodules` or manually run `git submodule update` as needed after pulling.

Once you have the sources, you can build the app and run it in a VT. Unless your user account has special privileges, `sudo` will be needed to access input device files.

```
$ meson _build
$ meson compile -C _build
$ sudo chvt 2
$ sudo ./_build/lvglcharger
```

With meson <0\.55 use `ninja` instead of `meson compile`\.

### Optional features

If [libdrm] is installed, the DRM backend will be compiled automatically. It's possible to
change this behaviour using the `with-drm` meson feature. For example,

```
$ meson _build -Dwith-drm=disabled
```

will forcibly disable the DRM backend regardless if libdrm is installed or not.

## Backends

LVGL Charger supports multiple lvgl display drivers, which are herein referred as "backends".

Currently supported backends:

- fbdev
- drm (optional)
- minui (optional)

The backend can be switched at runtime by modifying the `general.backend` configuration.

## Fonts

In order to work with [LVGL], fonts need to be converted to bitmaps, stored as C arrays. LVGL charger currently uses a combination of the [OpenSans] font for text and the [FontAwesome] font for pictograms. For both fonts only limited character ranges are included to reduce the binary size. To (re)generate the C file containing the combined font, run the following command

```
$ ./regenerate-fonts.sh
```

Below is a short explanation of the different unicode ranges used above.

- [OpenSans]
  - Basic Latin (`0x0020-0x007F`)
  - Latin-1 supplement (`0x00A0-0x00FF`)
  - Latin extended A (`0x0100-0x017F`)
  - Greek and Coptic (`0x0370-0x03FF`)
  - General punctuation (`0x2000-0x206F`)
  - Currency symbols (`0x20A0-0x20CF`)
  - Mathematical operators (`0x2200-0x22FF`)
- [FontAwesome]
  - Standard `LV_SYMBOL_*` glyphs (`0xF001,0xF008,0xF00B,0xF00C,0xF00D,0xF011,0xF013,0xF015,0xF019,0xF01C,0xF021,0xF026,0xF027,0xF028,0xF03E,0xF0E0,0xF304,0xF043,0xF048,0xF04B,0xF04C,0xF04D,0xF051,0xF052,0xF053,0xF054,0xF067,0xF068,0xF06E,0xF070,0xF071,0xF074,0xF077,0xF078,0xF079,0xF07B,0xF093,0xF095,0xF0C4,0xF0C5,0xF0C7,0xF0C9,0xF0E7,0xF0EA,0xF0F3,0xF11C,0xF124,0xF158,0xF1EB,0xF240,0xF241,0xF242,0xF243,0xF244,0xF287,0xF293,0xF2ED,0xF55A,0xF7C2,0xF8A2`)
  - [adjust](https://fontawesome.com/v5/icons/adjust) (`0xF042`)
  - [arrow-alt-circle-up](https://fontawesome.com/v5/icons/arrow-alt-circle-up) (`0xF35B`)
  - [chevron-left](https://fontawesome.com/v5/icons/chevron-left) (`0xF053`)
