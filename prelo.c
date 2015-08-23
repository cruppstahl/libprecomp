#include "precomp.h"
#include <stdlib.h>

static inline int
__compare(const uint8_t *lhs, size_t lhs_size,
                    const uint8_t *rhs, size_t rhs_size)
{
  if (lhs_size < rhs_size) {
    int m = memcmp(lhs, rhs, lhs_size);
    return (m == 0 ? -1 : m);
  }
  if (rhs_size < lhs_size) {
    int m = memcmp(lhs, rhs, rhs_size);
    return (m == 0 ? +1 : m);
  }
  return memcmp(lhs, rhs, lhs_size);
}

static inline prelo_index_t *
__index(const prelo_block_t *block, const uint8_t *block_data, int position)
{
  prelo_index_t *it = (prelo_index_t *)(block_data + block->prefix_size);
  return (it + position);
}

static inline prelo_index_t *
__begin(const prelo_block_t *block, const uint8_t *block_data)
{
  return (prelo_index_t *)(block_data + block->prefix_size);
}

static inline prelo_index_t *
__end(const prelo_block_t *block, const uint8_t *block_data)
{
  return __begin(block, block_data) + block->length + 1;
}

/*
 * TODO
 * use binary search instead of linear search!
 */
static int
__find_position(const prelo_block_t *block, const uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size, int *presult)
{
  const uint8_t *data = block_data;
  prelo_index_t *begin = __begin(block, block_data);
  prelo_index_t *end = __end(block, block_data);
  prelo_index_t *it;

  /* invariant: the prefix of |ptr| and of the block are identical; we can */
  /* focus on the suffix and ignore the prefix */
  data += block->prefix_size;
  ptr += block->prefix_size;
  ptr_size -= block->prefix_size;

  for (it = begin; it < end; it++) {
    int cmp = __compare(&data[it->offset], it->size, ptr, ptr_size);
    if (cmp == 0) {
      *presult = PRE_ALREADY_EXISTS;
      return it - begin;
    }
    if (cmp < 0) {
      *presult = PRE_NOT_FOUND;
      return it - begin;
    }
  }

  *presult = PRE_NOT_FOUND;
  return block->length;
}

size_t
prelo_used_size(const prelo_block_t *block, const uint8_t *block_data)
{
  return __index(block, block_data, block->length)->offset;
}

size_t
prelo_uncompressed_size(const prelo_block_t *block, const uint8_t *block_data)
{
  size_t total = 0;
  uint32_t prefix_size = block->prefix_size;
  prelo_index_t *it = __begin(block, block_data);
  prelo_index_t *end = __end(block, block_data);

  for (; it < end; it++) {
    total += prefix_size + it->size;
  }

  return total;
}

int
prelo_insert(prelo_block_t *block, uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size)
{
  int result;
  int position;
  size_t suffix_size;
  prelo_index_t *pindex;
  pre_offset_t new_offset;

  /* invariant: common prefix of |ptr| and |block|! */
  assert(ptr_size >= block->prefix_size
      && memcmp(ptr, block_data, block->prefix_size) == 0);

  position = __find_position(block, block_data, ptr, ptr_size, &result);

  if (result != PRE_NOT_FOUND)
    return result;

  suffix_size = ptr_size - block->prefix_size;
  new_offset = (pre_offset_t)prelo_used_size(block, block_data);

  /* check if the suffix fits into the block */
  if (new_offset + suffix_size + sizeof(prelo_index_t) > block->size)
    return PRE_BLOCK_FULL;

  /* create a gap for the new offset */
  pindex = __index(block, block_data, position);
  memmove(pindex + 1, pindex, new_offset - ((uint8_t *)pindex - block_data));
  pindex->offset = new_offset;
  pindex->size = suffix_size;

  /* now write the new string's data */
  memcpy(block_data + new_offset, ptr + block->prefix_size, suffix_size);

  block->length++;
  return position;
}

int
prelo_delete(prelo_block_t *block, uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size)
{
  int result;
  int position;
  prelo_index_t *pindex;

  /* invariant: common prefix of |ptr| and |block|! */
  assert(ptr_size >= block->prefix_size
      && memcmp(ptr, block_data, block->prefix_size) == 0);

  position = __find_position(block, block_data, ptr, ptr_size, &result);

  if (result != PRE_ALREADY_EXISTS)
    return result;

  /* remove the index */
  pindex = __index(block, block_data, position);
  memmove(pindex, pindex + 1,
          sizeof(prelo_index_t) * (block->length - position - 1));

  block->length--;
  return position;
}

size_t
prelo_select(const prelo_block_t *block, const uint8_t *block_data,
                    int position, uint8_t *out, size_t out_size)
{
  uint32_t prefix_size = block->prefix_size;
  const uint8_t *prefix = block_data;
  prelo_index_t *it = __index(block, block_data, position);

  assert(position >= 0 && position < block->length);

  /* verify that the decoded string fits into |out| */
  if (prefix_size + it->size <= out_size) {
    memcpy(out, prefix, prefix_size);
    memcpy(out + prefix_size, block_data + it->offset, it->size);
  }

  return prefix_size + it->size;
}

int
prelo_find(const prelo_block_t *block, const uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size)
{
  int position;
  int result;

  /* invariant: common prefix of |ptr| and |block|! */
  assert(ptr_size >= block->prefix_size
      && memcmp(ptr, block_data, block->prefix_size) == 0);

  position = __find_position(block, block_data, ptr, ptr_size, &result);

  if (result == PRE_ALREADY_EXISTS)
    return position;
  return result;
}

int
prelo_find_lowerbound(const prelo_block_t *block, const uint8_t *block_data,
                    const uint8_t *ptr, size_t ptr_size)
{
  int result;

  /* invariant: common prefix of |ptr| and |block|! */
  assert(ptr_size >= block->prefix_size
      && memcmp(ptr, block_data, block->prefix_size) == 0);

  return __find_position(block, block_data, ptr, ptr_size, &result);
}

void
prelo_uncompress(const prelo_block_t *block, const uint8_t *block_data,
                    uint8_t **ptr, size_t *ptr_sizes, uint8_t *data_out)
{
  int i = 0;
  uint32_t prefix_size = block->prefix_size;
  const uint8_t *prefix = block_data;

  prelo_index_t *it = __begin(block, block_data);
  prelo_index_t *end = __end(block, block_data);

  for (; it < end; it++, i++) {
    ptr[i] = data_out;
    ptr_sizes[i] = prefix_size + it->size;
    memcpy(data_out, prefix, prefix_size);
    memcpy(data_out + prefix_size, block_data + it->offset, it->size);

    data_out += prefix_size + it->size;
  }
}

size_t
prelo_grow_prefix(prelo_block_t *block, uint8_t *block_data)
{
  /* for each suffix: check how many characters are identical */
  prelo_index_t *begin = __begin(block, block_data);
  prelo_index_t *end = __end(block, block_data);
  prelo_index_t *it;
  size_t grow = 0;
  uint8_t ch;

  while (1) {
    it = begin;
    ch = block_data[it->offset + grow];
    for (it = begin; it < end; it++) {
      if (it->size < grow)
        goto break_while;
      if (block_data[it->offset + grow] != ch)
        goto break_while;
    }
    grow++;
  }

break_while:
  /* cannot grow the prefix? */
  if (grow == 0)
    return 0;

  /* shift indices and payload "to the right" and create a gap for the new
   * prefix data */
  memmove(block_data + block->prefix_size + grow,
          block_data + block->prefix_size,
          prelo_used_size(block, block_data) - block->prefix_size);

  /* append |grow| bytes to the existing prefix */
  memcpy(block_data + block->prefix_size, block_data + begin->offset + grow,
          grow);
  block->prefix_size += grow;

  /* adjust offset and size of each index */
  for (it = begin; it < end; it++) {
    it->offset += grow;
    it->size -= grow;
  }

  return grow;
}

typedef struct sorted_index
{
  prelo_index_t index;
  int position;
} sorted_index_t;

static int
__sort_cb(const void *lhs, const void *rhs)
{
  pre_offset_t l = *(pre_offset_t *)lhs;
  pre_offset_t r = *(pre_offset_t *)rhs;
  if (l < r)
    return -1;
  if (l > r)
    return +1;
  return 0;
}

extern void
prelo_vacuumize(prelo_block_t *block, uint8_t *block_data)
{
  prelo_index_t *begin = __begin(block, block_data);
  prelo_index_t *end = __end(block, block_data);
  int requires_sort = 0;
  pre_offset_t last_offset = 0;
  prelo_index_t *it;
  sorted_index_t *sorted;
  int i;

  /* create a copy of the indices and check whether the offsets are sequential
   * or not (if not then the copy will be sorted later) */
  sorted = (sorted_index_t *)alloca(block->length * sizeof(sorted_index_t));
  for (i = 0, it = begin; it < end; it++, i++) {
    sorted[i].position = i;
    sorted[i].index = *it;
    if (it->offset < last_offset)
      requires_sort = 1;
    last_offset = it->offset;
  }

  /* if required then sort the index by offset */
  if (requires_sort)
    qsort(sorted, block->length, sizeof(sorted_index_t), __sort_cb);

  /* now remove the gaps */
  last_offset = sizeof(prelo_index_t) * (block->length + 1);
  for (i = 0; i < block->length; i++) {
    it = begin + sorted[i].position;
    if (last_offset != it->offset) {
      memmove(block_data + last_offset, block_data + it->offset, it->size);
      it->offset = last_offset;
    }
    last_offset = it->offset + it->size;
  }
}
