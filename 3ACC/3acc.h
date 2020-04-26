#ifndef _3ACC_H_
#define _3ACC_H_

/* Because of the pervasive parity checking in this processor, data
 * paths are the next size up from what they might seem to require.
 */
#define PH 0x20000;
#define PL 0x10000;
#define R16 0xffff;
#define R12 0xfff;

/* is this how addresses work? */
#define R20  0xfffff;
#define RPH 0x200000;
#define RPL 0x100000;
typedef uint16 reg8;
typedef uint16 reg12;
typedef uint32 reg16;
typedef uint32 reg20;

#define NBIT(pos) 2<<pos
// masking constants for the SS register
static const int SR_SS_AME =    NBIT( 0);
static const int SR_SS_BHC =    NBIT( 1);
static const int SR_SS_BIN =    NBIT( 2);
static const int SR_SS_BTC =    NBIT( 3);
static const int SR_SS_DME =    NBIT( 4);
static const int SR_SS_HLT =    NBIT( 5);
static const int SR_SS_ISC1 =   NBIT( 6);
static const int SR_SS_ISC2 =   NBIT( 7);
static const int SR_SS_LOF =    NBIT( 8);
static const int SR_SS_LON =    NBIT( 9);
static const int SR_SS_MAN =    NBIT(10);
static const int SR_SS_MINT =   NBIT(11);
static const int SR_SS_CC =     NBIT(12);
static const int SR_SS_REJ =    NBIT(13);
static const int SR_SS_STOP =   NBIT(14);
static const int SR_SS_DISA =   NBIT(15);
static const int SR_SS_PRI =    NBIT(16);
static const int SR_SS_DISP =   NBIT(17);
static const int SR_SS_BPC =    NBIT(18);
static const int SR_SS_IPLTRK = NBIT(19);
static const int SR_SS_CC0 =    NBIT(20);
static const int SR_SS_CC1 =    NBIT(21);

// masking constants for the MCS register
static const int MCS_CF =  NBIT( 0);
static const int MCS_DS =  NBIT( 1);
static const int MCS_TR1 = NBIT( 2);
static const int MCS_TR2 = NBIT( 3);
static const int MCS_DR =  NBIT( 4);
static const int MCS_RU =  NBIT( 5);
static const int MCS_IFF = NBIT( 6);
static const int MCS_OPF = NBIT( 7);

typedef union {
	struct {
		uint from:8, to:8;
		uint na:12;
		uint pna:1, pta:1;
		uint ca:1, cb:1;
	} mi;
	uint32_t q;
	uint16_t w[2];
	uint8_t b[4];
} microinstruction;

struct proc {
	reg16 r[16]; /* general registers */
	reg16 gb; /* gating bus */

	struct {
		reg16 mms;
		reg16 sdr, sir;
		reg16 pa;
		reg16 sar;
	} memctl;

	struct {
		/* matcher registers */
		reg16 di, dk; /* data */
		reg20 ai, ak; /* address */

		reg20 db; /* display buffer */
	} panel;

	struct {
		/* dragons */
		uint32_t ucode[2**12];
		reg12 mar, rar, erar;

		reg16 mcs;

		struct {
			bool ru0, ru1, rub;
			bool alo;
			bool lint;
			bool lnop;
			bool lsir;
			bool malz;
			bool smint;
			bool dmint;
			bool bm;
			bool mpsm;
			bool pna;
			bool inh_ck;
			bool fnp;
			bool rarp;
		} ff;
	} micro;

	struct {
		reg20 mchtr;
		reg20 mchb;
		reg8 mchc;
	} mch;

	struct {
		reg16 is, im;
	} interrupt;

	struct {
		reg20 ar, br;
		reg8 fr;
	} dml[2];

	struct {
		reg20 cr;
		reg16 ms;
		reg16 hg;
		reg20 er;
		reg20 ss;
	} miscreg;

	/* io channels.  normally only #0 is equipped but there is no harm
	 * in having the other two idly exist.
	 */
	struct {
		reg12 ios;
		uint32 iod;
	} io[3];
};

#endif /* _3ACC_H_ */
