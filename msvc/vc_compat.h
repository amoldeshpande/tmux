#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#include <afunix.h>
#include <mswsock.h>
#include <Shlwapi.h>
#include <io.h>
#include <direct.h>
#include <time.h>
#include <process.h>
#include <stdio.h>
#include <io.h>
#pragma warning(disable:/*4022*/ 4028 4057 4100 4101 4127 /*4131 4189*/ 4201 4204 4244 4267 4310 4701 4703 4706)//         4013) 
						//4022 pointer mismatch 
						//4028 formal parameter 1 different from declaration
						//4057 '=': 'const u_char *' differs in indirection to slightly different base types from 'char [3]'
						//4100 unreferenced formal parameter
						//4101 unreferenced local variable
						//4127 conditional expression is constant
						//4131 'b64_ntop': uses old-style declarator
						//4189 local variable is initialized but not referenced
						//4201 nonstandard extension used: nameless struct/union
						//4204 nonstandard extension used: non-constant aggregate initializer
						//4244 int to u_char, possible loss of data
						//4267 conversion from 'size_t' to 'ULONG', possible loss of data
						//4310 cast truncates constant value
						//4701 potentially uninitialized local variable  used
						//4706 assignment within conditional expression
						//4013 <xyz> undefined;assuming extern returning int
#include <stdint.h>

#define VERSION "1.00"

typedef unsigned char u_char;
typedef DWORD pid_t;
typedef DWORD uid_t;
typedef void* caddr_t;
typedef uintptr_t fd_t;

#include <termios.h>


#define INVALID_FD (fd_t)-1

#define iovec _WSABUF
#define iov_base buf
#define iov_len len
#define msghdr _WSAMSG
#define msg_iov lpBuffers
#define msg_iovlen dwBufferCount
#define msg_name name
#define msg_namelen namelen
#define msg_control Control.buf
#define msg_controllen  Control.len
#define IOV_MAX 1024


#define STDIN_FILENO _get_osfhandle(_fileno(stdin))
#define STDOUT_FILENO _get_osfhandle(_fileno(stdout))
#define STDERR_FILENO _get_osfhandle(_fileno(stderr))
#define SHUT_WR SD_SEND
#define getuid() 0
#define getpwuid(a) NULL
#define getpwnam(a) NULL
#define ttyname(a) NULL
#define TIOCSWINSZ -1
#define TIOCGWINSZ -1
#define X_OK 06
#define PATH_MAX MAX_PATH
#define SIGTSTP 23
#define SIGHUP  24
#define SIGCHLD 25

#define ENOBUFS WSAENOBUFS
#define ECONNREFUSED WSAECONNREFUSED
#define ECONNABORTED WSAECONNABORTED
#define sockerrno WSAGetLastError()
#define unlink_socket(a) (!DeleteFile(a))
#undef CMSG_DATA // @#$@wincrypt.h
#define CMSG_DATA WSA_CMSG_DATA
#define SCM_RIGHTS 0x01

#define fnmatch(pat,str,ignore) !PathMatchSpecExA(str,pat,0)
#define mkdir(a,b) _mkdir(a)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define gmtime_r(a,b) gmtime_s(b,a)
#define getpid() GetCurrentProcessId()
#define setenv(a,b,x) SetEnvironmentVariable(a,b)
#define HAVE_SETENV
#define getppid() 0
#define umask(a) 0
#define close win_close
#define dup   win_dup
#define isatty win_isatty
#define SERVER_SINGLE_INTSTANCE_MUTEX_NAME L"TMUX_SERVER_SINGLE_INSTANCE_MUTEX"
#define getdtablesize() INT_MAX


typedef unsigned char	cc_t;

struct passwd { char* pw_dir; char* pw_shell; };
typedef int mode_t;

struct utsname {
	char sysname[MAX_PATH];    /* Operating system name (e.g., "Linux") */
	char nodename[MAX_PATH];   /* Name within "some implementation-defined
						  network" */
	char release[MAX_PATH];    /* Operating system release (e.g., "2.6.28") */
	char version[MAX_PATH];    /* Operating system version */
	char machine[MAX_PATH];    /* Hardware identifier */
#ifdef _GNU_SOURCE
	char domainname[]; /* NIS or YP domain name */
#endif
};

struct winsize { int ws_row; int ws_col; };
typedef struct _sigset_t { int ignore; } sigset_t;
#define sigprocmask(a,b,c)
#define sigfillset(a)

#define lstat(a,b) stat(a,b)
#define S_ISDIR(a) ((a & S_IFDIR) != 0)


#define WIFEXITED(a) 0
#define WEXITSTATUS(a) 0
#define WIFSIGNALED(a) 0
#define WTERMSIG(a) 0
#define WNOHANG(a) 0

#undef environ


#define wcwidth mk_wcwidth
struct timezone
{
	time_t tz_minuteswest;         /* minutes W of Greenwich */
	time_t tz_dsttime;             /* type of dst correction */
};

#ifdef __cplusplus
extern "C"
{
#endif
	extern void win_environ_init(void);
	struct tmuxproc;
	struct event_base;
	extern void run_windows_server(struct tmuxproc* client_proc, struct event_base* base);

	extern const char* const get_tmp_dir();
	extern char* basename(const char*);
	extern char* dirname(char*);
	extern char* realpath(const char*, char*);
	extern char* getcwd(char*, int);
	extern void win_close(fd_t fd);
	extern fd_t win_dup(fd_t);
	extern int isatty(fd_t);
	extern int mk_wcwidth(wchar_t ucs);
	extern int gettimeofday(struct timeval* tv, struct timezone* tz);
	extern int getpagesize(void);
	extern int mk_wcwidth(wchar_t ucs);
	extern void usleep(int64_t usec);
	extern char* ctime_r(const time_t* timep, char* buf);
	extern int uname(struct utsname* name);
	extern ssize_t writev(fd_t fildes, const struct iovec* iov, int iovcnt);
	extern void kill(int pid, int signal);
	extern void dprintf(const char* fmt, ...);
	extern int socketpair(int domain, int type, int protocol, fd_t sv[2]);
	extern char* win_convert_env(void* env);
	extern void win_free_env(char* ptr);
	extern char* tparm(char* str, ...);
	extern int ioctl(fd_t, int, void*);

	ssize_t recvmsg(fd_t sockfd, struct msghdr* msg, int flags);
	ssize_t sendmsg(fd_t sockfd, struct msghdr* msg, int flags);
#ifdef __cplusplus
}
#endif
