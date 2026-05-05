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
  'src/coreinitd/socket_activation.c',
  'src/coreinitd/service_manager.c',
  'src/coreinitd/timerd.c'
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
executable('coreinitd', coreinitd_src,
  dependencies: libsd,
  install: true,
  install_dir: '/sbin',
  override_options: ['b_lto=true'])
