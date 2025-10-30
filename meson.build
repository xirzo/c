project(
  'c',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

dependencies = []
srcs = ['./bin/main.c', './lib/src/lexer.c']

exe = executable(
  'c',
  sources : srcs,
  dependencies : dependencies,
  include_directories : include_directories('./lib/include/'),
  install : true,
)
