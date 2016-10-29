/*
 * lxc: linux Container library
 *
 * Copyright © 2016 Canonical Ltd.
 *
 * Authors:
 * Christian Brauner <christian.brauner@mailbox.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lxctest.h"
#include "utils.h"

void test_lxc_deslashify(void)
{
	char *s = strdup("/A///B//C/D/E/");
	if (!s)
		exit(EXIT_FAILURE);
	lxc_test_assert_abort(lxc_deslashify(&s));
	lxc_test_assert_abort(strcmp(s, "/A/B/C/D/E") == 0);
	free(s);

	s = strdup("/A");
	if (!s)
		exit(EXIT_FAILURE);
	lxc_test_assert_abort(lxc_deslashify(&s));
	lxc_test_assert_abort(strcmp(s, "/A") == 0);
	free(s);

	s = strdup("");
	if (!s)
		exit(EXIT_FAILURE);
	lxc_test_assert_abort(lxc_deslashify(&s));
	lxc_test_assert_abort(strcmp(s, "") == 0);
	free(s);

	s = strdup("//");
	if (!s)
		exit(EXIT_FAILURE);
	lxc_test_assert_abort(lxc_deslashify(&s));
	lxc_test_assert_abort(strcmp(s, "/") == 0);
	free(s);
}

void test_detect_ramfs_rootfs(void)
{
	size_t i;
	int ret;
	int fret = EXIT_FAILURE;
	size_t len = 5 /* /proc */ + 21 /* /int_as_str */ + 7 /* /ns/mnt */ + 1 /* \0 */;
	char path[len];
	int init_ns = -1;
	char tmpf1[] = "lxc-test-utils-XXXXXX";
	char tmpf2[] = "lxc-test-utils-XXXXXX";
	int fd1 = -1, fd2 = -1;
	FILE *fp1 = NULL, *fp2 = NULL;
	char *mountinfo[] = {
		"18 24 0:17 / /sys rw,nosuid,nodev,noexec,relatime shared:7 - sysfs sysfs rw",
		"19 24 0:4 / /proc rw,nosuid,nodev,noexec,relatime shared:13 - proc proc rw",
		"20 24 0:6 / /dev rw,nosuid,relatime shared:2 - devtmpfs udev rw,size=4019884k,nr_inodes=1004971,mode=755",
		"21 20 0:14 / /dev/pts rw,nosuid,noexec,relatime shared:3 - devpts devpts rw,gid=5,mode=620,ptmxmode=000",
		"22 24 0:18 / /run rw,nosuid,noexec,relatime shared:5 - tmpfs tmpfs rw,size=807912k,mode=755",

		/* This is what we care about. */
		"24 0 8:2 / / rw - rootfs rootfs rw,size=1004396k,nr_inodes=251099",

		"25 18 0:12 / /sys/kernel/security rw,nosuid,nodev,noexec,relatime shared:8 - securityfs securityfs rw",
		"26 20 0:20 / /dev/shm rw,nosuid,nodev shared:4 - tmpfs tmpfs rw",
		"27 22 0:21 / /run/lock rw,nosuid,nodev,noexec,relatime shared:6 - tmpfs tmpfs rw,size=5120k",
		"28 18 0:22 / /sys/fs/cgroup ro,nosuid,nodev,noexec shared:9 - tmpfs tmpfs ro,mode=755",
		"29 28 0:23 / /sys/fs/cgroup/systemd rw,nosuid,nodev,noexec,relatime shared:10 - cgroup cgroup rw,xattr,release_agent=/lib/systemd/systemd-cgroups-agent,name=systemd",
		"30 18 0:24 / /sys/fs/pstore rw,nosuid,nodev,noexec,relatime shared:11 - pstore pstore rw",
		"31 18 0:25 / /sys/firmware/efi/efivars rw,nosuid,nodev,noexec,relatime shared:12 - efivarfs efivarfs rw",
		"32 28 0:26 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid,nodev,noexec,relatime shared:14 - cgroup cgroup rw,cpu,cpuacct",
		"33 28 0:27 / /sys/fs/cgroup/net_cls,net_prio rw,nosuid,nodev,noexec,relatime shared:15 - cgroup cgroup rw,net_cls,net_prio",
		"34 28 0:28 / /sys/fs/cgroup/blkio rw,nosuid,nodev,noexec,relatime shared:16 - cgroup cgroup rw,blkio",
		"35 28 0:29 / /sys/fs/cgroup/freezer rw,nosuid,nodev,noexec,relatime shared:17 - cgroup cgroup rw,freezer",
		"36 28 0:30 / /sys/fs/cgroup/memory rw,nosuid,nodev,noexec,relatime shared:18 - cgroup cgroup rw,memory",
		"37 28 0:31 / /sys/fs/cgroup/hugetlb rw,nosuid,nodev,noexec,relatime shared:19 - cgroup cgroup rw,hugetlb",
		"38 28 0:32 / /sys/fs/cgroup/cpuset rw,nosuid,nodev,noexec,relatime shared:20 - cgroup cgroup rw,cpuset",
		"39 28 0:33 / /sys/fs/cgroup/devices rw,nosuid,nodev,noexec,relatime shared:21 - cgroup cgroup rw,devices",
		"40 28 0:34 / /sys/fs/cgroup/pids rw,nosuid,nodev,noexec,relatime shared:22 - cgroup cgroup rw,pids",
		"41 28 0:35 / /sys/fs/cgroup/perf_event rw,nosuid,nodev,noexec,relatime shared:23 - cgroup cgroup rw,perf_event",
		"42 19 0:36 / /proc/sys/fs/binfmt_misc rw,relatime shared:24 - autofs systemd-1 rw,fd=32,pgrp=1,timeout=0,minproto=5,maxproto=5,direct",
		"43 18 0:7 / /sys/kernel/debug rw,relatime shared:25 - debugfs debugfs rw",
		"44 20 0:37 / /dev/hugepages rw,relatime shared:26 - hugetlbfs hugetlbfs rw",
		"45 20 0:16 / /dev/mqueue rw,relatime shared:27 - mqueue mqueue rw",
		"46 43 0:9 / /sys/kernel/debug/tracing rw,relatime shared:28 - tracefs tracefs rw",
		"76 18 0:38 / /sys/fs/fuse/connections rw,relatime shared:29 - fusectl fusectl rw",
		"78 24 8:1 / /boot/efi rw,relatime shared:30 - vfat /dev/sda1 rw,fmask=0077,dmask=0077,codepage=437,iocharset=iso8859-1,shortname=mixed,errors=remount-ro",
	};

	ret = snprintf(path, len, "/proc/self/ns/mnt");
	if (ret < 0 || (size_t)ret >= len) {
		lxc_error("%s\n", "Failed to create path with snprintf().");
		goto non_test_error;
	}

	init_ns = open(path, O_RDONLY | O_CLOEXEC);
	if (init_ns < 0) {
		lxc_error("%s\n", "Failed to open initial mount namespace.");
		goto non_test_error;
	}

	if (unshare(CLONE_NEWNS) < 0) {
		lxc_error("%s\n", "Could not unshare mount namespace.");
		close(init_ns);
		init_ns = -1;
		goto non_test_error;
	}

	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0) < 0) {
		lxc_error("Failed to remount / private: %s.\n", strerror(errno));
		goto non_test_error;
	}

	fd1 = mkstemp(tmpf1);
	if (fd1 < 0) {
		lxc_error("%s\n", "Could not create temporary file.");
		goto non_test_error;
	}

	fd2 = mkstemp(tmpf2);
	if (fd2 < 0) {
		lxc_error("%s\n", "Could not create temporary file.");
		goto non_test_error;
	}

	fp1 = fdopen(fd1, "r+");
	if (!fp1) {
		lxc_error("%s\n", "Could not fdopen() temporary file.");
		goto non_test_error;
	}

	fp2 = fdopen(fd2, "r+");
	if (!fp2) {
		lxc_error("%s\n", "Could not fdopen() temporary file.");
		goto non_test_error;
	}

	/* Test if it correctly detects - rootfs rootfs */
	for (i = 0; i < sizeof(mountinfo) / sizeof(mountinfo[0]); i++) {
		if (fprintf(fp1, "%s\n", mountinfo[i]) < 0) {
			lxc_error("Could not write \"%s\" to temporary file.", mountinfo[i]);
			goto non_test_error;
		}
	}
	fclose(fp1);
	fp1 = NULL;

	/* Test if it correctly fails to detect when no - rootfs rootfs */
	for (i = 0; i < sizeof(mountinfo) / sizeof(mountinfo[0]); i++) {
		if (strcmp(mountinfo[i], "24 0 8:2 / / rw - rootfs rootfs rw,size=1004396k,nr_inodes=251099") == 0)
			continue;
		if (fprintf(fp2, "%s\n", mountinfo[i]) < 0) {
			lxc_error("Could not write \"%s\" to temporary file.", mountinfo[i]);
			goto non_test_error;
		}
	}
	fclose(fp2);
	fp2 = NULL;

	if (mount(tmpf1, "/proc/self/mountinfo", NULL, MS_BIND, 0) < 0) {
		lxc_error("%s\n", "Could not overmount \"/proc/self/mountinfo\".");
		goto non_test_error;
	}

	lxc_test_assert_abort(detect_ramfs_rootfs());

	if (mount(tmpf2, "/proc/self/mountinfo", NULL, MS_BIND, 0) < 0) {
		lxc_error("%s\n", "Could not overmount \"/proc/self/mountinfo\".");
		goto non_test_error;
	}

	lxc_test_assert_abort(!detect_ramfs_rootfs());
	fret = EXIT_SUCCESS;

non_test_error:
	if (fp1)
		fclose(fp1);
	else if (fd1 > 0)
		close(fd1);
	if (fp2)
		fclose(fp2);
	else if (fd2 > 0)
		close(fd2);

	if (init_ns > 0) {
		if (setns(init_ns, 0) < 0) {
			lxc_error("Failed to switch back to initial mount namespace: %s.\n", strerror(errno));
			fret = EXIT_FAILURE;
		}
		close(init_ns);
	}
	if (fret == EXIT_SUCCESS)
		return;
	exit(fret);
}

void test_lxc_safe_uint(void)
{
	int ret;
	unsigned int n;
	size_t len = /* 2^64 = 21 - 1 */ 21;
	char uint_max[len];

	ret = snprintf(uint_max, len, "%lu", (unsigned long)UINT_MAX + 1);
	if (ret < 0 || (size_t)ret >= len) {
		lxc_error("%s\n", "Failed to create string via snprintf().");
		exit(EXIT_FAILURE);
	}

	lxc_test_assert_abort((0 == lxc_safe_uint("1234345", &n)) && n == 1234345);
	lxc_test_assert_abort((0 == lxc_safe_uint("   345", &n)) && n == 345);
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("   g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("   3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("   345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("g345   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("3g45   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("345g   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_uint("   g345", &n)));
	lxc_test_assert_abort((-ERANGE == lxc_safe_uint(uint_max, &n)));
}

void test_lxc_safe_int(void)
{
	int ret;
	signed int n;
	size_t len = /* 2^64 = 21 - 1 */ 21;
	char int_max[len];

	ret = snprintf(int_max, len, "%ld", (signed long)INT_MAX + 1);
	if (ret < 0 || (size_t)ret >= len) {
		lxc_error("%s\n", "Failed to create string via snprintf().");
		exit(EXIT_FAILURE);
	}

	lxc_test_assert_abort((0 == lxc_safe_int("1234345", &n)) && n == 1234345);
	lxc_test_assert_abort((0 == lxc_safe_int("   345", &n)) && n == 345);
	lxc_test_assert_abort((0 == lxc_safe_int("-1234345", &n)) && n == -1234345);
	lxc_test_assert_abort((0 == lxc_safe_int("   -345", &n)) && n == -345);
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("   g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("   3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("   345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("g345   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("3g45   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("345g   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_int("   g345", &n)));
	lxc_test_assert_abort((-ERANGE == lxc_safe_int(int_max, &n)));
}

void test_lxc_safe_long(void)
{
	int ret;
	signed long int n;
	size_t len = /* 2^64 = 21 - 1 */ 21;
	char long_max[len];

	ret = snprintf(long_max, len, "%lld", LLONG_MAX);
	if (ret < 0 || (size_t)ret >= len) {
		lxc_error("%s\n", "Failed to create string via snprintf().");
		exit(EXIT_FAILURE);
	}

	lxc_test_assert_abort((0 == lxc_safe_long("1234345", &n)) && n == 1234345);
	lxc_test_assert_abort((0 == lxc_safe_long("   345", &n)) && n == 345);
	lxc_test_assert_abort((0 == lxc_safe_long("-1234345", &n)) && n == -1234345);
	lxc_test_assert_abort((0 == lxc_safe_long("   -345", &n)) && n == -345);
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("   g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("   3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("   345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("g345", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("3g45", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("345g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("g345   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("3g45   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("345g   ", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("g", &n)));
	lxc_test_assert_abort((-EINVAL == lxc_safe_long("   g345", &n)));
	if (LONG_MAX != LLONG_MAX)
		lxc_test_assert_abort((-ERANGE == lxc_safe_long(long_max, &n)));
	else
		lxc_test_assert_abort((0 == lxc_safe_long(long_max, &n)) && n == LONG_MAX);
}

void test_lxc_string_replace(void)
{
	char *s;

	s = lxc_string_replace("A", "A", "A");
	lxc_test_assert_abort(strcmp(s, "A") == 0);
	free(s);

	s = lxc_string_replace("A", "AA", "A");
	lxc_test_assert_abort(strcmp(s, "AA") == 0);
	free(s);

	s = lxc_string_replace("A", "AA", "BA");
	lxc_test_assert_abort(strcmp(s, "BAA") == 0);
	free(s);

	s = lxc_string_replace("A", "AA", "BAB");
	lxc_test_assert_abort(strcmp(s, "BAAB") == 0);
	free(s);

	s = lxc_string_replace("AA", "A", "AA");
	lxc_test_assert_abort(strcmp(s, "A") == 0);
	free(s);

	s = lxc_string_replace("AA", "A", "BAA");
	lxc_test_assert_abort(strcmp(s, "BA") == 0);
	free(s);

	s = lxc_string_replace("AA", "A", "BAAB");
	lxc_test_assert_abort(strcmp(s, "BAB") == 0);
	free(s);

	s = lxc_string_replace("\"A\"A", "\"A\"", "B\"A\"AB");
	lxc_test_assert_abort(strcmp(s, "B\"A\"B") == 0);
	free(s);
}

void test_lxc_string_in_array(void)
{
	lxc_test_assert_abort(lxc_string_in_array("", (const char *[]){"", NULL}));
	lxc_test_assert_abort(!lxc_string_in_array("A", (const char *[]){"", NULL}));
	lxc_test_assert_abort(!lxc_string_in_array("AAA", (const char *[]){"", "3472", "jshH", NULL}));

	lxc_test_assert_abort(lxc_string_in_array("A", (const char *[]){"A", NULL}));
	lxc_test_assert_abort(lxc_string_in_array("A", (const char *[]){"A", "B", "C", NULL}));
	lxc_test_assert_abort(lxc_string_in_array("A", (const char *[]){"B", "A", "C", NULL}));

	lxc_test_assert_abort(lxc_string_in_array("ABC", (const char *[]){"ASD", "ATR", "ABC", NULL}));
	lxc_test_assert_abort(lxc_string_in_array("GHJ", (const char *[]){"AZIU", "WRT567B", "879C", "GHJ", "IUZ89", NULL}));
	lxc_test_assert_abort(lxc_string_in_array("XYZ", (const char *[]){"BERTA", "ARQWE(9", "C8Zhkd", "7U", "XYZ", "UOIZ9", "=)()", NULL}));
}

int main(int argc, char *argv[])
{
	test_lxc_string_replace();
	test_lxc_string_in_array();
	test_lxc_deslashify();
	test_detect_ramfs_rootfs();
	test_lxc_safe_uint();
	test_lxc_safe_int();
	test_lxc_safe_long();

	exit(EXIT_SUCCESS);
}
