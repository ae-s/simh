#ifndef _3ACC_H_
#define _3ACC_H_

/***** First, the maintenance channel. *****/
#define MCH_CLER 0x65
#define MCH_CLMSR 0x35
#define MCH_CLPT 0xc5
#define MCH_CLTTO 0x2d
#define MCH_DISA 0xa5
#define MCH_DISB 0xe1
#define MCH_INITCLK 0x47
#define MCH_LDMAR 0x4d
#define MCH_LDMCHB 0x99
#define MCH_LDMIRH 0x8d
#define MCH_LDMIRL 0x1d
#define MCH_MSTART 0xc9
#define MCH_MSTOP 0xd1
#define MCH_RTNER 0x8b
#define MCH_RTNMB 0xa3
#define MCH_RTNMCHB 0xb1
#define MCH_RTNMMH 0x55
#define MCH_RTNMML 0x95
#define MCH_RTNSS 0x93
#define MCH_SPCLK 0x17
#define MCH_STCLK 0xc3
#define MCH_SWITCH 0x0f
#define MCH_TOGCLK 0x27


/***** Now some Registers Stuff *****/

/* Because of the pervasive parity checking in this processor, data
 * paths are the next size up from what they might seem to require.
 */
#define M_R16   0xffff
#define M_R12    0xfff

/* is this how addresses work? */
#define M_R20  0xfffff

// bits 20 and 21 are always parity
#define M_PH  0x200000
#define M_PL  0x100000

#define MM_R16 (M_PH|M_PL|M_R16)
#define MM_R20 (M_PH|M_PL|M_R20)

typedef uint16_t reg8;
typedef uint16_t reg12;
typedef uint32_t reg16;
typedef uint32_t reg20;

/* register numbers, after the 16 general purpose registers */
// xxx get the actual numbers - are those the [mrXX] numbers in 1c900?
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
#define NUM_PT NUM_TI
#define M_REG_PT 0xff00
#define M_REG_TI 0x00ff

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

// 44 already taken NUM_MCHTR

// front panel switch registers, i assume
#define NUM_SW1 45
#define NUM_SW2 46
#define NUM_SW3 47

// randomly chosen numbers for registers not otherwise listed
// instruction buffer
#define NUM_IB 48

// function register (in dml)
#define NUM_FR0 49
#define NUM_FR1 50
// seven bits, cite sd-1c900-01 sh b2gb loc c4
// dml is kinda weird, the function register doesn't have a bit 0
#define M_REG_FR  0376
#define M_FR_AD1    02
#define M_FR_ADS    04
#define M_FR_ADL   010
#define M_FR_ADD   016
#define M_FR_BOOL 0360

#define NUM_REGISTERS 51



#define MAXMEMSIZE 2<<20

#define NBIT(pos) 1<<pos
#define REGBITS_16(REG, N0, N1, N2, N3, N4, N5, N6, N7, N8, N9, N10, N11, N12, N13, N14, N15, PL, PH) \
        static const int REG ## _ ##  N0 = NBIT( 0); \
        static const int REG ## _ ##  N1 = NBIT( 1); \
        static const int REG ## _ ##  N2 = NBIT( 2); \
        static const int REG ## _ ##  N3 = NBIT( 3); \
        static const int REG ## _ ##  N4 = NBIT( 4); \
        static const int REG ## _ ##  N5 = NBIT( 5); \
        static const int REG ## _ ##  N6 = NBIT( 6); \
        static const int REG ## _ ##  N7 = NBIT( 7); \
        static const int REG ## _ ##  N8 = NBIT( 8); \
        static const int REG ## _ ##  N9 = NBIT( 9); \
        static const int REG ## _ ## N10 = NBIT(10); \
        static const int REG ## _ ## N11 = NBIT(11); \
        static const int REG ## _ ## N12 = NBIT(12); \
        static const int REG ## _ ## N13 = NBIT(13); \
        static const int REG ## _ ## N14 = NBIT(14); \
        static const int REG ## _ ## N15 = NBIT(15); \
        static const int REG ## _ ## PL = NBIT(16);  \
        static const int REG ## _ ## PH = NBIT(17);
#define REGBITS_HI(REG, N16, N17, N18, N19, PL, PH)  \
        static const int REG ## _ ## N16 = NBIT(16); \
        static const int REG ## _ ## N17 = NBIT(17); \
        static const int REG ## _ ## N18 = NBIT(18); \
        static const int REG ## _ ## N19 = NBIT(19); \
        static const int REG ## _ ## PL = NBIT(20);  \
        static const int REG ## _ ## PH = NBIT(21);

#define REGBITS_20(REG, N0, N1, N2, N3, N4, N5, N6, N7, N8, N9, N10, N11, N12, N13, N14, N15, N16, N17, N18, N19, PL, PH) \
    REGBITS_16(REG, N0, N1, N2, N3, N4, N5, N6, N7, N8, N9, N10, N11, N12, N13, N14, N15, xxxl, xxxh); \
    REGBITS_HI(REG, N16, N17, N18, N19, PL, PH);

// masking constants for the SS register
// for details see sd-1c900-01 sh b5gg
REGBITS_20(SR_SS,
           AME, BHC, BIN, BTC,
           DME, HLT, ISC1, ISC2,
           LOF, LON, MAN, MINT,
           CC, REJ, STOP, DISA,
           PRI, DISP, BPC, IPLTRK,
           CC0, CC1);

// masking constants for the MCS register
REGBITS_20(MCS,
		   CF0, CF1,
		   DS0, DS1,
		   TR10, TR11,
		   TR20, TR21,
		   DR0, DR1,
		   RU0, RU1,
		   IFF0, IFF1,
		   OPF0, OPF1,
		   MARP0, MARP1,
		   ERU0, ERU1,
		   PL, PH);

// masking constants for the ER register
REGBITS_20(SR_ER,
        T4o8, F4o8, // 4-of-8 decode errors
        IBPL, // bad low parity in IB register
        GBPT, // bad parity on gating bus
        DML,
        MARPT, // bad parity in MAR
        CLK, // clock error
        S0ERA, // store 0 (my), error A
        MARMT, // MAR matcher
        FNPT, // dml function register
        SERC, // any store, err C (bad parity)
        S0ERB, // store 0 (my), err B (write protect)
        S0TO, // store 0 (my), fast timeout, 38-50us
        BACK, // branch allow check fail
        S1ERB, // store 1 (other), err B (write protect)
        S1ERA, // store 1 (other), err A
        S1TO, // store 1 (other), fast timeout, 38-50us
        IOCS, // io main channel select error
        MCHPT, // program timer expiry
        MCHMR, // switch error received by online cc
        IOH, // io channel error
        IOPT // io parity error
           );

// masking constants for the interrupt register
// see sd-1c900-01 sh b5ge
REGBITS_16(SR_INTS,
        S0, // spares
        UTIL, // utility & test
        S2,
        PANMT, // panel match
        S4,
        CCER, // cc error
        S6,
        OCCI, // other cc interrupt
        S8, S9,
        TTYE, TTYO, // tty even & odd
        S12,
        PANL, // panel (key?)
        S14, S15,
        PL, PH);

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
