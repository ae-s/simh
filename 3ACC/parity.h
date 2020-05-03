#pragma once

/* ripped from
 * http://www.graphics.stanford.edu/~seander/bithacks.html#ParityParallel
 */
inline int paritybit(uint8_t b)
{
	b ^= b >> 4;
	b &= 017;
	return (0x6996 >> b) & 1;
}
