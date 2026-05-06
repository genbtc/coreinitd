# coreinitd command-line argument plan

coreinitd should feel familiar to users of UNIX daemons and systemd-style tools:
small inspection commands must exit quickly, daemon controls should be explicit,
and test/demo controls should expose parser and event-loop behavior without
requiring a full boot environment.

## Implemented now

- `--help`, `-h`: print usage, supported options, and the argument roadmap.
- `--version`, `-V`: print the program version.
- `--config PATH`, `-c PATH`: load a specific coreinitd config file.
- `--unit-dir DIR`, `-u DIR`: override the configured unit directory.
- `--drop-in DIR`: load extra unit snippets for demos after the main unit directory.
- `--check`: parse configuration and all unit files, then exit non-daemonized.
- `--list-units`: print parsed unit type/name/description data.
- `--show-unit UNIT`: print parsed fields for one unit by basename or path.
- `--no-services`: do not auto-start plain services.
- `--no-sockets`: do not bind/listen on socket units.
- `--no-timers`: do not schedule timer units.
- `--run-for-sec SEC`: automatically leave the event loop after a bounded smoke-test interval.
- `--timer-fires N`: automatically leave the event loop after N timer callbacks; this is intended to test recurring timers.

## Near-term UNIXy command ideas

### Unit-oriented subcommands

- `start UNIT`: start one parsed service immediately.
- `stop UNIT`: stop a tracked service process.
- `restart UNIT`: stop and start a tracked service.
- `status [UNIT]`: print service state, PIDs, sockets, and timer next-fire information.
- `cat UNIT`: print the resolved unit file.
- `verify UNIT|DIR`: parse units and report unknown keys, missing target services, bad time values, and unsupported combinations.
- `reload`: reload configuration and unit files.

### Daemon operation

- `--foreground`: stay attached to the terminal and log to stderr.
- `--log-level LEVEL`: select `debug`, `info`, `notice`, `warning`, or `error`.
- `--pid-file PATH`: write the daemon PID.
- `--state-dir DIR`: place runtime state under a chosen directory.
- `--user` / `--system`: select user-session or system mode.
- `--root DIR`: inspect or run units under an alternate filesystem root.
- `--dry-run`: parse and plan actions without forking services or binding sockets.

### Timer demos and manual controls

- `timer list`: show timers, target units, next due times, and recurrence settings.
- `timer trigger UNIT`: manually fire a timer target once.
- `timer run UNIT --cycles N`: fire a timer in a bounded test harness.
- `timer explain UNIT`: explain `OnBootSec`, `OnUnitActiveSec`, and target matching for one timer.

### Socket and file-descriptor demos

- `socket list`: show sockets, listen addresses, Accept mode, and target services.
- `socket fdinfo UNIT`: show the descriptor name/index that would be passed to a service.
- `socket activate UNIT -- COMMAND...`: run a command with inherited descriptors to demonstrate fd passing.
- `--fd-store PATH`: save or restore listener metadata for future reload/reexec work.

### Config overrides

- `--set KEY=VALUE`: override config keys from the command line.
- `--env KEY=VALUE`: append service environment data for manual runs.

## Functional priority recommendation

The most useful immediate path is to make inspection and bounded test controls
excellent before adding a long list of mutating subcommands. `--list-units`,
`--show-unit`, `--check`, `--run-for-sec`, and `--timer-fires` let users see the
parser, timer scheduler, and socket setup without needing init-system privileges.
After that, the next best feature is `status`, because it will require coreinitd
to maintain accurate process/timer/socket state that all later `stop`, `restart`,
and reload controls can reuse.
