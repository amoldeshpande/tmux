extern int win_server_start(struct tmuxproc* client, struct event_base* base);

static int open_mutex()
{
	HANDLE hMut = OpenMutexW(SYNCHRONIZE, FALSE, SERVER_SINGLE_INTSTANCE_MUTEX_NAME);
	if (hMut == NULL) // mutex does not exist. start the server
	{
		return -1;
	}
	else //mutex exists, retry the connect
	{
		CloseHandle(hMut);
		return -2;
	}

}
/* Connect client to server. */
fd_t
client_connect(struct event_base *base, const char *path, int start_server)
{
	struct sockaddr_un	sa;
	size_t			size;
	fd_t			fd;
	int retry_connect = 0;
	int mutex_exists = 0;

	memset(&sa, 0, sizeof sa);
	sa.sun_family = AF_UNIX;
	size = strlcpy(sa.sun_path, path, sizeof sa.sun_path);
	if (size >= sizeof sa.sun_path) {
		errno = ENAMETOOLONG;
		return INVALID_FD;
	}
	log_debug("socket is %s", path);

retry:
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == INVALID_FD)
		return (INVALID_FD);

	log_debug("trying connect");
	if (connect(fd, (struct sockaddr *)&sa, sizeof sa) == -1) {
		log_debug("connect failed: %s", strerror(sockerrno));
		if (sockerrno != ECONNREFUSED && sockerrno != ENOENT)
			goto failed;
		if (!start_server || (retry_connect++ > 10))
			goto failed;
		close(fd);

		if (!mutex_exists) {
			if ((mutex_exists = open_mutex()) < 0) {
				if (mutex_exists == -2)
					goto retry;
			}
			log_debug("mutex exists (%d)", mutex_exists);
		}
		if ((retry_connect > 1) || (win_server_start(client_proc, base ) == 0) )
		{
			Sleep(1000);
			goto retry;
		}
	}
	setblocking(fd, 0);
	return (fd);

failed:
	closesocket(fd);
	return INVALID_FD;
}
