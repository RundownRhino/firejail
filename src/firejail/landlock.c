/*
 * Copyright (C) 2014-2023 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifdef HAVE_LANDLOCK
#include "firejail.h"
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <linux/prctl.h>
#include <linux/landlock.h>
#include <sys/utsname.h>

static int rset_fd = -1;

// return 1 if the kernel is older than 6.1
static int old_kernel(void) {
	struct utsname u;
	int rv = uname(&u);
	if (rv != 0)
		errExit("uname");
	unsigned major;
	unsigned minor;
	if (2 != sscanf(u.release, "%u.%u", &major, &minor)) {
		fprintf(stderr, "Error: cannot extract Linux kernel version: %s\n", u.version);
		exit(1);
	}

	unsigned version = (major << 8) + minor;
	if (version < ((6 << 8) + 1))
		return 1;

	return 0;
}

int ll_get_fd(void) {
	return rset_fd;
}

#ifndef landlock_create_ruleset
int ll_create_ruleset(struct landlock_ruleset_attr *rsattr, size_t size, __u32 flags) {
	return syscall(__NR_landlock_create_ruleset, rsattr, size, flags);
}
#endif

#ifndef landlock_add_rule
int ll_add_rule(int fd, enum landlock_rule_type t, void *attr, __u32 flags) {
	return syscall(__NR_landlock_add_rule, fd, t, attr, flags);
}
#endif

#ifndef landlock_restrict_self
static inline int landlock_restrict_self(const int ruleset_fd,
		const __u32 flags) {
	return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}
#endif

static int ll_create_full_ruleset() {
	struct landlock_ruleset_attr attr;
	attr.handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_WRITE_FILE |
				 LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
				 LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
				 LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_EXECUTE;
	return ll_create_ruleset(&attr, sizeof(attr), 0);
}

int ll_read(char *allowed_path) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return 1;
	}

	if (rset_fd == -1)
		rset_fd = ll_create_full_ruleset();

	int result;
	int allowed_fd = open(allowed_path, O_PATH | O_CLOEXEC);
	struct landlock_path_beneath_attr target;
	target.parent_fd = allowed_fd;
	target.allowed_access = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR;
	result = ll_add_rule(rset_fd, LANDLOCK_RULE_PATH_BENEATH, &target, 0);
	close(allowed_fd);
	return result;
}

int ll_write(char *allowed_path) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return 1;
	}

	if (rset_fd == -1)
		rset_fd = ll_create_full_ruleset();

	int result;
	int allowed_fd = open(allowed_path, O_PATH | O_CLOEXEC);
	struct landlock_path_beneath_attr target;
	target.parent_fd = allowed_fd;
	target.allowed_access = LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR |
				LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |
				LANDLOCK_ACCESS_FS_MAKE_SYM;
	result = ll_add_rule(rset_fd, LANDLOCK_RULE_PATH_BENEATH, &target, 0);
	close(allowed_fd);
	return result;
}

static int ll_special(char *allowed_path) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return 1;
	}

	if (rset_fd == -1)
		rset_fd = ll_create_full_ruleset();

	int result;
	int allowed_fd = open(allowed_path, O_PATH | O_CLOEXEC);
	struct landlock_path_beneath_attr target;
	target.parent_fd = allowed_fd;
	target.allowed_access = LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK;
	result = ll_add_rule(rset_fd, LANDLOCK_RULE_PATH_BENEATH, &target, 0);
	close(allowed_fd);
	return result;
}

static int ll_exec(char *allowed_path) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return 1;
	}

	if (rset_fd == -1)
		rset_fd = ll_create_full_ruleset();

	int result;
	int allowed_fd = open(allowed_path, O_PATH | O_CLOEXEC);
	struct landlock_path_beneath_attr target;
	target.parent_fd = allowed_fd;
	target.allowed_access = LANDLOCK_ACCESS_FS_EXECUTE;
	result = ll_add_rule(rset_fd, LANDLOCK_RULE_PATH_BENEATH, &target, 0);
	close(allowed_fd);
	return result;
}

void ll_basic_system(void) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return;
	}

	if (rset_fd == -1)
		rset_fd = ll_create_full_ruleset();

	assert(cfg.homedir);
	int home_fd = open(cfg.homedir, O_PATH | O_CLOEXEC);
	struct landlock_path_beneath_attr target;
	target.parent_fd = home_fd;
	target.allowed_access = LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
				LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_FILE |
				LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_MAKE_CHAR |
				LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |
				LANDLOCK_ACCESS_FS_MAKE_SYM;
	if (ll_add_rule(rset_fd, LANDLOCK_RULE_PATH_BENEATH, &target, 0))
		fprintf(stderr, "Error: cannot set the basic Landlock filesystem\n");
	close(home_fd);

	char *rundir;
	if (asprintf(&rundir, "/run/user/%d", getuid()) == -1)
		errExit("asprintf");

	if (ll_read("/") ||	// whole system read
	    ll_special("/") ||	// sockets etc.

	    ll_write("/tmp") ||    // write access
	    ll_write("/dev") ||
	    ll_write("/run/shm") ||
	    ll_write(rundir) ||

	    ll_exec("/opt") ||    // exec access
	    ll_exec("/bin") ||
	    ll_exec("/sbin") ||
	    ll_exec("/bin") ||
	    ll_exec("/lib") ||
	    ll_exec("/lib32") ||
	    ll_exec("/libx32") ||
	    ll_exec("/lib64") ||
	    ll_exec("/usr/bin") ||
	    ll_exec("/usr/sbin") ||
	    ll_exec("/usr/games") ||
	    ll_exec("/usr/lib") ||
	    ll_exec("/usr/lib32") ||
	    ll_exec("/usr/libx32") ||
	    ll_exec("/usr/lib64") ||
	    ll_exec("/usr/local/bin") ||
	    ll_exec("/usr/local/sbin") ||
	    ll_exec("/usr/local/games") ||
	    ll_exec("/usr/local/lib") ||
	    ll_exec("/run/firejail")) // appimage and various firejail features
		fprintf(stderr, "Error: cannot set the basic Landlock filesystem\n");

	free(rundir);
}

int ll_restrict(__u32 flags) {
	if (old_kernel()) {
		fprintf(stderr, "Warning: Landlock not enabled, a 6.1 or newer Linux kernel is required\n");
		return 0;
	}

	LandlockEntry *ptr = cfg.lprofile;
	while (ptr) {
		char *fname = NULL;
		int (*fnc)(char *) = NULL;

		if (strncmp(ptr->data, "landlock.read", 13) == 0) {
			fname = ptr->data + 14;
			fnc = ll_read;
		}
		else if (strncmp(ptr->data, "landlock.write", 14) == 0) {
			fname = ptr->data + 15;
			fnc = ll_write;
		}
		else if (strncmp(ptr->data, "landlock.special", 16) == 0) {
			fname = ptr->data + 17;
			fnc = ll_special;
		}
		else if (strncmp(ptr->data, "landlock.execute", 16) == 0) {
			fname = ptr->data + 17;
			fnc = ll_exec;
		}
		else
			assert(0);

		if (access(fname, F_OK) == 0) {
			if (fnc(fname))
				fprintf(stderr,"Error: failed to add Landlock rule for %s\n", fname);
		}

		ptr = ptr->next;
	}


	if (rset_fd == -1)
		return 0;

	prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	int result = landlock_restrict_self(rset_fd, flags);
	if (result != 0) return result;
	else {
		close(rset_fd);
		return 0;
	}
}

void ll_add_profile(const char *data) {
	if (old_kernel())
		return;
	LandlockEntry *ptr = malloc(sizeof(LandlockEntry));
	if (!ptr)
		errExit("malloc");
	memset(ptr, 0, sizeof(LandlockEntry));
	ptr->data = strdup(data);
	if (!ptr->data)
       		errExit("strdup");
//printf("add profile #%s#\n", ptr->data);
	ptr->next = cfg.lprofile;
	cfg.lprofile=ptr;
}

#endif
