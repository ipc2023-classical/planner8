/***
 * Copyright (c)2022 Daniel Fiser <danfis@danfis.cz>
 *
 *  This file is part of cpddl.
 *
 *  Distributed under the OSI-approved BSD License (the "License");
 *  see accompanying file BDS-LICENSE for details or see
 *  <http://www.opensource.org/licenses/bsd-license.php>.
 *
 *  This software is distributed WITHOUT ANY WARRANTY; without even the
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the License for more information.
 */

#include "internal.h"
#include "pddl/subprocess.h"

static void waitForSubprocess(pid_t pid, pddl_exec_status_t *status)
{
    int wstatus;
    waitpid(pid, &wstatus, 0);
    if (WIFEXITED(wstatus)){
        if (status != NULL){
            status->exited = 1;
            status->exit_status = WEXITSTATUS(wstatus);
        }

    }else if (WIFSIGNALED(wstatus)){
        if (status != NULL){
            status->signaled = 1;
            status->signum = WTERMSIG(wstatus);
        }
    }
}

#define CMD_BUFSIZ 1024
#define READ_INIT_BUFSIZ 256
static void logCommand(char *const argv[], pddl_err_t *err)
{
    char cmd[CMD_BUFSIZ];
    int written = 0;
    for (int i = 0; argv[i] != NULL && CMD_BUFSIZ - written > 0; ++i)
        written += snprintf(cmd + written, CMD_BUFSIZ - written, " '%s'", argv[i]);
    LOG(err, "Command:%s", cmd);
}

struct buf {
    char *buf;
    int size;
    int alloc;

    char **out;
    int *outsize;
};

static void bufInit(struct buf *buf, char **out, int *outsize)
{
    if (out != NULL){
        *out = NULL;
        *outsize = 0;
    }
    ZEROIZE(buf);
    buf->out = out;
    buf->outsize = outsize;
}

static void bufAlloc(struct buf *buf)
{
    ASSERT(buf->out != NULL);
    if (buf->alloc == buf->size){
        if (buf->alloc == 0)
            buf->alloc = READ_INIT_BUFSIZ;
        buf->alloc *= 2;
        buf->buf = REALLOC_ARR(buf->buf, char, buf->alloc);
    }
}

static int bufRead(struct buf *buf, int fd)
{
    ASSERT(buf->out != NULL);
    bufAlloc(buf);
    int remain = buf->alloc - buf->size;
    ssize_t r = read(fd, buf->buf + buf->size, remain);
    if (r > 0){
        buf->size += r;
        return 0;
    }
    return -1;
}

static void bufFinalize(struct buf *buf)
{
    if (buf->out == NULL)
        return;

    bufAlloc(buf);
    buf->buf[buf->size] = '\x0';
    *buf->out = buf->buf;
    *buf->outsize = buf->size;
}

int pddlExecvp(char *const argv[],
               pddl_exec_status_t *status,
               const char *write_stdin,
               int write_stdin_size,
               char **read_stdout,
               int *read_stdout_size,
               char **read_stderr,
               int *read_stderr_size,
               pddl_err_t *err)
{
    CTX(err, "execvp", "exec");
    logCommand(argv, err);
    fflush(stdout);
    fflush(stderr);
    pddlErrFlush(err);

    if (status != NULL)
        ZEROIZE(status);

    struct buf bufout, buferr;
    bufInit(&bufout, read_stdout, read_stdout_size);
    bufInit(&buferr, read_stderr, read_stderr_size);

    int fd_stdin[2] = { -1, -1 };
    int fd_stdout[2] = { -1, -1 };
    int fd_stderr[2] = { -1, -1 };

    int written = 0;
    if (write_stdin != NULL){
        if (pipe(fd_stdin) != 0){
            perror("pipe() failed");
            CTXEND(err);
            return -1.;
        }
    }
    if (read_stdout != NULL){
        if (pipe(fd_stdout) != 0){
            if (fd_stdin[0] >= 0)
                close(fd_stdin[0]);
            if (fd_stdin[1] >= 0)
                close(fd_stdin[1]);
            perror("pipe() failed");
            CTXEND(err);
            return -1.;
        }
    }

    if (read_stderr != NULL){
        if (pipe(fd_stderr) != 0){
            if (fd_stdin[0] >= 0)
                close(fd_stdin[0]);
            if (fd_stdin[1] >= 0)
                close(fd_stdin[1]);
            if (fd_stdout[0] >= 0)
                close(fd_stdout[0]);
            if (fd_stdout[1] >= 0)
                close(fd_stdout[1]);
            perror("pipe() failed");
            CTXEND(err);
            return -1.;
        }
    }

    pid_t pid = fork();
    if (pid < 0){
        PANIC("fork() failed: %s", strerror(errno));

    }else if (pid == 0){
        if (fd_stdin[1] >= 0)
            close(fd_stdin[1]);
        if (fd_stdin[0] >= 0){
            ASSERT_RUNTIME(dup2(fd_stdin[0], STDIN_FILENO) == STDIN_FILENO);
            close(fd_stdin[0]);
        }

        if (fd_stdout[0] >= 0)
            close(fd_stdout[0]);
        if (fd_stdout[1] >= 0){
            ASSERT_RUNTIME(dup2(fd_stdout[1], STDOUT_FILENO) == STDOUT_FILENO);
            close(fd_stdout[1]);
        }else{
            int fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (fd >= 0){
                ASSERT_RUNTIME(dup2(fd, STDOUT_FILENO) == STDOUT_FILENO);
                close(fd);
            }
        }

        if (fd_stderr[0] >= 0)
            close(fd_stderr[0]);
        if (fd_stderr[1] >= 0){
            ASSERT_RUNTIME(dup2(fd_stderr[1], STDERR_FILENO) == STDERR_FILENO);
            close(fd_stderr[1]);
        }else{
            int fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
            if (fd >= 0){
                ASSERT_RUNTIME(dup2(fd, STDERR_FILENO) == STDERR_FILENO);
                close(fd);
            }
        }

        execvp(argv[0], argv);
        PANIC("exec failed!");
    }

    struct pollfd pfd[3];
    int pfdsize = 0;

    if (fd_stdin[0] >= 0)
        close(fd_stdin[0]);
    if (fd_stdin[1] >= 0){
        pfd[pfdsize].fd = fd_stdin[1];
        pfd[pfdsize].events = POLLOUT | POLLWRBAND | POLLHUP;
        ++pfdsize;
    }

    if (fd_stdout[1] >= 0)
        close(fd_stdout[1]);
    if (fd_stdout[0] >= 0){
        pfd[pfdsize].fd = fd_stdout[0];
        pfd[pfdsize].events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLHUP;
        ++pfdsize;
    }

    if (fd_stderr[1] >= 0)
        close(fd_stderr[1]);
    if (fd_stderr[0] >= 0){
        pfd[pfdsize].fd = fd_stderr[0];
        pfd[pfdsize].events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLHUP;
        ++pfdsize;
    }

    int rpoll = 0;
    while (pfdsize > 0 && (rpoll = poll(pfd, pfdsize, -1)) > 0){
        pfdsize = 0;
        int fdi = 0;
        if (fd_stdin[1] >= 0){
            if ((pfd[fdi].revents & POLLOUT)
                    || (pfd[fdi].revents & POLLWRBAND)){
                int remaining = write_stdin_size - written;
                ssize_t w = write(fd_stdin[1], write_stdin, remaining);
                if (w > 0)
                    written += w;
                if (written == write_stdin_size){
                    close(fd_stdin[1]);
                    fd_stdin[1] = -1;
                }

            }else if (pfd[fdi].revents & POLLHUP){
                close(fd_stdin[1]);
                fd_stdin[1] = -1;
            }

            if (fd_stdin[1] >= 0){
                pfd[pfdsize].fd = fd_stdin[1];
                pfd[pfdsize].events = POLLOUT | POLLWRBAND | POLLHUP;
                ++pfdsize;
            }
            ++fdi;
        }

        if (fd_stdout[0] >= 0){
            if ((pfd[fdi].revents & POLLIN)
                    || (pfd[fdi].revents & POLLRDNORM)
                    || (pfd[fdi].revents & POLLRDBAND)
                    || (pfd[fdi].revents & POLLPRI)){
                if (bufRead(&bufout, fd_stdout[0]) != 0){
                    close(fd_stdout[0]);
                    fd_stdout[0] = -1;
                }

            }else if (pfd[fdi].revents & POLLHUP){
                close(fd_stdout[0]);
                fd_stdout[0] = -1;
            }

            if (fd_stdout[0] >= 0){
                pfd[pfdsize].fd = fd_stdout[0];
                pfd[pfdsize].events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLHUP;
                ++pfdsize;
            }
            ++fdi;
        }

        if (fd_stderr[0] >= 0){
            if ((pfd[fdi].revents & POLLIN)
                    || (pfd[fdi].revents & POLLRDNORM)
                    || (pfd[fdi].revents & POLLRDBAND)
                    || (pfd[fdi].revents & POLLPRI)){
                if (bufRead(&buferr, fd_stderr[0]) != 0){
                    close(fd_stderr[0]);
                    fd_stderr[0] = -1;
                }

            }else if (pfd[fdi].revents & POLLHUP){
                close(fd_stderr[0]);
                fd_stderr[0] = -1;
            }

            if (fd_stderr[0] >= 0){
                pfd[pfdsize].fd = fd_stderr[0];
                pfd[pfdsize].events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLHUP;
                ++pfdsize;
            }
        }
    }

    waitForSubprocess(pid, status);

    if (fd_stdin[1] >= 0)
        close(fd_stdin[1]);
    if (fd_stdout[0] >= 0)
        close(fd_stdout[0]);
    if (fd_stderr[0] >= 0)
        close(fd_stderr[0]);

    bufFinalize(&bufout);
    bufFinalize(&buferr);

    if (write_stdin != NULL)
        LOG(err, "Written %d / %d", written, write_stdin_size);

    if (read_stdout != NULL){
        LOG(err, "Read %d bytes from stdout, allocated %d bytes",
            bufout.size, bufout.alloc);
    }

    if (read_stderr != NULL){
        LOG(err, "Read %d bytes from stderr, allocated %d bytes",
            buferr.size, buferr.alloc);
    }

    if (status != NULL){
        LOG(err, "status: exited: %d, exit_status: %d,"
            " signaled: %d, signum: %d (%s)",
            status->exited, status->exit_status,
            status->signaled, status->signum,
            (status->signaled ? strsignal(status->signum) : "" ));
    }

    CTXEND(err);
    return 0;
}

int pddlForkSharedMem(int (*fn)(void *sharedmem, void *userdata),
                      void *in_out_data,
                      size_t data_size,
                      void *userdata,
                      pddl_exec_status_t *status,
                      pddl_err_t *err)
{
    CTX(err, "fork", "fork");
    fflush(stdout);
    fflush(stderr);
    pddlErrFlush(err);

    if (status != NULL)
        ZEROIZE(status);

    void *shared = mmap(NULL, data_size, PROT_WRITE | PROT_READ,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED){
        LOG(err, "Could not allocate shared memory of size %lu using mmap: %s",
            (unsigned long)data_size, strerror(errno));
        CTXEND(err);
        return -1;
    }

    memcpy(shared, in_out_data, data_size);
    LOG(err, "In data copied to the shared memory.");
    pid_t pid = fork();
    if (pid < 0){
        PANIC("fork() failed: %s", strerror(errno));

    }else if (pid == 0){
        int ret = fn(shared, userdata);
        exit(ret);
    }

    waitForSubprocess(pid, status);

    memcpy(in_out_data, shared, data_size);
    LOG(err, "Out data copied to the output memory.");
    if (munmap(shared, data_size) != 0){
        LOG(err, "Could not release mmaped memory: %s", strerror(errno));
        CTXEND(err);
        return -1;
    }

    CTXEND(err);
    return 0;
}

int pddlForkPipe(int (*fn)(int fdout, void *userdata),
                 void *userdata,
                 void **out,
                 int *out_size,
                 pddl_exec_status_t *status,
                 pddl_err_t *err)
{
    CTX(err, "fork", "fork");
    fflush(stdout);
    fflush(stderr);
    pddlErrFlush(err);

    if (status != NULL)
        ZEROIZE(status);

    int fd[2];
    if (pipe(fd) != 0){
        PANIC("pipe() failed: %s", strerror(errno));
    }

    pid_t pid = fork();
    if (pid < 0){
        PANIC("fork() failed: %s", strerror(errno));

    }else if (pid == 0){
        close(fd[0]);
        int ret = fn(fd[1], userdata);
        close(fd[1]);
        exit(ret);
    }

    close(fd[1]);
    struct buf buf;
    bufInit(&buf, (char **)out, out_size);
    while (bufRead(&buf, fd[0]) == 0)
        ;
    close(fd[0]);
    bufFinalize(&buf);
    LOG(err, "Read %d bytes, allocated %d bytes", buf.size, buf.alloc);

    waitForSubprocess(pid, status);

    CTXEND(err);
    return 0;
}
