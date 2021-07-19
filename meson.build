project('nwg-launchers', 'cpp',
	default_options: [
			'cpp_std=c++17',
			'warning_level=3'
			],
	license: 'GPL-3.0-or-later',
	version: '0.5.0'
)

compiler = meson.get_compiler('cpp')

if compiler.has_link_argument('-lc++fs')
	add_global_link_arguments('-lc++fs', language: 'cpp')
elif compiler.has_link_argument('-lstdc++fs')
	add_global_link_arguments('-lstdc++fs', language: 'cpp')
endif

# Dependencies
gdk_x11 = dependency('gdk-x11-3.0', required: get_option('gdk-x11'))
if gdk_x11.found()
	add_project_arguments('-DHAVE_GDK_X11', language: 'cpp')
endif

## gtkmm
gtkmm = dependency('gtkmm-3.0', required: true)

## gtk-layer-shell
gtk_layer_shell = dependency('gtk-layer-shell-0', version: ['>= 0.5.0'], required: get_option('layer-shell'))
if gtk_layer_shell.found()
    add_project_arguments('-DHAVE_GTK_LAYER_SHELL', language: 'cpp')
endif

## nlohmann-json
json = dependency('nlohmann_json', required: false)
## If nlohmann-json is not installed on the system
## we download the repository and use the single header file they have
json_header_dir = []
if not json.found()
	subproject('nlohmann_json')
	json_header_dir = include_directories('subprojects/nlohmann_json/single_include')
endif

# Generate configuration header
conf_data = configuration_data()
conf_data.set('version', meson.project_version())
conf_data.set('prefix', get_option('prefix'))
conf_data.set('datadir', get_option('prefix') / get_option('datadir') / 'nwg-launchers')
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
