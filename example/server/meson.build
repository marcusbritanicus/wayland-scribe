wayland_scanner = find_program( 'wayland-scanner' )

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-server.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

protocols = [
    '../hello-world.xml',
]

wl_protos_src = []
wl_protos_headers = []

foreach p : protocols
	xml = join_paths( p )
	wl_protos_src += wayland_scanner_code.process( xml )
	wl_protos_headers += wayland_scanner_client.process( xml )
endforeach
