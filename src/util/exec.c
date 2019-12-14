#include "../../build/feature.h" // Must be first include
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "exec.h"

static UNUSED bool close_on_exec(int fd)
{
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        return false;
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
        return false;
    }
    return true;
}

bool pipe_close_on_exec(int fd[2])
{
#ifdef HAVE_PIPE2
    return (pipe2(fd, O_CLOEXEC) == 0);
#else
    if (pipe(fd) != 0) {
        return false;
    }
    close_on_exec(fd[0]);
    close_on_exec(fd[1]);
    return true;
#endif
}

static int dup_close_on_exec(int oldfd, int newfd)
{
#ifdef HAVE_DUP3
    return dup3(oldfd, newfd, O_CLOEXEC);
#else
    if (oldfd == newfd) {
        // Replicate dup3() behaviour:
        errno = EINVAL;
        return -1;
    }
    if (dup2(oldfd, newfd) < 0) {
        return -1;
    }
    close_on_exec(newfd);
    return newfd;
#endif
}

static void reset_signal_handler(int signum)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    sigemptyset(&act.sa_mask);
    act.sa_handler = SIG_DFL;
    sigaction(signum, &act, NULL);
}

static void handle_child(char **argv, int fd[3], int error_fd)
{
    int error;
    int nr_fds = 3;
    bool move = error_fd < nr_fds;
    int max = error_fd;

    // Find if we must move fds out of the way
    for (int i = 0; i < nr_fds; i++) {
        if (fd[i] > max) {
            max = fd[i];
        }
        if (fd[i] < i) {
            move = true;
        }
    }

    if (move) {
        int next_free = max + 1;

        if (error_fd < nr_fds) {
            error_fd = dup_close_on_exec(error_fd, next_free++);
            if (error_fd < 0) {
                goto out;
            }
        }
        for (int i = 0; i < nr_fds; i++) {
            if (fd[i] < i) {
                fd[i] = dup_close_on_exec(fd[i], next_free++);
                if (fd[i] < 0) {
                    goto out;
                }
            }
        }
    }

    // Now it is safe to duplicate fds in this order
    for (int i = 0; i < nr_fds; i++) {
        if (i == fd[i]) {
            // Clear FD_CLOEXEC flag
            fcntl(fd[i], F_SETFD, 0);
        } else {
            if (dup2(fd[i], i) < 0) {
                goto out;
            }
        }
    }

    // Unignore signals (see man page exec(3p) for more information)
    reset_signal_handler(SIGINT);
    reset_signal_handler(SIGQUIT);

    execvp(argv[0], argv);
out:
    error = errno;
    error = write(error_fd, &error, sizeof(error));
    exit(42);
}

pid_t fork_exec(char **argv, int fd[3])
{
    int error = 0;
    int ep[2];

    if (!pipe_close_on_exec(ep)) {
        return -1;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        error = errno;
        close(ep[0]);
        close(ep[1]);
        errno = error;
        return pid;
    }
    if (!pid) {
        handle_child(argv, fd, ep[1]);
    }

    close(ep[1]);
    const ssize_t rc = read(ep[0], &error, sizeof(error));
    if (rc > 0 && rc != sizeof(error)) {
        error = EPIPE;
    }
    if (rc < 0) {
        error = errno;
    }
    close(ep[0]);

    if (!rc) {
        // Child exec was successful
        return pid;
    }

    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        ;
    }
    errno = error;
    return -1;
}

int wait_child(pid_t pid)
{
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        return -errno;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status) & 0xFF;
    }
    if (WIFSIGNALED(status)) {
        return WTERMSIG(status) << 8;
    }
    if (WIFSTOPPED(status)) {
        return WSTOPSIG(status) << 8;
    }
#if defined(WIFCONTINUED)
    if (WIFCONTINUED(status)) {
        return SIGCONT << 8;
    }
#endif
    return -EINVAL;
}
