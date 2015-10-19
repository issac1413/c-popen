/*	$OpenBSD: popen.c,v 1.17 2005/08/08 08:05:34 espie Exp $ */
/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "popen_scm.h"

static struct pid {
	struct pid *next;
	FILE *fp;
	pid_t pid;
} *pidlist;

extern char **environ;

static int get_max_file_descriptor()
{
    long long sys_max_fd = sysconf(_SC_OPEN_MAX);

    struct rlimit rl;
    long long lim_max_fd = -1;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        lim_max_fd = 0;
    } else {
        lim_max_fd = (long long)rl.rlim_max;
    }

    if (sys_max_fd > lim_max_fd) {
        return sys_max_fd;
    } else {
        return lim_max_fd;
    }
}

FILE *
popen_scm(const char *program, const char *type)
{
	struct pid * volatile cur;
	FILE *iop;
	int pdes[2];
	pid_t pid;
	char *argp[] = {"sh", "-c", NULL, NULL};

	if ((*type != 'r' && *type != 'w') || type[1] != '\0') {
		errno = EINVAL;
		return (NULL);
	}
	if ((cur = malloc(sizeof(struct pid))) == NULL)
		return (NULL);

	if (pipe(pdes) < 0) {
		free(cur);
		return (NULL);
	}

	switch (pid = fork()) {
	case -1:			/* Error. */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		free(cur);
		return (NULL);
		/* NOTREACHED */
	case 0:				/* Child. */
	    {
        int maxfd, i;
		struct pid *pcur;
		/*
		 * We fork()'d, we got our own copy of the list, no
		 * contention.
		 */
        maxfd = get_max_file_descriptor();
        for (i = 0; i < maxfd; ++i) {
            if (i == STDOUT_FILENO || i == STDIN_FILENO || i == STDERR_FILENO || i == pdes[0] || i == pdes[1])
                continue;
            close(i);
        }  // for

		if (*type == 'r') {
			(void) close(pdes[0]);
			if (pdes[1] != STDOUT_FILENO) {
				(void)dup2(pdes[1], STDOUT_FILENO);
				(void)close(pdes[1]);
			}
		} else {
			(void)close(pdes[1]);
			if (pdes[0] != STDIN_FILENO) {
				(void)dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
		}
		argp[2] = (char *)program;
		execve("/bin/sh", argp, environ);
		_exit(127);
		/* NOTREACHED */
	    }
	}

	/* Parent; assume fdopen can't fail. */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)close(pdes[0]);
	}

	/* Link into list of file descriptors. */
	cur->fp = iop;
	cur->pid =  pid;
	cur->next = pidlist;
	pidlist = cur;

	return (iop);
}

/*
 * pclose --
 *	Pclose returns -1 if stream is not associated with a `popened' command,
 *	if already `pclosed', or waitpid returns an error.
 */
int
pclose_scm(FILE *iop)
{
	struct pid *cur, *last;
	int pstat;
	pid_t pid;

	/* Find the appropriate file pointer. */
	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
		if (cur->fp == iop)
			break;

	if (cur == NULL)
		return (-1);

	(void)fclose(iop);

	do {
		pid = waitpid(cur->pid, &pstat, 0);
	} while (pid == -1 && errno == EINTR);

	/* Remove the entry from the linked list. */
	if (last == NULL)
		pidlist = cur->next;
	else
		last->next = cur->next;
	free(cur);

	return (pid == -1 ? -1 : pstat);
}

