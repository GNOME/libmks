lib_test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'MALLOC_CHECK_=2',
  'LD_LIBRARY_PATH=@0@/lib'.format(meson.project_build_root()),
]

lib_testsuite_c_args = [
  '-DG_ENABLE_DEBUG',
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
]

lib_testsuite = {
  'test-audio-format': {},
  'test-mks': {},
  'test-mks-transport': {},
}

lib_testsuite_deps = [
  libmks_dep,
]

foreach test_name, params: lib_testsuite
  test_exe = executable(test_name,
                        ['@0@.c'.format(test_name)],
                        c_args: lib_testsuite_c_args,
                        dependencies: lib_testsuite_deps,
                        include_directories: [include_directories('..'), include_directories('.')],
  )

  if not params.get('skip', false)
    test(test_name, test_exe, env: lib_test_env)
  endif
endforeach
