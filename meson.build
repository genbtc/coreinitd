project('coreinitd', 'c',
  version : '0.1',
  default_options : ['warning_level=2', 'c_std=gnu11'])

libsystemd_dep = dependency('libelogind', required: true)

coreinitd_src = files(
  'src/coreinitd/main.c',
  'src/coreinitd/event_loop.c',
  'src/coreinitd/unit_loader.c',
  'src/coreinitd/socket_activation.c',
  'src/coreinitd/service_manager.c',
  'src/coreinitd/timerd.c'
)

# Helper binaries (each has its own main())
executable('get_unit_info', 'src/helpers/get_unit_info.c')
executable('listen_fds',    'src/helpers/listen_fds.c', dependencies: libsystemd_dep)
executable('notify_ready',  'src/helpers/notify_ready.c', dependencies: libsystemd_dep)
executable('sandbox_launch','src/helpers/sandbox_launch.c')

executable('coreinitd', coreinitd_src,
  dependencies: libsystemd_dep,
  install: true,
  install_dir: '/sbin',
  override_options: ['b_lto=true'])
