# Tux-Dock Development Log

## 0.4.1-beta

### TUI stability

- Fixed duplicate port mappings shown in the container list TUI. Docker's `/containers/json` API returns separate `Ports` entries for IPv4 (`0.0.0.0`) and IPv6 (`::`) bindings of the same host port; the container parser now deduplicates these into a single `public:private` display string.
- Added a container parser regression test covering IPv4/IPv6 duplicate port bindings.

### Version

- Bumped version to `0.4.1-beta`.

## 0.4-beta

### ARM64 / tuxreaperd fixes

- Fixed `tuxreaperdasm.c` on ARM64 by using the architecture-specific `O_DIRECTORY` value (`0x4000` on `aarch64`, `0x10000` on `x86_64`). The hardcoded x86_64 value caused `openat("/proc", ...)` to fail with `EINVAL`, which broke `/proc` scanning, descendant waiting, and SIGTERM forwarding on ARM64 Chromebooks/VMs.
- Verified graceful Apache shutdown on ARM64: tuxreaperd remaps `SIGTERM` to `SIGWINCH` for Apache processes and waits for descendants to finish, allowing in-flight HTTP responses to complete before the container exits.
- Added nginx-aware `SIGTERM`→`SIGQUIT` remapping in both `tuxreaperdgnu.c` and `tuxreaperdasm.c`. When `docker stop` sends `SIGTERM`, nginx processes (`/usr/sbin/nginx`, `/usr/local/nginx/sbin/nginx`) receive `SIGQUIT` for graceful shutdown instead of `SIGTERM`.
- Added PHP-FPM-aware `SIGTERM`→`SIGQUIT` remapping with prefix matching (e.g., `/usr/sbin/php-fpm` matches `/usr/sbin/php-fpm8.4`) so PHP-FPM receives the correct graceful-shutdown signal when the container stops.
- Documented that PHP-FPM graceful shutdown requires `process_control_timeout` to be set in the PHP-FPM pool configuration (e.g., `process_control_timeout = 60s`). Without it, the PHP-FPM master exits immediately on `SIGQUIT` and in-flight requests are dropped.
- Set the container stop timeout to 60 seconds in `DockerManager::createContainer` (`--stop-timeout=60`) and raised the TUI stop default to 60 seconds, aligning Docker's grace period with `tuxreaperd`'s descendant wait so graceful shutdowns are not cut short by a premature SIGKILL.
- Added `--debug-reaper` flag to `compile.sh` to build tuxreaperd with `TUXREAPERD_DEBUG` and run verbose tests; useful for diagnosing `/proc` scanning, signal broadcasts, and descendant waits.
- Added adaptive TTY/non-TTY spinner to `compile.sh` for clearer feedback during the test phase.
- Added a "Why tuxreaperd?" comparison section to `README.md` with a feature matrix and ASCII size chart contrasting `tuxreaperd` with `tini`, `dumb-init`, and `mini-init-asm`.
- Added a "Web workload guide" section to `README.md` covering Apache, nginx, and PHP-FPM signal conventions, required PHP-FPM `process_control_timeout` configuration, container startup examples, and the `sleep.php` verification test.

### Version

- Bumped version to `0.4-beta`.

## 0.3.1-beta
Added an optional timeout_seconds parameter to stopContainer

## 0.3-beta

This release introduces `tuxreaperd`, a micro init system for containers.

### Micro init

- Split the reaper into two implementations:
  - `tuxreaperdgnu.c`: the original standard POSIX/libc daemon used as PID 1 inside containers created by tux-dock.
  - `tuxreaperdasm.c`: a new freestanding `-nostdlib` syscall engine for `x86_64` and `aarch64` with a tiny static footprint.
- Both run as subreapers (`PR_SET_CHILD_SUBREAPER`) with a SIGCHLD handler that reaps every reparented/terminated descendant, so orphaned processes never accumulate as zombies.
- Both proxy lifecycle signals (SIGTERM, SIGQUIT, SIGINT, SIGHUP, SIGUSR1, SIGUSR2) to the child process tree and propagate the primary workload's exit status (128 + signal for signal deaths).
- Special-cases Apache's questionable use of `SIGWINCH` for graceful shutdown: when `SIGTERM` is received, any process whose `/proc/<pid>/exe` is `/usr/sbin/apache2`, `/usr/sbin/httpd`, or `/usr/local/apache2/bin/httpd` gets `SIGWINCH` instead, while every other process receives `SIGTERM`.
- `compile.sh` detects the host architecture and builds the freestanding implementation when possible, falling back to `tuxreaperdgnu.c` on failure or unsupported hosts.
- `DockerManager::createContainer` bind-mounts `tuxreaperd` into the container as PID 1 and resolves the binary next to the `tux-dock` executable when it is not in the current directory.

### Tests

- Added `tuxreaperd-zombie-tests`, an in-container test suite that boots a container with `tuxreaperd` as PID 1 and verifies orphaned grandchildren are reaped (zero zombies), exit status is propagated, and `docker stop` cleanly shuts down via forwarded SIGTERM (exit 143).
- The HTML web test view now includes a "Tuxreaperd Zombie Reaping" section alongside the Docker integration and TUI transcript sections.

## 0.1.2-beta

This release adds signal trapping to prevent state corruption during long-running Docker operations.

### Reliability

- Added SIGINT (Ctrl+C) signal trapping during busy operations to prevent premature exit and socket corruption
- Added SIGTSTP (Ctrl+Z) signal trapping during busy operations to prevent suspension mid-operation
- Signals chain to previous handlers when not busy, preserving normal exit and suspend behavior
- Trap re-installs after terminal I/O restoration cycles to maintain protection through Attach Shell sessions
- Used SA_RESTART flag to prevent EINTR on blocking socket reads during signal delivery

### TUI

- Added visual feedback in busy modal when signals are trapped
- Shows "Ctrl+C ignored: operation in progress" when SIGINT is trapped
- Shows "Ctrl+Z ignored: operation in progress" when SIGTSTP is trapped

## 0.1.1-beta

This release establishes the first beta-quality Docker integration and TUI workflow.

### Docker integration

- Added direct Docker Engine API access through `/var/run/docker.sock`.
- Added startup Docker preflight using `GET /_ping` before launching the TUI.
- Added synchronous initial container and image loading.
- Added asynchronous cache refreshes after mutations and interactive sessions.
- Preserved cached state when a refresh fails.
- Added structured container/image JSON mapping with exited-container support.

### Process execution

- Replaced `system()` and `popen()` with a `fork`/`exec` process runner.
- Added captured stdout/stderr and inherited terminal I/O modes.
- Kept direct CLI execution for interactive terminal handoffs.

### Reliability

- Added HTTP response parsing for Content-Length, chunked transfer, and bodyless responses.
- Added robust handling for Docker `204` and idempotent `304` responses.
- Added tri-state stop polling: stopped, running, and unknown.
- Added bounded stop retries after transport timeouts.
- Stop completion now refreshes cached state before showing the result modal.

### TUI

- Replaced the persistent status panel with a centered actions panel.
- Added busy-operation modals with spinner feedback.
- Blocked input during long-running Docker operations.
- Added structured, scrollable container and image list dialogs.

### Tests

- Added HTTP response parser tests.
- Added container JSON mapping tests.
- Added operation-state tests.
- Added stop polling and timeout-sequence regression tests.
