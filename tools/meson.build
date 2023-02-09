tools = {
  'mks-connect': {},
}

foreach tool, params: tools
  tool_exe = executable(tool, '@0@.c'.format(tool),
    dependencies: libmks_dep,
  )
endforeach