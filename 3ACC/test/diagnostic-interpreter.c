#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

#include "../3acc.h"

/*
 * small, partial interpreter for the table-driven diagnostics used in
 * the #3 ESS.
 */

/*
L NNB BBB BBB III III
 */

#define DEBUG false

#define M_LOOP 0100000
#define M_NARG 0060000
#define M_PARM 0017700
#define M_CMD  0000077

FILE* fd_cmd, * fd_resp;

pid_t simh_pid = 0;

void cleanup(void);

#define NFAILTST 017
#define NPASSTST 021
#define NCOMPARE 014
#define NSEND    006
#define NRETRN   010
#define NSTOP    027
#define NFRZ     033
#define N2RETRN  011

#define OP(loop, narg, param, inst) (loop<<15 | narg<<13 | param<<6 | inst)

uint16_t table[] = {
	// bits of test 1 segment 2
#if 0
	OP(0, 2, 077, NSEND), 017, 0137771, // send 07737771
	OP(0, 0, 077, NRETRN), // get offline mchb
	OP(0, 2, 0100, NCOMPARE), 017, 0137771, // compare to 07737771
	OP(0, 0, 0, NFAILTST),// failure
	OP(0, 0, 0, NPASSTST),// success
#endif
	// test 1 segment 4: cdgmch, page 15

#if 0
	// my shit transcriptions
	OP(0, 0, 0176, NSTOP), // set STOP bit (MSTOP command)
	OP(0, 1, 0, NFRZ), 00, // load and freeze offline MAR with zero
	OP(0, 0, 0167, N2RETRN), // xxx RTNSS get the SS register

	// check that the stop bit is clear
	021614, 0,
	//OP(0, 1, 016, NCOMPARE), 00,
	OP(0, 1, 0, NFAILTST), 036,

	// page 16
	OP(0, 0, 0176, NSTOP), // set STOP bit (MSTOP command)
	OP(0, 0, 0167, N2RETRN), // xxx RTNSS get the SS register

	// check that the stop bit is set
	021614, 01,
	OP(0, 1, 0, NFAILTST), 036,
#endif

	// actually, just the numbers
	// page 15
	017627,
	020033, 0,
	016711,
	021614, 0,
	020017, 036, // oct 36 = dec 30
	// page 16
	017627,
	016711,
	021614, 1,
	020017, 037,
	020005, 023712,
	002005, 0125342,
	000012,
	013710,
	// page 17
	040013, 0,01777,
	054014, 0,01777,
	020017, 042,
	013710,
	040013, 017,017600,
	020017, 043,

	000012,
	013710,
	020014, 0,
	020017, 040,
	020114, 0,
	020017, 041,
	000021, // NPASSTST
};

int
main(int argc, char** argv)
{
	int fd;

#if FILE_TABLE
	if (argc == 1) {
		printf("need an argument, of filename contain test to run\n");
		return -1;
	}
	fd = open((const char*)argv[1], O_RDONLY);

	// n.b. 1024 bytes max, so 512 words
	table = mmap(NULL, 1024, PROT_READ, MAP_SHARED, fd, 0);
#endif

	atexit(&cleanup);
	start_simh();

	return run_test(table);
}

void
cleanup(void)
{
	if (simh_pid != 0) {
		kill(9, simh_pid);
	}
}

int
start_simh(void)
{
	int cmds[2], resps[2];
	int err;

	err = pipe(cmds);
	assert(err == 0);
	err = pipe(resps);
	assert(err == 0);

	simh_pid = fork();
	if (simh_pid < 0) {
		// error
		return -1;
	} else if (simh_pid > 0) {
		// parent
		// retain the command-write fd
		close(cmds[0]);
		fd_cmd = fdopen(cmds[1], "w");
		// retain the response-read fd
		fd_resp = fdopen(resps[0], "r");
		close(resps[1]);
		return 0;
	} else if (simh_pid == 0) {
		// child
		dup2(cmds[0], STDIN_FILENO);
		close(cmds[1]);
		close(resps[0]);
		dup2(resps[1], STDOUT_FILENO);
		err = execl("../../BIN/3ess", "3ess", "<stdin>", NULL);
		perror("fork error");
	}
	return err; /* uhm.. i guess. */
}

static uint32_t
getarg(int loop_var, uint16_t* w)
{
	int narg = (w[0] & M_NARG) >> 13;
	uint32_t ret = 0;
	w++;
	if (narg == 1) ret = (uint32_t)w[loop_var];
	if (narg == 2) ret = (uint32_t)(w[loop_var*2])<<16 |
	                     (uint32_t)(w[loop_var*2+1]);
	return ret;
}

// eat whatever spew happens to be lying around
void
swallow(void)
{
	char tmp[1000];
	fgets((char*)&tmp, 1000, fd_resp);
	printf("%s\n", &tmp);
	return;
}

uint32_t
mch_call(uint32_t cmd, uint32_t data)
{
	char *msg_text = NULL;
	uint32_t message = cmd | data<<8;
	size_t len;
	uint32_t response;

	printf("mch call %x\n", message);
	len = fprintf(fd_cmd, "mch %x\n", message);
	fflush(fd_cmd);

	int num = 0;
	do {
		getline(&msg_text, &len, fd_resp);
		num = sscanf(msg_text, "mch: 0x%x", &response);
		if (num == 0) printf("%s", msg_text);
		free(msg_text); msg_text = NULL;
	} while (num == 0);
	printf("mch resp %x\n", response);

	return response;
}

int
run_test(uint16_t* test)
{
	int loc = 0;
	int loop_count = 1;
	int loop_var = 0;
	int loop_begin;
	uint32_t mchb;

	sleep(1);
	swallow();

	while (true) {
		uint16_t w = (test[loc]);
		int narg = (w & M_NARG) >> 13;
		int param = (w & M_PARM) >> 6;
		uint32_t arg = getarg(loop_var, &test[loc]);
		switch (w & M_CMD) {
		case NFAILTST:
			printf("*** TEST FAILED AT %d: trouble # %d ***\n", loc, arg);
			if (param == 0) {
				return arg;
			}
			assert(false);
			break;
		case NPASSTST:
			printf("*** TEST PASSED ***\n");
			return 0;
		case NCOMPARE:
			puts("ncompare");
		{ // param bitfield is A,BCC,CCC
			int A = param & 0100; // what to compare
			int B = param & 0040;
			int C = param & 0037;
			bool compare;
			if (A == 0100) { // whole register
				compare = (mchb == arg);
			} else { // A == 0, single bit
				compare = (((mchb>>C) & 1) == arg);
			}
			if (B != 0) {
				// figure out what this means
				assert(false);
			}

			if (compare) {
				// if true then skip next table entry
				loc += 1 + loop_count*narg;
				w = (test[loc]);
				narg = (w & M_NARG) >> 13;
			}
			break;
		}
		case NSEND:
			puts("nsend");
			switch (param) {
			case 077:
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				break;
			}
			break;
		case NRETRN:
			puts("nretrn");
			switch (param) {
			case 077:
				mchb = mch_call(MCH_RTNMCHB, arg) >> 8;
				break;
			}
			break;

		case N2RETRN:
			puts("n2retrn");
			switch(param) {
			case 0167: // system status register
				puts("-ss");
				mchb = mch_call(MCH_RTNSS, 0) << 8;
				break;
			}
			break;
		case NSTOP:
			puts("nstop");
			mchb = mch_call(MCH_MSTOP, 0);
			break;
		case NFRZ:
			puts("nfrz");
			mchb = mch_call(MCH_LDMAR, arg);
			break;
		default: assert(false);
		}

		loc += 1 + loop_count*narg;
	}
}
