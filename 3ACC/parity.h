#pragma once

/* ripped from
 * http://www.graphics.stanford.edu/~seander/bithacks.html#ParityParallel
 */
static inline int
paritybit(uint8_t b)
{
	b ^= b >> 4;
	b &= 017;
	return (0x6996 >> b) & 1;
}

static inline reg16
paritycalc16(reg16 r)
{
	
}

static inline reg20
paritycalc20(reg20 r)
{
	
}
