sources = files(
	'bar.cc'
)

executable(
	'nwgbar',
	sources,
	dependencies: [json, gtkmm],
	include_directories: json_header_dir,
	install: true
)

install_data(
	['style.css', 'bar.json', 'icon-missing.svg'],
	install_dir: conf_data.get('datadir', '') / 'nwgbar'
)
