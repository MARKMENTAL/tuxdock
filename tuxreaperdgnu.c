#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#ifdef TUXREAPERD_DEBUG
#include <stdarg.h>
static void debug(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
#else
static void debug(const char *fmt, ...) {
    (void)fmt;
}
#endif

/* Apache hijacks SIGWINCH (window resize) for graceful shutdown. Yes,
   really. So when the outside world sends SIGTERM, we translate it to
   SIGWINCH for anything whose /proc/<pid>/exe smells like apache2.
   Other processes receive the original signal unchanged. */
static const char * const apache_exes[] = {
    "/usr/sbin/apache2",
    "/usr/sbin/httpd",
    "/usr/local/apache2/bin/httpd",
    0
};
#define APACHE_IN_SIG  SIGTERM
#define APACHE_OUT_SIG SIGWINCH

/* How long to keep the container alive after the main child exits, waiting
   for remaining descendants (e.g., Apache workers) to finish gracefully. */
#define DESCENDANT_TIMEOUT_SECONDS 60

/* Delay between the two /proc scans when broadcasting a signal, to catch
   processes that spawned just after the first scan. */
#define BROADCAST_SCAN_DELAY_US 100000

static volatile pid_t g_main_child = 0;
static volatile sig_atomic_t g_child_exited = 0;
static volatile int g_main_status = 0;
static volatile sig_atomic_t g_pending_signals = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    // Reap all terminated descendants. Note when the main child exits.
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_main_child) {
            g_child_exited = 1;
            g_main_status = status;
        }
    }
}

static void proxy_signal_handler(int sig) {
    // Record the signal in a bitmask. Signals 1..31 fit comfortably.
    if (sig >= 1 && sig <= 31) {
        g_pending_signals |= (sig_atomic_t)(1U << (sig - 1));
    }
}

static void proc_remap_signal(int in_sig, int out_sig, const char *const *target_exes) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        debug("[tuxreaperd] opendir(/proc) failed: %s", strerror(errno));
        return;
    }

    struct dirent *de;

    while ((de = readdir(proc)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char *endptr;
        long pid = strtol(de->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 1) continue;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%ld/exe", pid);

        char linkbuf[256];
        ssize_t linklen = readlink(path, linkbuf, sizeof(linkbuf) - 1);
        if (linklen <= 0) continue;
        linkbuf[linklen] = '\0';

        int matched = 0;
        for (int i = 0; target_exes[i]; i++) {
            size_t target_len = strlen(target_exes[i]);
            if ((size_t)linklen == target_len && strcmp(linkbuf, target_exes[i]) == 0) {
                matched = 1;
                break;
            }
        }

        int sent_sig = matched ? out_sig : in_sig;
        debug("[tuxreaperd] scan pid=%ld exe=%s matched=%d sending sig=%d",
              pid, linkbuf, matched, sent_sig);
        if (kill((pid_t)pid, sent_sig) < 0) {
            debug("[tuxreaperd] kill(%ld, %d) failed: %s", pid, sent_sig, strerror(errno));
        }
    }

    closedir(proc);
}

static void broadcast_signal(int in_sig, int out_sig, const char *const *target_exes) {
    debug("[tuxreaperd] broadcasting in_sig=%d out_sig=%d", in_sig, out_sig);
    proc_remap_signal(in_sig, out_sig, target_exes);
    usleep(BROADCAST_SCAN_DELAY_US);
    proc_remap_signal(in_sig, out_sig, target_exes);
}

static int count_descendants(void) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    int count = 0;
    struct dirent *de;
    while ((de = readdir(proc)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char *endptr;
        long pid = strtol(de->d_name, &endptr, 10);
        if (*endptr != '\0' || pid <= 1) continue;
        count++;
    }
    closedir(proc);
    return count;
}

static void drain_zombies(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

static void handle_pending_signals(void) {
    sig_atomic_t pending = g_pending_signals;
    g_pending_signals = 0;

    for (int sig = 1; sig <= 31; sig++) {
        if (!(pending & (sig_atomic_t)(1U << (sig - 1)))) continue;

        debug("[tuxreaperd] handling pending signal %d", sig);
        if (sig == APACHE_IN_SIG) {
            broadcast_signal(APACHE_IN_SIG, APACHE_OUT_SIG, apache_exes);
        } else {
            broadcast_signal(sig, sig, apache_exes);
        }
    }
}

static long long monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    // Become a subreaper for any reparented processes down the tree.
    if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) < 0) {
        perror("prctl(PR_SET_CHILD_SUBREAPER)");
        return 1;
    }

    // Set up signal forwarding.
    struct sigaction sa_forward;
    sa_forward.sa_handler = proxy_signal_handler;
    sigemptyset(&sa_forward.sa_mask);
    sa_forward.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa_forward, NULL);
    sigaction(SIGQUIT, &sa_forward, NULL);
    sigaction(SIGINT, &sa_forward, NULL);
    sigaction(SIGHUP, &sa_forward, NULL);
    sigaction(SIGUSR1, &sa_forward, NULL);
    sigaction(SIGUSR2, &sa_forward, NULL);

    // Set up SIGCHLD reaper handler.
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    // Block the signals we wait on so the check/suspend loop is race-free.
    sigset_t block_mask, old_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGCHLD);
    sigaddset(&block_mask, SIGTERM);
    sigaddset(&block_mask, SIGQUIT);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGHUP);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

    // Spawn the primary workload.
    g_main_child = fork();
    if (g_main_child < 0) {
        perror("fork");
        return 1;
    }

    if (g_main_child == 0) {
        // Become the leader of a new process group so signals can be
        // broadcast to the whole workload tree.
        setpgid(0, 0);
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(127);
    }

    debug("[tuxreaperd] started main_child=%d", (int)g_main_child);

    long long child_exit_time_ms = 0;

    while (1) {
        int descendants = 0;

        if (g_child_exited) {
            long long now = monotonic_ms();
            if (child_exit_time_ms == 0) {
                child_exit_time_ms = now;
                debug("[tuxreaperd] main child exited, waiting up to %ds for descendants",
                      DESCENDANT_TIMEOUT_SECONDS);
            }

            descendants = count_descendants();
            debug("[tuxreaperd] descendants=%d", descendants);

            if (descendants == 0) {
                debug("[tuxreaperd] no descendants remaining, exiting cleanly");
                break;
            }

            if (now - child_exit_time_ms >= DESCENDANT_TIMEOUT_SECONDS * 1000LL) {
                debug("[tuxreaperd] descendant timeout reached, exiting");
                break;
            }
        }

        // Wait for a signal. When one arrives, reap and handle it, then loop
        // back to re-evaluate descendants and timeouts.
        sigsuspend(&old_mask);

        drain_zombies();
        handle_pending_signals();
    }

    // Final sweep of any remaining lingering zombies.
    drain_zombies();

    if (WIFEXITED(g_main_status)) {
        return WEXITSTATUS(g_main_status);
    } else if (WIFSIGNALED(g_main_status)) {
        return 128 + WTERMSIG(g_main_status);
    }

    return 0;
}
