# C safety risk register

This register ranks service-manager and unit-parser bug classes that can turn
unexpected unit files, configuration, or lifecycle events into reliability or
security failures. Scores use a simple 1-10 project risk scale: **impact**
(process compromise, daemon crash, data loss) plus **likelihood** (reachable from
unit/config input or normal process chaos), capped at 10.

| Score | Severity | Bug class | CWE / vulnerability class | Current mitigation | Follow-up hardening |
| ---: | --- | --- | --- | --- | --- |
| 9 | Critical | Unterminated fixed-size strings passed to `strlen`, `%s`, `strcasecmp`, or `execl` | CWE-170 improper null termination; CWE-125 out-of-bounds read | Service-manager string use is gated through bounded `memchr` checks before logging, policy parsing, timer parsing, and shell execution. Unit parsing truncates with an explicit trailing NUL. | Add fuzz tests for `Unit` objects and unit files with no embedded NUL inside field capacities. |
| 8 | High | NULL `Unit`, `ServiceEntry`, list, key, or value pointers during error paths | CWE-476 NULL pointer dereference | Public unit loading and service start paths reject NULL inputs; internal restart, status, and list helpers tolerate NULL and fail closed. | Add sanitizer CI and unit tests for service-manager API NULL handling. |
| 8 | High | Capacity/config mismatch overflowing static service/socket/unit tables | CWE-787 out-of-bounds write | Service starts clamp runtime `max_services` to the compiled service-table capacity before indexing. Config loading already clamps parsed limits. | Apply the same defensive clamp pattern at all static unit/socket table insertion sites. |
| 7 | High | Malformed or oversized string-list counts scanning outside list storage | CWE-125 out-of-bounds read | Status-list scanning clamps `UnitStringList.count` to `UNIT_LIST_MAX` and skips unterminated entries. | Add central validation helpers for every `UnitStringList` consumer. |
| 7 | High | Restart timer callbacks with stale or NULL user data | CWE-416 use after free / stale pointer risk; CWE-476 | Restart callbacks now tolerate NULL `RestartJob` and missing entries before dereferencing. | Track and cancel pending restart event sources when a service is stopped or removed. |
| 6 | Medium | Negative or malformed time values and integer conversion edge cases | CWE-190 integer overflow or wraparound; CWE-20 improper input validation | Restart delay parsing falls back when the bounded time field is empty, malformed, or unterminated. | Clamp parsed seconds to a sane maximum before converting to microseconds. |
| 6 | Medium | Ignored process-control syscall failures during shutdown | CWE-252 unchecked return value | Shutdown paths log intent and reap available children, but `kill()` failures are not yet classified per `errno`. | Handle `ESRCH`, `EPERM`, and transient errors separately and propagate degraded shutdown status. |
| 5 | Medium | Shell command execution from unit files | CWE-78 command injection risk when unit files are not trusted | Existing behavior uses `/bin/sh -c` only after bounded `ExecStart` validation. | Prefer direct `execve` argv parsing for trusted systemd-like command lines or enforce trusted unit-file ownership/permissions. |
| 4 | Low | Silent truncation of long unit values | CWE-20 improper input validation | Unit parsing preserves memory safety by truncating and NUL-terminating fixed-size destinations. | Emit warnings when truncation occurs so operators can fix invalid unit files. |

## Baseline safety rules

1. Treat unit files, config files, environment variables, and child-process state
   as untrusted inputs.
2. Do not call C string APIs on fixed-size buffers until a bounded NUL check has
   succeeded for that field's capacity.
3. Reject NULL public API inputs and make internal callbacks fail closed when
   asynchronous user data is missing.
4. Clamp runtime limits to compiled static-array capacities immediately before
   indexing.
5. Prefer explicit diagnostics over silent fallback when input is malformed, but
   keep the supervisor alive whenever a single bad unit can be isolated.
