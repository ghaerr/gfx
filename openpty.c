#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
/* 
 * pty create/open routines
 */

#define UNIX98	    1		/* use new-style /dev/ptmx, /dev/pts/0 */
#define error       printf

static char *nargv[2] = {"/bin/sh", NULL};

#if UNIX98 && !ELKS
int open_pty(void)
{
	int tfd;
	pid_t pid;
	char ptyname[50];
	
	signal(SIGCHLD, SIG_DFL);	/* required before grantpt() */
	tfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tfd < 0 || grantpt(tfd) || unlockpt(tfd)) {
	    error("Can't create pty /dev/ptmx\n");
	    return -1;
    }
	strcpy(ptyname, ptsname(tfd));

	if ((pid = fork()) == -1) {
		error("No processes\n");
		return -1;
	}
	if (!pid) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(tfd);

		setsid();
		if ((tfd = open(ptyname, O_RDWR)) < 0) {
			error("Child: Can't open pty %s\n", ptyname);
			exit(1);
		}
		
		close(STDERR_FILENO);
		dup2(tfd, STDIN_FILENO);
		dup2(tfd, STDOUT_FILENO);
		dup2(tfd, STDERR_FILENO);
		execv(nargv[0], nargv);
		exit(1);
	}
	return tfd;
}

#elif !defined(__FreeBSD)	/* !UNIX98 || ELKS*/

int open_pty(void)
{
	int tfd;
	pid_t pid;
	int n = 0;
	char pty_name[12];

again:
	sprintf(pty_name, "/dev/ptyp%d", n);
	if ((tfd = open(pty_name, O_RDWR | O_NONBLOCK)) < 0) {
		if (errno == EBUSY && n < 3) {
			n++;
			goto again;
		}
		error("Can't create pty %s\n", pty_name);
		return -1;
	}

	if ((pid = fork()) == (pid_t)-1) {
		error("No processes\n");
		return -1;
	}
	if (!pid) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(tfd);

		setsid();
		pty_name[5] = 't';
		if ((tfd = open(pty_name, O_RDWR)) < 0) {
			error("Can't open pty %s\n", pty_name);
			exit(1);
		}
		close(STDERR_FILENO);
		dup2(tfd, STDIN_FILENO);
		dup2(tfd, STDOUT_FILENO);
		dup2(tfd, STDERR_FILENO);
		execv(nargv[0], nargv);
		exit(1);
	}
	return tfd;
}

#elif defined(__FreeBSD)

#include <libutil.h>

int open_pty(void)
{
	char *ptr;
    int i, pipeh;
    static char pty[LINEBUF];
    static struct winsize winsz;

    winsz.ws_col = 80;
    winsz.ws_row = 24;
    if ((pid = forkpty(&pipeh, pty, NULL, &winsz)) < 0)  {
		error("Can't create pty\n");
		exit(1);
    }
    if (!pid) {
		for (i = getdtablesize(); --i >= 3; )
	    	close (i);

		/* SIG_IGN are not reset on exec() */
		for (i = _NSIG; --i >= 0; )
	    	signal (i, SIG_DFL);
 
		seteuid(getuid());
		setegid(getgid());
		execvp(shell, argv);

		error("Can't start shell\r\n");
		_exit(1);
    }
}
#endif /* __FreeBSD__*/
