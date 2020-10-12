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
 * Small, partial interpreter for the table-driven diagnostics used in
 * the #3 ESS.  This allows me to use the machine's own diagnostics as
 * unit tests for the emulator.
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

void init(void);
void cleanup(void);

#define NTESTSEG 000
#define NBEGIN   001
#define NLP_END  002
#define NTMTEST    3
#define NPROS      4
#define NMICRO   005
#define NSEND    006
#define N2SEND   007
#define NRETRN   010
#define N2RETRN  011
#define NZERO_ER 012
#define NMASK    013
#define NCOMPARE 014
#define NEXECUTE  13
#define NTTY      14
#define NFAILTST 017
#define NFLBITS  020
#define NPASSTST 021
#define NADDOFF   18
#define NADMIC    19
#define NLDMIR    20
#define NSKIP     21
#define NSTRT     22
#define NSTOP    027
#define ZERPT     24
#define NDISABA   25
#define NDISABB   26
#define NFRZ     033
#define NSTRTCLK  28
#define NSTPCLK   29
#define NRDMICRO  30
#define NTGCLK    31
#define NRESCLK   32
#define NABT      33
#define NCLRMS    34
#define NNO3CD   043
#define NRTNBLM   36
#define NBGN     045
#define NWDTST    38
#define NTST_LP   39
#define NDONE 077 // hack, ahistorical

#define OP(narg, param, inst) (narg<<13 | param<<6 | inst)

int testno, testseg;

#include "test1_cmch.h"
#include "test2_cbus.h"
#include "test3_cclock.h"
#include "test4_cinitial.h"
#include "test9_cdgreg.h"
#include "testdml_cdgfn.h"

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

	init();

	//run_test(test1_cmch);
	//run_test(test2_cbus);
	//run_test(test3_cclock);
	//run_test(test4_cinitial);
	//run_test(test9_cdgreg);
	run_test(test12_function_register);
}

/* instructions to run that yield a clean environment in the processor
 * for testing.  replaces unknown functionality in PR-1C910.
 */
// xxx todo: duplicated in nbgn, ugh.  clean that up.
uint16_t init_testseq[] = {
	/* */
	OP(2, 0, NSEND), (M_PH|M_PL)>>16, 0,
	OP(0,0,NDONE),
};

void
init(void)
{
	run_test(init_testseq);
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
	bool multiple = ((w[0] & M_LOOP) == M_LOOP);
	uint32_t ret = 0;
	w++;
	if (multiple) {
		if (narg == 1) ret = (uint32_t)w[loop_var];
		if (narg == 2) ret = (uint32_t)(w[loop_var*2])<<16 |
						   (uint32_t)(w[loop_var*2+1]);
		printf("-loop_var=%d\n", loop_var);
	} else {
		if (narg == 1) ret = (uint32_t)w[0];
		if (narg ==2) ret =  (uint32_t)(w[0])<<16 |
						   (uint32_t)(w[1]);
	}

	printf("-narg=%d so arg=%o\n", narg, ret);
	return ret;
}

uint32_t
mch_call(uint32_t cmd, uint32_t data)
{
	char *msg_text = NULL;
	uint32_t message = cmd | data<<8;
	size_t len;
	uint32_t response;

	printf("-mch call %x\n", message);
	len = fprintf(fd_cmd, "mch %x\n", message);
	fflush(fd_cmd);

	int num = 0;
	do {
		getline(&msg_text, &len, fd_resp);
		num = sscanf(msg_text, "mch: 0x%x", &response);
		if (num == 0) printf("%s", msg_text);
		free(msg_text); msg_text = NULL;
	} while (num == 0);
	printf("-mch resp %x\n", response);

	return response;
}

void
unimplemented(int instr)
{
	printf("-*** UNIMPLEMENTED: %o [test %d seg %d] ***\n",
		   instr, testno, testseg);
	//assert(false);
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
	printf("-running another test ...");

	while (true) {
		uint16_t w = (test[loc]);
		bool loop = (w & M_LOOP) == M_LOOP;
		int narg = (w & M_NARG) >> 13;
		int param = (w & M_PARM) >> 6;
		uint32_t arg;

		printf("-exec: %o ====================\n", w);
		arg = getarg(loop_var, &test[loc]);
		switch (w & M_CMD) {
		case NFAILTST: // fail
			puts("-nfailtest");
			printf("-*** TEST %d-%d FAILED: TROUBLE # %d *** (arg %o)\n",
				   testno, testseg,
				   100*testno + arg, arg);
			printf("- online MCHB: %o\n", mchb);
			if (param == 0) {
				return arg;
			}
			unimplemented(w);
			break;
		case NFLBITS: // fail bitslice
			puts("-nflbits");
			// individual trouble number is arg + ordinal of lowest set bit in mchb
			printf("-*** TEST %d-%d FAILED: TROUBLE # %d ***\n",
				   testno, testseg,
				   100*testno + arg + ffs(mchb));
			printf("- online MCHB: %o\n", mchb);
			return arg + ffs(mchb);
			break;
		case NPASSTST:
			printf("-*** TEST %d SEGMENT %d PASSED ***\n", testno, testseg);
			return 0;
		case NCOMPARE:
			puts("-ncompare");
		{ // param bitfield is A,BCC,CCC
			int A = param & 0100; // what to compare
			int B = param & 0040;
			int C = param & 0037;
			bool compare;
			printf("- a=%o b=%o c=%o\n", A, B, C);
			if (A == 0100) { // A==1, whole register.
				// Parity bits not compared.
				compare = ((mchb & M_R20) == (arg & M_R20));
				printf("- compare %o==%o? &%o, %d\n", mchb, arg, mchb&arg, compare);
			} else { // A == 0, single bit
				compare = (((mchb>>C) & 1) == (arg & 1));
				printf("- compare bit[%d]: %d==%d, %d\n", C, (mchb>>C)&1, arg, compare);
			}
			if (B != 0) compare = !compare;

			if (compare) {
				printf(" compare true so skip %d*%d\n",
					   (loop?loop_count:1), narg);
				// if true then skip next table entry
				loc += 1 + (loop?loop_count:1)*narg;

				// same as loop header, sigh
				w = (test[loc]);
				loop = (w & M_LOOP) == M_LOOP;
				narg = (w & M_NARG) >> 13;
				param = (w & M_PARM) >> 6;
			}
			break;
		}
		case NSEND:
			puts("-nsend");
			switch (param) {
			case 077:
				puts("- -mchb");
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				break;
			case 0157:
				// AR0+1, ref PR-1c915-50 p.19 l.20
				puts("- -ar");
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				// mchb =99=> gb =b1=> ar
				mchb = mch_call(MCH_LDMIRL, 0xb199) >> 8;
				break;
			case 0167:
				puts("- -br");
				// BR0+1, ref PR-1c917-50 p.7 l.35
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				// mchb =99=> gb =71=> br
				mchb = mch_call(MCH_LDMIRL, 0x7199) >> 8;
				break;
			case 0173:
				// CR, ref PR-1c915-50 p.19 l.14
				puts("- -cr");
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				// mchb =99=> gb =33=> cr
				mchb = mch_call(MCH_LDMIRL, 0x3399) >> 8;
				break;
			case 0175:
				puts("- -fr");
				// FR, ref PR-1C917-50 p.10
				// both duplicate registers
				mchb = mch_call(MCH_LDMCHB, arg) >> 8;
				// mchb =99=> gb =17=> RAR
				mchb = mch_call(MCH_LDMIRL, 0x1799) >> 8;
				// rar =d83a=> fn
				mchb = mch_call(MCH_LDMIRL, 0xd83a) >> 8;
				break;
			default: unimplemented(param);
			}
			break;
		case NRETRN:
			puts("-nretrn");
			switch (param) {
			case 077: // MCHB
				puts("- -mchb");
				mchb = mch_call(MCH_RTNMCHB, arg) >> 8;
				break;
			case 0137: // ER
				puts("- -er");
				mchb = mch_call(MCH_RTNER, arg) >> 8;
				break;
			case 0157: // CR
				// cite pr-1c915-50 p.19 l.33
				puts("- -cr");
				// cr0 =33=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x9933) >> 8;
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			case 0167: // BR
				puts("- -br0");
				// return br0

				// br0 =e2=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x99e2) >> 8;
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			case 0173: // AR
				puts("- -ar0");
				// pr-1c912-50 p61
				// ar0 =b1=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x99b1) >> 8;
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			case 0175: // DML1
				puts("- -dml1");
				// pr-1c917-50 p.8 l.42
				// dml1 =b836=> cr
				mchb = mch_call(MCH_LDMIRL, 0xb836) >> 8;
				// cr =33=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x9933) >> 8;
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			case 0176: // DML0
				puts("- -dml0");
				// pr-1c917-50 p.8 l.12
				// dml0 =b4=> gb =99=> mchb
				mchb = mch_call(MCH_LDMIRL, 0x99b4) >> 8;
				mchb = mch_call(MCH_RTNMCHB, 0) >> 8;
				break;
			default:
				unimplemented(param);
				// DML
				// DML1
				// CR
				break;
			}
			break;

		case N2RETRN:
			puts("-n2retrn");
			switch(param) {
			case 0167: // system status register
				puts("- -ss");
				mchb = mch_call(MCH_RTNSS, 0) >> 8;
				break;
			default: unimplemented(param);
				// R0-R15
				// MCS
				// MISC
				break;
			}
			break;
		case NSTOP:
			puts("-nstop");
			mchb = mch_call(MCH_MSTOP, 0);
			break;
		case NFRZ:
			puts("-nfrz");
			mchb = mch_call(MCH_LDMAR, arg);
			break;
		case NNO3CD:
			printf("-*** THIS TEST WILL NEVER WORK ***\n");
			printf("-*** NNO3CD commands require a full emulator ***\n");
			return -1;
		case NBGN:
			/* Sets up a linkage between the diagnostic monitor and
			 * the diagnostic program.
			 *
			 * Also, initialize the registers.
			 *
			 * r0-r15 (general): 0
			 * im hg cr is: 0
			 * ss: 0440006
			 * pa sar ib sir ak ai dk di sdr: 0
			 * ar: 0
			 * mmsr: 0200
			 */
			printf("-nbgn %d\n", param);
			testno = param;
			// zbr, 2bca
			mch_call(MCH_LDMIRL, 0x2bca);

			// br0 =e2=> gb =53=> r0
			mch_call(MCH_LDMIRL, 0x53e2);
			// br0 =e2=> gb =55=> r1
			mch_call(MCH_LDMIRL, 0x55e2);
			// br0 =e2=> gb =56=> r2
			mch_call(MCH_LDMIRL, 0x56e2);
			// br0 =e2=> gb =59=> r3
			mch_call(MCH_LDMIRL, 0x59e2);
			// br0 =e2=> gb =5a=> r4
			mch_call(MCH_LDMIRL, 0x5ae2);
			// br0 =e2=> gb =5c=> r5
			mch_call(MCH_LDMIRL, 0x5ce2);
			// br0 =e2=> gb =63=> r6
			mch_call(MCH_LDMIRL, 0x63e2);
			// br0 =e2=> gb =65=> r7
			mch_call(MCH_LDMIRL, 0x65e2);
			// br0 =e2=> gb =66=> r8
			mch_call(MCH_LDMIRL, 0x66e2);
			// br0 =e2=> gb =69=> r9
			mch_call(MCH_LDMIRL, 0x69e2);
			// br0 =e2=> gb =6a=> r10
			mch_call(MCH_LDMIRL, 0x6ae2);
			// br0 =e2=> gb =6c=> r11
			mch_call(MCH_LDMIRL, 0x6ce2);
			// br0 =e2=> gb =47=> r12
			mch_call(MCH_LDMIRL, 0x47e2);
			// br0 =e2=> gb =4b=> r13
			mch_call(MCH_LDMIRL, 0x4be2);
			// br0 =e2=> gb =4d=> r14
			mch_call(MCH_LDMIRL, 0x4de2);
			// br0 =e2=> gb =4e=> r15
			mch_call(MCH_LDMIRL, 0x4ee2);

			// br0 =e2=> gb =87=> IM
			mch_call(MCH_LDMIRL, 0x87e2);
			// br0 =e2=> gb =35=> HG
			mch_call(MCH_LDMIRL, 0x35e2);
			// br0 =e2=> gb =33=> CR
			mch_call(MCH_LDMIRL, 0x33e2);
			// br0 =e2=> gb =18=> IS_clr
			// xxx this causes the from field decoder to wig out?
			mch_call(MCH_LDMIRL, 0x36e2);

			// load SS to 0440006
			mch_call(MCH_LDMCHB, 0440006);
			// mchb =99=> gb =8b=> SS_clr
			mch_call(MCH_LDMIRL, 0x8b99);
			// mchb =99=> gb =8e=> SS_st
			mch_call(MCH_LDMIRL, 0x8e99);

			// br0 =e2=> gb =96=> PA
			mch_call(MCH_LDMIRL, 0x96e2);
			// SAR 95
			mch_call(MCH_LDMIRL, 0x95e2);
			// IB c6
			mch_call(MCH_LDMIRL, 0xc6e2);
			// SIR 1e
			mch_call(MCH_LDMIRL, 0x1ee2);
			// AK 9c
			mch_call(MCH_LDMIRL, 0x9ce2);
			// AI a3
			mch_call(MCH_LDMIRL, 0xa3e2);
			// DK a5
			mch_call(MCH_LDMIRL, 0xa5e2);
			// DI a6
			mch_call(MCH_LDMIRL, 0xa6e2);
			// SDR 2e
			mch_call(MCH_LDMIRL, 0x2ee2);
			// AR b1
			mch_call(MCH_LDMIRL, 0xb1e2);

			// load MMSR to 0200
			mch_call(MCH_LDMCHB, 0200);
			// mchb =99=> gb =71=> br
			mch_call(MCH_LDMIRL, 0x7199);
			// br =caca=> MMSR (0200)
			mch_call(MCH_LDMIRL, 0xcaca);

			// zero out br again
			mch_call(MCH_LDMIRL, 0x2bca);

			break;
		case NTESTSEG:
			printf("-ntestseg %d\n", param);
			testseg = param;
			break;
		case NMICRO:
			puts("-nmicro");
			mchb = mch_call(MCH_LDMIRL, arg) >> 8;
			break;
		case NZERO_ER:
			puts("-nzero_er");
			mchb = mch_call(MCH_CLER, arg) >> 8;
			break;
		case NMASK:
			puts("-nmask");
			mchb &= arg;
			break;
		case NBEGIN:
			puts("-nbegin");
			loop_begin = loc+1;
			loop_count = param;
			loop_var = 0;
			break;
		case NLP_END:
			puts("-nlp_end");
			puts("-looping!!");
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
		case NDONE: // hack
			break;
		case NLDMIR:
			puts("-nldmir");
			mchb = mch_call(MCH_LDMIRH, arg) >> 8;
			break;
		default:
			printf("-cur instr %o\n", w);
			unimplemented(w & M_CMD);
			break;
		}

		loc += 1 + (loop?loop_count:1)*narg;
	}
}
