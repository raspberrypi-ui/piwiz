sources = files (
    'piwiz.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
packagekit = dependency('packagekit-glib2')
netman = dependency('libnm')

deps = [ gtk, packagekit, netman ]

executable (meson.project_name(), sources, dependencies: deps, install: true,
  c_args : [ '-DPACKAGE_DATA_DIR="' + resource_dir + '"', '-DGETTEXT_PACKAGE="' + meson.project_name() + '"' ]
)
