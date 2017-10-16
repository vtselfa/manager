
void perf_setup()
{
	if (pfm_initialize() != PFM_SUCCESS) {
		errx(1, "libpfm initialization failed\n");
	}
ret = perf_setup_list_events(options.events[0], &(node->fds), &(node->num_fds));

if (ret || (node->num_fds == 0)) {
	        exit (1);
			    }
node->fds[0].fd = -1;
    for (i=0; i<node->num_fds; i++) {

        node->fds[i].hw.disabled = 0;  /* start immediately */

        /* request timing information necessary for scaling counts */
        node->fds[i].hw.read_format = PERF_FORMAT_SCALE;
        node->fds[i].hw.pinned = !i && options.pinned;
        node->fds[i].fd = perf_event_open(&node->fds[i].hw, node->pid, -1, (options.group? node->fds[i].fd : -1), 0);
        if (node->fds[i].fd == -1) {
            errx(1, "cannot attach event %s", node->fds[i].name);
        }
    }



}
void finalitzar_events (node *node) {
    int i;

    // Llibera els descriptors
    for(i=0; i < node->num_fds; i++) {
        close(node->fds[i].fd);
    }

    // Llibera els comptadors
    perf_free_fds(node->fds, node->num_fds);

    node->fds = NULL;


    if (p_seleccio == 1) {
        node->current_run = 0;
    }

    else {

        if (canvi_interquantum == 0) {
            node->current_run++;
            if (node->current_run == RUNS) {
                node->current_run = 1;
            }
        } // si estem en en canvi_interquantum 1, no avancem el current run
    }
}


int perf_init()
{
	perf_event_desc_t *fds = NULL;

	int status = 0, ret, i, num_fds = 0, grp, group_fd;
	int ready[2], go[2];
	char buf;
	pid_t pid;

	go[0] = go[1] = -1;

	if (pfm_initialize() != PFM_SUCCESS)
		errx(1, "libpfm initialization failed");

	for (grp = 0; grp < options.num_groups; grp++)
	{
		int ret;
		ret = perf_setup_list_events(options.events[grp], &fds, &num_fds);
		if (ret || !num_fds)
			exit(1);
	}

	pid = options.pid;
	if (!pid) {
		ret = pipe(ready);
		if (ret)
			err(1, "cannot create pipe ready");

		ret = pipe(go);
		if (ret)
			err(1, "cannot create pipe go");


		/*
		 * Create the child task
		 */
		if ((pid=fork()) == -1)
			err(1, "Cannot fork process");

		/*
		 * and launch the child code
		 *
		 * The pipe is used to avoid a race condition
		 * between for() and exec(). We need the pid
		 * of the new tak but we want to start measuring
		 * at the first user level instruction. Thus we
		 * need to prevent exec until we have attached
		 * the events.
		 */
		if (pid == 0) {
			close(ready[0]);
			close(go[1]);

			/*
			 * let the parent know we exist
			 */
			close(ready[1]);
			if (read(go[0], &buf, 1) == -1)
				err(1, "unable to read go_pipe");


			exit(child(arg));
		}

		close(ready[1]);
		close(go[0]);

		if (read(ready[0], &buf, 1) == -1)
			err(1, "unable to read child_ready_pipe");

		close(ready[0]);
	}

	for(i=0; i < num_fds; i++) {
		int is_group_leader; /* boolean */

		is_group_leader = perf_is_group_leader(fds, i);
		if (is_group_leader) {
			/* this is the group leader */
			group_fd = -1;
		} else {
			group_fd = fds[fds[i].group_leader].fd;
		}

		/*
		 * create leader disabled with enable_on-exec
		 */
		if (!options.pid) {
			fds[i].hw.disabled = is_group_leader;
			fds[i].hw.enable_on_exec = is_group_leader;
		}

		fds[i].hw.read_format = PERF_FORMAT_SCALE;
		/* request timing information necessary for scaling counts */
		if (is_group_leader && options.format_group)
			fds[i].hw.read_format |= PERF_FORMAT_GROUP;

		if (options.inherit)
			fds[i].hw.inherit = 1;

		if (options.pin && is_group_leader)
			fds[i].hw.pinned = 1;
		fds[i].fd = perf_event_open(&fds[i].hw, pid, -1, group_fd, 0);
		if (fds[i].fd == -1) {
			warn("cannot attach event%d %s", i, fds[i].name);
			goto error;
		}
	}

	if (!options.pid && go[1] > -1)
		close(go[1]);

	if (options.print) {
		if (!options.pid) {
			while(waitpid(pid, &status, WNOHANG) == 0) {
				sleep(1);
				print_counts(fds, num_fds);
			}
		} else {
			while(quit == 0) {
				sleep(1);
				print_counts(fds, num_fds);
			}
		}
	} else {
		if (!options.pid)
			waitpid(pid, &status, 0);
		else
			pause();
		print_counts(fds, num_fds);
	}

	for(i=0; i < num_fds; i++)
		close(fds[i].fd);

	perf_free_fds(fds, num_fds);

	/* free libpfm resources cleanly */
	pfm_terminate();

	return 0;
error:
	free(fds);
	if (!options.pid)
		kill(SIGKILL, pid);

	/* free libpfm resources cleanly */
	pfm_terminate();

	return -1;
}

