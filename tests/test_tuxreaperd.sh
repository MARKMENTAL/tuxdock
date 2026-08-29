#!/bin/sh

# In-container tests for tuxreaperd, the tux-dock micro init system.
#
# tuxreaperd is container-tailored: tux-dock mounts it into created containers
# where it runs as PID 1, acting as a subreaper (PR_SET_CHILD_SUBREAPER) with a
# SIGCHLD reaper loop so orphaned descendants never linger as zombies.
#
# These tests drive tuxreaperd the same way the app does (bind-mounted, PID 1)
# and verify zombie reaping, exit-status propagation, and signal forwarding.

set -eu

image=${TUX_DOCK_TEST_IMAGE:-debian:forky}
reaper_bin=${1:-build/tuxreaperd}
reaper_src=${2:-}
name="tux-reaper-test-$$-$(date +%s)"
cleanup_status=0
failures=0

run_raw() {
    command_name=$1
    shift
    started=$(date +%s)
    printf '%s\n' "[TEST OUTPUT BEGIN] $command_name"
    set +e
    "$@" 2>&1
    command_status=$?
    set -e
    elapsed=$(( $(date +%s) - started ))
    printf '%s\n' "[TEST OUTPUT END] $command_name"
    printf '%s\n' "[TEST RESULT] command=$command_name status=$command_status elapsed=${elapsed}s"
    return "$command_status"
}

cleanup() {
    if docker container inspect "$name" >/dev/null 2>&1; then
        printf '%s\n' "[TUI] Cleanup: Remove Container"
        if ! docker rm -f "$name" >/dev/null 2>&1; then
            cleanup_status=1
        fi
        printf '%s\n' "[TUI] Complete: Container removed"
    fi
}
trap cleanup EXIT INT TERM

# Resolve an absolute path for the bind mount (docker requires absolute paths).
if [ ! -f "$reaper_bin" ]; then
    if command -v gcc >/dev/null 2>&1 && [ -n "$reaper_src" ]; then
        printf '%s\n' "[TUI] tuxreaperd binary not found, building $reaper_bin"
        mkdir -p "$(dirname "$reaper_bin")"
        gcc -O2 -static -Wall -Wextra "$reaper_src" -o "$reaper_bin"
    else
        printf '%s\n' "tuxreaperd binary not found at $reaper_bin (run ./compile.sh)" >&2
        exit 1
    fi
fi
reaper_dir=$(CDPATH= cd "$(dirname "$reaper_bin")" && pwd)
reaper_bin=$reaper_dir/$(basename "$reaper_bin")

if ! docker image inspect "$image" >/dev/null 2>&1; then
    printf '%s\n' "[TUI] Pull Docker Image: $image"
    run_raw "docker pull $image" docker pull "$image"
else
    printf '%s\n' "[TUI] Image available: $image"
fi

printf '%s\n' "[TUI] Create container with tuxreaperd as PID 1"
run_raw "docker create tuxreaperd container" \
    docker create --name "$name" -v "$reaper_bin:/usr/local/bin/tuxreaperd:ro" \
    "$image" /usr/local/bin/tuxreaperd sleep infinity
run_raw "docker start $name" docker start "$name"

printf '%s\n' "[TUI] Verify tuxreaperd is PID 1"
if [ "$(docker exec "$name" cat /proc/1/comm 2>/dev/null)" = "tuxreaperd" ]; then
    printf '%s\n' "[PASS] tuxreaperd is PID 1 in the container"
else
    printf '%s\n' "[FAIL] PID 1 is not tuxreaperd"
    failures=$((failures + 1))
fi

printf '%s\n' "[TUI] Spawn orphaned grandchildren inside the container"
# Each inner shell backgrounds a short-lived sleep and exits; the orphaned
# sleep reparents to PID 1 (tuxreaperd, the subreaper) and becomes a zombie on
# exit unless the SIGCHLD reaper loop reaps it.
docker exec "$name" sh -c '
    i=0
    while [ "$i" -lt 20 ]; do
        sh -c "sleep 0.1 &"
        i=$((i + 1))
    done
    sleep 2
'
printf '%s\n' "[TUI] Count zombie processes under PID 1"
zombie_count() {
    docker exec "$name" sh -c '
        zombies=0
        for f in /proc/[0-9]*/stat; do
            [ -r "$f" ] || continue
            line=
            IFS= read -r line < "$f" || continue
            rest=${line##*) }
            set -- $rest
            [ "$1" = Z ] && zombies=$((zombies + 1))
        done
        printf "%s\n" "$zombies"
    '
}
zombies=999
attempt=0
while [ "$attempt" -lt 10 ]; do
    zombies=$(zombie_count)
    [ "$zombies" -eq 0 ] && break
    attempt=$((attempt + 1))
    sleep 1
done
if [ "$zombies" -eq 0 ]; then
    printf '%s\n' "[PASS] No zombie processes remain under tuxreaperd"
else
    printf '%s\n' "[FAIL] $zombies zombie process(es) not reaped"
    failures=$((failures + 1))
fi

printf '%s\n' "[TUI] Verify exit-status propagation from the reaped child"
status=0
run_raw "docker run tuxreaperd exit-status" \
    docker run --rm -v "$reaper_bin:/usr/local/bin/tuxreaperd:ro" \
    "$image" /usr/local/bin/tuxreaperd sh -c 'exit 42' || status=$?
if [ "$status" -eq 42 ]; then
    printf '%s\n' "[PASS] Exit status 42 propagated from the child workload"
else
    printf '%s\n' "[FAIL] Expected exit status 42, got $status"
    failures=$((failures + 1))
fi

printf '%s\n' "[TUI] Verify SIGTERM forwarding through docker stop"
if docker stop "$name" >/dev/null 2>&1; then
    code=$(docker wait "$name")
    if [ "$code" = "143" ]; then
        printf '%s\n' "[PASS] SIGTERM forwarded to workload, clean shutdown (exit 143)"
    else
        printf '%s\n' "[FAIL] Expected container exit 143, got $code"
        failures=$((failures + 1))
    fi
else
    printf '%s\n' "[FAIL] docker stop failed (did tuxreaperd fail to forward SIGTERM?)"
    failures=$((failures + 1))
fi

printf '%s\n' "[TUI] Verify tuxreaperd waits for descendants after main child exits"
descendant_start=$(date +%s)
run_raw "docker run tuxreaperd descendant-wait" \
    docker run --rm -v "$reaper_bin:/usr/local/bin/tuxreaperd:ro" \
    "$image" /usr/local/bin/tuxreaperd sh -c 'sleep 3 & exit 0' || true
descendant_elapsed=$(( $(date +%s) - descendant_start ))
if [ "$descendant_elapsed" -ge 3 ] && [ "$descendant_elapsed" -le 10 ]; then
    printf '%s\n' "[PASS] tuxreaperd waited for descendant to finish (elapsed ${descendant_elapsed}s)"
else
    printf '%s\n' "[FAIL] Expected ~3s wait, got ${descendant_elapsed}s"
    failures=$((failures + 1))
fi

trap - EXIT INT TERM
if [ "$failures" -gt 0 ]; then
    printf '%s\n' "tuxreaperd tests failed: $failures failure(s)" >&2
    exit 1
fi

printf '%s\n' "[TUI] Remove Container: $name"
run_raw "docker rm -f $name" docker rm -f "$name"
printf '%s\n' "[TUI] Complete: Container removed"

printf '%s\n' "[TUI] Verify removal: docker container inspect $name"
if run_raw "docker container inspect $name" docker container inspect "$name"; then
    printf '%s\n' "Container still exists after removal" >&2
    exit 1
else
    printf '%s\n' "[TUI] Complete: Removal verified"
fi


printf '%s\n' "tuxreaperd tests passed"
exit "$cleanup_status"
