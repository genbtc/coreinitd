# Verbs 3 implementation plan

This plan covers the next pass over the `systemd-user` verb inventory in `docs/verbs-2.txt`. The current step makes the loader section-aware and stores every listed verb naively in `Unit` so later service-manager, socket-activation, dependency, install, sandbox, and D-Bus work has somewhere stable to read from.

## Parser baseline for this step

- Recognize `[Unit]`, `[Service]`, `[Socket]`, `[Install]`, and existing `[Timer]` headers.
- Trim leading/trailing whitespace around keys and values.
- Ignore blank lines and whole-line `#` or `;` comments.
- Keep scalar fields for verbs that are normally singular in the observed unit set.
- Keep fixed-size value lists for verbs that naturally repeat or contain multiple directives across files.
- Preserve existing behavior for `Description=`, `ExecStart=`, `NotifyAccess=`, service `Socket=`, `ListenStream=`, `Accept=`, and timer verbs.
- Defer semantic validation, dependency graph state, process supervision changes, D-Bus activation, cgroup state, sandboxing enforcement, environment expansion, specifier expansion, install-state mutation, and restart policy execution.

## Section headers

### `[Unit]`

Implementation plan:

1. Parse the section header and route following keys to unit metadata/dependency storage.
2. Use this section as the future source for dependency graph construction and start-rate-limiting policy.
3. Later implementation details:
   - validate target/unit names,
   - normalize duplicates,
   - split whitespace-separated dependency lists,
   - detect cycles,
   - honor ordering separately from requirement dependencies.

### `[Service]`

Implementation plan:

1. Parse the section header and route following service execution, lifecycle, D-Bus, restart, and sandbox keys to service storage.
2. Keep launch behavior unchanged for now except that `ExecStart=` still feeds the existing service manager.
3. Later implementation details:
   - support service type state machines,
   - implement command-prefix semantics such as leading `-`,
   - add restart scheduling,
   - apply sandbox flags in the child before exec,
   - integrate D-Bus name ownership where available.

### `[Socket]`

Implementation plan:

1. Parse the section header and route following socket activation keys to socket storage.
2. Keep current `ListenStream=` behavior as the active socket activation path.
3. Later implementation details:
   - apply modes/directories before bind,
   - map socket `Service=` overrides to services,
   - preserve configured file descriptor names in activation environment,
   - add TCP/specifier support beyond current UNIX socket support.

### `[Install]`

Implementation plan:

1. Parse the section header and store install relationship verbs.
2. Do not mutate filesystem state or create wants symlinks in this parser pass.
3. Later implementation details:
   - add an enable/disable helper,
   - materialize `.wants/` links or an equivalent coreinitd install database,
   - keep runtime dependency graph separate from install metadata.

## Verb-by-verb plan

| Verb | Section | Parser storage now | Future implementation plan |
| --- | --- | --- | --- |
| `After` | `[Unit]` | Store each value in an `after` list. | Build ordering edges; start this unit only after named units have reached an acceptable state. Do not imply requirement semantics. |
| `AmbientCapabilities` | `[Service]` | Store each raw capability list entry. | Parse capabilities, validate against kernel-supported names, and apply ambient/inheritable/permitted capability sets before `exec`. |
| `BusName` | `[Service]` | Store scalar D-Bus bus name. | For `Type=dbus`, wait for ownership of this name before marking active; optionally activate by D-Bus request. |
| `Description` | `[Unit]` | Continue storing scalar description. | Use in status/log output and user-facing tooling. |
| `DirectoryMode` | `[Socket]` | Store scalar mode string. | Parse octal mode and apply to auto-created socket parent directories. |
| `Documentation` | `[Unit]` | Store each documentation value in a list. | Expose through status/introspection commands; optionally validate URI/man/info forms. |
| `Environment` | `[Service]` | Store each raw environment assignment line. | Parse quoted assignments, merge with inherited manager environment, support variable reset behavior, and pass to child process. |
| `ExecReload` | `[Service]` | Store scalar reload command. | Add reload verb to service manager and execute the command against running services. |
| `ExecStart` | `[Service]` | Continue storing scalar start command. | Replace shell-based launching with parsed argv handling and systemd-compatible command prefixes. |
| `ExecStartPost` | `[Service]` | Store each post-start command in a list. | Execute after start success; honor failure semantics and command prefixes. |
| `FileDescriptorName` | `[Socket]` | Store scalar descriptor name. | Pass named descriptors to services using `LISTEN_FDNAMES` alongside `LISTEN_FDS`. |
| `KillMode` | `[Service]` | Store scalar kill mode. | Choose whether stop/restart kills only the main process, control group, mixed mode, or none. |
| `ListenStream` | `[Socket]` | Continue storing scalar stream endpoint. | Support multiple streams, ports, IPv4/IPv6, path specifier expansion, and collision handling. |
| `MemoryDenyWriteExecute` | `[Service]` | Store boolean. | Enforce with seccomp/filtering or equivalent platform support before exec. |
| `NoNewPrivileges` | `[Service]` | Store boolean. | Call `prctl(PR_SET_NO_NEW_PRIVS)` before exec and propagate failures. |
| `PartOf` | `[Unit]` | Store each value in a `part_of` list. | Propagate stop/restart actions from listed units to this unit without implying start requirement. |
| `Requires` | `[Unit]` | Store each value in a `requires` list. | Add requirement edges; start required units with this unit and fail/stop dependents on required-unit failure. |
| `Restart` | `[Service]` | Store scalar restart policy. | Evaluate exit cause/status against policy and schedule restarts. |
| `RestartForceExitStatus` | `[Service]` | Store each raw status list entry. | Parse names/numbers and force restart even when `Restart=` would not otherwise restart. |
| `RestartSec` | `[Service]` | Store scalar duration string. | Parse duration and schedule restart timer delays. |
| `Service` | `[Socket]` | Store scalar service override. | Use this override instead of implicit socket-to-service basename matching. |
| `Slice` | `[Service]` | Store scalar slice name. | Place service processes into the requested cgroup slice when cgroup support lands. |
| `SocketMode` | `[Socket]` | Store scalar mode string. | Parse octal mode and `chmod` UNIX socket nodes after bind. |
| `StartLimitBurst` | `[Unit]` | Store integer. | Track start attempts in the configured interval and suppress starts after the burst limit. |
| `StartLimitIntervalSec` | `[Unit]` | Store scalar duration string. | Parse duration for the start-rate-limiting window. |
| `SuccessExitStatus` | `[Service]` | Store each raw status list entry. | Treat parsed statuses/signals as clean exits in service state and restart decisions. |
| `SystemCallArchitectures` | `[Service]` | Store scalar architecture list. | Enforce with seccomp architecture filters where available. |
| `TimeoutStopSec` | `[Service]` | Store scalar duration string. | Use as the graceful shutdown timeout before escalation to SIGKILL. |
| `Type` | `[Service]` | Store scalar service type. | Implement `simple`, `exec`, `forking`, `oneshot`, `dbus`, and `notify` activation state transitions. |
| `WantedBy` | `[Install]` | Store each value in a `wanted_by` list. | Enable units by linking/recording them under wanted targets for boot/default activation. |

## Follow-up milestones

1. **Semantic parser pass**: split list-valued fields on systemd-style whitespace, preserve quoting, report malformed booleans/modes/durations.
2. **Dependency graph**: implement `After=`, `Requires=`, `PartOf=`, and install-derived target wants as explicit graph edges.
3. **Service lifecycle**: add service states, restart timers, success/force status handling, stop timeouts, and reload commands.
4. **Socket activation expansion**: support `Service=`, descriptor names, socket/directory modes, multiple listeners, and specifier expansion.
5. **Sandbox/cgroup pass**: apply capabilities, no-new-privileges, memory W^X denial, syscall architecture filters, and slices.
6. **D-Bus pass**: support `Type=dbus` with `BusName=` activation and readiness handling.
