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
#define NFLBITS  020
#define NPASSTST 021
#define NCOMPARE 014
#define NSEND    006
#define NRETRN   010
#define NSTOP    027
#define NFRZ     033
#define N2RETRN  011
#define NNO3CD   043
#define NTESTSEG 000
#define NBGN     045
#define NMICRO   005
#define NZERO_ER 012
#define NMASK    013
#define NBEGIN   001
#define NLP_END  002

#define OP(loop, narg, param, inst) (loop<<15 | narg<<13 | param<<6 | inst)

int testno, testseg;

uint16_t test1_cmch[] = {
	0145, // transplanted test header, for test #1
	0400, // segment 4
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
	020005, 0125342,
	000012,
	013710,
	// page 17
	040013, 0,01777,
	054014, 0,01777,
	020017, 042,
	013710,
	040013, 017,0176000,
	054014, 017, 0176000,
	020017, 043,

	000012,
	013710,
	020014, 0,
	020017, 040,
	020114, 0,
	020017, 041,
	000021, // NPASSTST

//#if 0
//#endif
};

uint16_t test2_cbus[] = {
	// test 2
	0245,
	// test 2, segment 1
	0100,
	// page 28
	020005, 0114400,

	007710,

	050014, 0,0,
	020020, 1,
	// page 29
	022414, 0,
	020017, 025,
	022514, 0,
	020017, 025,
	// page 30
	021, // NPASSTST
};

uint16_t test3_cclock[] = {
	0345,
	0100,
	021,
	// xxx total fiction
	020017, 0,
};

uint16_t test4_cinitial[] = {
	// test 4 - initialization
	// page 48
	0445,
	// page 49
	0100, // segment 1
	020005, 025750,
	020005, 023712,
	016710,
	050014, 017,0177777,
	020017, 01,
	// page 50
	022414, 01,
	020017, 02,
	022514, 01,
	020017, 02,
	013710,
	020414, 0,
	// page 51
	020017, 063,
	020005, 025712,
	016710,
	050014, 0,0,
	020017, 03,
	022414, 01,
	// page 52
	020017, 04,
	022514, 01,
	020017, 04,
	013710,
	020414, 0,
	020017, 063,

	// segment 2
	// page 53
	0200,
	02301, // NLP_BEGIN

	0120005,
	0114523,
	0114525,
	0114526,
	0114531,
	0114532,
	0114534,
	0114543,
	0114545,
	0114546,
	0114551,
	0114552,
	0114554,
	0114507,
	0114513,
	0114515,
	0114516,
	0114607,
	0114465,
	0114463,

	007710,
	// page 54
	050014, 0,0,
	020020, 05,
	022414, 01,
	020017, 031,
	022514, 01,
	// page 55
	020017, 031,
	040002, 0, 015215-015152, // end of loop

	016711,
	040013, 017,074377,
	050014, 02,040006,

	// page 56
	020020, 05,

	// page 57
	// segment 3
	0300,
	001101,
	0120005,
	0114626,
	0114625,
	0114706,
	0114634,
	0114643,
	0114645,
	0114646,
	0114456,

	007710,
	050014, 0,0,
	// page 58
	020020, 032,
	022414, 01,
	020017, 056,
	022514, 01,
	020017, 056,
	// page 59
	040002, 0, 0,015263-015232,

	// page 60
	// segment 4
	021,
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

	run_test(test1_cmch);
	run_test(test2_cbus);
	run_test(test3_cclock);
	run_test(test4_cinitial);
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
	if ((*w & M_LOOP) == M_LOOP) {
		if (narg == 1) ret = (uint32_t)w[loop_var];
		if (narg == 2) ret = (uint32_t)(w[loop_var*2])<<16 |
						   (uint32_t)(w[loop_var*2+1]);
	} else {
		if (narg == 1) ret = (uint32_t)w[0];
		if (narg ==2) ret =  (uint32_t)(w[0])<<16 |
						   (uint32_t)(w[1]);
	}

	printf("narg=%d so arg=%o\n", narg, ret);
	return ret;
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

void
unimplemented(int instr)
{
	printf("*** UNIMPLEMENTED: %o [test %d seg %d] ***\n",
		   instr, testno, testseg);
	assert(false);
}

int
run_test(uint16_t* test)
{
	int loc = 0;
	int loop_count = 1;
	int loop_var = 0;
	int loop_begin;
	uint32_t mchb;

	//sleep(1);
	printf("running another test ...");

	while (true) {
		uint16_t w = (test[loc]);
		bool loop = (w & M_LOOP) == M_LOOP;
		int narg = (w & M_NARG) >> 13;
		int param = (w & M_PARM) >> 6;
		uint32_t arg = getarg(loop_var, &test[loc]);
		printf("exec: %o\n", w);
		switch (w & M_CMD) {
		case NFAILTST:
			puts("nfailtest");
			printf("*** TEST %d-%d FAILED: TROUBLE # %d ***\n",
				   testno, testseg,
				   100*testno + arg);
			printf(" online MCHB: %o\n", mchb);
			if (param == 0) {
				return arg;
			}
			unimplemented(w);
			break;
		case NFLBITS:
			puts("nflbits");
			// individual trouble number is arg + ordinal of lowest set bit in mchb
			printf("*** TEST %d-%d FAILED: TROUBLE # %d ***\n",
				   testno, testseg,
				   100*testno + arg + ffs(mchb));
			printf(" online MCHB: %o\n", mchb);
			return arg + ffs(mchb);
			break;
		case NPASSTST:
			printf("*** TEST %d SEGMENT %d PASSED ***\n", testno, testseg);
			return 0;
		case NCOMPARE:
			puts("ncompare");
		{ // param bitfield is A,BCC,CCC
			int A = param & 0100; // what to compare
			int B = param & 0040;
			int C = param & 0037;
			bool compare;
			printf(" a=%o b=%o c=%o\n", A, B, C);
			if (A == 0100) { // A==1, whole register
				compare = ((mchb & M_R20) == (arg & M_R20));
				printf(" compare %o==%o? &%o, %d\n", mchb, arg, mchb&arg, compare);
			} else { // A == 0, single bit
				compare = (((mchb>>C) & 1) == (arg & 1));
				printf(" compare bit[%d]: %d==%d, %d\n", C, (mchb>>C)&1, arg, compare);
			}
			if (B != 0) compare = !compare;

			if (compare) {
				printf(" compare true so skip %d*%d\n",
					   (loop?loop_count:1), narg);
				// if true then skip next table entry
				loc += 1 + (loop?loop_count:1)*narg;
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
			default: unimplemented(w);
			}
			break;
		case NRETRN:
			puts("nretrn");
			switch (param) {
			case 077:
				puts(" -mchb");
				mchb = mch_call(MCH_RTNMCHB, arg) >> 8; break;
			case 0173:
				puts(" -ar0");
				// pr-1c912-50 p61
				unimplemented(w);
				break;
			case 0167:
				puts(" -br0");
				// return br0

				// br0 =e2=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x99e2) >> 8;
				// mchb -> mchtr -> return
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			case 0137:
				puts(" -er");
				mchb = mch_call(MCH_RTNER, arg) >> 8; break;
			default: unimplemented(w);
			}
			break;

		case N2RETRN:
			puts("n2retrn");
			switch(param) {
			case 0167: // system status register
				puts(" -ss");
				mchb = mch_call(MCH_RTNSS, 0) >> 8;
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
		case NNO3CD:
			printf("*** THIS TEST WILL NEVER WORK ***\n");
			printf("*** NNO3CD commands require a full emulator ***\n");
			return -1;
		case NTESTSEG: testseg = param; break;
		case NBGN: testno = param; break;
		case NMICRO:
			puts("nmicro");
			mchb = mch_call(MCH_LDMIRL, arg) >> 8;
			break;
		case NZERO_ER:
			puts("nzero_er");
			mchb = mch_call(MCH_CLER, arg) >> 8;
			break;
		case NMASK:
			puts("nmask");
			mchb &= arg;
			break;
		case NBEGIN:
			puts("nbegin");
			loop_begin = loc+1;
			loop_count = param;
			loop_var = 0;
			break;
		case NLP_END:
			puts("nlp_end");
			puts("looping!!");
			if (loop_count == loop_var+1) {
				// done
				loop_count = 1;
				loop_var = 0;
				break;
			} else {
				loc = loop_begin;
				loop_var += 1;
				continue;
			}
			break;
		default:
			unimplemented(w);
			break;
		}

		loc += 1 + (loop?loop_count:1)*narg;
	}
}
