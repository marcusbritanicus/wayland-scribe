wayland_scanner = find_program( 'wayland-scanner' )

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

wayland_scribe = find_program( 'wayland-scribe' )

wayland_scribe_code = generator(
	wayland_scribe,
	output: '@BASENAME@-client.cpp',
	arguments: ['--client', '@INPUT@', '--source', '@OUTPUT@'],
)

wayland_scribe_header = generator(
	wayland_scribe,
	output: '@BASENAME@-client.hpp',
	arguments: ['--client', '@INPUT@', '--header', '@OUTPUT@'],
)

this = include_directories( '.' )

protocols = [
    '../hello-world.xml',
]

wl_protos_src = []
wl_protos_headers = []

foreach p : protocols
	xml = join_paths( meson.source_root(), p )
	wl_protos_src += wayland_scanner_code.process( xml )
	wl_protos_headers += wayland_scanner_client.process( xml )
	wl_protos_src += wayland_scribe_code.process( xml )
	wl_protos_headers += wayland_scribe_header.process( xml )
endforeach

wayland_client = dependency( 'wayland-client' )
rt = meson.get_compiler( 'cpp' ).find_library( 'rt' )

static_library(
    'wl_protos', wl_protos_src + wl_protos_headers,
	dependencies: [wayland_client],
)
