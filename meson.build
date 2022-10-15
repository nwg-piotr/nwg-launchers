project('nwg-launchers', 'cpp',
	default_options: [
			'cpp_std=c++17',
			'warning_level=3'
			],
	license: 'GPL-3.0-or-later',
	version: '0.7.0'
)

compiler = meson.get_compiler('cpp')

if compiler.has_link_argument('-lc++fs')
	add_global_link_arguments('-lc++fs', language: 'cpp')
elif compiler.has_link_argument('-lstdc++fs')
	add_global_link_arguments('-lstdc++fs', language: 'cpp')
endif

# Dependencies
gdk_x11 = dependency('gdk-x11-3.0', required: get_option('gdk-x11'))

## gtkmm
gtkmm = dependency('gtkmm-3.0', required: true)

## gtk-layer-shell
gtk_layer_shell = dependency(
    'gtk-layer-shell-0',
    version: ['>= 0.5.0'],
    fallback: ['gtk-layer-shell-0', 'gtk-layer-shell'],
    required: get_option('layer-shell')
)
if gtk_layer_shell.found()
    add_project_arguments('-DHAVE_GTK_LAYER_SHELL', language: 'cpp')
endif

## nlohmann-json
json = dependency(
    'nlohmann_json',
    fallback: ['nlohmann_json', 'nlohmann_json_dep'],
    required: true
)

# Generate configuration header
conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('prefix', get_option('prefix'))
conf_data.set('datadir', get_option('prefix') / get_option('datadir') / 'nwg-launchers')
conf_data.set('HAVE_MODERN_NLOHMANN_JSON', json.version().version_compare('>=3.11.0'))
configure_file(
	input : 'nwgconfig.h.in',
	output : 'nwgconfig.h',
	configuration : conf_data
)
# Include nwgconfig.h
nwg_conf_inc = include_directories('.')

subdir('common')

if get_option('bar')
	subdir('bar')
endif

if get_option('dmenu')
	subdir('dmenu')
endif

if get_option('grid')
	subdir('grid')
endif

install_data(
    ['icon-missing.svg', 'icon-missing.png'],
    install_dir: conf_data.get('datadir')
)
