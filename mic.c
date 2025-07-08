/*
 * mic -- microphone feedback for FreeBSD.
 *
 * Without any arguments specified, it'll feed your microphone back to the
 * speakers with no delay.
 *
 * In case you provide an arugument for that, it'll be treated as a floating-
 * point number, representing the delay for loopback.  Since the data obtained
 * from /dev/dsp device is a raw sound data, it takes a considerable space, so
 * the maximum delay is limited to 8 seconds, just not to consume lots of memory
 * of yours.
 */

#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Maximum input/output buffer size. */
#define MXBFSZ 4096
/* Delta for enlargning accumulator buffer. */
#define XPNDACC 300000
#define MAX_DELAY 8


int fd; /* /dev/dsp file descriptor. */
char ibu[MXBFSZ]; /* Input buffer. */
float dlya; /* Delay argument. */
ssize_t arb; /* Actually read bytes. */
char accm; /* Are we in accumulating mode. */
int acci; /* Current accumulator index (when unloading it). */
struct timeval st; /* Start time (for tracking delay). */

/*
 * Simple loop (without delay).
 */
void
simploop()
{
	while((arb = read(fd, &ibu, MXBFSZ)) > 0)
		write(fd, &ibu, arb);
}

/*
 * Check delay.
 */
void
ckdly()
{
	struct timeval now;
	unsigned long diffmc; /* Difference in microseconds. */

	gettimeofday(&now, NULL);
	diffmc = (now.tv_sec - st.tv_sec) * 1000000 + (now.tv_usec -
	    st.tv_usec);

	if (diffmc > dlya * 1000000)
		accm = 0;
}

/*
 * Buffered loop (with delay).
 */
void
bufloop()
{
	char* acc; /* Accumulator. */
	int accsz; /* Accumulator total size. */
	int accl; /* Actual length of accumulator. */

	acc = NULL;
	accsz = 0;
	accl = 0;
	accm = 1;
	acci = 0;

	gettimeofday(&st, NULL);
	while((arb = read(fd, &ibu, MXBFSZ)) > 0) {
		if (accm)
			ckdly();
		
		/*
		 * The above call to `ckdly' may change the `accm', so we need
		 * to have two identical if-s in a row.
		 */
		if (accm) {
			if (accl + arb > accsz)
				if ((acc = realloc(acc, accsz += XPNDACC)) ==
				    NULL)
					err(1, "realloc()");

			memcpy(acc + accl, &ibu, arb);
			accl += arb;
			continue;
		}

		/*
		 * At this point we want to start unloading accumulated data.
		 */
		
		if ((acc = realloc(acc, accsz = accl)) == NULL)
			err(1, "realloc()");

		if (acci + arb <= accsz) {
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
			memcpy(acc, &ibu + on, tw);
		}

		acci = (acci + arb) % accsz;
	}
}

int
main(int argc, char** argv)
{
	if (argc == 1)
		dlya = 0;
	else
		dlya = strtof(argv[1], NULL);

	if (dlya > MAX_DELAY)
		errx(2, "the delay is too big - mind your memory!");

	if ((fd = open("/dev/dsp", O_RDWR)) == -1)
		err(1, "open(/dev/dsp)");

	if (dlya == 0)
		simploop();
	else
		bufloop();

	return 0;
}
