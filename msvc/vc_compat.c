#include <vc_compat.h>
#include <stdlib.h>
#include <shlobj.h>
#include <event.h>

#pragma warning(default :4022 4028 4057 4100 4101  4201 4204 4244 4267 4310 4701 4703 4706)//         4013) 


char** environ;

void myInvalidParameterHandler(const wchar_t* expression,
	const wchar_t* function,
	const wchar_t* file,
	unsigned int line,
	uintptr_t pReserved)
{
	UNREFERENCED_PARAMETER(expression);
	UNREFERENCED_PARAMETER(function);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	UNREFERENCED_PARAMETER(pReserved);
	return;
}
void win_environ_init()
{
	WSADATA wsd;
	environ = _environ;
	WSAStartup(WINSOCK_VERSION, &wsd);

	event_enable_debug_mode();
	event_enable_debug_logging(EVENT_DBG_ALL);
	_invalid_parameter_handler oldHandler, newHandler;
	newHandler = myInvalidParameterHandler;
	oldHandler = _set_invalid_parameter_handler(newHandler);

}
const char* const get_tmp_dir()
{
	static char tempPath[MAX_PATH*2 + 1];
	WCHAR* fU;
	const char* ptr = NULL;
	if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &fU) == S_OK)
	{
		WideCharToMultiByte(CP_ACP, 0, fU, -1, tempPath, ARRAYSIZE(tempPath), NULL, NULL);
		ptr = tempPath;
		CoTaskMemFree(fU);
	}
	return ptr;
}
int getpagesize(void)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return (int)si.dwPageSize;
}
char* basename(const char* pathname)
{
	char fname[MAX_PATH+1];
	char ext[_MAX_EXT + 1];
	__declspec(thread) static char result[MAX_PATH+1];

	result[0] = 0;
	int rc = _splitpath_s(pathname,
		NULL,
		0,
		NULL,
		0,
		fname,
		ARRAYSIZE(fname),
		ext,
		ARRAYSIZE(ext));

	if (rc == 0)
	{
		size_t flen = strlen(fname);
		strncpy(result, fname, flen);
		strncpy(result + flen, ext, strlen(ext));
	}
	return result;
}
char* dirname(char* pathname)
{
	char dname[MAX_PATH+1];
	__declspec(thread) static char result[MAX_PATH+1];

	result[0] = 0;
	int rc = _splitpath_s(pathname,
		dname,
		ARRAYSIZE(dname),
		NULL,
		0,
		NULL,
		0,
		NULL,
		0);

	if (rc == 0)
	{
		size_t dlen = strlen(dname);
		strncpy(result, dname, dlen);
	}
	return result;
}
char* ctime_r(const time_t* timep, char* buf)
{
	errno_t e = ctime_s(buf, 26, timep);
	return (e == 0 ? NULL :buf);
}
char* realpath(const char* path, char* realpathOut)
{
	if (realpathOut == NULL)
	{
		realpathOut = malloc(MAX_PATH);
	}
	if (realpathOut != NULL)
	{
		if (PathCanonicalizeA(realpathOut, path) == FALSE)
		{
			free(realpathOut);
			realpathOut = NULL;
		}
	}
	return realpathOut;
}
static HANDLE hSingleInstanceMutex;
int	 daemon(int x1, int x2)
{
	UNREFERENCED_PARAMETER(x1);
	UNREFERENCED_PARAMETER(x2);

	hSingleInstanceMutex = CreateMutexW(NULL, FALSE, SERVER_SINGLE_INTSTANCE_MUTEX_NAME);

	if ((hSingleInstanceMutex != NULL) && GetLastError() == ERROR_ALREADY_EXISTS) // another server instance has already started
	{
		return -1;
	}
	if (hSingleInstanceMutex == NULL) // failed to create a mutex completely
	{
		return -1;
	}

//	FreeConsole();
	return 0;
}
ssize_t writev(fd_t fd, const struct iovec* iov, int iovcnt)
{
	UNREFERENCED_PARAMETER(fd);
	UNREFERENCED_PARAMETER(iov);
	UNREFERENCED_PARAMETER(iovcnt);
	return -1;
}
int ioctl(fd_t fd, int code, void* param)
{
	UNREFERENCED_PARAMETER(fd);
	UNREFERENCED_PARAMETER(code);
	UNREFERENCED_PARAMETER(param);
	return -1;
}
char*
osdep_get_name(fd_t fd, char* tty)
{
	UNREFERENCED_PARAMETER(fd);
	UNREFERENCED_PARAMETER(tty);
	return "None";
}
char*
osdep_get_cwd(fd_t fd)
{
	UNREFERENCED_PARAMETER(fd);
	return "cwd";
}
int uname(struct utsname* name)
{
	memset(name, 0, sizeof(*name));
	strcpy(name->sysname, "Windows");
	strcpy(name->release, "10.666");
	return 0;
}
void closefrom(fd_t fd)
{
	UNREFERENCED_PARAMETER(fd);
}
void setblocking(fd_t fd, int on)
{
	on = 1 - on;
	ioctlsocket(fd, FIONBIO, (u_long*)& on);
}
void win_close(fd_t fd)
{
	if (fd != (fd_t)INT_MAX)
	{
		DWORD type = GetFileType((HANDLE)fd);
		if (type == FILE_TYPE_CHAR)
		{
			CloseHandle((HANDLE)fd);
		}
		else if(type  == FILE_TYPE_PIPE)
		{
			closesocket(fd);
		}
	}
}
int win_isatty(fd_t fd)
{
	DWORD type = GetFileType((HANDLE)fd);
	return (type == FILE_TYPE_CHAR ? 1 : 0);
}
fd_t win_dup(fd_t infd)
{
	fd_t newFd = INVALID_FD;
	DuplicateHandle(GetCurrentProcess(),
		(HANDLE)infd,
		GetCurrentProcess(),
		(HANDLE*)&newFd,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS);

	return newFd;
}

void dprintf(const char* fmt, ...)
{
	char buf[4096];
	char buf2[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE, fmt, args);
	va_end(args);
	snprintf(buf2, ARRAYSIZE(buf2),  "[%d:%d] %s", GetCurrentProcessId(), GetCurrentThreadId(), buf);
	OutputDebugStringA(buf);

}
int socketpair(int domain, int type, int protocol, fd_t sv[2])
{
	SOCKET fd0 = INVALID_FD;
	SOCKET fd1 = INVALID_FD;
	SOCKET fdListen = INVALID_FD;
	struct sockaddr_un sa;
	static int counter = 0;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	_snprintf_s(sa.sun_path, ARRAYSIZE(sa.sun_path),_TRUNCATE, " %d-%d", GetCurrentProcessId(),counter);
	sa.sun_path[0] = 0;

	if (domain != AF_UNIX || type != SOCK_STREAM || protocol != PF_UNSPEC)
	{
		return -1;
	}
	fdListen = WSASocketW(domain, type, protocol,NULL,0,0);
	if (fdListen == INVALID_SOCKET)
	{
		goto Error;
	}
	if (bind(fdListen, (struct sockaddr *)&sa, sizeof sa) == SOCKET_ERROR) {
		goto Error;
	}
	if (listen(fdListen, 5) == SOCKET_ERROR) {
		goto Error;
	}
	fd0 = socket(domain, type, protocol);
	if (fd0 == INVALID_SOCKET)
	{
		goto Error;
	}
	if (connect(fd0, (const struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR)
	{
		goto Error;
	}
	{
		int addrLen = sizeof(sa);;
		fd1 = accept(fdListen, (struct sockaddr*) & sa, &addrLen);
	}
	if (fd1 == SOCKET_ERROR)
	{
		goto Error;
	}

	sv[0] = fd0;
	sv[1] = fd1;
	closesocket(fdListen);
	return 0;
Error:
	if (fd0 != INVALID_FD)
	{
		closesocket(fd0);
	}
	if (fd1 != INVALID_FD)
	{
		closesocket(fd1);
	}
	if (fdListen != INVALID_FD)
	{
		closesocket(fdListen);
	}
	return -1;
}
int start_process_as_job(const char* cmd, const char* cwd, void* env, fd_t std_fd, HANDLE* outJob)
{
	BOOL bIsProcessInJob;
	HANDLE hJob = NULL;
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	int returnCode = -1;
	char* winenv = win_convert_env(env);

	if (IsProcessInJob(GetCurrentProcess(), NULL, &bIsProcessInJob) == FALSE)
	{
		return -1;
	}
	hJob = CreateJobObject(NULL, NULL);
	if (hJob == NULL)
	{
		goto Error;
	}
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInfo = { 0 };
	limitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if(!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &limitInfo, sizeof(limitInfo)))
	{ 
		goto Error;
	}
	char cmdlocation[MAX_PATH + 1];
	si.cb = sizeof(si);
	if (!DuplicateHandle(GetCurrentProcess(),
					(HANDLE)std_fd,
					GetCurrentProcess(),
					&si.hStdInput,
					0,
					TRUE,
					DUPLICATE_SAME_ACCESS))
	{
		goto Error;
	}
	if (!DuplicateHandle(GetCurrentProcess(),
					(HANDLE)std_fd,
					GetCurrentProcess(),
					&si.hStdOutput,
					0,
					TRUE,
					DUPLICATE_SAME_ACCESS))
	{
		goto Error;
	}

	int offset = GetSystemDirectoryA(cmdlocation, ARRAYSIZE(cmdlocation));
	if (offset == 0)
	{
		goto Error;
	}
	_snprintf_s(cmdlocation + offset, ARRAYSIZE(cmdlocation) - offset, _TRUNCATE, "\\cmd.exe /c %s", cmd);

	if (!CreateProcessA(NULL,
					cmdlocation,
					NULL,
					NULL,
					TRUE, // InheritHandles
					bIsProcessInJob ? CREATE_BREAKAWAY_FROM_JOB | CREATE_SUSPENDED : CREATE_SUSPENDED,
					env,
					cwd,
					&si,
					&pi))
	{
		goto Error;
	}
	if (!AssignProcessToJobObject(hJob, pi.hProcess))
	{
		goto Error;
	}
	ResumeThread(pi.hThread);

	*outJob = hJob;
	returnCode = 0;
Error:
	if( (returnCode != 0) && (hJob != NULL))
	{
		CloseHandle(hJob);
	}
	if (pi.hProcess != NULL)
	{
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	if (si.hStdInput != NULL)
	{
		CloseHandle(si.hStdInput);
	}
	if (si.hStdOutput != NULL)
	{
		CloseHandle(si.hStdOutput);
	}
	if (winenv != NULL)
	{
		win_free_env(winenv);
	}
	return returnCode;
}
void kill(int pid, int signal)
{
	UNREFERENCED_PARAMETER(pid);
	UNREFERENCED_PARAMETER(signal);
}
