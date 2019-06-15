/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "tmux.h"

//https://stackoverflow.com/questions/40159892/using-asprintf-on-windows
int vasprintf(char **strp, const char *fmt, va_list ap) {
    int len = _vscprintf(fmt, ap);
    if (len == -1) return -1;
    char *str = malloc((size_t) len + 1);
    if (!str) return -1;
    int r = vsnprintf(str, len + 1, fmt, ap); /* "secure" version of vsprintf */
    if (r == -1) return free(str), -1;
    *strp = str;
    return r;
}
int asprintf(char *strp[], const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
int gettimeofday(struct timeval* tv, struct timezone* tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch */
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tmpres /= 10;           /*convert into microseconds */
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}
struct event_base*
	osdep_event_init(void)
{
	return (event_init());
}
extern void spawn_log(const char* from, struct spawn_context* sc);
struct window_pane *
spawn_pane(struct spawn_context *sc, char **cause)
{
	struct cmdq_item	 *item = sc->item;
	struct client		 *c = item->client;
	struct session		 *s = sc->s;
	struct window		 *w = sc->wl->window;
	struct window_pane	 *new_wp;
	struct environ		 *child;
	struct environ_entry	 *ee;
	char			**argv, *cp, **argvp, *argv0, *cwd;
	const char		 *cmd, *tmp;
	int			  argc;
	u_int			  idx;
	struct termios		  now;
	u_int			  hlimit;
	struct winsize		  ws;
	sigset_t		  set, oldset;

	spawn_log(__func__, sc);

	/*
	 * If we are respawning then get rid of the old process. Otherwise
	 * either create a new cell or assign to the one we are given.
	 */
	hlimit = options_get_number(s->options, "history-limit");
	if (sc->flags & SPAWN_RESPAWN) {
		if (sc->wp0->fd != -1 && (~sc->flags & SPAWN_KILL)) {
			window_pane_index(sc->wp0, &idx);
			xasprintf(cause, "pane %s:%d.%u still active",
			    s->name, sc->wl->idx, idx);
			return (NULL);
		}
		if (sc->wp0->fd != -1) {
			bufferevent_free(sc->wp0->event);
			close(sc->wp0->fd);
		}
		window_pane_reset_mode_all(sc->wp0);
		screen_reinit(&sc->wp0->base);
		input_init(sc->wp0);
		new_wp = sc->wp0;
		new_wp->flags &= ~(PANE_STATUSREADY|PANE_STATUSDRAWN);
	} else if (sc->lc == NULL) {
		new_wp = window_add_pane(w, NULL, hlimit, sc->flags);
		layout_init(w, new_wp);
	} else {
		new_wp = window_add_pane(w, sc->wp0, hlimit, sc->flags);
		layout_assign_pane(sc->lc, new_wp);
	}

	/*
	 * Now we have a pane with nothing running in it ready for the new
	 * process. Work out the command and arguments.
	 */
	if (sc->argc == 0) {
		cmd = options_get_string(s->options, "default-command");
		if (cmd != NULL && *cmd != '\0') {
			argc = 1;
			argv = (char **)&cmd;
		} else {
			argc = 0;
			argv = NULL;
		}
	} else {
		argc = sc->argc;
		argv = sc->argv;
	}

	/*
	 * Replace the stored arguments if there are new ones. If not, the
	 * existing ones will be used (they will only exist for respawn).
	 */
	if (argc > 0) {
		cmd_free_argv(new_wp->argc, new_wp->argv);
		new_wp->argc = argc;
		new_wp->argv = cmd_copy_argv(argc, argv);
	}

	/*
	 * Work out the current working directory. If respawning, use
	 * the pane's stored one unless specified.
	 */
	if (sc->cwd != NULL)
		cwd = format_single(item, sc->cwd, c, s, NULL, NULL);
	else if (~sc->flags & SPAWN_RESPAWN)
		cwd = xstrdup(server_client_get_cwd(c, s));
	else
		cwd = NULL;
	if (cwd != NULL) {
		free(new_wp->cwd);
		new_wp->cwd = cwd;
	}

	/* Create an environment for this pane. */
	child = environ_for_session(s, 0);
	if (sc->environ != NULL)
		environ_copy(sc->environ, child);
	environ_set(child, "TMUX_PANE", "%%%u", new_wp->id);

	/*
	 * Then the PATH environment variable. The session one is replaced from
	 * the client if there is one because otherwise running "tmux new
	 * myprogram" wouldn't work if myprogram isn't in the session's path.
	 */
	if (c != NULL && c->session == NULL) { /* only unattached clients */
		ee = environ_find(c->environ, "PATH");
		if (ee != NULL)
			environ_set(child, "PATH", "%s", ee->value);
	}
	if (environ_find(child, "PATH") == NULL)
		environ_set(child, "%s", _PATH_DEFPATH);

	/* Then the shell. If respawning, use the old one. */
	if (~sc->flags & SPAWN_RESPAWN) {
		tmp = options_get_string(s->options, "default-shell");
		if (*tmp == '\0' || areshell(tmp))
			tmp = _PATH_BSHELL;
		free(new_wp->shell);
		new_wp->shell = xstrdup(tmp);
	}
	environ_set(child, "SHELL", "%s", new_wp->shell);

	/* Log the arguments we are going to use. */
	log_debug("%s: shell=%s", __func__, new_wp->shell);
	if (new_wp->argc != 0) {
		cp = cmd_stringify_argv(new_wp->argc, new_wp->argv);
		log_debug("%s: cmd=%s", __func__, cp);
		free(cp);
	}
	if (cwd != NULL)
		log_debug("%s: cwd=%s", __func__, cwd);
	cmd_log_argv(new_wp->argc, new_wp->argv, __func__);
	environ_log(child, "%s: environment ", __func__);

	/* If the command is empty, don't fork a child process. */
	if (sc->flags & SPAWN_EMPTY) {
		new_wp->flags |= PANE_EMPTY;
		new_wp->base.mode &= ~MODE_CURSOR;
		new_wp->base.mode |= MODE_CRLF;
		goto complete;
	}

	/* Initialize the window size. */
	memset(&ws, 0, sizeof ws);
	ws.ws_col = screen_size_x(&new_wp->base);
	ws.ws_row = screen_size_y(&new_wp->base);

	/* Block signals until fork has completed. */
	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oldset);

	/* Fork the new process. */
	new_wp->pid = fdforkpty(ptm_fd, &new_wp->fd, new_wp->tty, NULL, &ws);
	if (new_wp->pid == -1) {
		xasprintf(cause, "fork failed: %s", strerror(errno));
		new_wp->fd = -1;
		if (~sc->flags & SPAWN_RESPAWN) {
			layout_close_pane(new_wp);
			window_remove_pane(w, new_wp);
		}
		sigprocmask(SIG_SETMASK, &oldset, NULL);
		return (NULL);
	}

#if !_MSC_VER
	/* In the parent process, everything is done now. */
	if (new_wp->pid != 0)
		goto complete;

	/*
	 * Child process. Change to the working directory or home if that
	 * fails.
	 */
	if (chdir(new_wp->cwd) != 0) {
		if ((tmp = find_home()) == NULL || chdir(tmp) != 0)
			chdir("/");
	}

	/*
	 * Update terminal escape characters from the session if available and
	 * force VERASE to tmux's \177.
	 */
	if (tcgetattr(STDIN_FILENO, &now) != 0)
		_exit(1);
	if (s->tio != NULL)
		memcpy(now.c_cc, s->tio->c_cc, sizeof now.c_cc);
	now.c_cc[VERASE] = '\177';
	if (tcsetattr(STDIN_FILENO, TCSANOW, &now) != 0)
		_exit(1);

	/* Clean up file descriptors and signals and update the environment. */
	closefrom(STDERR_FILENO + 1);
	proc_clear_signals(server_proc, 1);
	sigprocmask(SIG_SETMASK, &oldset, NULL);
	log_close();
	environ_push(child);

	/*
	 * If given multiple arguments, use execvp(). Copy the arguments to
	 * ensure they end in a NULL.
	 */
	if (new_wp->argc != 0 && new_wp->argc != 1) {
		argvp = cmd_copy_argv(new_wp->argc, new_wp->argv);
		execvp(argvp[0], argvp);
		_exit(1);
	}

	/*
	 * If one argument, pass it to $SHELL -c. Otherwise create a login
	 * shell.
	 */
	cp = strrchr(new_wp->shell, '/');
	if (new_wp->argc == 1) {
		tmp = new_wp->argv[0];
		if (cp != NULL && cp[1] != '\0')
			xasprintf(&argv0, "%s", cp + 1);
		else
			xasprintf(&argv0, "%s", new_wp->shell);
		execl(new_wp->shell, argv0, "-c", tmp, (char *)NULL);
		_exit(1);
	}
	if (cp != NULL && cp[1] != '\0')
		xasprintf(&argv0, "-%s", cp + 1);
	else
		xasprintf(&argv0, "-%s", new_wp->shell);
	execl(new_wp->shell, argv0, (char *)NULL);
	_exit(1);
#endif

complete:
	new_wp->pipe_off = 0;
	new_wp->flags &= ~PANE_EXITED;

	sigprocmask(SIG_SETMASK, &oldset, NULL);
	window_pane_set_event(new_wp);

	if (sc->flags & SPAWN_RESPAWN)
		return (new_wp);
	if ((~sc->flags & SPAWN_DETACHED) || w->active == NULL) {
		if (sc->flags & SPAWN_NONOTIFY)
			window_set_active_pane(w, new_wp, 0);
		else
			window_set_active_pane(w, new_wp, 1);
	}
	if (~sc->flags & SPAWN_NONOTIFY)
		notify_window("window-layout-changed", w);
	return (new_wp);
}
pid_t
forkpty(int* amaster, char* name, struct termios* termp, struct winsize* winp)
{
	return (pid_t)-1;
}
