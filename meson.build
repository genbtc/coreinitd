project('coreinitd', 'c',
  version : '0.12',
  default_options : ['warning_level=2', 'c_std=gnu11'])

# Prefer elogind first if present, else fallback to systemd
libelogind = dependency('libelogind', required: false)
libsystemd = dependency('libsystemd', required: false)
# Dependency Requirement libelogind/libsystemd
if libelogind.found()
  libsd = libelogind
  message('Using libelogind. Found')
elif libsystemd.found()
  libsd = libsystemd
  message('Using libsystemd. Found')
else
  error('dependency error: Neither libelogind nor libsystemd could be found. Required!')
endif

coreinitd_src = files(
  'src/coreinitd/main.c',
  'src/coreinitd/event_loop.c',
  'src/coreinitd/unit_loader.c',
  'src/coreinitd/config.c',
  'src/coreinitd/socket_activation.c',
  'src/coreinitd/service_manager.c',
  'src/coreinitd/timerd.c',
  'src/helpers/parse_sec-to-int.c'
)

# Helper binaries (each has its own main())
executable('get_unit_info', 'src/helpers/get_unit_info.c')
executable('listen_fds',    'src/helpers/listen_fds.c', dependencies: libsd)
executable('notify_ready',  'src/helpers/notify_ready.c', dependencies: libsd)
executable('sandbox_launch','src/helpers/sandbox_launch.c')
executable('example-daemon','src/helpers/example-daemon.c', 
  install: true,
  install_dir: '/usr/bin')

#Main coreinitd
coreinitd = executable('coreinitd', coreinitd_src,
  dependencies: libsd,
  install: true,
  install_dir: '/sbin',
  override_options: ['b_lto=true'])


# Unit and smoke tests available through `meson test`.
parse_sec_test = executable('test-parse-sec',
  'tests/test-parse-sec.c',
  'src/helpers/parse_sec-to-int.c')

test('parse-sec', parse_sec_test)

unit_parsing_test = executable('test-unit-parsing',
  'tests/test-unit-parsing.c',
  'src/coreinitd/unit_loader.c',
  include_directories: include_directories('src/coreinitd'))

test('unit-parsing', unit_parsing_test,
  workdir: meson.project_source_root())

config_parsing_test = executable('test-config-parsing',
  'tests/test-config-parsing.c',
  'src/coreinitd/config.c',
  include_directories: include_directories('src/coreinitd'))

test('config-parsing', config_parsing_test,
  workdir: meson.project_source_root())

test('unix-socket-activation',
  find_program('tests/test-unix-socket-activation.sh'),
  env: {'COREINITD_BIN': coreinitd.full_path()},
  workdir: meson.project_source_root(),
  timeout: 20,
  depends: coreinitd)

test('ipv4-socket-activation',
  find_program('tests/test-ipv4-socket-activation.sh'),
  env: {'COREINITD_BIN': coreinitd.full_path()},
  workdir: meson.project_source_root(),
  timeout: 20,
  depends: coreinitd)

test('timer-recurrence',
  find_program('tests/test-timer-recurrence.sh'),
  env: {'COREINITD_BIN': coreinitd.full_path()},
  workdir: meson.project_source_root(),
  timeout: 10,
  depends: coreinitd)

test('cli-drop-in',
  find_program('tests/test-cli-drop-in.sh'),
  env: {'COREINITD_BIN': coreinitd.full_path()},
  workdir: meson.project_source_root(),
  timeout: 10,
  depends: coreinitd)
