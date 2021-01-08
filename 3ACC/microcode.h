#pragma once

#define UOP(pna, na, to,from, cacb, pta)                                \
	(uint32) (((uint32)cacb)<<30 |                                  \
                  ((uint32)pta)<<29 | ((uint32)pna)<<28 |               \
                  ((uint32)na)<<15 | ((uint32)to)<<7 | ((uint32)from))

#define ui(adr, pna, na, to, from, cacb, pta) \
	UCODE[0x##adr] = UOP(pna, 0x##na, 0x##to, 0x##from, cacb, pta)

// sed -e '/^$/d;/^\/\//d;s/^/ui(/;s/ /, /g;s/$/);/' < pk-4c002-01_iss01.txt > pk-4c002-01_iss01.h
// sed -e '/^[0-9a-f]/{s/^/ui(/;s/ /, /g;s/$/);/}' < pk-4c002-01_iss01.txt > pk-4c002-01_iss01.h


void init_ucode(void) {
#include "pk-4c002-01_iss01.h"
}
