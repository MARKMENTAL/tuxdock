#!/bin/sh

set -eu

run_tests=1
web_view=0
web_port=8095
force_libc_reaper=0

memory_limit=$(cat /sys/fs/cgroup/memory.max 2>/dev/null || true)
if [ -z "$memory_limit" ] || [ "$memory_limit" = max ]; then
    memory_limit=$(cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null || true)
fi
if [ -z "$memory_limit" ] || [ "$memory_limit" = max ] || [ "$memory_limit" -ge 9223372036854771712 ] 2>/dev/null; then
    memory_limit=$(awk '/MemTotal:/ { print $2 * 1024; exit }' /proc/meminfo 2>/dev/null || true)
fi
low_ram=0
if [ -n "$memory_limit" ] && [ "$memory_limit" -le 1073741824 ] 2>/dev/null; then
    low_ram=1
fi

build_jobs=4
cpu_count=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)
case "$cpu_count" in
    ''|*[!0-9]*) ;;
    *)
        if [ "$cpu_count" -lt "$build_jobs" ]; then
            build_jobs=$cpu_count
        fi
        ;;
esac
if [ "$low_ram" -eq 1 ]; then
    build_jobs=1
fi

while [ "$#" -gt 0 ]; do
    argument=$1
    shift
    case "$argument" in
        --no-test) run_tests=0 ;;
        --web-test-view)
            web_view=1
            if [ "$#" -gt 0 ] && printf '%s' "$1" | grep -Eq '^[0-9]+$'; then
                web_port=$1
                shift
            fi
            ;;
        --force-libc-reaper) force_libc_reaper=1 ;;
        *)
            printf '%s\n' "Usage: $0 [--no-test] [--web-test-view [port]] [--force-libc-reaper]" >&2
            exit 2
            ;;
    esac
done

if [ "$web_port" -lt 1 ] || [ "$web_port" -gt 65535 ]; then
    printf '%s\n' "Web test view port must be between 1 and 65535" >&2
    exit 2
fi

if [ "$web_view" -eq 1 ] && [ "$run_tests" -eq 0 ]; then
    printf '%s\n' "--web-test-view cannot be combined with --no-test" >&2
    exit 2
fi

cmake_args=""
if [ "$low_ram" -eq 1 ]; then
    cmake_args="-DCMAKE_BUILD_TYPE=MinSizeRel"
fi

if [ "$run_tests" -eq 1 ]; then
    # shellcheck disable=SC2086
    cmake -S . -B build -DBUILD_TESTING=ON $cmake_args
else
    # shellcheck disable=SC2086
    cmake -S . -B build -DBUILD_TESTING=OFF $cmake_args
fi

command -v gcc >/dev/null 2>&1 || {
    printf '%s\n' "gcc is required to build tuxreaperd" >&2
    exit 1
}

host_arch=$(uname -m)
asm_archs='x86_64 amd64 aarch64 arm64'
page_size=$(getconf PAGE_SIZE 2>/dev/null || printf '%s\n' 4096)
build_tuxreaperd() {
    case " $asm_archs " in
        *" $host_arch "*)
            if [ "$force_libc_reaper" -eq 0 ]; then
                printf '%s\n' "[compile] Building freestanding tuxreaperd for $host_arch"
                if gcc -nostdlib -static -fno-stack-protector -fno-asynchronous-unwind-tables \
                       -fno-builtin -O2 -s -Wl,--build-id=none -Wl,-z,noseparate-code \
                       -Wl,-z,max-page-size=${page_size} \
                       -Wall -Wextra tuxreaperdasm.c -o build/tuxreaperd 2>/tmp/tuxreaperd-asm-build.log; then
                    printf '%s\n' "[compile] Freestanding tuxreaperd built successfully"
                    return 0
                fi
                printf '%s\n' "[compile] Freestanding build failed, falling back to libc implementation" >&2
                cat /tmp/tuxreaperd-asm-build.log >&2
            else
                printf '%s\n' "[compile] --force-libc-reaper set, building libc tuxreaperd"
            fi
            ;;
    esac

    printf '%s\n' "[compile] Building standard tuxreaperd for $host_arch"
    gcc -O2 -static -Wall -Wextra tuxreaperdgnu.c -o build/tuxreaperd
}
build_tuxreaperd

CMAKE_BUILD_PARALLEL_LEVEL="$build_jobs" cmake --build build

if [ "$run_tests" -eq 1 ]; then
    report_dir=$(mktemp -d /tmp/tux-dock-test.XXXXXX)
    test_output="$report_dir/ctest-output.txt"
    test_status=0
    ctest --test-dir build --output-on-failure >"$test_output" 2>&1 || test_status=$?

    if [ "$web_view" -eq 1 ]; then
        report="$report_dir/report.html"
        escaped_output=$(mktemp "$report_dir/output.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$test_output" >"$escaped_output"
        total=$(awk '/tests passed, .* tests failed out of / { print $(NF); exit }' "$test_output" 2>/dev/null || true)
        failed=$(awk '/tests passed, .* tests failed out of / { print $4; exit }' "$test_output" 2>/dev/null || true)
        total=${total:-unknown}
        passed=${passed:-0}
        failed=${failed:-0}
        if [ "$total" != unknown ]; then
            passed=$((total - failed))
        fi
        duration=$(awk '/Total Test time/ { print $(NF - 1); exit }' "$test_output" 2>/dev/null || true)
        duration=${duration:-unknown}
        command_line='ctest --test-dir build --output-on-failure'
        test_rows=$(mktemp "$report_dir/test-rows.XXXXXX")
        awk '
            /^[[:space:]]*[0-9]+\/[0-9]+ Test #[0-9]+:/ {
                test_number = $1
                test_name = $3
                sub(/^#[0-9]+:/, "", test_name)
                status_field = 0
                for (i = 1; i <= NF; i++) {
                    if ($i == "Passed" || $i == "Failed" || $i == "Skipped") {
                        status_field = i
                        break
                    }
                }
                if (status_field > 4) {
                    for (i = 4; i < status_field; i++) {
                        if ($i !~ /^\.*$/) test_name = test_name " " $i
                    }
                    sub(/^[[:space:]]+/, "", test_name)
                    status = $(status_field)
                    seconds = $(status_field + 1)
                    printf "<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s sec</TD></TR>\n", test_number, test_name, status, seconds
                }
            }
        ' "$test_output" >"$test_rows"
        integration_output=$(mktemp "$report_dir/integration-output.XXXXXX")
        test_log="build/Testing/Temporary/LastTest.log"
        if [ -f "$test_log" ]; then
            awk '
                /^[0-9]+\/[0-9]+ Testing: tux-dock-docker-integration-tests$/ { capture=1 }
                capture { print }
                /^Test Passed\.$/ && capture { end=1 }
                end && /time elapsed:/ { print; exit }
            ' "$test_log" >"$integration_output"
        else
            printf '%s\n' 'Docker integration transcript unavailable: LastTest.log was not found.' >"$integration_output"
        fi
        escaped_integration=$(mktemp "$report_dir/integration-escaped.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$integration_output" >"$escaped_integration"
        raw_output=$(mktemp "$report_dir/raw-output.XXXXXX")
        awk '/\[TEST OUTPUT BEGIN\]/{capture=1} capture{print} /\[TEST RESULT\]/{capture=0}' "$integration_output" >"$raw_output"
        escaped_raw=$(mktemp "$report_dir/raw-escaped.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$raw_output" >"$escaped_raw"
        tui_output=$(mktemp "$report_dir/tui-output.XXXXXX")
        awk '!/\[TEST OUTPUT BEGIN\]/ && !/\[TEST OUTPUT END\]/ && !/\[TEST RESULT\]/ { print }' "$integration_output" >"$tui_output"
        escaped_tui=$(mktemp "$report_dir/tui-escaped.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$tui_output" >"$escaped_tui"
        reaper_output=$(mktemp "$report_dir/reaper-output.XXXXXX")
        if [ -f "$test_log" ]; then
            awk '
                /^[0-9]+\/[0-9]+ Testing: tuxreaperd-zombie-tests$/ { capture=1 }
                capture { print }
                /^Test Passed\.$/ && capture { end=1 }
                end && /time elapsed:/ { print; exit }
            ' "$test_log" >"$reaper_output"
        else
            printf '%s\n' 'tuxreaperd zombie transcript unavailable: LastTest.log was not found.' >"$reaper_output"
        fi
        escaped_reaper=$(mktemp "$report_dir/reaper-escaped.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$reaper_output" >"$escaped_reaper"
        {
            printf '%s\n' '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">'
            printf '%s\n' '<HTML><HEAD><TITLE>Tux-Dock Test Report</TITLE></HEAD><BODY>'
            printf '%s\n' '<H1>Tux-Dock Test Report</H1>'
            printf '<P>Generated: %s</P>\n' "$(date)"
            printf '%s\n' '<H2>Summary</H2><TABLE BORDER="1"><TR><TH>Metric</TH><TH>Value</TH><TH>Graph</TH></TR>'
            printf '<TR><TD>Total tests</TD><TD>%s</TD><TD><TABLE BORDER="0"><TR><TD BGCOLOR="blue" WIDTH="160">&nbsp;</TD></TR></TABLE></TD></TR>\n' "$total"
            printf '<TR><TD>Passed</TD><TD>%s</TD><TD><TABLE BORDER="0"><TR><TD BGCOLOR="green" WIDTH="160">&nbsp;</TD></TR></TABLE></TD></TR>\n' "$passed"
            printf '<TR><TD>Failed</TD><TD>%s</TD><TD><TABLE BORDER="0"><TR><TD BGCOLOR="red" WIDTH="%s">&nbsp;</TD></TR></TABLE></TD></TR>\n' "$failed" "$([ "$failed" -gt 0 ] 2>/dev/null && printf 160 || printf 1)"
            printf '<TR><TD>Duration</TD><TD>%s seconds</TD><TD>&nbsp;</TD></TR></TABLE>\n' "$duration"
            printf '%s\n' '<H2>Individual Tests</H2><TABLE BORDER="1"><TR><TH>#</TH><TH>Test</TH><TH>Status</TH><TH>Duration</TH></TR>'
            cat "$test_rows"
            printf '%s\n' '</TABLE>'
            printf '%s\n' '<H2>Exact Test Run</H2><PRE>'
            printf '%s\n' "$command_line"
            printf '%s\n' '</PRE><H2>Test Output</H2><PRE>'
            cat "$escaped_output"
            printf '%s\n' '</PRE><H2>Docker Integration Raw Output</H2><PRE>'
            cat "$escaped_raw"
            printf '%s\n' '</PRE><H2>TUI Transcript</H2><PRE>'
            cat "$escaped_tui"
            printf '%s\n' '</PRE><H2>Tuxreaperd Zombie Reaping</H2><PRE>'
            cat "$escaped_reaper"
            printf '%s\n' '</PRE></BODY></HTML>'
        } >"$report"

        response="$report_dir/http-response.txt"
        report_size=$(wc -c <"$report" | tr -d ' ')
        {
            printf 'HTTP/1.1 200 OK\r\n'
            printf 'Content-Type: text/html; charset=utf-8\r\n'
            printf 'Content-Length: %s\r\n' "$report_size"
            printf 'Connection: close\r\n'
            printf '\r\n'
            cat "$report"
        } >"$response"

        if command -v ncat >/dev/null 2>&1; then
            nc_command=ncat
            nc_args='-l'
            nc_address='0.0.0.0'
            nc_port="$web_port"
        elif command -v nc >/dev/null 2>&1; then
            nc_command=nc
            if "$nc_command" -h 2>&1 | grep -q -- '-N'; then
                nc_args='-l -N -s 0.0.0.0 -p'
            else
                nc_args='-l -s 0.0.0.0 -p'
            fi
            nc_address=''
            nc_port="$web_port"
        elif command -v netcat >/dev/null 2>&1; then
            nc_command=netcat
            if "$nc_command" -h 2>&1 | grep -q -- '-N'; then
                nc_args='-l -N -s 0.0.0.0 -p'
            else
                nc_args='-l -s 0.0.0.0 -p'
            fi
            nc_address=''
            nc_port="$web_port"
        else
            printf '%s\n' "Web report generated at $report, but nc/netcat is unavailable." >&2
            exit "$test_status"
        fi

        printf '%s\n' "Test report: http://0.0.0.0:$web_port/"
        printf '%s\n' "Report file: $report"
        printf '%s\n' "Press Ctrl-C to stop the server."
        while :; do
            # shellcheck disable=SC2086
            "$nc_command" $nc_args ${nc_address:+"$nc_address"} "$nc_port" <"$response"
        done
    fi
    exit "$test_status"
fi

printf '%s\n' "tux-dock successfully compiled!"
