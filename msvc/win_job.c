#include <vc_compat.h>

extern int start_process_as_job(const char* cmd, const char* cwd, void* env, fd_t std_fd,HANDLE* outJob);

/* Start a job running, if it isn't already. */
struct job *
job_run(const char *cmd, struct session *s, const char *cwd,
    job_update_cb updatecb, job_complete_cb completecb, job_free_cb freecb,
    void *data, int flags)
{
	struct job	*job;
	struct environ	*env;
	pid_t		 pid;
	fd_t		 nullfd, out[2];
	const char	*home;
	sigset_t	 set, oldset;
	HANDLE jobHandle;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, out) != 0)
		return (NULL);
	log_debug("%s: cmd=%s, cwd=%s", __func__, cmd, cwd == NULL ? "" : cwd);

	/*
	 * Do not set TERM during .tmux.conf, it is nice to be able to use
	 * if-shell to decide on default-terminal based on outside TERM.
	 */
	env = environ_for_session(s, !cfg_finished);

	pid = start_process_as_job(cmd, cwd, env, out[1],&jobHandle);

	switch (pid) {
	case -1:
		environ_free(env);
		closesocket(out[0]);
		closesocket(out[1]);
		return (NULL);
	}

	environ_free(env);
	closesocket(out[1]);

	job = xmalloc(sizeof *job);
	job->state = JOB_RUNNING;
	job->flags = flags;

	job->cmd = xstrdup(cmd);
	job->pid = pid;
	job->status = 0;
	job->hJobObject = jobHandle;

	LIST_INSERT_HEAD(&all_jobs, job, entry);

	job->updatecb = updatecb;
	job->completecb = completecb;
	job->freecb = freecb;
	job->data = data;

	job->fd = out[0];
	setblocking(job->fd, 0);

	job->event = bufferevent_new(job->fd, job_read_callback,
	    job_write_callback, job_error_callback, job);
	if (job->event == NULL)
		fatalx("out of memory");
	bufferevent_enable(job->event, EV_READ|EV_WRITE);

	log_debug("run job %p: %s, pid %ld", job, job->cmd, (long) job->pid);
	return (job);
}
