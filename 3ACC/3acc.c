/* 3acc.c: AT&T 3A Central Control (3ACC) implementation

   Copyright 2020, Astrid Smith
*/

#include <sim_defs.h>

#include <assert.h>

#include "3acc.h"
#include "parity.h"

#define MEM_SIZE              (cpu_unit.capac)

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

#define UOP(ca,cb, pta,pna, na, to,from) \
    (uint32) (((uint32)ca)<<31 | ((uint32)cb)<<30 | ((uint32)pta)<<29 | ((uint32)pna)<<28 | \
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
	R[NUM_MCHTR] = msg >> 8 & 0x3fffff;

	/* see sh B9GJ */
	switch (mcc) {
	case MCH_CLER:
		R[NUM_ER] = 0;
		break;
	case MCH_CLMSR:
		// clears the maintenance state register
		// xxx
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
		// xxx
		break;
	case MCH_DISA:
		// first i/o disable signal
		// xxx
		break;
	case MCH_DISB:
		// second i/o disable signal
		// xxx
		break;
	case MCH_INITCLK:
		// set the clock to phase 3
		// xxx
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
		break;
	case MCH_MSTART:
		// clear FRZ
		// xxx
		break;
	case MCH_MSTOP:
		R[NUM_SS] |= SR_SS_STOP;
		break;
	case MCH_RTNER:
		R[NUM_MCHTR] = R[NUM_ER];
		break;
	case MCH_RTNMB:
		// xxx
		break;
	case MCH_RTNMCHB:
		R[NUM_MCHTR] = R[NUM_MCHB];
		break;
	case MCH_RTNMMH:
		// xxx
		break;
	case MCH_RTNMML:
		// xxx
		break;
	case MCH_RTNSS:
		R[NUM_MCHTR] = R[NUM_SS];
		break;
	case MCH_SPCLK:
		// xxx
		break;
	case MCH_STCLK:
		// xxx
		break;
	case MCH_SWITCH:
		// hardware initialization
		cpu_hardware_switch();
		break;
	case MCH_TOGCLK:
		// advance through one half clock phase
		// xxx
		break;
	default: break;
	}

	return mcc | (R[NUM_MCHTR] << 8);
}

// handwritten boot microcode, from a textual description, in leiu of
// a dump which hasn't been made yet.
void
load_stub_ucode(void)
{
    int pos = 0;

    pos = 0277;

    // ZRU i guess
    UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0x81, 0xeb);

    // microcode checklist copied from PK-1C900 page B21

    // (a) Initialize RAR.
	//UCODE[pos++] = UOP();
    // (b) Save SS —> AI.
    UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0xa3, 0x8e);
    // (c) Clear MRFMCH INHIBIT FF and decoder maintenance status.
	// IMTC%
	UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0x1d, 0x36);
    // (d) Initialize the first 10 (0-9) I/O main channels.
    // (e) Initialize the IB and the function register.
    // (f) Zero SDR1.
    // (g) Save: PA —> AK, IS —> DI, IM --> DK.
	UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0x96, 0x9c); // pa => gb => ak
	UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0x8b, 0xa5); // is => gb => di
	UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0x87, 0xa5); // im => gb => dk
    // (h) Set interrupt mask to allow panel and other CC interrupts only.
    // (i) Zero the opcode FIL and I bits.
    UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0xd8, 0xcc); // zopf
    UCODE[pos++] = UOP(0,0, 0,0, pos+1, 0xd8, 0xca); // zi

    // (j) Zero SS register bits AME, DME, HLT, DISP, REJ, and SP2.
    // (k) Clear the IS.
    // (l) Set the main memory status register equal to 3CB0.
    // (m) If the RESET CKT FF equals 1, zero the BIN FF, zero the CC FF, and go to the HALT loop. If the RESET CKT FF equals 0, go to (n) .
    // (n) Enable I/O and then send a main store initialization message twice.
    // (o) If the ISC1 equal to 0, set ISC1 equal to 1 and begin the main memory initialization program at location 20 hex (40 octal) If the ISC1 equals 1, idle the maintenance channel switch sequencer and proceed to (p)
    // (p) If ISC2 equals 0, set ISC2 equal to 1, set ISC1 equal to and stop and switch to other CC. If ISC2 equals 1, SS —> BR and reload main memory from tape (IPL SEQ).
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

#include "ucode-list1.h"

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
	seg1_p0();

	seg0_p1();
	seg1_p1();

	seg0_p2();
	seg1_p2();

	seg0_p3();
	seg1_p3();

	return SCPE_OK;
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

	if (mir.mi.na == 0xfff) {
		/* all-ones detector: load the RAR into the MIR; return from
		 * microsubroutine */
		mar = R[NUM_RAR];
		uff.ru = TRUE;
	} else if (mir.mi.na == 0x000) {
		/* return from instruction */
		// xxx
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

#define GB18(from) gb = from
#define GB22(from) gb = from

	/* == from field decoder ==
	 * ref. sd-1c900-01 sh D13 (note 312)
	 */
	switch ((uint8_t)mymir.mi.from) {
		/* first 16 listed are 1o4 L and 3o4 R */
	case 0x17: // f1o4l1+f3o4r7 (B1GB)
		// RAR(0-11) => GB(8-19)
		// [zeros in GB(0-7)]
		gb = R[NUM_RAR] << 8;
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
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // cr => gb (22)
		GB22(R[NUM_CR]);
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
		// xxx: is it inhibited by the hardware or by microcode?
		// xxx
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
		// XXX
		GB22(R[NUM_ER]);
		break;
	case 0xac: // spare [mr11]
		break;

	case 0xc3: // sirc => gb (18)
		// XXX
		break;
	case 0xc5: // sdrc => gb (18)
		// XXX
		break;
	case 0xc6: // ib => gb (18)
		// XXX
		break;
	case 0xc9: // misc dec row 6
	case 0xca: // misc dec row 7
		goto misc_dec;
	case 0xcc: // PA+1
		// XXX
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
		// 16-bit words.

		// xxx
		break;
	case 0xb2: // spare
		break;
	case 0xb4: // dml => gb (22)
		// xxx
		break;
	case 0xb8: // ar(16-19, ph4) => gb(0-3, pl)
		// other part of operation described in 0xb1
		// xxx
		break;

	case 0xd1:
		// ar(0-15,pl) => gb(0-15,pl)
		// ar(ph) xor ar(ph4) => gb(ph)
		// xxx
		break;
	case 0xd2: // ibyt => gb (18)
		// xxx ??? wtf is ibyt
		break;
	case 0xd4: // ibxt => gb (18)
		// xxx ?? wtf is ibxt
		break;
	case 0xd8: // misc dec row 0
		goto misc_dec;

	case 0xe1: // really complicated
		// xxx
		break;
	case 0xe2: // br => gb (22)
		GB22(R[NUM_BR0]);
		break;
	case 0xe4: // really also complicated
		// xxx
		break;
	case 0xe8: // misc dec row 1
		goto misc_dec;


		/* next we have the all-zeros conditions */

	default:
		if (mir.mi.from & 0xf0 == 0) {
			// f4o4l0
			// nop
		} else if (mir.mi.from & 0x0f == 0) {
			// f4o4r0
			// spare
		} else {
			// xxx from field decode error
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
		// xxx
		break;
	case 0x1b: // misc dec col 13
	case 0x1d: // misc dec col 3
		goto misc_dec;
	case 0x1e: // gb => sir0 (18)
		// xxx
		break;
	case 0x27: // misc dec col 0
	case 0x2b: // misc dec col 1
	case 0x2d: // misc dec col 2
		goto misc_dec;
	case 0x2e: // gb => sdr0
		// xxx
		break;
	case 0x47: // gb => r12 (18)
		GB18(R[12]);
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
		// xxx
		break;
	case 0x8d: // gb => ms [mr14]
		GB18(R[NUM_MS]);
		break;
	case 0x8e: // gb => ss_st [mr15] (22)
		// xxx
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // gb => cr (22)
		GB22(R[NUM_CR]);
		break;
	case 0x35: // gb => hg (18)
		GB18(R[NUM_HG]);
		break;
	case 0x36: // gb => is_cl (18)
		// xxx
		break;
	case 0x39: // gb => is_s (18)
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
		printf("mchb loaded from gb %d\n", gb);
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
		// xxx
		break;

	case 0xc3: // gb => sard (22)
		// conditional on ff:dr=1
		// xxx
		break;
	case 0xc5: // gb => sari (22)
		// conditional on ff:dr=1
		// xxx
		break;
	case 0xc6: // gb => ib (18)
		// xxx
		break;
	case 0xc9: // misc dec col 12
	case 0xca: // misc dec col 11
	case 0xcc: // misc dec col 10
		goto misc_dec;

		/* next 16 listed are 3o4 L and 1o4 R */

	case 0x71: // gb => br (22)
		GB22(R[NUM_BR0]);
		GB22(R[NUM_BR1]);
		// yes we have two DML
		// B2GM loc g0: inh parity xxx
		break;
	case 0x72: // complex gating
		// xxx
		break;
	case 0x74: // complex gating
		// xxx
		break;
	case 0x78: // misc dec col 6
		goto misc_dec;

	case 0xb1: // gb => ar (22)
		// B2GM loc g0: inh parity
		GB22(R[NUM_AR0]);
		break;
	case 0xb2: // gb => ar0 (22)
		// xxx
		break;
	case 0xb4: // complex gating
		// xxx
		break;
	case 0xb8: // misc dec col 5
		goto misc_dec;

	case 0xd1: // complex gating
		// B2GM loc g0: inh parity
		// xxx
		break;
	case 0xd2: // spare
		break;
	case 0xd4: // complex gating
		// xxx
		break;
	case 0xd8: // misc dec col 4
		goto misc_dec;

	case 0xe1: // gb => mchc (0-7) (no parity)
		// xxx
		break;
	case 0xe2: // gb => sib1 (18)
		// B2GM loc g0: inh parity
		// xxx
		break;
	case 0xe4: // gb => sdr1 (18)
		// xxx
		break;
	case 0xe8: // misc dec col 7
		goto misc_dec;


		/* next we have the all-zeros conditions */

	default:
		if (mir.mi.from & 0xf0 == 0) {
			// nop
		} else if (mir.mi.from & 0x0f == 0) {
			// gate br(15) to br(16-19)
			// xxx
		} else {
			// xxx to field decode error
		}
	}
	

	/* == miscellaneous decoder ==
	 * ref: sd-1c900-01 sh D14 (note 313)
	 */
misc_dec:
	;
	// i got the bytes backwards in the case statement but i'm not
	// motivated to fix it
	uint32_t decode_pt = ntohs(mymir.q & 0xffff);

	/* xxx concerned that this switch might be slow */
	switch (decode_pt) {

// row 0, d8
	case 0xd827: break; // spare
	case 0xd82b: // pymch
		// gate parity bits from br0 to mchtr
		R[NUM_MCHTR] = R[NUM_BR0] && (M_PH|M_PL);
		break;
	case 0xd82d: // hmrf
		// hardware initialize the 3a cc
        cpu_hmrf();
        break;
	case 0xd81d: break; // tmch xxx
		// test the mch
        // gate MCH status to the C register
        // see MDTMCH0 on sh B9GE loc A4
	case 0xd8d8: break; // idlmch xxx
		// idle the mch
	case 0xd8b8: break; // stmch xxx
	case 0xd878: break; // spare
	case 0xd8e8: break; // spare
	case 0xd83a: break; // spare
	case 0xd83c: break; // sopf xxx
		// set opf
	case 0xd8cc: break; // zopf xxx
		// clear opf
	case 0xd8ca: break; // zi xxx
		// clear i bit
	case 0xd8c9: break; // ti xxx
		// test i bit
	case 0xd81b: break; // si xxx
		// set i bit

// row 1, e8
	case 0xe827: break; // spare
	case 0xe82b: // zer, clear the error register
        R[NUM_ER] = 0; // xxx this was "0 | M", what is that?
        break;
	case 0xe82d: break; // zmint, clear the microinterpret bit
		R[NUM_SS] &= SR_SS_MINT;
		break;
	case 0xe81d: break; // iocc xxx
		// interrupt the other cc
	case 0xe8d8: break; // zms xxx
		// clear the maintenance state register
	case 0xe8b8: // zpt, clear the program timer
		R[NUM_TI] &= M_REG_TI;
		break;
	case 0xe878: break; // spare
	case 0xe8e8: break; // spare
	case 0xe83a: break; // spare
	case 0xe83c: break; // ttr2 xxx
	case 0xe8cc: break; // tdr xxx
		// test dr
	case 0xe8ca: break; // ty xxx
	case 0xe8c9: break; // tx xxx
	case 0xe81b: // zru (B1GB), clear ru
		R[NUM_MCS] &= ~( MCS_RU0|MCS_RU1 );
        break;

// row 2, 36
	case 0x3627: break; // spare
	case 0x362b: break; // iod => r11 xxx
		// gate the iod to r11
	case 0x362d: break; // s1db xxx
		// gate switch register 1 to the db
	case 0x361d: // imtc
		// idle the mch
		// force-clear INRFMCH, sh B7GG loc F1
		break; // xxx
	case 0x36d8: break; // br => ti xxx
		// gate br to ti (16)
	case 0x36b8: break; // dml1 => cr xxx
		// gate output of dml1 to cr
	case 0x3678: break; // md4 stch xxx
		// start io
	case 0x36e8: break; // md0 idch xxx
		// idle io serial channel
	case 0x363a: break; // spare
	case 0x363c: break; // ttr1 xxx
	case 0x36cc: // ds => cf
		// gate ds flag to cf
		R[NUM_MCS] &= ~MCS_CF0;
		R[NUM_MCS] |= (R[NUM_MCS] & MCS_DS0) >> 1;
		break;
	case 0x36ca: break; // tds xxx
	case 0x36c9: break; // tcf xxx
		// test cf
	case 0x361b: break; // tflz xxx
		// test for low zero in ar

// row 3, 39
	case 0x3927: break; // spare
	case 0x392b: break; // sbtc xxx
		// set block timer check bit
	case 0x392d: break; // s2db xxx
		// gate switch register 2 to the db
	case 0x391d: break; // sdis xxx
		// set the disable flip-flop
	case 0x39d8: break; // inctc xxx
		// increment the timing counter
	case 0x39b8: break; // incpr xxx
		// increment the prescaler (part of tc)
	case 0x3978: break; // md5 eio5 xxx
		// enabli ios to iod
	case 0x39e8: break; // md1 chtn xxx
		// load r9 to ios (normal)
	case 0x393a: break; // enb xxx
	case 0x393c: // scf
		// set cf
		R[NUM_MCS] |= MCS_CF0|MCS_CF1;
		break;
	case 0x39cc: // sds, set ds
		R[NUM_MCS] |= MCS_DS0|MCS_DS1;
		break;
	case 0x39ca: // str1, set tr1
		R[NUM_MCS] |= MCS_TR10|MCS_TR11;
		break;
	case 0x39c9: // str2, set tr2
		R[NUM_MCS] |= MCS_TR20|MCS_TR21;
		break;
	case 0x391b: // sdr, set dr
		R[NUM_MCS] |= MCS_DR0|MCS_DR1;
		break;

// row 4, 3a
	case 0x3a27: break; // spare
	case 0x3a2b: break; // spare
	case 0x3a2d: // mchb => mchtr
		// gate mchb to mchtr [mdcmcbtr0]
		R[NUM_MCHTR] = R[NUM_MCHB];
		break;
	case 0x3a1d: break; // s3db xxx
		// gate switch register 3 to db
	case 0x3ad8: break; // rar => fn xxx
		// gate rar to fn [mdrarfn0]
	case 0x3ab8: break; // mclstr xxx
		// load the start code bit into mch
	case 0x3a78: break; // md6 raio xxx
		// enable r10 to iod
	case 0x3ae8: break; // md2 chtm xxx
		// load r9 to ios (maintenance)
	case 0x3a3a: break; // ena xxx
	case 0x3a3c: // zcf, clear cf
		R[NUM_MCS] &= ~( MCS_CF0|MCS_CF1 );
		break;
	case 0x3acc: // zds, clear ds
		R[NUM_MCS] &= ~( MCS_DS0|MCS_DS1 );
		break;
	case 0x3aca: // ztr1, clear tr1
		R[NUM_MCS] &= ~( MCS_TR10|MCS_TR11 );
		break;
	case 0x3ac9: // ztr2, clear tr2
		R[NUM_MCS] &= ~( MCS_TR20|MCS_TR21 );
		break;
	case 0x3a1b: // zdr, clear dr
		R[NUM_MCS] &= ~( MCS_DR0|MCS_DR1 );
		break;

// row 5, 3c
	case 0x3c27: break; // abrg xxx
	case 0x3c2b: break; // abr4 xxx
	case 0x3c2d: break; // abr8 xxx
	case 0x3c1d: break; // abr12 xxx
	case 0x3cd8: break; // spare
	case 0x3cb8: break; // spare
	case 0x3c78: break; // md7 spa xxx
		// spare io control
	case 0x3ce8: break; // md3 chc xxx
		// load r9 to ios
	case 0x3c3a: break; // spare
	case 0x3c3c: break; // tbr0 xxx
		// test bit 0 of br
	case 0x3ccc: break; // tint xxx
		// test for interrupts
	case 0x3cca: break; // tpar xxx
		// test parity bit in br
	case 0x3cc9: break; // sib => sir1 xxx
	case 0x3c1b: break; // tch xxx
		// test io channel

// row 6, c9
	case 0xc927: break; // bar0 xxx
	case 0xc92b: break; // bar1 xxx
	case 0xc92d: break; // bar2 xxx
	case 0xc91d: break; // bar3 xxx
	case 0xc9d8: break; // zdidk xxx
		// clear di1 and dk1
	case 0xc9b8: break; // dk1 => sdr1 xxx
		// gate dk1 to sdr
	case 0xc978: break; // di1 => sdr1 xxx
		// gate di1 to sdr
	case 0xc9e8: break; // br => pt xxx
	case 0xc93a: break; // spare
	case 0xc93c: break; // opf => ds xxx
		// gate opf to ds
	case 0xc9cc: break; // spare
	case 0xc9ca: break; // spare
	case 0xc9c9: break; // tmarp xxx
	case 0xc91b: break; // exec xxx
		// load a new opcode with servicing interrupts

// row 7, ca
	case 0xca27: // sbr: set BR data bits, and parity bits
		R[NUM_BR0] = M_R20 | M_PH | M_PL;
		R[NUM_BR1] = M_R20;
		break;
	case 0xca2b: // zbr
		// zbr: clear BR data bits, set ph/pl bits
		R[NUM_BR0] = 0 | M_PH | M_PL;
		// BR1 doesn't have parity bits, cite sd-1c900-01 sh b2ga loc e1
		R[NUM_BR1] = 0 ;
		break;
	case 0xca2d: break; // idswq xxx
		// idle the switch sequencer in the mch
	case 0xca1d: break; // sstp xxx
		// set the stop bit
	case 0xcad8: break; // stpasw xxx
		// initiate a stop and switch to the other cc
	case 0xcab8: break; // sba xxx
		// set the ba check list
	case 0xca78: break; // sd => dk+dk1; sa => ak (note 2) xxx
	case 0xcae8: break; // sd => di+di1 xxx
	case 0xca3a: break; // srw xxx
	case 0xca3c: break; // dfetch xxx
	case 0xcacc: break; // sseiz xxx
	case 0xcaca: // br => mms xxx
		// B4GB, loc B0 & B5
		R[NUM_MMSR] = R[NUM_BR0] & 0xfff;
		break;
	case 0xcac9: break; // erar => mar xxx
		// perform an error microsubroutine by gating erar => mar
		// erarmar00: b1gb, loc d4
		// b1gd, loc g1
	case 0xca1b: break; // sbpc xxx
	}
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
