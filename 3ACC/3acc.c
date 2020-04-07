/* 3acc.c: AT&T 3A Central Control (3ACC) implementation

   Copyright 2020, Astrid Smith
*/

#include "3acc.h"

static SIM_INLINE microcycle(struct proc *cu)
{
	microinstruction mi;

	/* ==== PHASE 1 START ====
	 * ==== PHASE 1 START ====
	 * ==== PHASE 1 START ====
	 * ==== PHASE 1 START ====
	 */

	mir.q = cu->micro.ucode[cu->micro.mar];

#define GB18(from) cu->gb = from
#define GB22(from) cu->gb = from

	/* == from field decoder ==
	 * ref. sd-1c900-01 sh D13 (note 312)
	 */
	switch (mir.mi.from) {
		/* first 16 listed are 1o4 L and 3o4 R */
	case 0x17: // f1o4l1+f3o4r7
		// RAR(0-11) => GB(8-19)
		// [zeros in GB(0-7)]
		// XXX
		break;
	case 0x1b: // spare
		break;
	case 0x1d: // spare, sir1
		break;
	case 0x1e: // sir0 => gb(18)
		GB18(cu->memctl.sir);
		break;
	case 0x27: // mchc(0-7) => gb(0-7)
		// xxx
		break;
	case 0x2b: // spare
		break;
	case 0x2d: // spare, sdr1
		break;
	case 0x2e: // sdr0 => gb (18)
		GB18(cu->memctl.sdr);
		break;
	case 0x47: // r12 => gb (18)
		GB18(cu->r[12]);
		break;
	case 0x4d: // r14 => gb (18)
		GB18(cu->r[14]);
		break;
	case 0x4e: // r15 => gb (18)
		GB18(cu->r[15]);
		break;
	case 0x87: // im  [mr12] => gb (18)
		GB18(cu->interrupt.im);
		break;
	case 0x8b: // is(0-15) [mr13] => gb(0-15)
		cu->gb = cu->interrupt.is;
		break;
	case 0x8d: // ms [mr14] => gb (18)
		GB18(cu->miscreg.ms);
		break;
	case 0x8e: // ss [mr15] => gb (18)
		GB18(cu->miscreg.ss);
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // cr => gb (22)
		GB22(cu->miscreg.cr);
		break;
	case 0x35: // hg => gb (18)
		GB18(cu->miscreg.hg);
		break;
	case 0x36: // misc dec row 2
	case 0x39: // misc dec row 3
	case 0x3a: // misc dec row 4
	case 0x3c: // misc dec row 5
		goto misc_dec;
	case 0x53: // r0 => gb (18)
		GB18(cu->r[0]);
		break;
	case 0x55: // r1 => gb (18)
		GB18(cu->r[1]);
		break;
	case 0x56: // r2 => gb (18)
		GB18(cu->r[2]);
		break;
	case 0x59: // r3 => gb (18)
		GB18(cu->r[3]);
		break;
	case 0x5a: // r4 => gb (18)
		GB18(cu->r[4]);
		break;
	case 0x5c: // r5 => gb (18)
		GB18(cu->r[5]);
		break;

	case 0x63: // r6 => gb (18)
		GB18(cu->r[6]);
		break;
	case 0x65: // r7 => gb (18)
		GB18(cu->r[7]);
		break;
	case 0x66: // r8 => gb (18)
		GB18(cu->r[8]);
		break;
	case 0x69: // r9 => gb (18)
		GB18(cu->r[9]);
		break;
	case 0x6a: // r10 => gb (18)
		GB18(cu->r[10]);
		break;
	case 0x6c: // r11 => gb (18)
		GB18(cu->r[11]);
		break;

	case 0x93: // ti [mr0] => gb (16)
		// bus parity needs inhibit when using this path
		// xxx: is it inhibited by the hardware or by microcode?
		// xxx
		break;
	case 0x95: // sar [mr1] => gb (22)
		GB22(cu->memctl.sar);
		break;
	case 0x96: // pa [mr2] => gb (22)
		GB22(cu->miscreg.pa);
		break;
	case 0x99: // mchb [mr3] => gb (22)
		GB22(cu->mch.mchb);
		break;
	case 0x9a: // mms [mr4] => gb (12, pl, ph)
		// xxx
		break;
	case 0x9c: // ak [mr5] => gb (22)
		GB22(cu->panel.ak);
		break;

	case 0xa3: // ai [mr6] => gb (22)
		GB22(cu->panel.ai);
		break;
	case 0xa5: // dk [mr9] => gb (18)
		GB18(cu->panel.dk);
		break;
	case 0xa6: // di [mr8] => gb (18)
		GB18(cu->panel.di);
		break;
	case 0xa9: // db [mr9] => gb (22)
		GB22(cu->panel.db);
		break;
	case 0xaa: // er [mr10] => gb (22)
		// bus parity needs inhibit
		// XXX
		GB22(cu->miscreg.er);
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
		GB22(cu->dml[0].ar)
		break;
	case 0x72: // spare
		break;
	case 0x74: // mcs => gb (22)
		GB22(cu->micro.mcs);
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
		GB22(cu->dml.br);
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

#undefine GB18
#undefine GB22

	/* == to field decoder ==
	 * ref: sd-1c900-01 sh D13 (note 312)
	 */
#define GB18(to) to = cu->gb
#define GB22(to) to = cu->gb

	switch (mir.mi.to) {
		/* first 16 listed are 1o4 L and 3o4 R */
	case 0x17: // gb(8-19,ph) => rar(0-11,ph)
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
		GB18(cu->r[12]);
		break;
	case 0x4d: // gb => r14 (18)
		GB18(cu->r[14]);
		break;
	case 0x4e: // gb => r15 (18)
		GB18(cu->r[15]);
		break;
	case 0x87: // gb => im [mr12] (18)
		GB18(cu->interrupt.im);
		break;
	case 0x8b: // gb => ss_cl [mr13] (22)
		// xxx
		break;
	case 0x8d: // gb => ms [mr14]
		GB18(cu->miscreg.ms);
		break;
	case 0x8e: // gb => ss_st [mr15] (22)
		// xxx
		break;

		/* next 36 listed are 2o4 L and 2o4 R */

	case 0x33: // gb => cr (22)
		GB22(cu->miscreg.cr);
		break;
	case 0x35: // gb => hg (18)
		GB18(cu->miscreg.hg);
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
		GB18(cu->r[0]);
		break;
	case 0x55: // gb => r1 (18)
		GB18(cu->r[1]);
		break;
	case 0x56: // gb => r2 (18)
		GB18(cu->r[2]);
		break;
	case 0x59: // gb => r3 (18)
		GB18(cu->r[3]);
		break;
	case 0x5a: // gb => r4 (18)
		GB18(cu->r[4]);
		break;
	case 0x5c: // gb => r5 (18)
		GB18(cu->r[5]);
		break;

	case 0x63: // gb => r6 (18)
		GB18(cu->r[6]);
		break;
	case 0x65: // gb => r7 (18)
		GB18(cu->r[7]);
		break;
	case 0x66: // gb => r8 (18)
		GB18(cu->r[8]);
		break;
	case 0x69: // gb => r9 (18)
		GB18(cu->r[9]);
		break;
	case 0x6a: // gb => r10 (18)
		GB18(cu->r[10]);
		break;
	case 0x6c: // gb => r11 (18)
		GB18(cu->r[11]);
		break;

	case 0x93: // gb => mchtr [mr0] (22)
		GB22(cu->mch.mchtr);
		break;
	case 0x95: // gb => sar [mr1] (22)
		GB22(cu->memctl.sar);
		break;
	case 0x96: // gb => pa [mr2] (22)
		GB22(cu->memctl.pa);
		break;
	case 0x99: // gb => mchb [mr3] (22)
		GB22(cu->mch.mchb);
		break;
	case 0x9a: // spare
		break;
	case 0x9c: // gb => ak [mr5] (22)
		GB22(cu->panel.ak);
		break;

	case 0xa3: // gb => ai [mr6] (22)
		GB22(cu->panel.ai);
		break;
	case 0xa5: // gb => dk [mr7] (18)
		GB18(cu->panel.dk);
		break;
	case 0xa6: // gb => di [mr8] (18)
		GB18(cu->panel.di);
		break;
	case 0xa9: // gb => db [mr0] (22)
		GB22(cu->panel.db);
		break;
	case 0xaa: // gb => er [mr10] (22)
		GB22(cu->miscreg.er);
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
		GB22(cu->dml[0].br);
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
		GB22(cu->dml[0].ar);
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
misc_decoder:
	uint32 decode_pt = mir.mi.from << 8 + mir.mi.to;

	/* xxx concerned that this switch might be slow */
	switch (decode_pt) {

// row 0, d8
	case 0xd827: break; // spare
	case 0xd82b: break; // pymch xxx
	case 0xd82d: break; // hmrf xxx
	case 0xd81d: break; // tmch xxx
	case 0xd8d8: break; // idlmch xxx
	case 0xd8b8: break; // stmch xxx
	case 0xd878: break; // spare
	case 0xd8e8: break; // spare
	case 0xd83a: break; // spare
	case 0xd83c: break; // sopf xxx
	case 0xd8cc: break; // zopf xxx
	case 0xd8ca: break; // zi xxx
	case 0xd8c9: break; // ti xxx
	case 0xd81b: break; // si xxx

// row 1, e8
	case 0xe827: break; // spare
	case 0xe82b: break; // zer xxx
	case 0xe82d: break; // zmint xxx
	case 0xe81d: break; // iocc xxx
	case 0xe8d8: break; // zms xxx
	case 0xe8b8: break; // zpt xxx
	case 0xe878: break; // spare
	case 0xe8e8: break; // spare
	case 0xe83a: break; // spare
	case 0xe83c: break; // ttr2 xxx
	case 0xe8cc: break; // tdr xxx
	case 0xe8ca: break; // ty xxx
	case 0xe8c9: break; // tx xxx
	case 0xe81b: break; // zru xxx

// row 2, 36
	case 0x3627: break; // spare
	case 0x362b: break; // iod => r11 xxx
	case 0x362d: break; // s1db xxx
	case 0x361d: break; // imtc xxx
	case 0x36d8: break; // br => ti xxx
	case 0x36b8: break; // dml1 => cr xxx
	case 0x3678: break; // md4 stch xxx
	case 0x36e8: break; // md0 idcd xxx
	case 0x363a: break; // spare
	case 0x363c: break; // ttr1 xxx
	case 0x36cc: break; // ds => cf xxx
	case 0x36ca: break; // tds xxx
	case 0x36c9: break; // tcf xxx
	case 0x361b: break; // tflz xxx

// row 3, 39
	case 0x3927: break; // spare
	case 0x392b: break; // sbtc xxx
	case 0x392d: break; // s2db xxx
	case 0x391d: break; // sdis xxx
	case 0x39d8: break; // inctc xxx
	case 0x39b8: break; // incpr xxx
	case 0x3978: break; // md5 eios xxx
	case 0x39e8: break; // md1 chtn xxx
	case 0x393a: break; // enb xxx
	case 0x393c: break; // scf xxx
	case 0x39cc: break; // sds xxx
	case 0x39ca: break; // str1 xxx
	case 0x39c9: break; // str2 xxx
	case 0x391b: break; // sdr xxx

// row 4, 3a
	case 0x3a27: break; // spare
	case 0x3a2b: break; // spare
	case 0x3a2d: break; // mchb => mchtr xxx
	case 0x3a1d: break; // s3db xxx
	case 0x3ad8: break; // rar => fn xxx
	case 0x3ab8: break; // mclstr xxx
	case 0x3a78: break; // md6 raio xxx
	case 0x3ae8: break; // md2 chtm xxx
	case 0x3a3a: break; // ena xxx
	case 0x3a3c: break; // zcf xxx
	case 0x3acc: break; // zds xxx
	case 0x3aca: break; // ztr1 xxx
	case 0x3ac9: break; // ztr2 xxx
	case 0x3a1b: break; // zdr xxx

// row 5, 3c
	case 0x3c27: break; // abrg xxx
	case 0x3c2b: break; // abr4 xxx
	case 0x3c2d: break; // abr8 xxx
	case 0x3c1d: break; // abr12 xxx
	case 0x3cd8: break; // spare
	case 0x3cb8: break; // spare
	case 0x3c78: break; // md7 spa xxx
	case 0x3ce8: break; // md3 chc xxx
	case 0x3c3a: break; // spare
	case 0x3c3c: break; // tbr0 xxx
	case 0x3ccc: break; // tint xxx
	case 0x3cca: break; // tpar xxx
	case 0x3cc9: break; // sib => sir1 xxx
	case 0x3c1b: break; // tch xxx

// row 6, c9
	case 0xc927: break; // bar0 xxx
	case 0xc92b: break; // bar1 xxx
	case 0xc92d: break; // bar2 xxx
	case 0xc91d: break; // bar3 xxx
	case 0xc9d8: break; // zdidk xxx
	case 0xc9b8: break; // dk1 => sdr1 xxx
	case 0xc978: break; // di1 => sdr1 xxx
	case 0xc9e8: break; // br => pt xxx
	case 0xc93a: break; // spare
	case 0xc93c: break; // opf => ds xxx
	case 0xc9cc: break; // spare
	case 0xc9ca: break; // spare
	case 0xc9c9: break; // tmarp xxx
	case 0xc91b: break; // exec xxx

// row 7, ca
	case 0xca27: break; // sbr xxx
	case 0xca2b: break; // zbr xxx
	case 0xca2d: break; // idswq xxx
	case 0xca1d: break; // sstp xxx
	case 0xcad8: break; // stpasw xxx
	case 0xcab8: break; // sba xxx
	case 0xca78: break; // sd => dk+dk1; sa => ak xxx
	case 0xcae8: break; // sd => di+di1 xxx
	case 0xca3a: break; // srw xxx
	case 0xca3c: break; // dfetch xxx
	case 0xcacc: break; // sseiz xxx
	case 0xcaca: break; // br => mms xxx
	case 0xcac9: break; // erar => mar xxx
	case 0xca1b: break; // sbpc xxx
	}
}
