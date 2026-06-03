project('z-ui', 'c')

incdir = include_directories('inc')

dep_wayland = dependency('wayland-client')
dep_m = meson.get_compiler('c').find_library('m')
wayland_scanner = find_program('wayland-scanner')

protocols = [
  'protocol/xdg-shell.xml',
  'protocol/wlr-layer-shell-unstable-v1.xml',
  'protocol/viewporter.xml',
  'protocol/linux-dmabuf-v1.xml',
]

proto_headers = []
proto_sources = []

foreach xml : protocols
  proto_headers += custom_target('@0@-client-header'.format(xml.split('/')[-1].split('.')[0]),
    input: xml,
    output: '@BASENAME@.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  proto_sources += custom_target('@0@-private-code'.format(xml.split('/')[-1].split('.')[0]),
    input: xml,
    output: '@BASENAME@.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

sources = files([
  'src/backend_wayland.c',
  'src/wayland_state.c',
  'src/wayland_dispatch.c',
  'src/wl_shm_pool.c',
  'src/z-ui.c',
  'src/demo.c'
])

executable('z-ui',
  sources, proto_sources, proto_headers,
  include_directories: incdir,
  dependencies: [dep_wayland, dep_m],
)
