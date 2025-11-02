project(
  'c',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11', 'b_sanitize=address,undefined'],
)

my_c_args = ['-DUNITY_OUTPUT_COLOR']
build_type = get_option('buildtype')

if build_type == 'debug'
  my_c_args += ['-DENABLE_LOGGING=1']
  message('Debug logging enabled (debug build)')
else
  my_c_args += ['-DENABLE_LOGGING=0']
  message('Debug logging disabled (release build)')
endif

dependencies = []
srcs = ['./bin/main.c', './lib/src/lexer.c', './lib/src/stb_ds.c', './lib/src/parser.c', './lib/src/code_generator.c', './lib/src/str.c']

exe = executable( 'c',
  sources : srcs,
  dependencies : dependencies,
  include_directories : include_directories('./lib/include/'),
  c_args: [my_c_args],
  install : true,
)

unity_src = ['./external/unity/unity.c']
test_include = include_directories('./external/unity', './lib/include/')

lexer_test_src = [
  './tests/lexer_tests.c',
  './lib/src/lexer.c', 
  './lib/src/stb_ds.c',
  './lib/src/str.c'
]

lexer_test = executable(
  'test_lexer',
  sources: lexer_test_src + unity_src,
  include_directories: test_include,
  dependencies: dependencies,
  c_args: [my_c_args]
)

test('lexer tests', lexer_test)

parser_test_src = [
  './tests/parser_tests.c',
  './lib/src/lexer.c', 
  './lib/src/parser.c', 
  './lib/src/stb_ds.c',
  './lib/src/str.c'
]

parser_test = executable(
  'test_parser',
  sources: parser_test_src + unity_src,
  include_directories: test_include,
  dependencies: dependencies,
  c_args: [my_c_args]
)

test('parser tests', parser_test)

code_gen_test_src = [
  './tests/code_gen_tests.c',
  './lib/src/lexer.c', 
  './lib/src/parser.c', 
  './lib/src/code_generator.c', 
  './lib/src/stb_ds.c',
  './lib/src/str.c'
]

code_gen_test = executable(
  'test_code_generator',
  sources: code_gen_test_src + unity_src,
  include_directories: test_include,
  dependencies: dependencies,
  c_args: [my_c_args]
)

test('code generator tests', code_gen_test)
