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
)

helpers_src = files(
  'src/helpers/get_unit_info.c',
  'src/helpers/listen_fds.c',
  'src/helpers/notify_ready.c',
  'src/helpers/sandbox_launch.c',
  'src/helpers/timerd.c'
)

all_sources = coreinitd_src + helpers_src

executable('coreinitd', all_sources,
  dependencies: libsystemd_dep,
  install: true,
  install_dir: '/sbin',
  override_options: ['b_lto=true'])
