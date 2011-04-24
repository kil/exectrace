/*-
 * Copyright (c) 2010 Kilian Klimek
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/timespec.h>
#include <sys/kernel.h>
#include <sys/mutex.h>

#define MODNAME	"exectrace"
#define BUFSIZ	512
#define OBUFSIZ	2048

#ifdef __NO_LOCK
#define LPRINTF(Y...)		printf(Y)
#else
#define LPRINTF(Y...)		do { \
					mtx_lock(&Giant); \
					printf(Y); \
					mtx_unlock(&Giant); \
				} while(0)
#endif

#define REPLACE_SYSCALL(X)	do { \
					o##X = sysent[SYS_##X].sy_call; \
					sysent[SYS_##X].sy_call = n##X; \
				} while(0)

#define RESTORE_SYSCALL(X)	do { \
					sysent[SYS_##X].sy_call = o##X; \
				} while(0)


static sy_call_t	*oexecve = NULL,
			*oexit = NULL,
			*ofork = NULL,
			*ovfork = NULL;


static pid_t
get_pid(struct thread *td)
{
	pid_t			pid;
	struct proc		*p;


	p = td->td_proc;
	PROC_LOCK(p);
	pid = p->p_pid;
	PROC_UNLOCK(p);

	return pid;
}


static pid_t
get_ppid(struct thread *td)
{
	pid_t			ppid;
	struct proc		*p;


	p = td->td_proc;
	PROC_LOCK(p);
	PROC_LOCK(p->p_pptr);
	ppid = p->p_pptr->p_pid;
	PROC_UNLOCK(p->p_pptr);
	PROC_UNLOCK(p);

	return ppid;
}


static int
nexit(struct thread *td, void *arg)
{
	pid_t			pid,
				ppid;
	struct timespec		t;


	pid = get_pid(td);
	ppid = get_ppid(td);
	nanouptime(&t);
	LPRINTF(MODNAME " exit %d.%ld %d-%d\n",
			(int) t.tv_sec, t.tv_nsec, pid, ppid);

	return oexit(td, arg);
}


static int
nexecve(struct thread *td, void *arg)
{
	int			x,
				ret;
	char			buf[BUFSIZ],
				outbuf[OBUFSIZ];
	size_t			buf_used;
	struct timespec		t;
	struct execve_args	*uap;
	pid_t			pid,
				ppid;


	pid = get_pid(td);
	ppid = get_ppid(td);

	uap = (struct execve_args *) arg;
	copyinstr(uap->fname, buf, BUFSIZ, &buf_used);

	nanouptime(&t);
	snprintf(outbuf, OBUFSIZ, MODNAME " execve %d.%ld %d-%d %s", (int) t.tv_sec,
			t.tv_nsec, pid, ppid, buf);

	if(uap->argv && uap->argv[0] != NULL) {
		for(x = 1; uap->argv[x] != NULL; x++) {
			copyinstr(uap->argv[x], buf, BUFSIZ, &buf_used);
			strlcat(outbuf, " ", OBUFSIZ);
			strlcat(outbuf, buf, OBUFSIZ);
		}
	}

	ret = oexecve(td, arg);

	if(ret == 0) {
		LPRINTF("%s\n", outbuf);
	}

	return ret;
}


static int
nfork(struct thread *td, void *arg)
{
	int			ret;
	struct timespec		t;
	pid_t			pid,
				pid2;


	nanouptime(&t);
	pid = get_pid(td);
	ret = ofork(td, arg);
	pid2 = td->td_retval[0];

	LPRINTF(MODNAME " fork %d.%ld %d-%d\n",
			(int) t.tv_sec, t.tv_nsec, pid2, pid);
	return ret;
}


static int
nvfork(struct thread *td, void *arg)
{
	int			ret;
	struct timespec		t;
	pid_t			pid,
				pid2;


	nanouptime(&t);
	pid = get_pid(td);
	ret = ovfork(td, arg);
	pid2 = td->td_retval[0];

	/* report vfork as fork */
	LPRINTF(MODNAME " fork %d.%ld %d-%d\n",
			(int) t.tv_sec, t.tv_nsec, pid2, pid);
	return ret;
}


static int
exectrace_handler(struct module *mod, int cmd, void *arg)
{
	switch(cmd) {
	case MOD_LOAD:
		printf(MODNAME " loaded\n");
		REPLACE_SYSCALL(execve);
		REPLACE_SYSCALL(exit);
		REPLACE_SYSCALL(fork);
		REPLACE_SYSCALL(vfork);
		break;
	case MOD_UNLOAD:
		printf(MODNAME " unloaded\n");
		RESTORE_SYSCALL(execve);
		RESTORE_SYSCALL(exit);
		RESTORE_SYSCALL(fork);
		RESTORE_SYSCALL(vfork);
		break;
	}

	return 0;
}


static moduledata_t exectrace_mod = {
	"exectrace", 
	exectrace_handler,
	NULL
};


DECLARE_MODULE(exectrace, exectrace_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
