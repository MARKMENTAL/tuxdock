# Tux-Dock Development Log

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
