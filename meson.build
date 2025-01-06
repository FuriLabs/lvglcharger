# Copyright 2021 Clayton Craft
#
# This file is part of lvglcharger, hereafter referred to as the program.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.


project(
  'lvglcharger',
  'c',
  version: '0.1.0',
  default_options: 'warning_level=3',
  meson_version: '>=0.53.0'
)

add_project_arguments('-DUL_VERSION="@0@"'.format(meson.project_version()), language: ['c'])

enable_static = (get_option('default_library') == 'static')

lvglcharger_sources = [
  'backends.c',
  'command_line.c',
  'config.c',
  'main.c',
  'terminal.c',
  'themes.c',
  'theme.c'
]

lvglcharger_dependencies = [
  dependency('inih', static: enable_static),
]

cc = meson.get_compiler('c')

libdrm_dep = dependency('libdrm', required: get_option('with-drm'), static: enable_static)
if libdrm_dep.found()
  lvglcharger_dependencies += [libdrm_dep]
  add_project_arguments('-DUSE_DRM=1', language: ['c'])
endif

minui_dep = dependency('minui', required: get_option('with-minui'), static: enable_static)
if minui_dep.found()
  lvglcharger_dependencies += [
    minui_dep,
    cc.find_library('m', required : false, static: enable_static)
  ]
  add_project_arguments('-DUSE_MINUI=1', language: ['c'])

  if get_option('minui-bgra')
    add_project_arguments('-DMINUI_IS_BGRA=1', language: ['c'])
  endif
endif

lvgl_sources = run_command('find-lvgl-sources.sh', 'lvgl', check: true).stdout().strip().split('\n')

lv_drivers_sources = run_command('find-lvgl-sources.sh', 'lv_drivers', check: true).stdout().strip().split('\n')

install_data(sources: 'lvglcharger.conf', install_dir : get_option('sysconfdir'))


executable(
  'lvglcharger',
  sources: lvglcharger_sources + lvgl_sources + lv_drivers_sources,
  include_directories: ['lvgl', 'lv_drivers'],
  dependencies: lvglcharger_dependencies,
  install: true
)
