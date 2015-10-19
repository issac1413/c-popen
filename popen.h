#ifndef _POPEN_SCM_H_
#define _POPEN_SCM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <sys/wait.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

extern FILE *popen_scm(const char *program, const char *type);
extern int pclose_scm(FILE *iop);

#ifdef __cplusplus
}
#endif

#endif




