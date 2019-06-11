#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#include <afunix.h>
#include <io.h>
#pragma warning(disable:4057 4100 4101 4127 4131 4189 4201 4204 4244 4267 4701 4703 4706         4013) 
						//4057:  '=': 'const u_char *' differs in indirection to slightly different base types from 'char [3]'
						//4100 unreferenced formal parameter
						//4101 unreferenced local variable
						//4127 conditional expression is constant
						//4131 'b64_ntop': uses old-style declarator
						//4189 local variable is initialized but not referenced
						//4201 nonstandard extension used: nameless struct/union
						//4204 nonstandard extension used: non-constant aggregate initializer
						//4244 int to u_char, possible loss of data
						//4267 conversion from 'size_t' to 'ULONG', possible loss of data
						//4701 potentially uninitialized local variable  used
						//4706 assignment within conditional expression
						//4013 <xyz> undefined;assuming extern returning int
#include <stdint.h>

#define VERSION "1.00"

typedef unsigned char u_char;
typedef DWORD pid_t;
typedef DWORD uid_t;

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


#define STDIN_FILENO _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)
#define SHUT_WR SD_SEND
#define getuid() 0
#define getpwuid(a) NULL
#define getpwnam(a) NULL
#define kill(a,b)
#define TIOCSWINSZ -1
#define TIOCGWINSZ -1
#define X_OK 06
#define PATH_MAX MAX_PATH

#define xstrdup(a)  _strdup(a)
#define mkdir(a,b) _mkdir(a)
#define strncasecmp _strnicmp
#define strcasecmp _stricmp

typedef unsigned char	cc_t;
typedef unsigned int	speed_t;
typedef unsigned int	tcflag_t;

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

//https://github.com/lattera/glibc/blob/master/sysdeps/unix/sysv/linux/bits/termios.h
#define NCCS 32
struct termios
{
	tcflag_t c_iflag;		/* input mode flags */
	tcflag_t c_oflag;		/* output mode flags */
	tcflag_t c_cflag;		/* control mode flags */
	tcflag_t c_lflag;		/* local mode flags */
	cc_t c_line;			/* line discipline */
	cc_t c_cc[NCCS];		/* control characters */
	speed_t c_ispeed;		/* input speed */
	speed_t c_ospeed;		/* output speed */
#define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
#define _HAVE_STRUCT_TERMIOS_C_OSPEED 1
};
#define VERASE 5
#define _POSIX_VDISABLE 666
#define OK ERROR_SUCCESS
#define TCSANOW -1
extern char* tigetstr(char*);
extern char* tparm(char* str, ...); 

struct winsize { int ws_row; int ws_col; };
typedef struct _sigset_t { int ignore; } sigset_t;
#define sigprocmask(a,b,c)

/*
struct event { uint8_t ignore; };
struct event_base { uint8_t ignore; };
struct evbuffer { uint8_t ignore; };
struct bufferevent { struct evbuffer* input; struct evbuffer* output; };
#define EV_TIMEOUT -1
#define EV_READ 0
#define EV_PERSIST 0
#define EV_WRITE -1
#define EVBUFFER_EOL_LF 0
#define EVBUFFER_DATA(a) (char*)NULL
#define EVBUFFER_LENGTH(a) 0
#define EVLOOP_ONCE -1




extern const char* ttyname(int);
extern char* evbuffer_readline(struct evbuffer*);
extern char* evbuffer_readln(struct evbuffer*,void*,int);
extern struct evbuffer* evbuffer_new();
extern struct bufferevent* bufferevent_new(int,...);
*/

#undef environ
struct environ { uint8_t ignore; };

extern char* basename(char*);
extern char* dirname(char*);
extern char* realpath(const char*, char*);
extern char* getcwd(char*, int);
