project(
  'c',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11', 'b_sanitize=address,undefined'],
)

build_type = get_option('buildtype')

if build_type == 'debug'
  add_project_arguments('-DENABLE_LOGGING=1', language: 'c')
  message('Debug logging enabled (debug build)')
else
  add_project_arguments('-DENABLE_LOGGING=0', language: 'c')
  message('Debug logging disabled (release build)')
endif

dependencies = []
srcs = ['./bin/main.c', './lib/src/lexer.c', './lib/src/stb_ds.c']

exe = executable(
  'c',
  sources : srcs,
  dependencies : dependencies,
  include_directories : include_directories('./lib/include/'),
  install : true,
)
