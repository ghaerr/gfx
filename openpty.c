#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
//#include <termios.h>
//#include <errno.h>
//#include <sys/select.h>

#define UNIX        1
#define UNIX98	    1		/* use new-style /dev/ptmx, /dev/pts/0*/
#define GrError     printf

#if UNIX
/* 
 * pty create/open routines
 */
static char *nargv[2] = {"/bin/sh", NULL};

//static void sigchild(int signo)
//{
	////close_keyboard();
    //printf("SIGCHLD\n");
	//exit(0);
//}

#if UNIX98 && !ELKS
int open_pty(void)
{
	int tfd;
	pid_t pid;
	char ptyname[50];
	
	tfd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (tfd < 0) goto err;
      
	signal(SIGCHLD, SIG_DFL);	/* required before grantpt()*/
	if (grantpt(tfd) || unlockpt(tfd)) goto err; 
	//signal(SIGCHLD, sigchild);
	//signal(SIGCHLD, SIG_IGN);
	//signal(SIGINT, sigchild);

	sprintf(ptyname,"%s", ptsname(tfd));

	if ((pid = fork()) == -1) {
		GrError("No processes\n");
		return -1;
	}
	if (!pid) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(tfd);

		setsid();
		if ((tfd = open(ptyname, O_RDWR)) < 0) {
			GrError("Child: Can't open pty %s\n", ptyname);
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
err:
	GrError("Can't create pty /dev/ptmx\n");
	return -1;	
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
		GrError("Can't create pty %s\n", pty_name);
		return -1;
	}

	signal(SIGCHLD, sigchild);
	signal(SIGINT, sigchild);
	if ((pid = fork()) == -1) {
		GrError("No processes\n");
		return -1;
	}
	if (!pid) {
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(tfd);

		setsid();
		pty_name[5] = 't';
		if ((tfd = open(pty_name, O_RDWR)) < 0) {
			GrError("Child: Can't open pty %s\n", pty_name);
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
#define NSIG 	_NSIG
static char pty[LINEBUF];
static struct winsize winsz;

int open_pty(void)
{
	char *ptr;

    winsz.ws_col = col;
    winsz.ws_row = row;
    if ((pid = forkpty(&pipeh, pty, NULL, &winsz)) < 0)  {
		GrError("Can't create pty\n");
		sleep(2);
		GrKillWindow(w1);
		exit(-1);
    }

    if ((ptr = rindex(pty, '/'))) 
		strcpy(pty, ptr + 1);
  
    if (!pid) {
		int i;
		for (i = getdtablesize(); --i >= 3; )
	    	close (i);
		/*
		 * SIG_IGN are not reset on exec()
	 	 */
		for (i = NSIG; --i >= 0; )
	    	signal (i, SIG_DFL);
 
		/* caution: start shell with correct user id! */
		seteuid(getuid());
		setegid(getgid());

		/* this shall not return */
		execvp(shell, argv);

		/* oops? */
		GrError("Can't start shell\r\n");
		sleep(3);
		GrKillWindow(w1);
		_exit(-1);
    }
}
#endif /* __FreeBSD__*/
#endif /* UNIX*/
