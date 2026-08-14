#!/bin/sh

set -eu

run_tests=1
web_view=0
web_port=8095

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
        *)
            printf '%s\n' "Usage: $0 [--no-test] [--web-test-view [port]]" >&2
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

if [ "$run_tests" -eq 1 ]; then
    cmake -S . -B build -DBUILD_TESTING=ON
else
    cmake -S . -B build -DBUILD_TESTING=OFF
fi

cmake --build build -j

if [ "$run_tests" -eq 1 ]; then
    report_dir=$(mktemp -d /tmp/tux-dock-test.XXXXXX)
    test_output="$report_dir/ctest-output.txt"
    test_status=0
    ctest --test-dir build --output-on-failure >"$test_output" 2>&1 || test_status=$?

    if [ "$web_view" -eq 1 ]; then
        report="$report_dir/report.html"
        escaped_output=$(mktemp "$report_dir/output.XXXXXX")
        sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g' "$test_output" >"$escaped_output"
        total=$(awk '/tests passed, [0-9]+ tests failed out of [0-9]+/ { print $(NF); exit }' "$test_output" 2>/dev/null || true)
        failed=$(awk '/tests passed, [0-9]+ tests failed out of [0-9]+/ { print $4; exit }' "$test_output" 2>/dev/null || true)
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
            match($0, /^[[:space:]]*([0-9]+)\/([0-9]+) Test #[0-9]+: ([^ ]+)[[:space:]]+\.+[[:space:]]+(Passed|Failed|Skipped)[[:space:]]+([0-9.]+) sec/, m) {
                printf "<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s sec</TD></TR>\n", m[1], m[3], m[4], m[5]
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

        if command -v nc >/dev/null 2>&1; then
            nc_command=nc
        elif command -v netcat >/dev/null 2>&1; then
            nc_command=netcat
        else
            printf '%s\n' "Web report generated at $report, but nc/netcat is unavailable." >&2
            exit "$test_status"
        fi

        printf '%s\n' "Test report: http://0.0.0.0:$web_port/"
        printf '%s\n' "Report file: $report"
        printf '%s\n' "Press Ctrl-C to stop the server."
        while :; do
            "$nc_command" -l -N -s 0.0.0.0 -p "$web_port" <"$response"
        done
    fi
    exit "$test_status"
fi

printf '%s\n' "tux-dock successfully compiled!"
