#ifndef _3ACC_H_
#define _3ACC_H_

/* Because of the pervasive parity checking in this processor, data
 * paths are the next size up from what they might seem to require.
 */
#define M_PH 0x20000;
#define M_PL 0x10000;
#define M_R16 0xffff;
#define M_R12 0xfff;

/* is this how addresses work? */
#define M_R20  0xfffff;
#define M_RPH 0x200000;
#define M_RPL 0x100000;
typedef uint16_t reg8;
typedef uint16_t reg12;
typedef uint32_t reg16;
typedef uint32_t reg20;

/* register numbers, after the 16 general purpose registers */
// xxx get the actual numbers
#define NUM_AI 31
#define NUM_AK 30
#define NUM_DI 29
#define NUM_DK 28
#define NUM_ER 27
#define NUM_HG 26
#define NUM_IM 25
#define NUM_IS 24
#define NUM_MCHB 23
#define NUM_MCHTR 44
#define NUM_MMSR 22
#define NUM_MCHC 17 // xxx what is this
#define NUM_MS 21
#define NUM_PA 20
#define NUM_SAR 19
#define NUM_SS 18
#define NUM_TI 17

#define NUM_MCS 16
// non visible registers are numbered 32 and up
#define NUM_DB 32

#define NUM_MIR 33
#define NUM_MAR 34
#define NUM_RAR 35
#define NUM_ERAR 36

#define NUM_AR0 37
#define NUM_AR1 38
#define NUM_BR0 39
#define NUM_BR1 40

#define NUM_CR 41

#define NUM_SIR 42
#define NUM_SDR 43

#define NUM_SW1 45
#define NUM_SW2 46
#define NUM_SW3 47

#define NUM_REGISTERS 48

// xxx add numbers for all the registers

#define MAXMEMSIZE 2<<20

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
		unsigned int from:8, to:8;
		unsigned int na:12;
		unsigned int pna:1, pta:1;
		unsigned int ca:1, cb:1;
	} mi;
	uint32_t q;
	uint16_t w[2];
	uint8_t b[4];
} microinstruction;

#endif /* _3ACC_H_ */
