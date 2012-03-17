#include "spawn.h"
#include "editor.h"
#include "buffer.h"
#include "regexp.h"
#include "gbuf.h"
#include "msg.h"
#include "term.h"
#include "fork.h"

static void handle_error_msg(struct compiler *c, char *str)
{
	int i, len;

	for (i = 0; str[i]; i++) {
		if (str[i] == '\n') {
			str[i] = 0;
			break;
		}
		if (str[i] == '\t')
			str[i] = ' ';
	}
	len = i;
	if (len == 0)
		return;

	for (i = 0; i < c->error_formats.count; i++) {
		const struct error_format *p = c->error_formats.ptrs[i];

		if (!regexp_match(p->pattern, str, len))
			continue;
		if (!p->ignore) {
			struct message *m = new_message(regexp_matches[p->msg_idx]);
			m->file = p->file_idx < 0 ? NULL : xstrdup(regexp_matches[p->file_idx]);
			m->u.location.line = p->line_idx < 0 ? 0 : atoi(regexp_matches[p->line_idx]);
			m->u.location.column = p->column_idx < 0 ? 0 : atoi(regexp_matches[p->column_idx]);
			add_message(m);
		}
		free_regexp_matches();
		return;
	}
	add_message(new_message(str));
}

static void read_errors(struct compiler *c, int fd, unsigned int flags)
{
	FILE *f = fdopen(fd, "r");
	char line[4096];

	if (!f) {
		// should not happen
		return;
	}

	while (fgets(line, sizeof(line), f)) {
		if (flags & SPAWN_PRINT_ERRORS)
			fputs(line, stderr);
		handle_error_msg(c, line);
	}
	fclose(f);
}

static void filter(int rfd, int wfd, struct filter_data *fdata)
{
	unsigned int wlen = 0;
	GBUF(buf);
	int rc;

	if (!fdata->in_len) {
		close(wfd);
		wfd = -1;
	}
	while (1) {
		fd_set rfds, wfds;
		fd_set *wfdsp = NULL;
		int fd_high = rfd;

		FD_ZERO(&rfds);
		FD_SET(rfd, &rfds);

		if (wfd >= 0) {
			FD_ZERO(&wfds);
			FD_SET(wfd, &wfds);
			wfdsp = &wfds;
		}
		if (wfd > fd_high)
			fd_high = wfd;

		rc = select(fd_high + 1, &rfds, wfdsp, NULL, NULL);
		if (rc < 0) {
			if (errno == EINTR)
				continue;
			error_msg("select: %s", strerror(errno));
			break;
		}

		if (FD_ISSET(rfd, &rfds)) {
			char data[8192];

			rc = read(rfd, data, sizeof(data));
			if (rc < 0) {
				error_msg("read: %s", strerror(errno));
				break;
			}
			if (!rc) {
				if (wlen < fdata->in_len)
					error_msg("Command did not read all data.");
				break;
			}
			gbuf_add_buf(&buf, data, rc);
		}
		if (wfdsp && FD_ISSET(wfd, &wfds)) {
			rc = write(wfd, fdata->in + wlen, fdata->in_len - wlen);
			if (rc < 0) {
				error_msg("write: %s", strerror(errno));
				break;
			}
			wlen += rc;
			if (wlen == fdata->in_len) {
				close(wfd);
				wfd = -1;
			}
		}
	}

	if (buf.len) {
		fdata->out_len = buf.len;
		fdata->out = gbuf_steal(&buf);
	} else {
		fdata->out_len = 0;
		fdata->out = NULL;
	}
}

int spawn_filter(char **argv, struct filter_data *data)
{
	int p0[2] = { -1, -1 };
	int p1[2] = { -1, -1 };
	int dev_null = -1;
	int fd[3], pid, ret;

	data->out = NULL;
	data->out_len = 0;

	if (pipe(p0) || pipe(p1)) {
		error_msg("pipe: %s", strerror(errno));
		goto error;
	}
	dev_null = open("/dev/null", O_WRONLY);
	if (dev_null < 0) {
		error_msg("Error opening /dev/null: %s", strerror(errno));
		goto error;
	}

	close_on_exec(p0[0]);
	close_on_exec(p0[1]);
	close_on_exec(p1[0]);
	close_on_exec(p1[1]);
	close_on_exec(dev_null);

	fd[0] = p0[0];
	fd[1] = p1[1];
	fd[2] = dev_null;
	pid = fork_exec(argv, fd);
	if (pid < 0) {
		error_msg("Error: %s", strerror(errno));
		goto error;
	}

	close(dev_null);
	close(p0[0]);
	close(p1[1]);
	filter(p1[0], p0[1], data);
	close(p1[0]);
	close(p0[1]);

	ret = wait_child(pid);
	if (ret < 0) {
		error_msg("waitpid: %s", strerror(errno));
		return -1;
	}
	if (ret >= 256) {
		error_msg("Child received signal %d", ret >> 8);
		return -1;
	}
	if (ret) {
		error_msg("Child returned %d", ret);
		return -1;
	}
	return 0;
error:
	close(p0[0]);
	close(p0[1]);
	close(p1[0]);
	close(p1[1]);
	close(dev_null);
	return -1;
}

void spawn_compiler(char **args, unsigned int flags, struct compiler *c)
{
	unsigned int redir = SPAWN_REDIRECT_STDOUT | SPAWN_REDIRECT_STDERR;
	int pid, quiet;
	int dev_null, p[2], fd[3];

	dev_null = open("/dev/null", O_WRONLY);
	if (dev_null < 0) {
		error_msg("Error opening /dev/null: %s", strerror(errno));
		return;
	}
	if (pipe(p)) {
		error_msg("pipe: %s", strerror(errno));
		close(dev_null);
		return;
	}

	fd[0] = dev_null;
	if (flags & SPAWN_READ_STDOUT) {
		fd[1] = p[1];
		fd[2] = 2;
	} else {
		fd[1] = 1;
		fd[2] = p[1];
	}

	flags |= SPAWN_PRINT_ERRORS;
	if (flags & SPAWN_REDIRECT_STDOUT) {
		if (fd[1] == p[1])
			flags &= ~SPAWN_PRINT_ERRORS;
		else
			fd[1] = dev_null;
	}
	if (flags & SPAWN_REDIRECT_STDERR) {
		if (fd[2] == p[1])
			flags &= ~SPAWN_PRINT_ERRORS;
		else
			fd[2] = dev_null;
	}

	quiet = !(flags & SPAWN_PRINT_ERRORS) && (flags & redir) == redir;
	if (!quiet) {
		child_controls_terminal = 1;
		ui_end();
	}

	close_on_exec(p[0]);
	close_on_exec(p[1]);
	close_on_exec(dev_null);
	pid = fork_exec(args, fd);
	if (pid < 0) {
		error_msg("Error: %s", strerror(errno));
		close(p[1]);
		flags = 0; // don't prompt
	} else {
		int ret;

		// Must close write end of the pipe before read_errors() or
		// the read end never gets EOF!
		close(p[1]);
		read_errors(c, p[0], flags);
		ret = wait_child(pid);
		if (ret < 0) {
			error_msg("waitpid: %s", strerror(errno));
		} else if (ret >= 256) {
			error_msg("Child received signal %d", ret >> 8);
		} else if (ret) {
			error_msg("Child returned %d", ret);
		}
	}
	if (!quiet) {
		term_raw();
		if (flags & SPAWN_PROMPT)
			any_key();
		resize();
		child_controls_terminal = 0;
	}
	close(p[0]);
	close(dev_null);
}

void spawn(char **args, int fd[3], int prompt)
{
	int i, pid, quiet, redir_count = 0;
	int dev_null = -1;

	for (i = 0; i < 3; i++) {
		if (fd[i] >= 0)
			continue;

		if (dev_null < 0) {
			dev_null = open("/dev/null", O_WRONLY);
			if (dev_null < 0) {
				error_msg("Error opening /dev/null: %s", strerror(errno));
				return;
			}
			close_on_exec(dev_null);
		}
		fd[i] = dev_null;
		redir_count++;
	}
	quiet = redir_count == 3;

	if (!quiet) {
		child_controls_terminal = 1;
		ui_end();
	}

	pid = fork_exec(args, fd);
	if (pid < 0) {
		error_msg("Error: %s", strerror(errno));
		prompt = 0;
	} else {
		int ret = wait_child(pid);
		if (ret < 0) {
			error_msg("waitpid: %s", strerror(errno));
		} else if (ret >= 256) {
			error_msg("Child received signal %d", ret >> 8);
		} else if (ret) {
			error_msg("Child returned %d", ret);
		}
	}
	if (!quiet) {
		term_raw();
		if (prompt)
			any_key();
		resize();
		child_controls_terminal = 0;
	}
	if (dev_null >= 0)
		close(dev_null);
}
