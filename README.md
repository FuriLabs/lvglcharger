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

Once you have the sources, you can build the app and run it in a VT.

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
