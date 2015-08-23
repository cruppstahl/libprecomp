
#include <stdint.h>
#include <assert.h>
#include <string.h> /* for memset */

/*
 * LOW-LEVEL BLOCK API
 *
 * TODO
 * - high-level API (compress many strings into several blocks, optimize
 *   many blocks)
 * - memory allocation; regular users want the library to allocate the memory,
 *   but for upscaledb the memory is allocated and the blocks have to be
 *   resized by the caller
 * - low-level format should be optimized for SIMD instructions, esp.
 *   select(), find()/find_lower_bound(), uncompress()
 */

/*
 * data format:
 *   prefix
 *   offset0
 *   size0
 *   offset1
 *   size1
 *   offset2
 *   size2
 *   ...
 *   offset(n+1)        points to unused data
 *   size(n+1)          unused, usually 0
 *   data0              (an position prefix_size + offset0)
 *   data1              (an position prefix_size + offset1)
 *   data2              (an position prefix_size + offset2)
 *   data3              (an position prefix_size + offset3)
 *   ...
 *
 *   etc
 *   -> dann könnte man sehr schnellen random access garantieren
 *      und schnelle binärsuche durchführen!
 *   -> bei delete und insert müssen alle offsets angepasst werden
 *   -> block->used wäre nicht nötig - ist das offset von n+1!
 */

/*
 * TODO TODO TODO
 * for small strings it would make sense to "instantiate" this file with
 * pre_size_t of 8 bit (uint8_t), for others with 16 bit or 32 bit!
 *
 * -> generate at compile time with macros (or with a perl script), and
 *  adjust the function names!
 * -> or dispatch the functions through the high-level API, but then the
 *  code is no longer inline
 */

/* this limits prefix and suffix sizes to 1<<16 */
typedef uint16_t pre_offset_t;
typedef uint16_t pre_size_t;

/*
 * WARNING
 * Make sure this structure does not require any packing!
 */
typedef struct prelo_index
{
  pre_offset_t offset;
  pre_size_t size;
} prelo_index_t;

/*
 * WARNING
 * Make sure this structure does not require any packing!
 */
typedef struct prelo_block
{
  uint32_t size;
  pre_size_t prefix_size;
  uint8_t length;
} prelo_block_t;

enum {
  PRE_ALREADY_EXISTS = -1,
  PRE_BLOCK_FULL = -2,
  PRE_NOT_FOUND = -3,
  PRE_NEEDS_REENCODE = -4
};

/*
 * Initializes a new block
 */
inline void
prelo_initialize(prelo_block_t *block, uint8_t *block_data)
{
  prelo_index_t *index;

  /* make sure that the prelo_block_t structure is not padded! */
  assert(sizeof(uint32_t)
       + sizeof(pre_size_t)
       + sizeof(uint8_t)
            == sizeof(prelo_block_t));

  /* make sure that the prelo_index_t structure is not padded! */
  assert(sizeof(pre_offset_t)
       + sizeof(pre_size_t)
            == sizeof(prelo_index_t));

  memset(block, 0, sizeof(*block));

  /* offset0 points to unused data */
  index = (prelo_index_t *)block_data;
  index->offset = sizeof(prelo_index_t);
  index->size = 0;
}

/*
 * Returns the length of the block (a.k.a the number of encoded strings)
 */
inline size_t
prelo_length(const prelo_block_t *block)
{
  return block->length;
}

/*
 * Returns the allocated size of the block
 */
inline size_t
prelo_allocated_size(const prelo_block_t *block)
{
  return block->size;
}

/*
 * Returns the actually used size of the block, which is usually smaller than
 * the allocated size
 */
extern size_t
prelo_used_size(const prelo_block_t *block, const uint8_t *block_data);

/*
 * Returns the size required to uncompress all strings
 */
extern size_t
prelo_uncompressed_size(const prelo_block_t *block, const uint8_t *block_data);

/*
 * Returns the prefix of the block
 */
inline const uint8_t *
prelo_prefix(const prelo_block_t *block, const uint8_t *block_data,
                size_t *pprefix_size)
{
  *pprefix_size = block->prefix_size;
  return block_data;
}

/*
 * Inserts a new string at |ptr| into a block
 *
 * Invariant: |ptr| and |block| have the same prefix.
 *
 * Returns the position of the string in the block, or
 *  PRE_ALREADY_EXISTS if the key already exists, or
 *  PRE_BLOCK_FULL     if the block has not enough space for the new string
 *  PRE_NEEDS_REENCODE if the new key does not share the block's prefix
 *                      and the block has to be re-encoded
 */
extern int
prelo_insert(prelo_block_t *block, uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size);

/*
 * Deletes a string from the block
 *
 * This function deletes a string, but the string's allocated memory is not
 * released and will not be reused until you call |prelo_vacuumize()|.
 *
 * Invariant: the block must store at least one compressed string.
 * Invariant: |ptr| and |block| have the same prefix.
 *
 * Returns the position of the deleted string, or
 *   PRE_NOT_FOUND     if the key was not found
 */
extern int
prelo_delete(prelo_block_t *block, uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size);

/*
 * Uncompresses the string at the given position, and stores the string
 * in |out|
 *
 * Returns the size of the uncompressed string. If |out_size| is too small
 * for the uncompressed string then the returned size is greater than
 * |out_size|.
 *
 * Example:
 *
 *    uint8_t out[...];
 *    size_t size = prelo_select(block, block_data, 0, &out[0], sizeof(out));
 *    if (size > sizeof(out))
 *      // try again, make sure that |out| can store at least |size| bytes!
 *    else
 *      // ok, string was decoded
 */
extern size_t
prelo_select(const prelo_block_t *block, const uint8_t *block_data,
                    int position, uint8_t *out, size_t out_size);

/*
 * Searches the block for |key|
 *
 * Returns the position of the string, or
 *   PRE_NOT_FOUND     if the key was not found
 */
extern int
prelo_find(const prelo_block_t *block, const uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size);

/*
 * Performs a lower-bound search for the block for |key|
 *
 * Returns the position of the string, or
 *   PRE_NOT_FOUND     if the key was not found
 */
extern int
prelo_find_lowerbound(const prelo_block_t *block, const uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size);

/*
 * Uncompresses all strings into a buffer
 *
 * |ptr| are pointers to the actual strings, and |ptr_sizes| are their
 * respective sizes. The pointers point into |data|.
 *
 * |ptr| and |ptr_sizes| must be large enough to store at least
 * |prelo_length()| elements.
 *
 * |data| must be large enough to store |prelo_uncompressed_size()| bytes.
 */
extern void
prelo_uncompress(const prelo_block_t *block, const uint8_t *block_data,
                    uint8_t **ptr, size_t *ptr_sizes, uint8_t *data_out);

/*
 * Tries to increase the size of the shared prefix, trying to reduce the
 * compressed size. This is a relatively cheap function and should be used
 * whenever a block overflows.
 *
 * This function does not rearrange the suffixes and therefore does not create
 * empty space. Use |prelo_vacuumize()| for this.
 *
 * Returns 0 if it was not possible to optimize the block (i.e. the prefix size
 * remains the same), otherwise > 0.
 */
extern size_t
prelo_grow_prefix(prelo_block_t *block, uint8_t *block_data);

/*
 * "Vacuumizes" the block; removes gaps to free space.
 *
 * Use this function after (successfully) growing the prefix with
 * |prelo_grow_prefix()|, or after deleting strings with |prelo_delete()|.
 */
extern void
prelo_vacuumize(prelo_block_t *block, uint8_t *block_data);


