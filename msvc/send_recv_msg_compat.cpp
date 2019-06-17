#include <memory>
#include <unordered_set>
#include <mutex>
#include <vc_compat.h>

#define CBR(cond)  if((cond) != true) {goto Error;}

//
// As usual, Microsoft does a half-assed job with AF_UNIX, so you can't use WSASendMsg/WSARecvMsg on SOCK_STREAM
// even with AF_UNIX
//
// So we define a protocol for exchanging "messages" on a stream transport.
// And keep a list of duplicated handles that need to be closed after they other side has acked
// 
static std::unordered_set<fd_t> s_duplicatedHandles;
static std::mutex s_dupHandleLock;

struct stream_msghdr
{
	// Field layout (in DWORDS)
	//Field Name      StartOffset		length
	//nameLen				0               1        
	//nBufCount	            1				1
	//bufferSize[nBufs]		2				nBufs
	//controlLen 			nBufs + 2		1

	// Immediately following all the application data are 3 fields, that are not listed in the header,
	// as they are all fixed size data.
	// DWORD msg->flags
	// DWORD pid of the current process // may not be present if SCM_RIGHTS is not in the cmsg type
	// ULONGLONG socket to be duplicated  // may not be present if SCM_RIGHTS is not in the cmsg type

	WSABUF hdrBuf;

	stream_msghdr(const WSAMSG& msg)
	{
		hdrBuf.len = sizeof(DWORD) * (msg.dwBufferCount + 3);
		hdrBuf.buf = (char*)malloc(hdrBuf.len);
		if (hdrBuf.buf != nullptr)
		{
			DWORD* ptr = (DWORD*)(hdrBuf.buf);
			ptr[0] = msg.namelen;
			ptr[1] = msg.dwBufferCount;
			for (unsigned int i = 0; i < msg.dwBufferCount; i++)
			{
				ptr[i + 2] = msg.lpBuffers[i].len;
				dprintf("size of hdr index %d (%d), %d\n", i + 2,i, msg.lpBuffers[i].len);
			}
			ptr[msg.dwBufferCount + 2] = msg.Control.len;
		}
		else
		{
			hdrBuf.len = 0;
		}
	}
	~stream_msghdr()
	{
		free(hdrBuf.buf);
	}
};
fd_t dup_handle_from_process(DWORD pid, fd_t origHandle)
{
	HANDLE hp = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
	fd_t duper = NULL;
	if (hp != NULL)
	{
		if (!DuplicateHandle(hp, (HANDLE)origHandle, GetCurrentProcess(), (HANDLE*)&duper, 0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			duper = NULL;
		}
	}
	CloseHandle(hp);
	return duper;
}
int read_exactly_bytes_from_socket(fd_t sockfd, void* buf, int len)
{
	int read = 0;
	int retries = 0;
	while (read < len)
	{
		int curr = recv(sockfd, (char*)buf + read, len - read, 0);
		if(curr == SOCKET_ERROR)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				return -1;
			}
			if (++retries > 10)
			{
				return -1;
			}
			Sleep(10);
		}
		if (curr == 0)
		{
			return -1;
		}
		read += curr;
	}
	return read;
}

ssize_t recvmsg(fd_t sockfd, struct msghdr* msg, int flags)
{
	DWORD bytesCallerExpectsToRecv = 0;
	DWORD overheadBytes = 0;
	int retVal = -1;
	if((flags != 0) || (msg->dwBufferCount != 1) ) // cheating a bit. we know the code never calls recvmsg with any other values for these two
	{
		return -1;
	}
	DWORD nameAndBuf[2];
	DWORD controlLen;
	std::vector<DWORD> bufLens;
	DWORD bufLenSum = 0;
	int rc = 0;

	rc = read_exactly_bytes_from_socket(sockfd, nameAndBuf, sizeof(nameAndBuf));
	CBR(rc > 0);

	overheadBytes += sizeof(nameAndBuf);
	
	for (unsigned int i = 0; i < nameAndBuf[1]; i++)
	{
		DWORD curBufLen;
		rc = read_exactly_bytes_from_socket(sockfd, &curBufLen, sizeof(curBufLen));
		overheadBytes += sizeof(curBufLen);
		CBR(rc > 0);
		bufLenSum += curBufLen;
		dprintf("(R) Wsamsg buffers index %d, len %d\n", i, curBufLen);
		bufLens.push_back(curBufLen);
	}
	rc = read_exactly_bytes_from_socket(sockfd, &controlLen, sizeof(controlLen));
	CBR(rc > 0);
	overheadBytes += sizeof(controlLen);

	msg->namelen = nameAndBuf[0];
	bytesCallerExpectsToRecv += msg->namelen;
	if (msg->namelen > 0)
	{
		rc = read_exactly_bytes_from_socket(sockfd, msg->name, msg->namelen);
		CBR(rc > 0);
	}

	// should have room in msg->lpBuffer
	CBR(msg->lpBuffers[0].len >= bufLenSum);
	
	msg->dwBufferCount = 1;

	rc = read_exactly_bytes_from_socket(sockfd, msg->lpBuffers[0].buf , bufLenSum );
	CBR(rc > 0);
	msg->lpBuffers[0].len = bufLenSum;
	bytesCallerExpectsToRecv += bufLenSum;;

	CBR(msg->Control.len >= controlLen);
	msg->Control.len = controlLen;

	if (msg->Control.len > 0)
	{
		rc = read_exactly_bytes_from_socket(sockfd, msg->Control.buf, msg->Control.len);
		CBR(rc > 0);
	}
	rc = read_exactly_bytes_from_socket(sockfd, &msg->dwFlags, sizeof(msg->dwFlags));
	

	CBR(rc > 0);
	
	{
		WSACMSGHDR* cmsg = CMSG_FIRSTHDR(msg);
		if (cmsg != NULL && cmsg->cmsg_type == SCM_RIGHTS) // expect a handle
		{
			DWORD pid;
			rc = read_exactly_bytes_from_socket(sockfd, &pid, sizeof(pid));
			CBR(rc > 0);
			overheadBytes += sizeof(pid);
			fd_t origHandle;
			rc = read_exactly_bytes_from_socket(sockfd, &origHandle, sizeof(origHandle));
			CBR(rc > 0);
			overheadBytes += sizeof(origHandle);

			fd_t* pfd = (fd_t*)CMSG_DATA(cmsg);

			*pfd = dup_handle_from_process(pid, origHandle);
			
			if (*pfd == NULL)
			{
				*pfd = INVALID_FD;
			}

			//sendack;
		}
	}
	retVal = bytesCallerExpectsToRecv;
	dprintf("Recvmsg got %d user bytes,overhead %d\n", bytesCallerExpectsToRecv,overheadBytes);
Error:
	if (WSAGetLastError() == WSAEWOULDBLOCK)
	{
		errno = EAGAIN;
	}
	return retVal;
}
ssize_t sendmsg(fd_t sockfd, struct msghdr* msg, int flags)
{
	DWORD bytesSent = 0; 
	DWORD bytesCallerExpectsSent = 0;
	DWORD overHeadBytes = 0;
	DWORD pid = GetCurrentProcessId();
	fd_t hdup;
	int retVal = -1;
	WSAMSG* wsaMsg = (WSAMSG*)msg;

	if(flags != 0)
	{
		errno = EINVAL;
		return -1;
	}

	stream_msghdr hdr(*msg);

	DWORD numWSABUFs = (wsaMsg->dwBufferCount + 6);  // header, name, buffers,control,flags,processId,handle value
	std::unique_ptr<WSABUF[]> ptrs = std::make_unique<WSABUF[]>(numWSABUFs) ;

	if (hdr.hdrBuf.len == 0)
	{
		errno = ENOMEM;
		return -1;
	}
	int currentIndex = 0;

	ptrs[currentIndex].buf = hdr.hdrBuf.buf;
	ptrs[currentIndex++].len = hdr.hdrBuf.len;
	overHeadBytes += hdr.hdrBuf.len;

	ptrs[currentIndex].buf = (char*)wsaMsg->name;
	ptrs[currentIndex++].len = wsaMsg->namelen;

	bytesCallerExpectsSent += wsaMsg->namelen;

	for (size_t si = 0; si < wsaMsg->dwBufferCount; si++)
	{
		ptrs[currentIndex].buf = (char*)wsaMsg->lpBuffers[si].buf;
		ptrs[currentIndex++].len = wsaMsg->lpBuffers[si].len;
		dprintf("(S) Wsamsg buffers index %d (%d), len %d\n",si,currentIndex-1,wsaMsg->lpBuffers[si].len);
		bytesCallerExpectsSent += wsaMsg->lpBuffers[si].len;
	}
	ptrs[currentIndex].buf = (char*)wsaMsg->Control.buf;
	ptrs[currentIndex++].len = wsaMsg->Control.len;

	ptrs[currentIndex].buf = (char*)&wsaMsg->dwFlags;
	ptrs[currentIndex++].len = sizeof(wsaMsg->dwFlags);
	

	WSACMSGHDR* cmsg = CMSG_FIRSTHDR(wsaMsg);
	if (cmsg != NULL && cmsg->cmsg_type == SCM_RIGHTS) // make a copy so that the caller can close the original whenever
	{
		fd_t* fd = (fd_t*)CMSG_DATA(cmsg);

		hdup = (fd_t)win_dup(*fd);
		if(hdup == NULL)
		{
			return -1;
		}

		ptrs[currentIndex].buf = (char*)&pid;
		ptrs[currentIndex++].len = sizeof(pid);
		overHeadBytes += sizeof(pid);

		ptrs[currentIndex].buf = (char*)& hdup;
		ptrs[currentIndex++].len = sizeof(hdup);
		overHeadBytes += sizeof(hdup);
		{
			std::lock_guard<std::mutex> guard(s_dupHandleLock);
			s_duplicatedHandles.insert(hdup);
		}
	}
	
	int rc = WSASend(sockfd, ptrs.get(), numWSABUFs, &bytesSent, flags, NULL, NULL);
	if (rc != SOCKET_ERROR && (bytesSent > bytesCallerExpectsSent) )
	{
		retVal = bytesCallerExpectsSent;
	}
	if ((rc == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK) )
	{
		errno = EAGAIN;
	}
	dprintf("sendmsg sent %d user bytes overhead %d\n", bytesCallerExpectsSent,overHeadBytes);
	return retVal;
}