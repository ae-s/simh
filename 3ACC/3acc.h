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
