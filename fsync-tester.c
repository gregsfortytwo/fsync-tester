/*
 * fsync-tester.c
 *
 * Written by Theodore Ts'o, 3/21/09.  Updated by Chris Mason to include
 * the random writer thread
 *
 * This file may be redistributed under the terms of the GNU Public
 * License, version 2.
 */
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>

#define SIZE (32768*32)
static char bigbuf[4096];

static float timeval_subtract(struct timeval *tv1, struct timeval *tv2)
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

static void random_io(int fd, loff_t total)
{
	loff_t cur = 0;
	int ret;

	/* just some constant so our runs are always the same */
	srand(4096);
	while(1) {
		/*
		 * we want a random offset into the file,
		 * but rand only returns max in.  So we make
		 * it a random block number instead, and multiply
		 * by 4096.
		 */
		cur = rand();
		cur = (cur * 4096) % (total - 4096);

		/* align our offset  to 4k */
		cur = cur / 4096;
		cur = cur * 4096;
		ret = pwrite(fd, bigbuf, 4096, cur);
		if (ret < 4096) {
			fprintf(stderr, "short write ret %d cur %llu\n",
				ret, (unsigned long long)cur);
			exit(1);
		}
	}
}

int main(int argc, char **argv)
{
	int	fd;
	struct timeval tv, tv2, start;
	char buf[SIZE];
	pid_t pid;
	loff_t total = ((loff_t)256) * 1024 * 1024;
	loff_t cur = 0;
	int rand_fd;
	int ret;
	int i;
	int status;
	struct stat st;

	memset(bigbuf, 0, 4096);

	rand_fd = open("fsync-tester.rnd-file", O_WRONLY|O_CREAT);
	if (rand_fd < 0) {
		perror("open");
		exit(1);
	}

	ret = fstat(rand_fd, &st);
	if (ret < 0) {
		perror("fstat");
		exit(1);
	}

	if (st.st_size < total) {
		printf("setting up random write file\n");
		while(cur < total) {
			ret = write(rand_fd, bigbuf, 4096);
			if (ret <= 0) {
				fprintf(stderr, "short write\n");
				exit(1);
			}
			cur += ret;
		}
		printf("done setting up random write file\n");
	}

	fd = open("fsync-tester.tst-file", O_WRONLY|O_CREAT);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	memset(buf, 'a', SIZE);

	pid = fork();
	if (pid == 0) {
		printf("starting random io!\n");
		random_io(rand_fd, total);
		exit(0);
	}

	close(rand_fd);

	gettimeofday(&start, NULL);
	printf("starting fsync run\n");
	for(i = 0; i < 60; i++) {
		float pwrite_time;

		gettimeofday(&tv2, NULL);
		pwrite(fd, buf, SIZE, 0);
		gettimeofday(&tv, NULL);

		pwrite_time = timeval_subtract(&tv, &tv2);

		fsync(fd);
		gettimeofday(&tv2, NULL);

		printf("write time: %5.4fs fsync time: %5.4fs\n", pwrite_time,
		       timeval_subtract(&tv2, &tv));

		if (timeval_subtract(&tv2, &start) > 60)
			break;
		sleep(4);
	}
	printf("run done %d fsyncs total, killing random writer\n", i + 1);
	fflush(stdout);
	kill(pid, SIGTERM);
	wait(&status);

	return 0;
}




