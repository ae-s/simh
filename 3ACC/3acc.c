/* 3acc.c: AT&T 3A Central Control (3ACC) implementation

   Copyright 2020, Astrid Smith
*/

#include <sim_defs.h>

#include <assert.h>

#include "3acc.h"
#include "parity.h"

#define MEM_SIZE              (cpu_unit.capac)
#define xxx_unimplemented() ASSURE(0==1)

t_stat cpu_reset(DEVICE* dptr);
t_stat mchmsg_cmd(int32 arg, const char* buf);
uint32 mchmsg(uint32 msg);

void cpu_hmrf(void);
void cpu_stop(void);
void cpu_complement_correction(void);

void cpu_hardware_switch(void);

void seg0_p0(void);
void seg0_p1(void);
void seg0_p2(void);
void seg0_p3(void);
void seg1_p0(void);
void seg1_p1(void);
void seg1_p2(void);
void seg1_p3(void);

uint32 R[NUM_REGISTERS];
/* these are the registers that are visible on the front panel.
 * microcode internal registers are not accessible to the user.
 */
REG cpu_reg[] = {
	/* first, general purpose registers */
	{ ORDATAD ( R0, R[0], 18, "General purpose register 0") },
	{ ORDATAD ( R1, R[1], 18, "General purpose register 1") },
	{ ORDATAD ( R2, R[2], 18, "General purpose register 2") },
	{ ORDATAD ( R3, R[3], 18, "General purpose register 3") },
	{ ORDATAD ( R4, R[4], 18, "General purpose register 4") },
	{ ORDATAD ( R5, R[5], 18, "General purpose register 5") },
	{ ORDATAD ( R6, R[6], 18, "General purpose register 6") },
	{ ORDATAD ( R7, R[7], 18, "General purpose register 7") },
	{ ORDATAD ( R8, R[8], 18, "General purpose register 8") },
	{ ORDATAD ( R9, R[9], 18, "General purpose register 9") },
	{ ORDATAD ( R10, R[10], 18, "General purpose register 10") },
	{ ORDATAD ( R11, R[11], 18, "General purpose register 11") },
	{ ORDATAD ( R12, R[12], 18, "General purpose register 12") },
	{ ORDATAD ( R13, R[13], 18, "General purpose register 13") },
	{ ORDATAD ( R14, R[14], 18, "General purpose register 14") },
	{ ORDATAD ( R15, R[15], 18, "General purpose register 15") },

	/* then the 16 registers that are accessible from the front panel */
	{ ORDATAD ( MCTL_STAT, R[NUM_MCS], 22, "Microcontrol Status" ) },
	{ ORDATAD ( TIM, R[NUM_TI], 0, "Timing Counter" ) },
	{ ORDATAD ( SYS_STAT, R[NUM_SS], 18, "System Status" ) },
	{ ORDATAD ( ST_ADRS, R[NUM_SAR], 20, "Store Address" ) },
	{ ORDATAD ( PROG_ADRS, R[NUM_PA], 20, "Program Address" ) },
	{ ORDATAD ( MTCE_STA, R[NUM_MS], 18, "Maintenance Status" ) },
	{ ORDATAD ( M_MEM_STAT, R[NUM_MMSR], 22, "Main Memory Status" ) },
	{ ORDATAD ( MCH_BUFR, R[NUM_MCHB], 0, "Maintenance Channel Buffer" ) },
	{ ORDATAD ( INT_SET, R[NUM_IS], 16, "Interrupt Set" ) },
	{ ORDATAD ( INT_MASK, R[NUM_IM], 0, "Interrupt Mask" ) },
	{ ORDATAD ( HOLD_GET, R[NUM_HG], 20, "Hold-Get" ) },
	{ ORDATAD ( ERR, R[NUM_ER], 22, "Error" ) },
	{ ORDATAD ( DATA_MASK, R[NUM_DK], 20, "Data Mask" ) },
	{ ORDATAD ( DATA_IN, R[NUM_DI], 20, "Data Input" ) },
	{ ORDATAD ( ADR_MASK, R[NUM_AK], 20, "Address Mask" ) },
	{ ORDATAD ( ADR_IN, R[NUM_AI], 20, "Address Input" ) },

	/* and now we have some non-user-visible registers, for microcode
	 * shit */
	// xxx
	{ NULL }
};

static uint32 gb;

REG *sim_PC = &cpu_reg[20];

UNIT cpu_unit = { UDATA (NULL, UNIT_FIX|UNIT_BINK|UNIT_IDLE|UNIT_DISABLE, MAXMEMSIZE ) };

MTAB cpu_mod[] = {
	{ 0 }
};

static DEBTAB cpu_deb_tab[] = {
	{ NULL, 0, NULL }
};

DEVICE cpu_dev = {
	"CPU",               /* Name */
	&cpu_unit,           /* Units */
	cpu_reg,             /* Registers */
	cpu_mod,             /* Modifiers */
	1,                   /* Number of Units */
	16,                  /* Address radix */
	16,                  /* Address width */
	1,                   /* Addr increment */
	16,                  /* Data radix */
	21,                  /* Data width */
	NULL,             /* Examine routine */ // xxx
	NULL,            /* Deposit routine */ // xxx
	&cpu_reset,          /* Reset routine */
	NULL,           /* Boot routine */ // xxx
	NULL,                /* Attach routine */
	NULL,                /* Detach routine */
	NULL,                /* Context */
	DEV_DEBUG,  /* Flags */
	0,                   /* Debug control flags */
	cpu_deb_tab,         /* Debug flag names */
	NULL,       /* Memory size change */
	NULL,                /* Logical names */
	NULL,           /* Help routine */ // xxx
	NULL,                /* Attach Help Routine */
	NULL,                /* Help Context */
	NULL,     /* Device Description */
};

// microcontrol flipflops
struct {
	t_bool ru; // xxx ??
	t_bool ru0, ru1, rub;
	t_bool alo;
	t_bool lint;
	t_bool lnop;
	t_bool lsir;
	t_bool malz;
	t_bool smint;
	t_bool dmint;
	t_bool bm;
	t_bool mpsm;
	t_bool pna;
	t_bool inh_ck;
	t_bool fnp;
	t_bool rarp;
} uff;

#define UCODE_LEN 1<<12
uint32 UCODE[2048];
uint32* RAM = NULL;

uint16  mar_jam = 0;

#define UOP(ca,cb, pta,pna, na, to,from)							\
	(uint32) (((uint32)ca)<<31 | ((uint32)cb)<<30 |					\
			  ((uint32)pta)<<29 | ((uint32)pna)<<28 |				\
			  ((uint32)na)<<15 | ((uint32)to)<<7 | ((uint32)from))

CTAB cmds[] = {
	{ "mch", mchmsg_cmd, 0,
	  "send maintenance channel message" },
	{ NULL }
};

t_stat
mchmsg_cmd(int32 arg, const char* buf)
{
	uint32 result;
	uint32 msg;

	msg = strtol(buf, NULL, 16);
	result = mchmsg(msg);
	printf("mch: %#x\n", (unsigned int) result);
	return SCPE_OK;
}

uint32
mchmsg(uint32 msg)
{
	uint8 mcc = msg & 0xff;
	R[NUM_MCHTR] = msg >> 8 & MM_R20;

	printf("mchmsg: msg= %x, mcc= %x, mchtr= %o\n",
		   msg, mcc, R[NUM_MCHTR]);

	/* see sh B9GJ */
	switch (mcc) {
	case MCH_CLER:
		R[NUM_ER] = 0;
		break;
	case MCH_CLMSR:
		// clears the maintenance state register
		xxx_unimplemented();
		break;
	case MCH_CLPT:
		// clears the program timer
		R[NUM_TI] &= M_REG_TI;
		// xxx check that it actually leaves TI untouched
		// xxx not executed if online
		break;
	case MCH_CLTTO:
		// clear the timer timeout register (TTO), which clears the
		// backup timing counter (BTC) after it has been set for a
		// while.
		xxx_unimplemented();
		break;
	case MCH_DISA:
		// first i/o disable signal
		xxx_unimplemented();
		break;
	case MCH_DISB:
		// second i/o disable signal
		xxx_unimplemented();
		break;
	case MCH_INITCLK:
		// set the clock to phase 3
		xxx_unimplemented();
		break;
	case MCH_LDMAR:
		// xxx check: cc=offline and either (stop or freeze)
		R[NUM_SS] &= ~SR_SS_STOP; // 1. clear stop
		R[NUM_MAR] = R[NUM_MCHTR] & 07777; // 2. load mar with low-12 of mchtr
		// 3. set freeze
		// xxx
		break;
	case MCH_LDMCHB:
		// gate from MCHTR into MCHB (20 bits + parity)
		R[NUM_MCHB] = R[NUM_MCHTR];
		break;
	case MCH_LDMIRH:
		// load high 16 of mir from mchb
		// xxx check if stop or freeze?
		R[NUM_MIR] &= 0x0000ffff;
		R[NUM_MIR] |= (R[NUM_MCHB] & 0xffff) << 16;
		break;
	case MCH_LDMIRL:
		// 1. load low 16 of mir from mchb
		R[NUM_MIR] &= 0xffff0000;
		R[NUM_MIR] |= R[NUM_MCHTR] & 0xffff;
		// 2. run decoders
		// xxx decoders, or a full machine cycle?
		seg1_p0();
		printf("gb = %o\n", gb);
		seg1_p1();
		printf("gb = %o\n", gb);
		seg1_p2();
		printf("gb = %o\n", gb);
		seg1_p3();
		printf("gb = %o\n", gb);
		break;
	case MCH_MSTART:
		// clear FRZ
		xxx_unimplemented();
		break;
	case MCH_MSTOP:
		R[NUM_SS] |= SR_SS_STOP;
		break;
	case MCH_RTNER:
		R[NUM_MCHTR] = R[NUM_ER];
		break;
	case MCH_RTNMB:
		xxx_unimplemented();
		break;
	case MCH_RTNMCHB:
		R[NUM_MCHTR] = R[NUM_MCHB];
		break;
	case MCH_RTNMMH:
		xxx_unimplemented();
		break;
	case MCH_RTNMML:
		xxx_unimplemented();
		break;
	case MCH_RTNSS:
		R[NUM_MCHTR] = R[NUM_SS];
		printf("return ss of %o\n", R[NUM_SS]);
		break;
	case MCH_SPCLK:
		xxx_unimplemented();
		break;
	case MCH_STCLK:
		xxx_unimplemented();
		break;
	case MCH_SWITCH:
		// hardware initialization
		cpu_hardware_switch();
		break;
	case MCH_TOGCLK:
		// advance through one half clock phase
		xxx_unimplemented();
		break;
	default: break;
	}

	printf("mchtr now =%o\n", R[NUM_MCHTR]);
	return mcc | (R[NUM_MCHTR] << 8);
}

/* hardware reset logic: pr-1c900-01 p B20 */
void cpu_hardware_switch(void)
{
	// step 1:

	// PT partially cleared
	// ref sd-1c900-01 sh b8ga
	R[NUM_PT] &= ~( M_REG_PT );

	// I/O channels disabled
	// xxx

	// CC=0
	// ISC2=0
	// ISC1=0
	R[NUM_SS] &= ~( SR_SS_ISC1 | SR_SS_ISC2 |
					SR_SS_CC0 | SR_SS_CC1 );

	// step 2:

	// BHC=0
	// STP=0
	// MINT=0
	R[NUM_SS] &= ~( SR_SS_MINT | SR_SS_BHC | SR_SS_STOP );
	// BTC=1
	R[NUM_SS] |= ( SR_SS_BTC );

	// MSR=0
	R[NUM_MMSR] = 0;
	// CLOCK=p3 xxx
	// FRZ=0 xxx
	// MAR= 0x0bf = 0277
	R[NUM_MAR] = 0277;
	// Initialize MCH xxx
}

/* MRF Logic: sd-1c900-01 sh b7gh */
void cpu_hmrf(void) { mar_jam = 0277; }

/* sd-1c900-01 sh b1gd */
void cpu_stop(void) { mar_jam = 0377; }

/* sd-1c900-01 sh b1gd */
void cpu_complement_correction(void) { mar_jam = 0777; }

/* xxx interrupt: jam with 0120 (sh b1gd) */

t_stat
cpu_reset(DEVICE* dptr)
{
	if (! sim_is_running) {
		if (RAM == NULL) {
			RAM = (uint32*) calloc((size_t)(MEM_SIZE >> 2), sizeof(uint32));
		}

#include "ucode-list1-literals.h"

		sim_vm_cmd = cmds;
	}

	cpu_hmrf();
	return SCPE_OK;
}

t_stat
sim_instr(void)
{
	/*
	 * [ /!\ ] notice
	 *
	 * this function presently performs one microcycle before exiting.
	 * a microcycle consists of four clock phases, 0 1 2 3.  at all
	 * times there is a microinstruction (uop) fetch being prepared at
	 * the same time that the previous uop is being executed.
	 *
	 * in other words, there is a two-stage microinstruction pipeline,
	 * which is interleaved into this code among the four-phase
	 * machine cycle.
	 */


	seg0_p0();
	printf("gb = %o\n", gb);
	seg1_p0();
	printf("gb = %o\n", gb);

	seg0_p1();
	printf("gb = %o\n", gb);
	seg1_p1();
	printf("gb = %o\n", gb);

	seg0_p2();
	printf("gb = %o\n", gb);
	seg1_p2();
	printf("gb = %o\n", gb);

	seg0_p3();
	printf("gb = %o\n", gb);
	seg1_p3();
	printf("gb = %o\n", gb);

	return SCPE_OK;
}

// dml_output[0] = dml0
// dml_output[1] = dml1
// dml_output[2] = bits to set in MCS

// dml is weird; the function register doesn't have a bit 0.
void
run_adders(uint32_t* dml_output) {
	int fr[2] = { NUM_FR0, NUM_FR1 };
	int ar[2] = { NUM_AR0, NUM_AR1 };
	int br[2] = { NUM_BR0, NUM_BR1 };
	// mask for DS which is parameterized by which DML# ran into
	// an overflow.  normally both but, this being bell labs, you
	// never know.
	uint32_t m_ds[2] = {MCS_DS0, MCS_DS1};
	int dml = 0;

	dml_output[0] = 0;
	dml_output[1] = 0;
	dml_output[2] = 0;
	// the adders are SLIGHTLY DIFFERENT, though they do both run
	// concurrently.  i have chosen to express this by way of a
	// for-loop (that runs exactly two times, once for each adder)
	// and some conditionals and indexing.
	for (dml = 0; dml < 2; dml++) {
		// add +1 to the sum, if +1 bit is set
		uint32_t add1 = (R[fr[dml]] & M_FR_AD1) ? 1 : 0;

		switch (R[fr[dml]] & (M_FR_ADS|M_FR_ADL)) { // add operation
		case M_FR_ADL: // add long
		{
			uint32_t out
				= (R[ar[dml]] & M_R20)
				+ (R[br[dml]] & M_R20)
				+ add1;
			dml_output[dml] = out & M_R20;
			printf("out%d = %o\n", dml, dml_output[dml]);
			// handle carry out, gated into MCS register
			// sh b2ge loc h5: "CARH01" out from adder
			// sh b2ca loc b6: "CCAR191" at loc 5h0
			// what is 5h0? idk
			// sh b2aa loc f2: "FLZ, AO00, CARH01 -> 1/5"
			// sh b1aa loc b5 (fs 1 sym 5): "2/2 -> FLZ, AO00, CARH01"
			// YES this links up \m/ hypertext
			// found CARH01/CARL01 on B1GL loc b7
			// gated by F (DML => GB) they both set DS

			// xxx (DML => GB) also operates G, gating GBALZT10 and
			// GBALZTA0 into MCS[DS]

			// sh b2gb loc e3: "see fs1 sh B1GL"
			// sh b1gl:
			if (out & 0x100000) {
				dml_output[2] |= m_ds[dml];
			}
			break;
		}
		case M_FR_ADS: // add short
		{
			uint32_t out
				= (R[ar[dml]] & M_R16)
				+ (R[br[dml]] & M_R16)
				+ add1;
			dml_output[dml] = out & M_R16;
			// handle carry out, gated into DS register
			if (out & 0x10000) {
				dml_output[2] |= m_ds[dml];
			}
			break;
		}
		case M_FR_ADS|M_FR_ADL: // test high 8 bits of AR
			// see sh b2gd
			xxx_unimplemented();
			break;
		}

		// boolean states
		// cite SD-1C1900-01 sh B2GB, table A
		if ((R[fr[dml]] & M_FR_ADD) == 0)
		switch ((R[fr[dml]] & M_FR_BOOL) >> 4) {
		case  0: dml_output[dml] = 0xffff                   ; break;
		case  1: dml_output[dml] = ~R[ar[dml]] | ~R[br[dml]]; break;
		case  2: dml_output[dml] = ~R[ar[dml]] |  R[br[dml]]; break;
		case  3: dml_output[dml] = ~R[ar[dml]]              ; break;
		case  4: dml_output[dml] =  R[ar[dml]] | ~R[br[dml]]; break;
		case  5: dml_output[dml] =               ~R[br[dml]]; break;
		case  6: dml_output[dml] = ~R[ar[dml]] & ~R[br[dml]]
		                         |  R[ar[dml]] &  R[br[dml]]; break;
		case  7: dml_output[dml] = ~R[ar[dml]] & ~R[br[dml]]; break;
		case  8: dml_output[dml] =  R[ar[dml]] |  R[br[dml]]; break;
		case  9: dml_output[dml] = ~R[ar[dml]] &  R[br[dml]]
		                         |  R[ar[dml]] & ~R[br[dml]]; break;
		case 10: dml_output[dml] =                R[br[dml]]; break;
		case 11: dml_output[dml] = ~R[ar[dml]] &  R[br[dml]]; break;
		case 12: dml_output[dml] =  R[ar[dml]]              ; break;
		case 13: dml_output[dml] =  R[ar[dml]] & ~R[br[dml]]; break;
		case 14: dml_output[dml] =  R[ar[dml]] &  R[br[dml]]; break;
		case 15: dml_output[dml] = 0                        ; break;
		}

		dml_output[dml] &= M_R20;
	}

	printf("dml0 output: ar0:%o br0:%o fr0:%o dml0:%o\n",
		   R[NUM_AR0], R[NUM_BR0], R[NUM_FR0], dml_output[0]);
	printf("dml1 output: ar1:%o br1:%o fr1:%o dml1:%o\n",
		   R[NUM_AR1], R[NUM_BR1], R[NUM_FR1], dml_output[1]);
}
	/* ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 */

static uint32 mar;
static microinstruction mir;
void seg0_p0(void) {
	/* **** microinstruction pipeline, stage 1 **** */
	mar = R[NUM_MAR];
	mir.q = R[NUM_MIR];

	/* the first stage of the uop pipeline is "determine address of
	 * next uop".  segments of the code demarked by "stage 1" will
	 * concern:
	 *
	 * - decide what the Next Address is
	 * - load the MicroInstruction Register from the Next Address
	 */

	if (mir.mi.ca == 1) {
		/* the CA bit means "increment the address instead of loading
		 * it", but due to a hardware limitation (this is done by
		 * ORing 1 into the low bit) it can only happen at even
		 * addresses. */
		mar |= 1;
	} else {
		mar = mir.mi.na;
	}

	// following if statement:
	// cite SD-1c900-01 sh B1GG loc B7 table A and B
	if (mir.mi.ca == 0 && mir.mi.cb == 0) {
		// normal microinstruction
	} else if (mir.mi.ca == 0 && mir.mi.cb == 1) {
		// main memory instruction fetch
		xxx_unimplemented();
	} else if (mir.mi.ca == 1 && mir.mi.cb == 0) {
		// microcontrol data operation
		xxx_unimplemented();
	} else if (mir.mi.ca == 1 && mir.mi.cb == 1) {
		// enable NA field auxiliary decoder
		switch (mir.mi.na & 0xf00) {
		case 0x600: // Load the function register from MIR NA[7-1]
			R[NUM_FR0] = (mir.mi.na) & M_REG_FR;
			R[NUM_FR1] = (mir.mi.na) & M_REG_FR;
			break;
		case 0x900: // I/O GB parity divert
			xxx_unimplemented();
			break;
		case 0xC00: // I/O DML match divert
			xxx_unimplemented();
			break;
		case 0xA00: // Turn off GB parity check for a microcycle
			xxx_unimplemented();
			break;
		case 0x500: // spare
			break;
		case 0x300: // spare
			break;
		default:
			break;
		}
	}

	if (mir.mi.na == 0xfff) {
		/* all-ones detector: load the RAR into the MIR; return from
		 * microsubroutine */
		mar = R[NUM_RAR];
		uff.ru = TRUE;
	} else if (mir.mi.na == 0x000) {
		/* return from instruction */
		xxx_unimplemented();
	}

	if ((R[NUM_MCS] & (MCS_RU0|MCS_RU1)) != 0) {
		/* rar update (RU flipflop) is enabled: we are not in a
		 * microsubroutine */
		R[NUM_RAR] = mar;
	}

	if ( FALSE /* complement correction required xxx */) {
		// xxx: is this the previous mir (cu->micro.mir)
		// or the current mir (mir)?
		// xxx i wrote mar even though specs seem to have said mir in the past
		R[NUM_ERAR] = R[NUM_MAR];
		mar = 00777; // hardwired, see sh B1GN loc E8
	}

	/* allow the MRF to chime in too; see function cpu_reset */
	mar |= mar_jam;
}

void
seg1_p0(void) {
	/* **** microinstruction pipeline, stage 2 **** */
	microinstruction mymir;
	mymir.q = R[NUM_MIR];
	/* sh b1gj loc d2 */
	uff.malz = ((mymir.mi.na == 0));

	gb = 0;

#define GB18(from) gb = (from & MM_R16)
#define GB22(from) gb = (from & MM_R20)

	/* == from field decoder ==
	 * ref. sd-1c900-01 sh D13 (note 312)
	 */
	switch ((uint8_t)mymir.mi.from) {
		/* first 16 listed are 1o4 L and 3o4 R */
	case 0x17: // f1o4l1+f3o4r7 (B1GB)
		// RAR(0-11) => GB(8-19)
		// [zeros in GB(0-7)]
		gb = (R[NUM_RAR] & 0xfff) << 8;
		gb |= R[NUM_RAR] & M_PH;
		break;
	case 0x1b: // spare
		break;
	case 0x1d: // spare, sir1
		break;
	case 0x1e: // sir0 => gb(18)
		GB18(R[NUM_SIR]);
		break;
	case 0x27: // mchc(0-7) => gb(0-7)
		gb = R[NUM_MCHC] & 0xff;
		break;
	case 0x2b: // spare
		break;
	case 0x2d: // spare, sdr1
		break;
	case 0x2e: // sdr0 => gb (18)
		GB18(R[NUM_SDR]);
		break;
	case 0x47: // r12 => gb (18)
		GB18(R[12]);
		break;
	case 0x4b: // r13 => gb (18)
		GB18(R[13]);
		break;
	case 0x4d: // r14 => gb (18)
		GB18(R[14]);
		break;
	case 0x4e: // r15 => gb (18)
		GB18(R[15]);
		break;
	case 0x87: // im  [mr12] => gb (18)
		GB18(R[NUM_IM]);
		break;
	case 0x8b: // is(0-15) [mr13] => gb(0-15)
		gb = R[NUM_IS] & 0xffff;
		break;
	case 0x8d: // ms [mr14] => gb (18)
		GB18(R[NUM_MS]);
		break;
	case 0x8e: // ss [mr15] => gb (18)
		GB18(R[NUM_SS]);
		printf("gb %o loaded from ss %o\n", gb, R[NUM_SS]);
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // cr => gb (22)
		GB22(R[NUM_CR]);
		printf("gb %o loaded from cr %o\n", gb, R[NUM_CR]);
		break;
	case 0x35: // hg => gb (18)
		GB18(R[NUM_HG]);
		break;
	case 0x36: // misc dec row 2
	case 0x39: // misc dec row 3
	case 0x3a: // misc dec row 4
	case 0x3c: // misc dec row 5
		goto misc_dec;
	case 0x53: // r0 => gb (18)
		GB18(R[0]);
		break;
	case 0x55: // r1 => gb (18)
		GB18(R[1]);
		break;
	case 0x56: // r2 => gb (18)
		GB18(R[2]);
		break;
	case 0x59: // r3 => gb (18)
		GB18(R[3]);
		break;
	case 0x5a: // r4 => gb (18)
		GB18(R[4]);
		break;
	case 0x5c: // r5 => gb (18)
		GB18(R[5]);
		break;

	case 0x63: // r6 => gb (18)
		GB18(R[6]);
		break;
	case 0x65: // r7 => gb (18)
		GB18(R[7]);
		break;
	case 0x66: // r8 => gb (18)
		GB18(R[8]);
		break;
	case 0x69: // r9 => gb (18)
		GB18(R[9]);
		break;
	case 0x6a: // r10 => gb (18)
		GB18(R[10]);
		break;
	case 0x6c: // r11 => gb (18)
		GB18(R[11]);
		break;

	case 0x93: // ti [mr0] => gb (16)
		// bus parity needs inhibit when using this path
		// inhibit source: see table B sd-1c900-01 sh b1gg loc d8
		xxx_unimplemented();
		break;
	case 0x95: // sar [mr1] => gb (22)
		GB22(R[NUM_SAR]);
		break;
	case 0x96: // pa [mr2] => gb (22)
		GB22(R[NUM_PA]);
		break;
	case 0x99: // mchb [mr3] => gb (22)
		GB22(R[NUM_MCHB]);
		break;
	case 0x9a: // mms [mr4] => gb (12, pl, ph)
		// B4GB loc C0
		GB18(R[NUM_MMSR] & 0xfff);
		break;
	case 0x9c: // ak [mr5] => gb (22)
		GB22(R[NUM_AK]);
		break;

	case 0xa3: // ai [mr6] => gb (22)
		GB22(R[NUM_AI]);
		break;
	case 0xa5: // dk [mr9] => gb (18)
		GB18(R[NUM_DK]);
		break;
	case 0xa6: // di [mr8] => gb (18)
		GB18(R[NUM_DI]);
		break;
	case 0xa9: // db [mr9] => gb (22)
		GB22(R[NUM_DB]);
		break;
	case 0xaa: // er [mr10] => gb (22)
		// bus parity needs inhibit?
		// xxx
		GB22(R[NUM_ER]);
		break;
	case 0xac: // spare [mr11]
		break;

	case 0xc3: // sirc => gb (18)
		xxx_unimplemented();
		break;
	case 0xc5: // sdrc => gb (18)
		xxx_unimplemented();
		break;
	case 0xc6: // ib => gb (18)
		// what the hell is IB? is it SIB?

		// NO. IB is Instruction Buffer, explained on b4ga.
		GB18(R[NUM_IB]);
		break;
	case 0xc9: // misc dec row 6
	case 0xca: // misc dec row 7
		goto misc_dec;
	case 0xcc: // PA+1
		xxx_unimplemented();
		break;

		/* next 16 listed are 3o4 L and 1o4 R */

	case 0x71: // ar => gb (22)
		GB22(R[NUM_AR0]);
		break;
	case 0x72: // spare
		break;
	case 0x74: // mcs => gb (22)
		GB22(R[NUM_MCS]);
		break;
	case 0x78: // spare
		break;

	case 0xb1:
		// ar => gb (0-15, PL)
		// ar(ph) xor ar(ph4) => gb(ph)

		// yikes? oh. this is for splitting 22-bit addresses into
		// 16-bit words ("packing").

		// xxx handle the parity correctly
		GB18(R[NUM_AR0]);
		break;
	case 0xb2: // spare
		break;
	case 0xb4: // dml => gb (22) // dml%f
	{
		// uctl [F]
		// uctl [G]
		uint32_t dml[3] = {0,0,0};
		run_adders(dml);
		// xxx compute parity

		// finish
		GB22(dml[0]);
		R[NUM_MCS] &= ~(MCS_DS0|MCS_DS1);
		R[NUM_MCS] |= dml[2];
	}
		break;
	case 0xb8: // ar(16-19, ph4) => gb(0-3, pl)
		// other part of operation described in 0xb1
		xxx_unimplemented();
		break;

	case 0xd1:
		// ar(0-15,pl) => gb(0-15,pl)
		// ar(ph) xor ar(ph4) => gb(ph)
		xxx_unimplemented();
		break;
	case 0xd2: // ibyt => gb (18)
		// ??? wtf is ibyt
		xxx_unimplemented();
		break;
	case 0xd4: // ibxt => gb (18)
		// ?? wtf is ibxt
		xxx_unimplemented();
		break;
	case 0xd8: // misc dec row 0
		goto misc_dec;

	case 0xe1: // really complicated
		xxx_unimplemented();
		break;
	case 0xe2: // br => gb (22)
		GB22(R[NUM_BR0]);
		printf("gb %o loaded from br0 %o\n", gb, R[NUM_BR0]);
		break;
	case 0xe4: // really also complicated
		xxx_unimplemented();
		break;
	case 0xe8: // misc dec row 1
		goto misc_dec;


		/* next we have the all-zeros conditions */

	default:
		if ((mymir.mi.from & 0x0f) == 0) {
			// f4o4l0
			// nop
		} else if ((mymir.mi.from & 0xf0) == 0) {
			// f4o4r0
			// spare
		} else {
			// xxx from field decode error
			printf("from field decode error, to=%hx from=%hx\n",
			       mymir.mi.to, mymir.mi.from);
			xxx_unimplemented();
		}
	}

#undef GB18
#undef GB22

	

	/* == to field decoder ==
	 * ref: sd-1c900-01 sh D13 (note 312)
	 */
#define GB18(to) to = gb
#define GB22(to) to = gb

	switch ((uint8_t)mymir.mi.to) {
		/* first 16 listed are 1o4 L and 3o4 R */
	case 0x17: // gb(8-19,ph) => rar(0-11,ph) (B1GB)
		printf("gbxrar %x\n", gb);
		R[NUM_RAR] = gb >> 8;
		R[NUM_RAR] |= gb & M_PH;
		break;
	case 0x1b: // misc dec col 13
	case 0x1d: // misc dec col 3
		goto misc_dec;
	case 0x1e: // gb => sir0 (18)
		// xxx: is sir0 different from sir
		GB18(R[NUM_SIR]);
		break;
	case 0x27: // misc dec col 0
	case 0x2b: // misc dec col 1
	case 0x2d: // misc dec col 2
		goto misc_dec;
	case 0x2e: // gb => sdr0
		GB18(R[NUM_SDR]);
		// xxx not sdr1?
		break;
	case 0x47: // gb => r12 (18)
		GB18(R[12]);
		break;
	case 0x4b: // gb => r13 (18)
		GB18(R[13]);
		break;
	case 0x4d: // gb => r14 (18)
		GB18(R[14]);
		break;
	case 0x4e: // gb => r15 (18)
		GB18(R[15]);
		break;
	case 0x87: // gb => im [mr12] (18)
		GB18(R[NUM_IM]);
		break;
	case 0x8b: // gb => ss_cl [mr13] (22)
		// 18 bit operation on the reset side of ss.
		// the gating bus must be idle on the prior microcycle to prevent false operation.
		// this operation does not change bits (15, 19, PL, PH).
		// bit 15 is connected to the disable I/O flipflop.
		// bit 19 is connected to the power key.
		// PL and PH are wired to provide a CC identity code.

		// Controllable bits:
		// 0001110111111111111111
#define SS_MASK 0x77fff
		R[NUM_SS] &= gb & SS_MASK;
		printf("ss loaded from gb %o=>%o\n", gb, R[NUM_SS]);

		break;
	case 0x8d: // gb => ms [mr14]
		GB18(R[NUM_MS]);
		break;
	case 0x8e: // gb => ss_st [mr15] (22)
		// 18 bit operation on the reset side of ss.
		// the gating bus must be idle on the prior microcycle to prevent false operation.
		// this operation does not change bits (15, 19, PL, PH).
		// bit 15 is connected to the disable I/O flipflop.
		// bit 19 is connected to the power key.
		// PL and PH are wired to provide a CC identity code.
		R[NUM_SS] |= gb & SS_MASK;
		printf("ss loaded from gb %o=>%o\n", gb, R[NUM_SS]);
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // gb => cr (22)
		GB22(R[NUM_CR]);
		break;
	case 0x35: // gb => hg (18)
		GB18(R[NUM_HG]);
		break;
	case 0x36: // gb => is_cl (18)
		// 18 bit operation on reset side of IS.  the gating bus must
		// be idle on the prior microcycle to prevent false operation.
		R[NUM_IS] &= ~gb;
		// xxx
		break;
	case 0x39: // gb => is_s (18)
		// 18 bit operation on set side of IS.  the gating bus must be
		// idle on the prior microcycle to prevent false operation.
		R[NUM_IS] |= gb;
		// xxx
		break;
	case 0x3a: // misc dec col 8
	case 0x3c: // misc dec col 9
		goto misc_dec;

	case 0x53: // gb => r0 (18)
		GB18(R[0]);
		break;
	case 0x55: // gb => r1 (18)
		GB18(R[1]);
		break;
	case 0x56: // gb => r2 (18)
		GB18(R[2]);
		break;
	case 0x59: // gb => r3 (18)
		GB18(R[3]);
		break;
	case 0x5a: // gb => r4 (18)
		GB18(R[4]);
		break;
	case 0x5c: // gb => r5 (18)
		GB18(R[5]);
		break;

	case 0x63: // gb => r6 (18)
		GB18(R[6]);
		break;
	case 0x65: // gb => r7 (18)
		GB18(R[7]);
		break;
	case 0x66: // gb => r8 (18)
		GB18(R[8]);
		break;
	case 0x69: // gb => r9 (18)
		GB18(R[9]);
		break;
	case 0x6a: // gb => r10 (18)
		GB18(R[10]);
		break;
	case 0x6c: // gb => r11 (18)
		GB18(R[11]);
		break;

	case 0x93: // gb => mchtr [mr0] (22)
		GB22(R[NUM_MCHTR]);
		break;
	case 0x95: // gb => sar [mr1] (22)
		GB22(R[NUM_SAR]);
		break;
	case 0x96: // gb => pa [mr2] (22)
		GB22(R[NUM_PA]);
		break;
	case 0x99: // gb => mchb [mr3] (22)
		GB22(R[NUM_MCHB]);
		printf("mchb loaded from gb %o\n", gb);
		break;
	case 0x9a: // spare
		break;
	case 0x9c: // gb => ak [mr5] (22)
		GB22(R[NUM_AK]);
		break;

	case 0xa3: // gb => ai [mr6] (22)
		GB22(R[NUM_AI]);
		break;
	case 0xa5: // gb => dk [mr7] (18)
		GB18(R[NUM_DK]);
		break;
	case 0xa6: // gb => di [mr8] (18)
		GB18(R[NUM_DI]);
		break;
	case 0xa9: // gb => db [mr9] (22)
		GB22(R[NUM_DB]);
		break;
	case 0xaa: // gb => er [mr10] (22)
		GB22(R[NUM_ER]);
		break;
	case 0xac: // gb => dbc [mr11] (22)
		xxx_unimplemented();
		break;

	case 0xc3: // gb => sard (22)
		// conditional on ff:dr=1
		xxx_unimplemented();
		break;
	case 0xc5: // gb => sari (22)
		// conditional on ff:dr=1
		xxx_unimplemented();
		break;
	case 0xc6: // gb => ib (18)
		GB18(R[NUM_IB]);
		break;
	case 0xc9: // misc dec col 12
	case 0xca: // misc dec col 11
	case 0xcc: // misc dec col 10
		goto misc_dec;

		/* next 16 listed are 3o4 L and 1o4 R */

	case 0x71: // gb => br (22)
		GB22(R[NUM_BR0]);
		GB22(R[NUM_BR1]);
		printf("br %o loaded from gb %o\n", R[NUM_BR0], gb);
		// yes we have two DML
		// B2GM loc g0: inh parity xxx
		break;
	case 0x72: // complex gating
		xxx_unimplemented();
		break;
	case 0x74: // complex gating
		xxx_unimplemented();
		break;
	case 0x78: // misc dec col 6
		goto misc_dec;

	case 0xb1: // gb => ar (22)
		// B2GM loc g0: inh parity
		GB22(R[NUM_AR0]);
		GB22(R[NUM_AR1]);
		break;
	case 0xb2: // gb => ar0 (22)
		GB22(R[NUM_AR0]);
		break;
	case 0xb4: // complex gating
		xxx_unimplemented();
		break;
	case 0xb8: // misc dec col 5
		goto misc_dec;

	case 0xd1: // complex gating
		// B2GM loc g0: inh parity
		xxx_unimplemented();
		break;
	case 0xd2: // spare
		break;
	case 0xd4: // complex gating
		xxx_unimplemented();
		break;
	case 0xd8: // misc dec col 4
		goto misc_dec;

	case 0xe1: // gb => mchc (0-7) (no parity)
		xxx_unimplemented();
		break;
	case 0xe2: // gb => sib1 (18)
		// B2GM loc g0: inh parity
		xxx_unimplemented();
		break;
	case 0xe4: // gb => sdr1 (18)
		xxx_unimplemented();
		break;
	case 0xe8: // misc dec col 7
		goto misc_dec;


		/* next we have the all-zeros conditions */

	default:
		if ((mymir.mi.to & 0x0f) == 0) {
			// nop
		} else if ((mymir.mi.to & 0xf0) == 0) {
			// gate br(15) to br(16-19)
			xxx_unimplemented();
		} else {
			// to field decode error
			printf("to field decode error, to=%hx from=%hx\n",
			       mymir.mi.to, mymir.mi.from);
			xxx_unimplemented();
		}
	}
	

	/* == miscellaneous decoder ==
	 * ref: sd-1c900-01 sh D14 (note 313)
	 */
	goto aftermd;
misc_dec:
	;
	uint32_t decode_pt = mymir.q & 0xffff;
	printf("md: %x\n", decode_pt);

	/* xxx concerned that this switch might be slow */
	switch (decode_pt) {

// row 0, d8
	case 0x27d8: // spare
		break;
	case 0x2bd8: // pymch
		// gate parity bits from br0 to mchtr
		R[NUM_MCHTR] = R[NUM_BR0] && (M_PH|M_PL);
		break;
	case 0x2dd8: // hmrf
		// hardware initialize the 3a cc
		cpu_hmrf();
		break;
	case 0x1dd8: // tmch
		xxx_unimplemented();
		// test the mch
		// gate MCH status to the C register
		// see MDTMCH0 on sh B9GE loc A4
		break;
	case 0xd8d8: // idlmch
		xxx_unimplemented();
		// idle the mch
		break;
	case 0xb8d8: // stmch
		xxx_unimplemented();
		break;
	case 0x78d8: // spare
		break;
	case 0xe8d8: // spare
		break;
	case 0x3ad8: // spare
		break;
	case 0x3cd8: // sopf
		xxx_unimplemented();
		// set opf
		break;
	case 0xccd8: // zopf
		xxx_unimplemented();
		// clear opf
		break;
	case 0xcad8: // zi
		xxx_unimplemented();
		// clear i bit
		break;
	case 0xc9d8: // ti
		xxx_unimplemented();
		// test i bit
		break;
	case 0x1bd8: // si
		xxx_unimplemented();
		// set i bit
		break;

// row 1, e8
	case 0x27e8: // spare
		break;
	case 0x2be8: // zer, clear the error register
		R[NUM_ER] = 0; // xxx this was "0 | M", what is that?
		break;
	case 0x2de8: // zmint, clear the microinterpret bit
		R[NUM_SS] &= SR_SS_MINT;
		break;
	case 0x1de8: // iocc
		xxx_unimplemented();
		// interrupt the other cc
		break;
	case 0xd8e8: // zms
		xxx_unimplemented();
		// clear the maintenance state register
		break;
	case 0xb8e8: // zpt, clear the program timer
		R[NUM_TI] &= M_REG_TI;
		break;
	case 0x78e8: break; // spare
	case 0xe8e8: break; // spare
	case 0x3ae8: break; // spare
	case 0x3ce8: // ttr2
		xxx_unimplemented();
		break;
	case 0xcce8: // tdr
		xxx_unimplemented();
		// test dr
		break;
	case 0xcae8: // ty
		xxx_unimplemented();
		break;
	case 0xc9e8: // tx
		xxx_unimplemented();
		break;
	case 0x1be8: // zru (B1GB), clear ru
		R[NUM_MCS] &= ~( MCS_RU0|MCS_RU1 );
		break;

// row 2, 36
	case 0x2736: // spare
		break;
	case 0x2b36: // iod => r11
		xxx_unimplemented();
		// gate the iod to r11
		break;
	case 0x2d36: // s1db
		xxx_unimplemented();
		// gate switch register 1 to the db
		break;
	case 0x1d36: // imtc
		// idle the mch
		// force-clear INRFMCH, sh B7GG loc F1
		//
		xxx_unimplemented();
		break;
	case 0xd836: // br => ti
		xxx_unimplemented();
		// gate br to ti (16)
		break;
	case 0xb836: // dml1 => cr
	{
		uint32_t dml[2];
		run_adders(dml);
		// gate output of dml1 to cr
		R[NUM_CR] = dml[1];
	}
		break;
	case 0x7836: // md4 stch
		xxx_unimplemented();
		// start io
		break;
	case 0xe836: // md0 idch
		xxx_unimplemented();
		// idle io serial channel
		break;
	case 0x3a36: // spare
		break;
	case 0x3c36: // ttr1
		xxx_unimplemented();
		break;
	case 0xcc36: // ds => cf
		// gate ds flag to cf
		R[NUM_MCS] &= ~MCS_CF0;
		R[NUM_MCS] |= (R[NUM_MCS] & MCS_DS0) >> 1;
		break;
	case 0xca36: // tds
		xxx_unimplemented();
		break;
	case 0xc936: // tcf
		xxx_unimplemented();
		// test cf
		break;
	case 0x1b36: // tflz
		// uctl [E]

		// test for low zero in ar, 16-bit operation
		// invert, then find first set
		// ordinal of low zero goes into low 4 bits of BR
		// if all ones, set DS of MSC register (?)
		// - (MSC, typo on B2GD loc E9)
		// if not all ones, clear DS
		if ((R[NUM_AR0] & M_R16) == M_R16) {
			R[NUM_MCS] |= MCS_DS0;
			printf("tflz0: all1\n");
		} else {
			int z = ffs(~(R[NUM_AR0] & M_R16)) - 1;
			// clear bottom 4 bits of BR0
			R[NUM_BR0] &= MM_R16 ^ 0x000f;
			// find the result
			R[NUM_BR0] |= z & 0x000f;
			R[NUM_MCS] &= ~MCS_DS0;
			printf("tflz0: %d\n", z);
		}

		// same business for dml1
		if ((R[NUM_AR1] & M_R16) == M_R16) {
			R[NUM_MCS] |= MCS_DS1;
			printf("tflz1: all1\n");
		} else {
			int z = ffs(~(R[NUM_AR1] & M_R16)) - 1;
			// clear bottom 4 bits of BR1
			R[NUM_BR1] &= MM_R16 ^ 0x000f;
			// find the result
			R[NUM_BR1] |= z & 0x000f;
			R[NUM_MCS] &= ~MCS_DS1;
			printf("tflz1: %d\n", ffs(~(R[NUM_AR1] & M_R16)));
		}

		break;

// row 3, 39
	case 0x2739: break; // spare
	case 0x2b39: // sbtc
		xxx_unimplemented();
		// set block timer check bit
		break;
	case 0x2d39: // s2db
		xxx_unimplemented();
		// gate switch register 2 to the db
		break;
	case 0x1d39: // sdis
		xxx_unimplemented();
		// set the disable flip-flop
		break;
	case 0xd839: // inctc
		xxx_unimplemented();
		// increment the timing counter
		break;
	case 0xb839: // incpr
		xxx_unimplemented();
		// increment the prescaler (part of tc)
		break;
	case 0x7839: // md5 eio5
		xxx_unimplemented();
		// enabli ios to iod
		break;
	case 0xe839: // md1 chtn
		xxx_unimplemented();
		// load r9 to ios (normal)
		break;
	case 0x3a39: // enb
		xxx_unimplemented();
		break;
	case 0x3c39: // scf
		// set cf
		R[NUM_MCS] |= MCS_CF0|MCS_CF1;
		break;
	case 0xcc39: // sds, set ds
		R[NUM_MCS] |= MCS_DS0|MCS_DS1;
		break;
	case 0xca39: // str1, set tr1
		R[NUM_MCS] |= MCS_TR10|MCS_TR11;
		break;
	case 0xc939: // str2, set tr2
		R[NUM_MCS] |= MCS_TR20|MCS_TR21;
		break;
	case 0x1b39: // sdr, set dr
		R[NUM_MCS] |= MCS_DR0|MCS_DR1;
		break;

// row 4, 3a
	case 0x273a: break; // spare
	case 0x2b3a: break; // spare
	case 0x2d3a: // mchb => mchtr
		// gate mchb to mchtr [mdcmcbtr0]
		R[NUM_MCHTR] = R[NUM_MCHB];
		break;
	case 0x1d3a: // s3db
		xxx_unimplemented();
		// gate switch register 3 to db
		break;
	case 0xd83a: // rar => fn
		// gate rar to fn [mdrarfn0]
		printf("mdrarxfn: %o\n", R[NUM_RAR] & M_REG_FR);
		R[NUM_FR0] = R[NUM_RAR] & M_REG_FR;
		R[NUM_FR1] = R[NUM_RAR] & M_REG_FR;
		break;
	case 0xb83a: // mclstr
		xxx_unimplemented();
		// load the start code bit into mch
		break;
	case 0x783a: // md6 raio
		xxx_unimplemented();
		// enable r10 to iod
		break;
	case 0xe83a: // md2 chtm
		xxx_unimplemented();
		// load r9 to ios (maintenance)
		break;
	case 0x3a3a: // ena
		xxx_unimplemented();
		break;
	case 0x3c3a: // zcf, clear cf
		R[NUM_MCS] &= ~( MCS_CF0|MCS_CF1 );
		break;
	case 0xcc3a: // zds, clear ds
		R[NUM_MCS] &= ~( MCS_DS0|MCS_DS1 );
		break;
	case 0xca3a: // ztr1, clear tr1
		R[NUM_MCS] &= ~( MCS_TR10|MCS_TR11 );
		break;
	case 0xc93a: // ztr2, clear tr2
		R[NUM_MCS] &= ~( MCS_TR20|MCS_TR21 );
		break;
	case 0x1b3a: // zdr, clear dr
		R[NUM_MCS] &= ~( MCS_DR0|MCS_DR1 );
		break;

// row 5, 3c
	case 0x273c: // abrg
		xxx_unimplemented();
		break;
	case 0x2b3c: // abr4
		xxx_unimplemented();
		break;
	case 0x2d3c: // abr8
		xxx_unimplemented();
		break;
	case 0x1d3c: // abr12
		xxx_unimplemented();
		break;
	case 0xd83c: break; // spare
	case 0xb83c: break; // spare
	case 0x783c: // md7 spa
		xxx_unimplemented();
		// spare io control
		break;
	case 0xe83c: // md3 chc
		xxx_unimplemented();
		// load r9 to ios
		break;
	case 0x3a3c: break; // spare
	case 0x3c3c: // tbr0
		xxx_unimplemented();
		// test bit 0 of br
		// uctl [D]
		break;
	case 0xcc3c: // tint
		xxx_unimplemented();
		// test for interrupts
		// uctl [C]
		break;
	case 0xca3c: // tpar
		xxx_unimplemented();
		// test parity bit in br
		// uctl [A]
		break;
	case 0xc93c: // sib => sir1
		xxx_unimplemented();
		break;
	case 0x1b3c: // tch
		xxx_unimplemented();
		// test io channel
		// uctl [B]
		break;

// row 6, c9
	case 0x27c9: // bar0
		xxx_unimplemented();
		break;
	case 0x2bc9: // bar1
		xxx_unimplemented();
		break;
	case 0x2dc9: // bar2
		xxx_unimplemented();
		break;
	case 0x1dc9: // bar3
		xxx_unimplemented();
		break;
	case 0xd8c9: // zdidk
		xxx_unimplemented();
		// clear di1 and dk1
		break;
	case 0xb8c9: // dk1 => sdr1
		xxx_unimplemented();
		// gate dk1 to sdr
		break;
	case 0x78c9: // di1 => sdr1
		xxx_unimplemented();
		// gate di1 to sdr
		break;
	case 0xe8c9: // br => pt
		xxx_unimplemented();
		break;
	case 0x3ac9: break; // spare
	case 0x3cc9: // opf => ds
		xxx_unimplemented();
		// gate opf to ds
		break;
	case 0xccc9: break; // spare
	case 0xcac9: break; // spare
	case 0xc9c9: // tmarp
		xxx_unimplemented();
		break;
	case 0x1bc9: // exec
		xxx_unimplemented();
		// load a new opcode with servicing interrupts
		break;

// row 7, ca
	case 0x27ca: // sbr
		// sbr: set BR data bits, and parity bits
		R[NUM_BR0] = M_PH | M_PL | M_R20;
		// BR1 doesn't have parity bits, cite sd-1c900-01 sh b2ga loc e1
		R[NUM_BR1] = M_R20;
		printf("sbr: br0 %o, br1 %o\n", R[NUM_BR0], R[NUM_BR1]);
		break;
	case 0x2bca: // zbr
		// zbr: clear BR data bits, set ph/pl bits
		printf("zbr\n");
		R[NUM_BR0] = 0 | M_PH | M_PL;
		// BR1 doesn't have parity bits, cite sd-1c900-01 sh b2ga loc e1
		R[NUM_BR1] = 0 ;
		break;
	case 0x2dca: // idswq
		xxx_unimplemented();
		// idle the switch sequencer in the mch
		break;
	case 0x1dca: // sstp
		xxx_unimplemented();
		// set the stop bit
		break;
	case 0xd8ca: // stpasw
		xxx_unimplemented();
		// initiate a stop and switch to the other cc
		break;
	case 0xb8ca: // sba
		xxx_unimplemented();
		// set the ba check list
		break;
	case 0x78ca: // sd => dk+dk1; sa => ak (note 2)
		xxx_unimplemented();
		break;
	case 0xe8ca: // sd => di+di1
		xxx_unimplemented();
		break;
	case 0x3aca: // srw
		xxx_unimplemented();
		break;
	case 0x3cca: // dfetch
		xxx_unimplemented();
		break;
	case 0xccca: // sseiz
		xxx_unimplemented();
		break;
	case 0xcaca: // br => mms xxx
		// B4GB, loc B0 & B5
		R[NUM_MMSR] = R[NUM_BR0] & 0xfff;
		break;
	case 0xc9ca: // erar => mar
		xxx_unimplemented();
		// perform an error microsubroutine by gating erar => mar
		// erarmar00: b1gb, loc d4
		// b1gd, loc g1
		break;
	case 0x1bca: // sbpc
		xxx_unimplemented();
		break;
	}

aftermd:
	; // thanks iso
}
	/* ==== CLOCK PHASE 1 START ==== */
	/* ==== CLOCK PHASE 1 START ==== */
	/* ==== CLOCK PHASE 1 START ==== */
	/* ==== CLOCK PHASE 1 START ==== */

	/* **** microinstruction pipeline, stage 1 **** */
void seg0_p1(void) {}
	/* **** microinstruction pipeline, stage 2 **** */
void seg1_p1(void) {}

	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */

	/* **** microinstruction pipeline, stage 1 **** */
void seg0_p2(void) {}
	/* **** microinstruction pipeline, stage 2 **** */
void seg1_p2(void) {}

	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */

	/* **** microinstruction pipeline, stage 1 **** */
// microinstruction pipeline, stage 1
void seg0_p3(void) {
	/* many ff's are cleared late in the microcycle, during phase P3.
	 * cite: sh B1GH, loc F0-F8 */
	uff.alo = FALSE;
	uff.lint = FALSE;
	uff.lnop = FALSE;
	uff.lsir = FALSE;
	uff.malz = FALSE;
	uff.smint = FALSE;

	// "microstore output stable", sh b1gc
	// shove these into registers that phase 2 will see next cycle
	mir.q = UCODE[mar];
	R[NUM_MIR] = mir.q;
	R[NUM_MAR] = mar;

	mar_jam = 0;
}

	/* **** microinstruction pipeline, stage 2 **** */
void seg1_p3(void) {}

