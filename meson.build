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

add_global_arguments( '-DPROJECT_VERSION="v@0@"'.format( meson.project_version() ), language : 'cpp' )
add_project_link_arguments(['-rdynamic'], language:'cpp')

QtDeps = dependency( 'qt6', modules: [ 'Core' ], required: false )
if QtDeps.found() == false
	QtDeps = dependency( 'qt5', modules: [ 'Core' ], required: true )
endif

executable(
	'wayland-scribe', [
		'main.cpp',
		'wayland-scribe.cpp' 
	],
	dependencies: QtDeps,
	install: true
)
