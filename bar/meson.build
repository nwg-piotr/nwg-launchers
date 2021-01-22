sources = files(
	'bar.cc',
	'bar_classes.cc',
	'bar_tools.cc'
)

executable(
	'nwgbar',
	sources,
	dependencies: [json, gtkmm],
	link_with: nwg,
	include_directories: [nwg_inc, nwg_conf_inc, json_header_dir],
	install: true
)

install_data(
	['style.css', 'bar.json', 'icon-missing.svg'],
	install_dir: conf_data.get('datadir', '') / 'nwgbar'
)
