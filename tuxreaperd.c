#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <errno.h>

static volatile pid_t g_main_child = 0;
static volatile sig_atomic_t g_child_exited = 0;
static volatile int g_main_status = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;

    // Reap all terminated orphans
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == g_main_child) {
            g_child_exited = 1;
            g_main_status = status;
        }
    }
}

static void forward_signal(int sig) {
    if (g_main_child > 0) {
        kill(g_main_child, sig);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    // Become a subreaper for any reparented processes down the tree
    if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) < 0) {
        perror("prctl(PR_SET_CHILD_SUBREAPER)");
        return 1;
    }

    // Set up signal forwarding
    struct sigaction sa_forward;
    sa_forward.sa_handler = forward_signal;
    sigemptyset(&sa_forward.sa_mask);
    sa_forward.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa_forward, NULL);
    sigaction(SIGINT, &sa_forward, NULL);
    sigaction(SIGHUP, &sa_forward, NULL);

    // Set up SIGCHLD reaper handler
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    // Spawn the primary workload
    g_main_child = fork();
    if (g_main_child < 0) {
        perror("fork");
        return 1;
    }

    if (g_main_child == 0) {
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(127);
    }

    // Block until the primary process exits
    while (!g_child_exited) {
        pause();
    }

    // Final sweep of any remaining lingering zombies
    while (waitpid(-1, NULL, WNOHANG) > 0);

    if (WIFEXITED(g_main_status)) {
        return WEXITSTATUS(g_main_status);
    } else if (WIFSIGNALED(g_main_status)) {
        return 128 + WTERMSIG(g_main_status);
    }

    return 0;
}

