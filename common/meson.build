sources = files(
	'nwg_tools.cc',
	'on_event.cc',
	'nwg_classes.cc'
)

nwg_inc = include_directories('.')

nwg = static_library(
	'nwg',
	sources,
	dependencies: [json, gtkmm],
	include_directories: [json_header_dir, nwg_conf_inc],
	install: false
)
