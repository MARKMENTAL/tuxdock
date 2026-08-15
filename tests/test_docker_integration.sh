#!/bin/sh

set -eu

image=${TUX_DOCK_TEST_IMAGE:-debian:forky}
name="tux-dock-test-$$-$(date +%s)"
cleanup_status=0

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
        printf '%s\n' "[TUI] Busy |"
        if ! docker rm -f "$name"; then
            cleanup_status=1
        fi
        printf '%s\n' "[TUI] Complete: Container removed"
    fi
}
trap cleanup EXIT INT TERM

printf '%s\n' "[TUI] Actions > Create Container"
printf '%s\n' "[TUI] Busy |"
printf '%s\n' "[TUI] docker create --name $name $image sleep infinity"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    printf '%s\n' "[TUI] Pull Docker Image: $image"
    run_raw "docker pull $image" docker pull "$image"
else
    printf '%s\n' "[TUI] Image available: $image"
fi

run_in_container() {
    command_name=$1
    shift
    printf '%s\n' "[TUI] Run command: $command_name"
    printf '%s\n' "[TUI] Busy |"
    run_raw "$command_name" docker exec "$name" "$@"
    printf '%s\n' "[TUI] Complete: $command_name succeeded"
}

run_raw "docker create --name $name $image sleep infinity" docker create --name "$name" "$image" sleep infinity
printf '%s\n' "[TUI] Complete: Container created"
run_raw "docker start $name" docker start "$name"
printf '%s\n' "[TUI] Complete: Container started"
run_in_container "ls /" ls /
run_in_container "cat /etc/os-release" cat /etc/os-release
run_in_container "apt-get update" apt-get update
run_in_container "bash --version" bash --version

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

trap - EXIT INT TERM
exit "$cleanup_status"
