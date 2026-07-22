project('xi_string', 'c',
  version : '1.0.0',
  default_options : ['warning_level=3', 'c_std=c11'],
)

xi_string_lib = static_library('xi_string',
  'lib/src/xi_string.c',
  include_directories : include_directories('lib/include'),
)

xi_string_dep = declare_dependency(
  include_directories : include_directories('lib/include'),
  link_with : xi_string_lib,
)
