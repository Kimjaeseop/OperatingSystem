#include <bitmap.h>
#include <limits.h>
#include <round.h>

/* Unit type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an unit represents bit K in the bitmap,
   then bit 1 in the unit represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long unit_type;


/* Number of bits in an unit. */
#define BIT_NUM (sizeof (unit_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of unit_type (defined above) that
   simulates an array of bits. */
struct bitmap
{
	size_t bit_count;     /* Number of bits. */
	unit_type *bits;    /* units that represent bits. */
};

/* Returns the index of the unit that contains the bit
   numbered BIT_INDEX. */
	static inline size_t
unit_index (size_t bit_index) 
{
	return bit_index / BIT_NUM;
}

/* Returns an unit_type where only the bit corresponding to
   BIT_INDEX is turned on. */
	static inline unit_type
bit_mask (size_t bit_index) 
{
	return (unit_type) 1 << (bit_index % BIT_NUM);
}

/* Returns the number of units required for BIT_COUNT bits. */
	static inline size_t
unit_count (size_t bit_count)
{
	return DIV_ROUND_UP (bit_count, BIT_NUM);
}

/* Returns the number of bytes required for BIT_COUNT bits. */
	static inline size_t
byte_count (size_t bit_count)
{
	return sizeof (unit_type) * unit_count (bit_count);
}

/* Returns a bit mask in which the bits actually used in the last
   unit of B's bits are set to 1 and the rest are set to 0. */
	static inline unit_type
end_mask (const struct bitmap *b) 
{
	int last_bits = b->bit_count % BIT_NUM;
	return last_bits ? ((unit_type) 1 << last_bits) - 1 : (unit_type) -1;
}

/* Creation and destruction. */

/* Creates and returns a bitmap with BIT_COUNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_COUNT). */
	struct bitmap *
create_bitmap (size_t bit_count, void *block, size_t block_size)
{
	struct bitmap *b = block; // 주소지정

	b->bit_count = bit_count;
	b->bits = (unit_type *) (b + 1);
	set_all_bitmap (b, false);
	return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_COUNT bits (for use with create_bitmap()). */
	size_t
bitmap_struct_size (size_t bit_count) 
{
	return sizeof (struct bitmap) + byte_count (bit_count);
}

/* Bitmap size. */

/* Returns the number of bits in B. */
	size_t
bitmap_size (const struct bitmap *b)
{
	return b->bit_count;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered INDEX in B to VALUE. */
	void
set_bitmap (struct bitmap *b, size_t index, bool value) 
{
	if (value)
		or_bitmap (b, index);
	else
		and_bitmap (b, index);
}

/* Atomically sets the bit numbered BIT_INDEX in B to true. */
	void
or_bitmap (struct bitmap *b, size_t bit_index) 
{
	size_t index = unit_index (bit_index);
	unit_type mask = bit_mask (bit_index);

	/* This is equivalent to `b->bits[index] |= mask' except that it
	   is guaranteed to be atomic on a uniprocessor machine.  See
	   the description of the OR instruction in [IA32-v2b]. */
	b->bits[index] |= mask;
}

/* Atomically sets the bit numbered BIT_INDEX in B to false. */
	void
and_bitmap (struct bitmap *b, size_t bit_index) 
{
	size_t index = unit_index (bit_index);
	unit_type mask = bit_mask (bit_index);

	/* This is equivalent to `b->bits[index] &= ~mask' except that it
	   is guaranteed to be atomic on a uniprocessor machine.  See
	   the description of the AND instruction in [IA32-v2a]. */
	b->bits[index] &= ~mask;
}

/* Atomically toggles the bit numbered INDEX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
	void
xor_bitmap (struct bitmap *b, size_t bit_index) 
{
	size_t index = unit_index (bit_index);
	unit_type mask = bit_mask (bit_index);

	/* This is equivalent to `b->bits[index] ^= mask' except that it
	   is guaranteed to be atomic on a uniprocessor machine.  See
	   the description of the XOR instruction in [IA32-v2b]. */
	b->bits[index] ^= mask;
}

/* Returns the value of the bit numbered INDEX in B. */
	bool
test_bitmap (const struct bitmap *b, size_t index) 
{
	return (b->bits[unit_index (index)] & bit_mask (index)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
	void
set_all_bitmap (struct bitmap *b, bool value) 
{
	set_multi_bitmap (b, 0, bitmap_size (b), value);
}

/* Sets the COUNT bits starting at FROM in B to VALUE. */
	void
set_multi_bitmap (struct bitmap *b, size_t from, size_t count, bool value) 
{
	size_t i;

	for (i = 0; i < count; i++)
		set_bitmap (b, from + i, value);
}

/* Returns the number of bits in B between FROM and FROM + COUNT,
   exclusive, that are set to VALUE. */
	size_t
bitmap_count (const struct bitmap *b, size_t from, size_t count, bool value) 
{
	size_t i, value_count;

	value_count = 0;
	for (i = 0; i < count; i++)
		if (test_bitmap (b, from + i) == value)
			value_count++;
	return value_count;
}

/* Returns true if any bits in B between FROM and FROM + COUNT,
   exclusive, are set to VALUE, and false otherwise. */
	bool
bitmap_contains (const struct bitmap *b, size_t from, size_t count, bool value) 
{
	size_t i;

	for (i = 0; i < count; i++)
		if (test_bitmap (b, from + i) == value)
			return true;
	return false;
}

/* Returns true if any bits in B between FROM and FROM + COUNT,
   exclusive, are set to true, and false otherwise.*/
	bool
bitmap_any (const struct bitmap *b, size_t from, size_t count) 
{
	return bitmap_contains (b, from, count, true);
}

/* Returns true if no bits in B between FROM and FROM + COUNT,
   exclusive, are set to true, and false otherwise.*/
	bool
bitmap_none (const struct bitmap *b, size_t from, size_t count) 
{
	return !bitmap_contains (b, from, count, true);
}

/* Returns true if every bit in B between FROM and FROM + COUNT,
   exclusive, is set to true, and false otherwise. */
	bool
bitmap_all (const struct bitmap *b, size_t from, size_t count) 
{
	return !bitmap_contains (b, from, count, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of COUNT
   consecutive bits in B at or after FROM that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
	size_t
find_bitmap (const struct bitmap *b, size_t from, size_t count, bool value) 
{
	if (count <= b->bit_count) 
	{
		size_t last = b->bit_count - count;
		size_t i;
		for (i = from; i <= last; i++)
			if (!bitmap_contains (b, i, count, !value))
				return i; 
	}

	return BITMAP_ERROR;
}

/* Finds the first group of COUNT consecutive bits in B at or after
   FROM that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If COUNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
	size_t
find_set_bitmap (struct bitmap *b, size_t from, size_t count, bool value)
{
	size_t index = find_bitmap (b, from, count, value);

	if (index != BITMAP_ERROR) 
		set_multi_bitmap (b, index, count, !value);
	return index;
}
