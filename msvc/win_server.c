extern void server_add_accept(int timeout);
int server_create_socket(char** cause);
int
win_server_start(struct tmuxproc* client, struct event_base* base)
{
	int numArgs = 0;
	WCHAR procFile[MAX_PATH + 1];
	WCHAR **argvArray;
	WCHAR* winServerArg = L"-W";
	WCHAR* cmdline = GetCommandLineW();
	WCHAR** argvs = CommandLineToArgvW(cmdline, &numArgs);
	if (argvs != NULL)
	{
		argvArray = malloc(sizeof( WCHAR*)*(numArgs + 2));
		if (argvArray != NULL)
		{
			for (int i= 0; i < numArgs ; i++)
			{
				argvArray[i] = argvs[i];
			}
			argvArray[numArgs] = winServerArg;
			argvArray[numArgs + 1] = NULL;
			if (GetModuleFileNameW(NULL, procFile, ARRAYSIZE(procFile)) != 0)
			{
				HANDLE h = (HANDLE)_wspawnv(_P_NOWAIT, procFile, argvArray);
				if (h != INVALID_HANDLE_VALUE)
				{
					CloseHandle(h);
				}
			}
			free(argvArray);
			return 0;
		}
	}
	return -1;
}
void run_windows_server(struct tmuxproc	*client_proc,struct event_base *base)
{
	struct client	*c;
	char		*cause = NULL;

	if (daemon(1, 0) != 0)
		fatal("daemon failed");

	server_proc = proc_start("server");

	if (log_get_level() > 1)
		tty_create_log();

	RB_INIT(&windows);
	RB_INIT(&all_window_panes);
	TAILQ_INIT(&clients);
	RB_INIT(&sessions);
	key_bindings_init();

	gettimeofday(&start_time, NULL);

	server_fd = server_create_socket(&cause);
	dprintf("listen fd = %d\n", server_fd);
	if (server_fd != -1)
		server_update_socket();

	start_cfg();
	server_add_accept(0);

	proc_loop(server_proc, server_loop);

	job_kill_all();
	status_prompt_save_history();
	exit(0);
}

