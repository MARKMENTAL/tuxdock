struct k_sigaction {
    void (*handler)(int);
    unsigned long flags;
    void (*restorer)(void);
    unsigned long mask;
};

#if defined(__x86_64__)

static inline long sys_write(int fd, const void *buf, long n) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(1), "D"((long)fd), "S"(buf), "d"(n)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_exit_group(int status) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(231), "D"((long)status)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_fork(void) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(57)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_execve(const char *path, char *const argv[], char *const envp[]) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(59), "D"(path), "S"(argv), "d"(envp)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_wait4(long pid, int *status, int options, void *rusage) {
    long ret;
    register long r10 __asm__("r10") = (long)rusage;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(61), "D"(pid), "S"(status), "d"(options), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_kill(long pid, int sig) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(62), "D"(pid), "S"((long)sig)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
                             unsigned long arg4, unsigned long arg5) {
    long ret;
    register long r10 __asm__("r10") = (long)arg4;
    register long r8 __asm__("r8") = (long)arg5;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(157), "D"((long)option), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_rt_sigaction(int sig, const struct k_sigaction *act,
                                    struct k_sigaction *oldact, unsigned long sigsetsize) {
    long ret;
    register long r10 __asm__("r10") = (long)sigsetsize;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(13), "D"((long)sig), "S"(act), "d"(oldact), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_rt_sigprocmask(int how, const unsigned long *set,
                                      unsigned long *oldset, unsigned long sigsetsize) {
    long ret;
    register long r10 __asm__("r10") = (long)sigsetsize;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(14), "D"((long)how), "S"(set), "d"(oldset), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_rt_sigsuspend(const unsigned long *mask, unsigned long sigsetsize) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(130), "D"(mask), "S"(sigsetsize)
        : "rcx", "r11", "memory"
    );
    return ret;
}

__asm__(
    ".text\n"
    ".globl restore_rt\n"
    "restore_rt:\n"
    "    movq $15, %rax\n"
    "    syscall\n"
);

__asm__(
    ".text\n"
    ".globl _start\n"
    "_start:\n"
    "    xorl %ebp, %ebp\n"
    "    movq (%rsp), %rdi\n"      /* argc */
    "    leaq 8(%rsp), %rsi\n"     /* argv */
    "    movq %rsi, %rdx\n"        /* scan pointer = argv */
    "1:\n"
    "    addq $8, %rdx\n"
    "    cmpq $0, (%rdx)\n"
    "    jne 1b\n"
    "    addq $8, %rdx\n"          /* envp */
    "    andq $-16, %rsp\n"        /* align stack */
    "    call main\n"
    "    movq %rax, %rdi\n"
    "    movq $231, %rax\n"        /* exit_group */
    "    syscall\n"
);

#elif defined(__aarch64__)

static inline long sys_write(int fd, const void *buf, long n) {
    register long x8 __asm__("x8") = 64;
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = n;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_exit_group(int status) {
    register long x8 __asm__("x8") = 94;
    register long x0 __asm__("x0") = status;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_fork(void) {
    /* AArch64 has no legacy fork syscall; clone(SIGCHLD, 0) gives fork semantics. */
    register long x8 __asm__("x8") = 220;  /* __NR_clone */
    register long x0 __asm__("x0") = 17;   /* SIGCHLD */
    register long x1 __asm__("x1") = 0;    /* newsp = 0 */
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_execve(const char *path, char *const argv[], char *const envp[]) {
    register long x8 __asm__("x8") = 221;
    register long x0 __asm__("x0") = (long)path;
    register long x1 __asm__("x1") = (long)argv;
    register long x2 __asm__("x2") = (long)envp;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_wait4(long pid, int *status, int options, void *rusage) {
    register long x8 __asm__("x8") = 260;
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = (long)status;
    register long x2 __asm__("x2") = options;
    register long x3 __asm__("x3") = (long)rusage;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_kill(long pid, int sig) {
    register long x8 __asm__("x8") = 129;
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = sig;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
                             unsigned long arg4, unsigned long arg5) {
    register long x8 __asm__("x8") = 167;
    register long x0 __asm__("x0") = option;
    register long x1 __asm__("x1") = arg2;
    register long x2 __asm__("x2") = arg3;
    register long x3 __asm__("x3") = arg4;
    register long x4 __asm__("x4") = arg5;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigaction(int sig, const struct k_sigaction *act,
                                    struct k_sigaction *oldact, unsigned long sigsetsize) {
    register long x8 __asm__("x8") = 134;
    register long x0 __asm__("x0") = sig;
    register long x1 __asm__("x1") = (long)act;
    register long x2 __asm__("x2") = (long)oldact;
    register long x3 __asm__("x3") = sigsetsize;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigprocmask(int how, const unsigned long *set,
                                      unsigned long *oldset, unsigned long sigsetsize) {
    register long x8 __asm__("x8") = 135;
    register long x0 __asm__("x0") = how;
    register long x1 __asm__("x1") = (long)set;
    register long x2 __asm__("x2") = (long)oldset;
    register long x3 __asm__("x3") = sigsetsize;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigsuspend(const unsigned long *mask, unsigned long sigsetsize) {
    register long x8 __asm__("x8") = 133;
    register long x0 __asm__("x0") = (long)mask;
    register long x1 __asm__("x1") = sigsetsize;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1)
        : "memory", "cc"
    );
    return x0;
}

__asm__(
    ".text\n"
    ".globl restore_rt\n"
    "restore_rt:\n"
    "    mov x8, #139\n"
    "    svc #0\n"
);

__asm__(
    ".text\n"
    ".globl _start\n"
    "_start:\n"
    "    ldr x0, [sp]\n"          /* argc */
    "    add x1, sp, #8\n"        /* argv */
    "    mov x2, x1\n"            /* scan pointer = argv */
    "1:\n"
    "    ldr x3, [x2]\n"
    "    add x2, x2, #8\n"
    "    cbnz x3, 1b\n"          /* x2 now points to envp */
    "    mov x4, sp\n"
    "    bic x4, x4, #15\n"      /* align stack via scratch register */
    "    mov sp, x4\n"
    "    bl main\n"
    "    mov x8, #94\n"           /* exit_group */
    "    svc #0\n"
);

#else
#error "tuxreaperdasm.c supports x86_64 and aarch64 only"
#endif

static long my_strlen(const char *s) {
    long n = 0;
    while (s[n]) n++;
    return n;
}

static int my_strncmp(const char *a, const char *b, long n) {
    for (long i = 0; i < n; i++) {
        unsigned char ca = a[i];
        unsigned char cb = b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0) return 0;
    }
    return 0;
}

static const char *my_strchr(const char *s, char c) {
    while (*s) {
        if (*s == c) return s;
        s++;
    }
    return 0;
}

static void my_memcpy(char *dst, const char *src, long n) {
    for (long i = 0; i < n; i++) dst[i] = src[i];
}

static const char *my_getenv(const char *name, char *const *envp) {
    long nlen = my_strlen(name);
    for (int i = 0; envp[i]; i++) {
        if (my_strncmp(envp[i], name, nlen) == 0 && envp[i][nlen] == '=') {
            return envp[i] + nlen + 1;
        }
    }
    return 0;
}

static void my_execvp(const char *file, char *const argv[], char *const envp[]) {
    if (my_strchr(file, '/')) {
        sys_execve(file, argv, envp);
        return;
    }

    const char *path = my_getenv("PATH", envp);
    if (!path) path = "/bin:/usr/bin";

    long flen = my_strlen(file);
    const char *p = path;

    while (*p) {
        const char *colon = my_strchr(p, ':');
        long dirlen = colon ? (colon - p) : my_strlen(p);
        char full[512];

        if (dirlen + 1 + flen + 1 <= (long)sizeof(full)) {
            my_memcpy(full, p, dirlen);
            full[dirlen] = '/';
            my_memcpy(full + dirlen + 1, file, flen);
            full[dirlen + 1 + flen] = '\0';
            sys_execve(full, argv, envp);
        }

        if (!colon) break;
        p = colon + 1;
    }
}

static volatile long g_main_child = 0;
static volatile int g_child_exited = 0;
static volatile int g_main_status = 0;

static void forward_sig(int sig) {
    if (g_main_child > 0) {
        sys_kill(g_main_child, sig);
    }
}

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    long pid;

    while ((pid = sys_wait4(-1, &status, 1 /* WNOHANG */, 0)) > 0) {
        if (pid == g_main_child) {
            g_child_exited = 1;
            g_main_status = status;
        }
    }
}

#define WIFEXITED(s)   (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) ((((s) & 0x7f) != 0) && (((s) & 0x7f) != 0x7f))
#define WTERMSIG(s)    ((s) & 0x7f)

void restore_rt(void);

int main(int argc, char **argv, char **envp) {
    if (argc < 2) {
        static const char usage[] = "Usage: tuxreaperd <command> [args...]\n";
        sys_write(2, usage, sizeof(usage) - 1);
        return 1;
    }

    if (sys_prctl(36 /* PR_SET_CHILD_SUBREAPER */, 1, 0, 0, 0) < 0) {
        static const char err[] = "prctl(PR_SET_CHILD_SUBREAPER) failed\n";
        sys_write(2, err, sizeof(err) - 1);
        return 1;
    }

    struct k_sigaction sa;
    sa.handler = sigchld_handler;
    sa.flags = 0x10000000 /* SA_RESTART */ | 1 /* SA_NOCLDSTOP */ | 0x04000000 /* SA_RESTORER */;
    sa.restorer = restore_rt;
    sa.mask = 0;
    sys_rt_sigaction(17 /* SIGCHLD */, &sa, 0, sizeof(sa.mask));

    sa.handler = forward_sig;
    sa.flags = 0x10000000 /* SA_RESTART */ | 0x04000000 /* SA_RESTORER */;
    sa.restorer = restore_rt;
    sa.mask = 0;
    sys_rt_sigaction(15 /* SIGTERM */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(2 /* SIGINT */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(1 /* SIGHUP */, &sa, 0, sizeof(sa.mask));

    unsigned long block_mask =
        (1UL << (17 - 1)) | (1UL << (15 - 1)) |
        (1UL << (2 - 1))  | (1UL << (1 - 1));
    unsigned long old_mask;
    sys_rt_sigprocmask(0 /* SIG_BLOCK */, &block_mask, &old_mask, sizeof(block_mask));

    g_main_child = sys_fork();
    if (g_main_child == 0) {
        sys_rt_sigprocmask(2 /* SIG_SETMASK */, &old_mask, 0, sizeof(old_mask));
        my_execvp(argv[1], &argv[1], envp);
        static const char err[] = "execvp failed\n";
        sys_write(2, err, sizeof(err) - 1);
        sys_exit_group(127);
    }

    while (!g_child_exited) {
        sys_rt_sigsuspend(&old_mask, sizeof(old_mask));
    }

    int status;
    while (sys_wait4(-1, &status, 1 /* WNOHANG */, 0) > 0);

    if (WIFEXITED(g_main_status)) {
        return WEXITSTATUS(g_main_status);
    } else if (WIFSIGNALED(g_main_status)) {
        return 128 + WTERMSIG(g_main_status);
    }

    return 0;
}
