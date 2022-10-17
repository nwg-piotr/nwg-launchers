
configure_file(input: 'help.h.in', output: 'help.h', configuration: conf_data)

sources = files(
	'bar.cc',
	'bar_classes.cc',
	'bar_tools.cc'
)

bar_exe = executable(
	'nwgbar',
	sources,
	dependencies: [json, gtkmm, gtk_layer_shell],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc],
	install: true
)

install_data(
	['style.css', 'bar.json'],
	install_dir: conf_data.get('datadir', '') / 'nwgbar'
)
