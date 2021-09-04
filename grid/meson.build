sources = files(
	'grid.cc',
	'grid_classes.cc',
	'grid_tools.cc'
)

executable(
	'nwggrid',
	files('grid_client.cc', 'grid_classes.cc', 'grid_tools.cc'),
	dependencies: [gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

executable(
	'nwggrid-server',
	sources,
	dependencies: [json, gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

install_data(
	['style.css', 'icon-missing.svg'],
	install_dir: conf_data.get('datadir', '') / 'nwggrid'
)
