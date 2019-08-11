
#ifndef __BITMAP_H__
#define __BITMAP_H__

#define BITS_PER_LONG 64
#define BITS_TO_LONG(n) \
	(((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)


#define DECLARE_BITMAP(name, bits) \
	unsigned long name[BITS_TO_LONG(bits)]

static void inline bitmap_zero(unsigned long *bitmap, int bits)
{
	for (int i = 0; i < BITS_TO_LONG(bits); i++)
		bitmap[i] = 0;
}

static void inline bitmap_fill(unsigned long *bitmap, int bits)
{
	for (int i = 0; i < BITS_TO_LONG(bits); i++)
		bitmap[i] = ~0;
}

#define bitmap_set(b, n) \
	b[n / BITS_PER_LONG] |= 1 << (n % BITS_PER_LONG)

#define bitmap_clear(b, n) \
	b[n / BITS_PER_LONG] &= ~(1 << (n % BITS_PER_LONG))

#define bitmap_test(b, n) \
	(b[n / BITS_PER_LONG] & (1 << (n % BITS_PER_LONG))) != 0

#endif
