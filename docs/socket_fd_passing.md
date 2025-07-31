# Design: Socket FD Passing (`LISTEN_FDS`) to Services

## Background

When a `.socket` unit triggers a `.service` via socket activation, the service must receive open socket file descriptors (FDs) so it can accept client connections immediately. Systemd passes these FDs starting at FD 3 and sets environment variables so the service knows how many FDs it received.

## Goals

- Pass all socket FDs associated with a `.service` unit on `fork`/`exec`.
- Set environment variables: 
  - `LISTEN_PID` = PID of the service process 
  - `LISTEN_FDS` = number of passed file descriptors
- Ensure FD 3 and onwards are the passed socket FDs.
- Close unused FDs in the child before `exec`.

## Data Structures

### Unit Association

- Each `.socket` unit records the path and FD of its bound socket.
- `.service` units are linked to `.socket` units by basename matching (`foo.socket` → `foo.service`).
- Maintain a global table or map from service unit → array of socket FDs.

## Workflow

### At Socket Activation Time

- Bind all `.socket` units and store their FDs.
- Map socket FDs to the associated `.service` unit.

### When Starting a Service (`service_manager_start()`)

- Lookup socket FDs associated with the service.
- Before `fork()`:
  - Create a copy of the current environment.
  - Add or override:
    - `LISTEN_PID=<child_pid>` (after fork)
    - `LISTEN_FDS=<num_fds>`
- After fork, in the child:
  - Duplicate each socket FD to FD starting at 3 (if needed).
  - Clear `FD_CLOEXEC` flags on those FDs.
  - Close all other unrelated FDs.
- Execute the service’s `ExecStart` command.

## Edge Cases & Notes

- Services without socket units get no FDs and no `LISTEN_FDS`/`LISTEN_PID` set.
- If multiple socket units correspond to one service, pass all their FDs.
- Respect the systemd convention: `LISTEN_FDS` only counts passed FDs starting from 3.
- Ensure race conditions do not close or reuse FDs prematurely.

## Future Extensions

- Support `Accept=yes` socket units (fork new service instance per connection).
- Implement environment inheritance and manipulation utilities.
- Integrate with sandboxing (close extra FDs).
- Add logging around socket FD passing.

