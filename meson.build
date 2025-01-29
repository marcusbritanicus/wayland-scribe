project(
	'wayland-scribe',
	'c',
	'cpp',
	version: '1.0.0',
	license: 'GPLv3',
	meson_version: '>=0.51.0',
	default_options: [
		'cpp_std=c++17',
		'warning_level=2',
		'werror=false',
	],
)

add_global_arguments( '-DPROJECT_VERSION="v@0@"'.format( meson.project_version() ), language : 'cpp' )
add_project_link_arguments(['-rdynamic'], language:'cpp')

XML = dependency( 'pugixml' )

executable(
	'wayland-scribe', [
		'scribe/main.cpp',
		'scribe/wayland-scribe.cpp'
	],
	dependencies: XML,
	install: true
)
