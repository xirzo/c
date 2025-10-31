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

unity_src = ['./external/unity/unity.c']
lexer_test_include = include_directories('./external/unity', './lib/include/')

lexer_test_src = [
  './tests/lexer_tests.c',
  './lib/src/lexer.c', 
  './lib/src/stb_ds.c'
]

lexer_test = executable(
  'test_lexer',
  sources: lexer_test_src + unity_src,
  include_directories: lexer_test_include,
  dependencies: dependencies,
  c_args: ['-DUNITY_OUTPUT_COLOR']
)

test('lexer tests', lexer_test)
