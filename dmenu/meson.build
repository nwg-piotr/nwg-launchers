sources = files(
	'dmenu.cc',
	'dmenu_classes.cc',
	'dmenu_tools.cc'
)

executable(
	'nwgdmenu',
	sources,
	dependencies: [json, gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc, json_header_dir],
	install: true
)

install_data(
	['style.css'],
	install_dir: conf_data.get('datadir', '') / 'nwgdmenu'
)
