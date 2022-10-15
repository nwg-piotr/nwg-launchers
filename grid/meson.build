
configure_file(input: 'help.h.in', output: 'help.h', configuration: conf_data)

sources = files(
	'grid.cc',
	'grid_classes.cc',
	'grid_tools.cc',
	'grid_entries.cc'
)

grid_server_exe = executable(
	'nwggrid',
	files('grid_client.cc', 'grid_classes.cc', 'grid_tools.cc'),
	dependencies: [json, gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

grid_client_exe = executable(
	'nwggrid-server',
	sources,
	dependencies: [json, gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

install_data(
	['style.css', 'grid.conf'],
	install_dir: conf_data.get('datadir', '') / 'nwggrid'
)
