configure_file(input: 'help.h.in', output: 'help.h', configuration: conf_data)

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
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

install_data(
	['style.css'],
	install_dir: conf_data.get('datadir', '') / 'nwgdmenu'
)
