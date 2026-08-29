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
   Nginx and PHP-FPM both use SIGQUIT for graceful shutdown, so we
   translate SIGTERM to SIGQUIT for those processes. Other processes
   receive the original signal unchanged. */
static const char * const apache_exes[] = {
    "/usr/sbin/apache2",
    "/usr/sbin/httpd",
    "/usr/local/apache2/bin/httpd",
    0
};
#define APACHE_IN_SIG  SIGTERM
#define APACHE_OUT_SIG SIGWINCH

static const char * const nginx_exes[] = {
    "/usr/sbin/nginx",
    "/usr/local/nginx/sbin/nginx",
    0
};
#define NGINX_IN_SIG  SIGTERM
#define NGINX_OUT_SIG SIGQUIT

/* PHP-FPM installs versioned binaries like /usr/sbin/php-fpm8.4, so these
   rules use prefix matching to catch any version without maintaining a list. */
static const char * const phpfpm_exes[] = {
    "/usr/sbin/php-fpm",
    "/usr/local/sbin/php-fpm",
    0
};
#define PHPFPM_IN_SIG  SIGTERM
#define PHPFPM_OUT_SIG SIGQUIT

struct sig_rule {
    const char *const *target_exes;
    int out_sig;
    int prefix_match;  /* 0 = exact, 1 = prefix */
};

/* How long to keep the container alive after the main child exits, waiting
   for remaining descendants (e.g., Apache workers) to finish gracefully. */
#define DESCENDANT_TIMEOUT_SECONDS 60

/* Delay between the two /proc scans when broadcasting a signal, to catch
   processes that spawned just after the first scan. */
#define BROADCAST_SCAN_DELAY_US 100000

/* How often to poll while waiting for descendants after the main child exits.
   Short enough to react quickly, long enough to avoid burning CPU. */
#define DESCENDANT_POLL_INTERVAL_US 50000

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

static void proc_remap_signal(int in_sig, const struct sig_rule *rules, int num_rules) {
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

        int sent_sig = in_sig;
        for (int r = 0; r < num_rules; r++) {
            int matched = 0;
            for (int i = 0; rules[r].target_exes[i]; i++) {
                size_t target_len = strlen(rules[r].target_exes[i]);
                if (rules[r].prefix_match) {
                    if ((size_t)linklen >= target_len &&
                        strncmp(linkbuf, rules[r].target_exes[i], target_len) == 0) {
                        matched = 1;
                        break;
                    }
                } else {
                    if ((size_t)linklen == target_len &&
                        strcmp(linkbuf, rules[r].target_exes[i]) == 0) {
                        matched = 1;
                        break;
                    }
                }
            }
            if (matched) {
                sent_sig = rules[r].out_sig;
                break;
            }
        }

        debug("[tuxreaperd] scan pid=%ld exe=%s sending sig=%d",
              pid, linkbuf, sent_sig);
        if (kill((pid_t)pid, sent_sig) < 0) {
            debug("[tuxreaperd] kill(%ld, %d) failed: %s", pid, sent_sig, strerror(errno));
        }
    }

    closedir(proc);
}

static void broadcast_signal(int in_sig, const struct sig_rule *rules, int num_rules) {
    debug("[tuxreaperd] broadcasting in_sig=%d", in_sig);
    proc_remap_signal(in_sig, rules, num_rules);
    usleep(BROADCAST_SCAN_DELAY_US);
    proc_remap_signal(in_sig, rules, num_rules);
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
        if (sig == APACHE_IN_SIG || sig == NGINX_IN_SIG || sig == PHPFPM_IN_SIG) {
            struct sig_rule rules[3];
            int num_rules = 0;
            if (sig == APACHE_IN_SIG) {
                rules[num_rules].target_exes = apache_exes;
                rules[num_rules].out_sig = APACHE_OUT_SIG;
                rules[num_rules].prefix_match = 0;
                num_rules++;
            }
            if (sig == NGINX_IN_SIG) {
                rules[num_rules].target_exes = nginx_exes;
                rules[num_rules].out_sig = NGINX_OUT_SIG;
                rules[num_rules].prefix_match = 0;
                num_rules++;
            }
            if (sig == PHPFPM_IN_SIG) {
                rules[num_rules].target_exes = phpfpm_exes;
                rules[num_rules].out_sig = PHPFPM_OUT_SIG;
                rules[num_rules].prefix_match = 1;
                num_rules++;
            }
            broadcast_signal(sig, rules, num_rules);
        } else {
            broadcast_signal(sig, NULL, 0);
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
        if (g_child_exited) {
            long long now = monotonic_ms();
            if (child_exit_time_ms == 0) {
                child_exit_time_ms = now;
                debug("[tuxreaperd] main child exited, waiting up to %ds for descendants",
                      DESCENDANT_TIMEOUT_SECONDS);
            }

            drain_zombies();
            int descendants = count_descendants();
            debug("[tuxreaperd] descendants=%d", descendants);

            if (descendants == 0) {
                debug("[tuxreaperd] no descendants remaining, exiting cleanly");
                break;
            }

            if (now - child_exit_time_ms >= DESCENDANT_TIMEOUT_SECONDS * 1000LL) {
                debug("[tuxreaperd] descendant timeout reached, exiting");
                break;
            }

            // Poll briefly so the 60-second deadline can tick even if no
            // SIGCHLD or external signal arrives. handle_pending_signals()
            // catches any signal that was delivered while we slept.
            usleep(DESCENDANT_POLL_INTERVAL_US);
            handle_pending_signals();
        } else {
            // Workload active: block atomically with zero CPU wakeups until a
            // signal arrives.
            sigsuspend(&old_mask);
            drain_zombies();
            handle_pending_signals();
        }
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
