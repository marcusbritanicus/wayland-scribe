project(
	'wayland-scribe',
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

add_project_link_arguments(['-rdynamic'], language:'cpp')

QtDeps = dependency( 'qt6', modules: [ 'Core' ] )
executable(
	'wayland-scribe', 'wayland-scribe.cpp',
	dependencies: QtDeps,
	install: true
)
