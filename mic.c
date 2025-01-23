/*
	mic -- loop microphone back to speakes in FreeBSD.
*/


#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/time.h>
#include<string.h>
#include<stdio.h>


/* maximum input/output buffer size. */
#define MXBFSZ 4096
/* delta for enlargning accumulator buffer. */
#define XPNDACC 300000


/* /dev/dsp file descriptor. */
int fd;
/* input buffer. */
char ibu[MXBFSZ];
/* delay argument. */
float dlya;
/* actually read bytes. */
ssize_t arb;
/* are we in accumulating mode. */
char accm;
/* current accumulator index (when unloading it). */
int acci;
/* start time (for tracking delay). */
struct timeval st;


/* simple loop (without delay). */
void
simploop() {
	while((arb = read(fd, &ibu, MXBFSZ)) > 0) {
		write(fd, &ibu, arb);
	}
}

/* check delay. */
void
ckdly() {
	struct timeval now;

	/* diff in microseconds. */
	unsigned long diffmc;

	gettimeofday(&now, NULL);
	diffmc = (now.tv_sec - st.tv_sec)*1000000
	         + (now.tv_usec - st.tv_usec);

	if (diffmc > dlya*1000000) {
		accm = 0;
	}
}

/* buffered loop (with delay). */
void
bufloop() {
	/* accumulator. */
	char* acc;
	/* accumulator total size. */
	int accsz;
	/* actual length of accumulator. */
	int accl;

	acc = NULL;
	accsz = 0;
	accl = 0;
	accm = 1;
	acci = 0;

	gettimeofday(&st, NULL);
	while((arb = read(fd, &ibu, MXBFSZ)) > 0) {
		if (accm) {
			ckdly();
		}

		if (accm) {
			if (accl + arb > accsz) {
				acc = realloc(acc, accsz += XPNDACC);
				if (!acc) {
					dprintf(2, "[mic]: can't realloc buffer.\n");
					exit(1);
				}
			}

			memcpy(acc+accl, &ibu, arb);
			accl += arb;
			continue;
		}

		/* at this point we want to start unloading accumulated data. */
		acc = realloc(acc, accsz=accl);
		if (!acc) {
			dprintf(2, "[mic]: can not shrink accumulator.\n");
			exit(1);
		}

		if (acci+arb <= accsz) {
			write(fd, acc+acci, arb);
			memcpy(acc+acci, &ibu, arb);
		}
		else {
			int on;
			int tw;

			on = accsz - acci;
			tw = arb - on;

			write(fd, acc+acci, on);
			write(fd, acc, tw);

			memcpy(acc+acci, &ibu, on);
			memcpy(acc, &ibu+on, tw);
		}

		acci = (acci + arb) % accsz;
	}
}

int
main(int argc, char** argv) {
	if (argc == 1) {
		dlya = 0;
	}
	else {
		dlya = strtof(argv[1], NULL);
	}

	if (dlya > 8) {
		dprintf(2, "[mic]: the delay is too big - mind your memory!\n");
		return 1;
	}

	fd = open("/dev/dsp", O_RDWR);
	if (fd == -1) {
		dprintf(2, "[mic]: can not open /dev/dsp device.\n");
		return 1;
	}

	if (!dlya) {
		simploop();
	}
	else {
		bufloop();
	}

	return 0;
}
