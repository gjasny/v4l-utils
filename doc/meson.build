doxygen_doc_option = get_option('doxygen-doc')
if doxygen_doc_option.disabled() or not prog_doxygen.found()
    subdir_done()
endif

cdata = configuration_data()
cdata.set('DOCDIR', 'doc')
cdata.set('GENERATE_HTML', get_option('doxygen-html'))
cdata.set('GENERATE_MAN', get_option('doxygen-man'))
cdata.set('PROJECT', meson.project_name())
cdata.set('SRCDIR', source_root)
cdata.set('VERSION', as_version)

doxyfile = configure_file(input : 'Doxyfile.in',
                          output : 'Doxyfile',
                          configuration : cdata)

doxygen_install_dirs = []

doxygen_output = []
if get_option('doxygen-html')
    doxygen_output += 'html'
    doxygen_install_dirs += docdir
endif
if get_option('doxygen-man')
    doxygen_output += 'man'
    doxygen_install_dirs += get_option('mandir') / '..'
endif

custom_target('doxygen',
              input : doxyfile,
              output : doxygen_output,
              command : [prog_doxygen, doxyfile],
              install : true,
              install_dir : doxygen_install_dirs)
