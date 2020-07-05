sources = files(
	'grid.cc'
)

executable(
	'nwggrid',
	sources,
	dependencies: [json, gtkmm],
	include_directories: json_header_dir,
	install: true
)

install_data(
	['style.css', 'icon-missing.svg'],
	install_dir: conf_data.get('datadir', '') / 'nwggrid'
)
