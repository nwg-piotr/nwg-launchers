sources = files(
	'nwg_tools.cc',
	'nwg_classes.cc'
)

nwg_inc = include_directories('.')

nwg = static_library(
	'nwg',
	sources,
	dependencies: [json, gdk_x11, gtkmm, gtk_layer_shell],
	include_directories: [nwg_conf_inc],
	install: false
)
