
// initialize a block
// check used size
// check block size
// check uncompressed size
// negative tests of find, find_lower_bound

// initialize a block
// insert strings till the block is full
// after each insert: check used size (must grow)
// check block size
// check uncompressed size
// uncompress -> verify

// initialize a block
// insert strings till the block is full
// after each insert: lookup string with find and find_lower_bound
// negative tests of find, find_lower_bound

// initialize a block
// insert strings till the block is full
// delete from front
// after each delete: uncompress, compare

// initialize a block
// insert strings till the block is full
// delete from back
// after each delete: uncompress, compare

// initialize a block
// insert strings till the block is full
// delete from random positions
// after each delete: uncompress, compare

// initialize a block
// insert strings till the block is full; make sure that strings share a prefix
// grow prefix
// after each "insert/grow": uncompress, compare, check used size (must not
//      change)
// then vacuumize and uncompress, compare, check used size (must shrink)

