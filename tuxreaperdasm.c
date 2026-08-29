struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct k_sigaction {
    void (*handler)(int);
    unsigned long flags;
    void (*restorer)(void);
    unsigned long mask;
};

#if defined(__x86_64__)
#define SA_RESTORER_FLAG 0x04000000UL
void restore_rt(void);
#elif defined(__aarch64__)
#define SA_RESTORER_FLAG 0UL
#define restore_rt ((void (*)(void))0)
#endif

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

static inline long sys_setpgid(long pid, long pgid) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(109), "D"(pid), "S"(pgid)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_openat(int dirfd, const char *path, int flags, int mode) {
    long ret;
    register long r10 __asm__("r10") = mode;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(257), "D"((long)dirfd), "S"(path), "d"(flags), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_getdents64(int fd, void *dirp, unsigned int count) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(217), "D"((long)fd), "S"(dirp), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_readlink(const char *path, char *buf, long bufsiz) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(89), "D"(path), "S"(buf), "d"(bufsiz)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_close(int fd) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(3), "D"(fd)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_clock_gettime(int clk_id, struct timespec *tp) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(228), "D"((long)clk_id), "S"(tp)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(35), "D"(req), "S"(rem)
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
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)buf;
    register long x2 __asm__("x2") = n;
    register long x8 __asm__("x8") = 64;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_exit_group(int status) {
    register long x0 __asm__("x0") = status;
    register long x8 __asm__("x8") = 94;
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
    register long x0 __asm__("x0") = 17;   /* SIGCHLD */
    register long x1 __asm__("x1") = 0;    /* newsp = 0 */
    register long x8 __asm__("x8") = 220;  /* __NR_clone */
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_execve(const char *path, char *const argv[], char *const envp[]) {
    register long x0 __asm__("x0") = (long)path;
    register long x1 __asm__("x1") = (long)argv;
    register long x2 __asm__("x2") = (long)envp;
    register long x8 __asm__("x8") = 221;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_wait4(long pid, int *status, int options, void *rusage) {
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = (long)status;
    register long x2 __asm__("x2") = options;
    register long x3 __asm__("x3") = (long)rusage;
    register long x8 __asm__("x8") = 260;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_kill(long pid, int sig) {
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = sig;
    register long x8 __asm__("x8") = 129;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_prctl(int option, unsigned long arg2, unsigned long arg3,
                             unsigned long arg4, unsigned long arg5) {
    register long x0 __asm__("x0") = option;
    register long x1 __asm__("x1") = arg2;
    register long x2 __asm__("x2") = arg3;
    register long x3 __asm__("x3") = arg4;
    register long x4 __asm__("x4") = arg5;
    register long x8 __asm__("x8") = 167;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigaction(int sig, const struct k_sigaction *act,
                                    struct k_sigaction *oldact, unsigned long sigsetsize) {
    register long x0 __asm__("x0") = sig;
    register long x1 __asm__("x1") = (long)act;
    register long x2 __asm__("x2") = (long)oldact;
    register long x3 __asm__("x3") = sigsetsize;
    register long x8 __asm__("x8") = 134;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigprocmask(int how, const unsigned long *set,
                                      unsigned long *oldset, unsigned long sigsetsize) {
    register long x0 __asm__("x0") = how;
    register long x1 __asm__("x1") = (long)set;
    register long x2 __asm__("x2") = (long)oldset;
    register long x3 __asm__("x3") = sigsetsize;
    register long x8 __asm__("x8") = 135;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_rt_sigsuspend(const unsigned long *mask, unsigned long sigsetsize) {
    register long x0 __asm__("x0") = (long)mask;
    register long x1 __asm__("x1") = sigsetsize;
    register long x8 __asm__("x8") = 133;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_setpgid(long pid, long pgid) {
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = pgid;
    register long x8 __asm__("x8") = 154;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_openat(int dirfd, const char *path, int flags, int mode) {
    register long x0 __asm__("x0") = dirfd;
    register long x1 __asm__("x1") = (long)path;
    register long x2 __asm__("x2") = flags;
    register long x3 __asm__("x3") = mode;
    register long x8 __asm__("x8") = 56;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_getdents64(int fd, void *dirp, unsigned int count) {
    register long x0 __asm__("x0") = fd;
    register long x1 __asm__("x1") = (long)dirp;
    register long x2 __asm__("x2") = count;
    register long x8 __asm__("x8") = 61;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_readlink(const char *path, char *buf, long bufsiz) {
    /* AArch64 has no plain readlink syscall; readlinkat is the ABI. */
    register long x0 __asm__("x0") = -100; /* AT_FDCWD */
    register long x1 __asm__("x1") = (long)path;
    register long x2 __asm__("x2") = (long)buf;
    register long x3 __asm__("x3") = bufsiz;
    register long x8 __asm__("x8") = 78;   /* __NR_readlinkat */
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_close(int fd) {
    register long x0 __asm__("x0") = fd;
    register long x8 __asm__("x8") = 57;
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_clock_gettime(int clk_id, struct timespec *tp) {
    register long x0 __asm__("x0") = (long)clk_id;
    register long x1 __asm__("x1") = (long)tp;
    register long x8 __asm__("x8") = 113;  /* __NR_clock_gettime */
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    register long x0 __asm__("x0") = (long)req;
    register long x1 __asm__("x1") = (long)rem;
    register long x8 __asm__("x8") = 101;  /* __NR_nanosleep */
    __asm__ volatile (
        "svc #0"
        : "+r"(x0)
        : "r"(x1), "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

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

/* Apache hijacks SIGWINCH for graceful shutdown. When the outside world
   sends SIGTERM, we translate it to SIGWINCH for Apache processes and send
   the original signal to everyone else. */
static const char * const apache_exes[] = {
    "/usr/sbin/apache2",
    "/usr/sbin/httpd",
    "/usr/local/apache2/bin/httpd",
    0
};
#define APACHE_IN_SIG  15   /* SIGTERM */
#define APACHE_OUT_SIG 28   /* SIGWINCH */

#define DESCENDANT_TIMEOUT_SECONDS 60
#define BROADCAST_SCAN_DELAY_US     100000
#define DESCENDANT_POLL_INTERVAL_US 50000

#define AT_FDCWD       -100
#define O_RDONLY       0
#define O_DIRECTORY    00200000

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#ifdef TUXREAPERD_DEBUG
static void debug_write(const char *s) {
    sys_write(2, s, my_strlen(s));
}

static void debug_write_int(long n) {
    char buf[32];
    int i = 0;
    if (n < 0) {
        buf[i++] = '-';
        n = -n;
    }
    int start = i;
    do {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    for (int j = start, k = i - 1; j < k; j++, k--) {
        char tmp = buf[j];
        buf[j] = buf[k];
        buf[k] = tmp;
    }
    sys_write(2, buf, i);
}

static void debug_msg(const char *msg) {
    debug_write("[tuxreaperd] ");
    debug_write(msg);
    debug_write("\n");
}
#else
static void debug_write(const char *s) { (void)s; }
static void debug_write_int(long n) { (void)n; }
static void debug_msg(const char *msg) { (void)msg; }
#endif

static int my_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static long parse_pid(const char *s) {
    long pid = 0;
    while (*s) {
        if (!is_digit(*s)) return -1;
        pid = pid * 10 + (*s - '0');
        s++;
    }
    return pid;
}

/* Iterate /proc, read each process's /proc/<pid>/exe symlink, and send
   out_sig to matching PIDs while sending in_sig to everyone else.
   Skip PID 1 (the reaper itself). */
static void proc_remap_signal(int in_sig, int out_sig, const char *const *target_exes) {
    int procfd = (int)sys_openat(AT_FDCWD, "/proc", O_RDONLY | O_DIRECTORY, 0);
    if (procfd < 0) {
        debug_msg("opendir(/proc) failed");
        return;
    }

    char dirbuf[4096];

    for (;;) {
        long n = sys_getdents64(procfd, dirbuf, sizeof(dirbuf));
        if (n <= 0) break;

        for (long pos = 0; pos < n;) {
            struct linux_dirent64 *de = (struct linux_dirent64 *)(dirbuf + pos);
            pos += de->d_reclen;

            const char *name = de->d_name;
            if (name[0] == '.') continue;

            long pid = parse_pid(name);
            if (pid <= 1) continue;

            char path[32];
            char *p = path;
            const char *prefix = "/proc/";
            for (long i = 0; prefix[i]; i++) *p++ = prefix[i];
            const char *np = name;
            while (*np) *p++ = *np++;
            const char *suffix = "/exe";
            for (long i = 0; suffix[i]; i++) *p++ = suffix[i];
            *p = '\0';

            char linkbuf[256];
            long linklen = sys_readlink(path, linkbuf, sizeof(linkbuf) - 1);
            if (linklen <= 0) continue;
            linkbuf[linklen] = '\0';

            int matched = 0;
            for (int i = 0; target_exes[i]; i++) {
                long target_len = my_strlen(target_exes[i]);
                if (linklen == target_len && my_strcmp(linkbuf, target_exes[i]) == 0) {
                    matched = 1;
                    break;
                }
            }

            int sent_sig = matched ? out_sig : in_sig;
#ifdef TUXREAPERD_DEBUG
            debug_write("[tuxreaperd] scan pid=");
            debug_write_int(pid);
            debug_write(" exe=");
            debug_write(linkbuf);
            debug_write(" matched=");
            debug_write_int(matched);
            debug_write(" sig=");
            debug_write_int(sent_sig);
            debug_write("\n");
#endif
            sys_kill(pid, sent_sig);
        }
    }

    sys_close(procfd);
}

static void sleep_us(long us) {
    struct timespec req;
    req.tv_sec = us / 1000000;
    req.tv_nsec = (us % 1000000) * 1000;
    sys_nanosleep(&req, 0);
}

static void broadcast_signal(int in_sig, int out_sig, const char *const *target_exes) {
#ifdef TUXREAPERD_DEBUG
    debug_write("[tuxreaperd] broadcasting in_sig=");
    debug_write_int(in_sig);
    debug_write(" out_sig=");
    debug_write_int(out_sig);
    debug_write("\n");
#endif
    proc_remap_signal(in_sig, out_sig, target_exes);
    sleep_us(BROADCAST_SCAN_DELAY_US);
    proc_remap_signal(in_sig, out_sig, target_exes);
}

static int count_descendants(void) {
    int procfd = (int)sys_openat(AT_FDCWD, "/proc", O_RDONLY | O_DIRECTORY, 0);
    if (procfd < 0) return 0;

    int count = 0;
    char dirbuf[4096];

    for (;;) {
        long n = sys_getdents64(procfd, dirbuf, sizeof(dirbuf));
        if (n <= 0) break;

        for (long pos = 0; pos < n;) {
            struct linux_dirent64 *de = (struct linux_dirent64 *)(dirbuf + pos);
            pos += de->d_reclen;

            const char *name = de->d_name;
            if (name[0] == '.') continue;

            long pid = parse_pid(name);
            if (pid <= 1) continue;
            count++;
        }
    }

    sys_close(procfd);
    return count;
}

static long long monotonic_ms(void) {
    struct timespec ts;
    if (sys_clock_gettime(1 /* CLOCK_MONOTONIC */, &ts) < 0) return 0;
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static volatile long g_main_child = 0;
static volatile int g_child_exited = 0;
static volatile int g_main_status = 0;
static volatile int g_pending_signals = 0;

static void proxy_sig_handler(int sig) {
    if (sig >= 1 && sig <= 31) {
        g_pending_signals |= (1U << (sig - 1));
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

static void drain_zombies(void) {
    int status;
    while (sys_wait4(-1, &status, 1 /* WNOHANG */, 0) > 0);
}

static void handle_pending_signals(void) {
    int pending = g_pending_signals;
    g_pending_signals = 0;

    for (int sig = 1; sig <= 31; sig++) {
        if (!(pending & (1U << (sig - 1)))) continue;
#ifdef TUXREAPERD_DEBUG
        debug_write("[tuxreaperd] handling pending signal ");
        debug_write_int(sig);
        debug_write("\n");
#endif
        if (sig == APACHE_IN_SIG) {
            broadcast_signal(APACHE_IN_SIG, APACHE_OUT_SIG, apache_exes);
        } else {
            broadcast_signal(sig, sig, apache_exes);
        }
    }
}

#define WIFEXITED(s)   (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) ((((s) & 0x7f) != 0) && (((s) & 0x7f) != 0x7f))
#define WTERMSIG(s)    ((s) & 0x7f)

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
    sa.flags = 0x10000000 /* SA_RESTART */ | 1 /* SA_NOCLDSTOP */ | SA_RESTORER_FLAG;
    sa.restorer = restore_rt;
    sa.mask = 0;
    sys_rt_sigaction(17 /* SIGCHLD */, &sa, 0, sizeof(sa.mask));

    sa.handler = proxy_sig_handler;
    sa.flags = 0x10000000 /* SA_RESTART */ | SA_RESTORER_FLAG;
    sa.restorer = restore_rt;
    sa.mask = 0;
    sys_rt_sigaction(15 /* SIGTERM */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(3 /* SIGQUIT */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(2 /* SIGINT */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(1 /* SIGHUP */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(10 /* SIGUSR1 */, &sa, 0, sizeof(sa.mask));
    sys_rt_sigaction(12 /* SIGUSR2 */, &sa, 0, sizeof(sa.mask));

    unsigned long block_mask =
        (1UL << (17 - 1)) | /* SIGCHLD */
        (1UL << (15 - 1)) | /* SIGTERM */
        (1UL << (3 - 1))  | /* SIGQUIT */
        (1UL << (2 - 1))  | /* SIGINT */
        (1UL << (1 - 1))  | /* SIGHUP */
        (1UL << (10 - 1)) | /* SIGUSR1 */
        (1UL << (12 - 1));  /* SIGUSR2 */
    unsigned long old_mask;
    sys_rt_sigprocmask(0 /* SIG_BLOCK */, &block_mask, &old_mask, sizeof(block_mask));

    g_main_child = sys_fork();
    if (g_main_child == 0) {
        /* Become the leader of a new process group so signals can be
           broadcast to the whole workload tree. */
        sys_setpgid(0, 0);
        sys_rt_sigprocmask(2 /* SIG_SETMASK */, &old_mask, 0, sizeof(old_mask));
        my_execvp(argv[1], &argv[1], envp);
        static const char err[] = "execvp failed\n";
        sys_write(2, err, sizeof(err) - 1);
        sys_exit_group(127);
    }

#ifdef TUXREAPERD_DEBUG
    debug_write("[tuxreaperd] started main_child=");
    debug_write_int(g_main_child);
    debug_write("\n");
#endif

    long long child_exit_time_ms = 0;

    while (1) {
        if (g_child_exited) {
            long long now = monotonic_ms();
            if (child_exit_time_ms == 0) {
                child_exit_time_ms = now;
                debug_msg("main child exited, waiting up to 60s for descendants");
            }

            drain_zombies();
            int descendants = count_descendants();
#ifdef TUXREAPERD_DEBUG
            debug_write("[tuxreaperd] descendants=");
            debug_write_int(descendants);
            debug_write("\n");
#endif
            if (descendants == 0) {
                debug_msg("no descendants remaining, exiting cleanly");
                break;
            }
            if (now - child_exit_time_ms >= DESCENDANT_TIMEOUT_SECONDS * 1000LL) {
                debug_msg("descendant timeout reached, exiting");
                break;
            }

            /* Poll briefly so the 60-second deadline can tick even if no
               SIGCHLD or external signal arrives. handle_pending_signals()
               catches any signal that was delivered while we slept. */
            sleep_us(DESCENDANT_POLL_INTERVAL_US);
            handle_pending_signals();
        } else {
            /* Workload active: block atomically with zero CPU wakeups until a
               signal arrives. */
            sys_rt_sigsuspend(&old_mask, sizeof(old_mask));
            drain_zombies();
            handle_pending_signals();
        }
    }

    drain_zombies();

    if (WIFEXITED(g_main_status)) {
        return WEXITSTATUS(g_main_status);
    } else if (WIFSIGNALED(g_main_status)) {
        return 128 + WTERMSIG(g_main_status);
    }

    return 0;
}
