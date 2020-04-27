/* 3acc.c: AT&T 3A Central Control (3ACC) implementation

   Copyright 2020, Astrid Smith
*/

#include <sim_defs.h>

#include "3acc.h"

#define MEM_SIZE              (cpu_unit.capac)

t_stat cpu_reset(DEVICE* dptr);

uint32 R[NUM_REGISTERS];
/* these are the registers that are visible on the front panel.
 * microcode internal registers are not accessible to the user.
 */
REG cpu_reg[] = {
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
	{ NULL }
};

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
uint32* UCODE = NULL;
uint32* RAM = NULL;

t_stat
cpu_reset(DEVICE* dptr)
{
	if (! sim_is_running) {
		if (UCODE == NULL) {
			UCODE = (uint32*) calloc((size_t) (UCODE_LEN >> 2), sizeof(uint32));
		}
		if (RAM == NULL) {
			RAM = (uint32*) calloc((size_t)(MEM_SIZE >> 2), sizeof(uint32));
		}
	}

	return SCPE_OK;
}

t_stat
sim_instr(void)
{
	uint32 gb;
	microinstruction mi;

	// for now, the two-stage microinstruction pipeline isn't
	// properly interleaved.  that will come.

	/* **** microinstruction pipeline, stage 1 **** */
	uint32 mar = R[NUM_MAR];
	microinstruction mir;
	mir.q = R[NUM_MIR];

	// fundamentally, this microcycle needs to decide what the Next
	// Address should be, and load from the microstore to the MIR.

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
		/* return from instruction xxx */
	}

	if (uff.ru) {
		/* rar update (RU flipflop) is enabled: we are not in a
		 * microsubroutine */
		R[NUM_RAR] = mar;
	}

	if ( FALSE /* complement correction required xxx */) {
		// xxx: is this the previous mir (cu->micro.mir)
		// or the current mir (mir)?
		// xxx i wrote mar even though specs seem to have said mir in the past
		R[NUM_ERAR] = R[NUM_MAR];
		mar = 00777; // hardcoded, see sh B1GN loc E8
	}

	/* many ff's are cleared late in the microcycle, during phase P3.
	 * cite: sh B1GH, loc F0-F8 */
	uff.alo = FALSE;
	uff.lint = FALSE;
	uff.lnop = FALSE;
	uff.lsir = FALSE;
	uff.malz = FALSE;
	uff.smint = FALSE;

    // ==== PHASE 3
	// "microstore output stable", sh b1gc
	mir.q = UCODE[mar];
	R[NUM_MAR] = mar;

	/* **** microinstruction pipeline, stage 2 **** */

	/* ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 * ==== CLOCK PHASE 0 START ====
	 */

#define GB18(from) gb = from
#define GB22(from) gb = from

	/* == from field decoder ==
	 * ref. sd-1c900-01 sh D13 (note 312)
	 */
	switch ((uint8_t)mir.mi.from) {
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
		}
	}

#undef GB18
#undef GB22

	/* == to field decoder ==
	 * ref: sd-1c900-01 sh D13 (note 312)
	 */
#define GB18(to) to = gb
#define GB22(to) to = gb

	switch ((uint8_t)mir.mi.to) {
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
	case 0xa9: // gb => db [mr0] (22)
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
		// B2GM loc g0: inh parity
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
		}
	}
	

	/* == miscellaneous decoder ==
	 * ref: sd-1c900-01 sh D14 (note 313)
	 */
misc_dec:
	{
	uint32_t decode_pt = mir.mi.from << 8 + mir.mi.to;

	/* xxx concerned that this switch might be slow */
	switch (decode_pt) {

// row 0, d8
	case 0xd827: break; // spare
	case 0xd82b: break; // pymch xxx
		// gate bits from br to mchtr
	case 0xd82d: break; // hmrf xxx
		// hardware initialize the 3a cc
	case 0xd81d: break; // tmch xxx
		// test the mch
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
	case 0xe82b: break; // zer xxx
		// clear the error register
	case 0xe82d: break; // zmint xxx
		// clear the microinterpret bit
	case 0xe81d: break; // iocc xxx
		// interrupt the other cc
	case 0xe8d8: break; // zms xxx
		// clear the maintenance state register
	case 0xe8b8: break; // zpt xxx
		// clear the program timer
	case 0xe878: break; // spare
	case 0xe8e8: break; // spare
	case 0xe83a: break; // spare
	case 0xe83c: break; // ttr2 xxx
	case 0xe8cc: break; // tdr xxx
		// test dr
	case 0xe8ca: break; // ty xxx
	case 0xe8c9: break; // tx xxx
	case 0xe81b: break; // zru (B1GB) xxx
		uff.ru0 = FALSE;
		// clear ru

// row 2, 36
	case 0x3627: break; // spare
	case 0x362b: break; // iod => r11 xxx
		// gate the iod to r11
	case 0x362d: break; // s1db xxx
		// gate switch register 1 to the db
	case 0x361d: break; // imtc xxx
		// idle the mch
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
	case 0x36cc: break; // ds => cf xxx
		// gate ds to cf
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
	case 0x393c: break; // scf xxx
		// set cf
	case 0x39cc: break; // sds xxx
		// set ds
	case 0x39ca: break; // str1 xxx
		// set tr1
	case 0x39c9: break; // str2 xxx
		// set tr2
	case 0x391b: break; // sdr xxx
		// set dr

// row 4, 3a
	case 0x3a27: break; // spare
	case 0x3a2b: break; // spare
	case 0x3a2d: break; // mchb => mchtr xxx
		// gate mchb to mchtr [mdcmcbtr0]
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
	case 0x3a3c: break; // zcf xxx
		// clear cf
	case 0x3acc: break; // zds xxx
		// clear ds
	case 0x3aca: break; // ztr1 xxx
		// clear tr1
	case 0x3ac9: break; // ztr2 xxx
		// clear tr2
	case 0x3a1b: break; // zdr xxx
		// clear dr

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
	case 0xca27: break; // sbr xxx
	case 0xca2b: break; // zbr xxx
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

	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */
	/* ==== CLOCK PHASE 2 START ==== */

	

	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */
	/* ==== CLOCK PHASE 3 START ==== */


	return SCPE_OK;
}
