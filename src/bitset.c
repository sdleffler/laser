/*

Copyright (c) 2016 Sean Leffler

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

/// A simple bitset implementation, accelerated with C. Uses 32-bit integers for
/// hypothetical portability. Tested and compatible with LuaJIT and Lua 5.1.
// @module bitset

#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#define ERRORMSG_OUT_OF_MEMORY "Error: out of memory! Could not allocate bitset."

#define LUA_BITSET_LIBNAME "bitset"
#define LUA_BITSET_TYPENAME "_bitset_ty"

#define AUTHOR_STRING "Sean Leffler <sean@errno.com>"
#define VERSION_STRING "1.0"

#define BITWIDTH (8 * sizeof(block_t))
#define ALL_ONES (~(block_t)0)
#define JUST_ONE ((block_t)0x1)

typedef uint32_t block_t;

/*** A bitset type.
@type Bitset
*/
typedef struct Bitset {
    block_t *bits;
    size_t len;
} Bitset;


static void error_out_of_memory(lua_State *L) {
    lua_pushliteral(L, ERRORMSG_OUT_OF_MEMORY);
    lua_error(L);
}


static Bitset* bs_alloc(lua_State *L, size_t sz) {
    Bitset *bitset =
        (Bitset*)lua_newuserdata(L, sizeof(Bitset));

    bitset->bits = (block_t*)calloc(sz, sizeof(block_t));

    if (bitset->bits == NULL) {
        error_out_of_memory(L);
    }

    bitset->len = sz;

    luaL_getmetatable(L, LUA_BITSET_TYPENAME);
    lua_setmetatable(L, -2);

    return bitset;
}


static int bs_gc(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    free(bitset->bits);
    return 0;
}


/*** Allocate a new bitset.
Capable of allocating a new bitset, with an optional bit capacity. If specified,
the bits of the newly allocated bitset can be set to all one. If called with a
bitset as the first argument instead of a number, the bitset will be cloned.

@function new
@tparam num|Bitset size the size of a bitset to allocate. Or, if a bitset, the bitset to copy.
@tparam bool init if true, set all newly allocated bits.
@treturn Bitset a newly allocated bitset.
*/
static int bs_new(lua_State *L) {
    if (lua_isnumber(L, 1)) {
        lua_Integer int_sz = lua_tointeger(L, 1);

        if (int_sz < 0) {
            luaL_argerror(L, 1, "expected positive size");
        }

        int init = lua_toboolean(L, 2);

        // We are assuming Lua 5.1's API/ABI, so we don't have a lua_Unsigned type
        // to work with, unfortunately. `size_t` should do the trick in the
        // meantime.
        Bitset *const bitset =
            bs_alloc(L, (size_t)int_sz / BITWIDTH + 1);

        if (lua_isboolean(L, 2) && init) {
            memset(bitset->bits, -1, (bitset->len - 1) * sizeof(block_t));
            bitset->bits[bitset->len - 1] |=
               ~(ALL_ONES << ((size_t)int_sz % BITWIDTH));
        }
    } else if (lua_isuserdata(L, 1)) {
        const Bitset *const src = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
        Bitset *const dst = bs_alloc(L, src->len);
        memcpy(src->bits, dst->bits, src->len * sizeof(block_t));
    } else {
        bs_alloc(L, 0);
    }

    return 1;
}


/*** Sets a single bit in the bitset, at a given index.
The bitset is modified in place, but for convenience, it is also returned.

@function Bitset:set
@tparam num idx the index of the bit to set.
@treturn Bitset the modified bitset.
*/
static int bs_set(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    size_t idx = (size_t)int_idx;

    // If we're trying to set/clear a bit that's far out of range, we have
    // to reallocate.
    if (idx >= bitset->len * BITWIDTH) {
        size_t len_old = bitset->len;
        bitset->len = idx / BITWIDTH + 1;
        bitset->bits = (block_t*)realloc(bitset->bits,
            sizeof(block_t) * bitset->len);

        if (bitset->bits == NULL) {
            error_out_of_memory(L);
        }

        memset(bitset->bits + len_old, 0,
            (bitset->len - len_old) * sizeof(block_t));
    }

    size_t bit = idx % BITWIDTH;
    idx = idx / BITWIDTH;

    bitset->bits[idx] |= (JUST_ONE << bit);

    lua_pushvalue(L, 1);
    return 1;
}


/*** Sets a range of bits in the bitset, `[lo, hi)`.
The bitset is modified in place, but for convenience, it is also returned.
The parameters are named `lo` and `hi`, but if `lo` is greater than `hi`,
`set_range` will swap them internally. The low bound is inclusive while the high
bound is exclusive.

@function Bitset:set_range
@tparam num lo the low index of the range to set.
@tparam num hi the high index of the range to set.
@treturn Bitset the modified bitset.
*/
static int bs_set_range(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_lo = luaL_checkinteger(L, 2);
    lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    size_t lo = (size_t)int_lo;
    size_t hi = (size_t)int_hi;

    if (lo > hi) {
        size_t tmp = lo;
        lo = hi;
        hi = tmp;
    }

    // If we're trying to set/clear any bits that are far out of range, we have
    // to reallocate.
    if (hi >= bitset->len * BITWIDTH) {
        size_t len_old = bitset->len;
        bitset->len = hi / BITWIDTH + 1;
        bitset->bits = (block_t*)realloc(bitset->bits,
            sizeof(block_t) * bitset->len);

        if (bitset->bits == NULL) {
            error_out_of_memory(L);
        }

        memset(bitset->bits + len_old, 0,
            (bitset->len - len_old) * sizeof(block_t));
    }

    block_t mask;

    const size_t lo_bit = lo % BITWIDTH;
    const size_t lo_blk = lo / BITWIDTH;

    const size_t hi_bit = hi % BITWIDTH;
    const size_t hi_blk = hi / BITWIDTH;

    if (hi_blk > lo_blk) {
        mask = ALL_ONES << lo_bit;

        bitset->bits[lo_blk] |= mask;

        mask = ~(ALL_ONES << hi_bit);

        bitset->bits[hi_blk] |= mask;

        size_t blk;
        for (blk = lo_blk + 1; blk < hi_blk; blk++) {
            bitset->bits[blk] = ALL_ONES;
        }
    } else {
        mask = (ALL_ONES << lo_bit) & ~(ALL_ONES << hi_bit);

        bitset->bits[lo_blk] |= mask;
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** Clears a single bit in the bitset, at a given index.
The bitset is modified in place, but for convenience, it is also returned.

@function Bitset:clear
@tparam num idx the index of the bit to clear.
@treturn Bitset the modified bitset.
*/
static int bs_clear(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    size_t idx = (size_t)int_idx;

    // If we're clearing a bit that's out of range, well, who cares. We're
    // clearing it, not setting it. No need to reallocate, just return.
    if (idx < bitset->len * BITWIDTH) {
        size_t bit = idx % BITWIDTH;
        idx = idx / BITWIDTH;

        bitset->bits[idx] &= ~(JUST_ONE << bit);
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** Clears a range of bits in the bitset, `[lo, hi)`.
The bitset is modified in place, but for convenience, it is also returned.
The parameters are named `lo` and `hi`, but if `lo` is greater than `hi`,
`clear_range` will swap them internally. The low bound is inclusive while the
high bound is exclusive.

@function Bitset:clear_range
@tparam num lo the low index of the range to clear.
@tparam num hi the high index of the range to clear.
@treturn Bitset the modified bitset.
*/
static int bs_clear_range(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_lo = luaL_checkinteger(L, 2);
    lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    size_t lo = (size_t)int_lo;
    size_t hi = (size_t)int_hi;

    if (lo > hi) {
        size_t tmp = lo;
        lo = hi;
        hi = tmp;
    }

    // No need to reallocate anything. We're just clearing bits here, so we just
    // make sure that `lo` and `hi` are in range. All is well.
    if (lo < bitset->len * BITWIDTH) {
        if (hi >= bitset->len * BITWIDTH) {
            hi = bitset->len * BITWIDTH - 1;
        }

        block_t mask;

        const size_t lo_bit = lo % BITWIDTH;
        const size_t lo_blk = lo / BITWIDTH;

        const size_t hi_bit = hi % BITWIDTH;
        const size_t hi_blk = hi / BITWIDTH;

        if (hi_blk > lo_blk) {
            mask = ~(ALL_ONES << lo_bit);

            bitset->bits[lo_blk] &= mask;

            mask = ALL_ONES << hi_bit;

            bitset->bits[hi_blk] &= mask;

            size_t blk;
            for (blk = lo_blk + 1; blk < hi_blk; blk++) {
                bitset->bits[blk] = 0;
            }
        } else {
            mask = ~(ALL_ONES << lo_bit) | (ALL_ONES << hi_bit);

            bitset->bits[lo_blk] &= mask;
        }
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** Gets a single bit in the bitset, at a given index.
@function Bitset:get
@tparam num idx the index of the bit to get.
@treturn bool whether the bit is set.
*/
static int bs_get(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    size_t idx = (size_t)int_idx;

    // If we're trying to check a bit that's out of range, no need to actually
    // do the work. We know it's unset, since bits are unset until they're set.
    if (idx >= bitset->len * BITWIDTH) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int is_set = (bitset->bits[idx / BITWIDTH] & (JUST_ONE << (idx % BITWIDTH))) > 0;

    lua_pushboolean(L, is_set);
    return 1;
}


/*** Gets a range of bits in the bitset, as a table.
@function Bitset:get_range
@tparam num lo the low bound of the range to get.
@tparam num hi the high bound of the range to get.
@treturn {bool,...} the range set. Indexing starts at 1, and ends at `hi - lo + 1`.
*/
static int bs_get_range(lua_State *L) {
    Bitset *const bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    const lua_Integer int_lo = luaL_checkinteger(L, 2);
    const lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    size_t lo;
    size_t hi;

    if (int_lo > int_hi) {
        lo = (size_t)int_hi;
        hi = (size_t)int_lo;
    } else {
        lo = (size_t)int_lo;
        hi = (size_t)int_hi;
    }

    lua_createtable(L, hi - lo, 0);

    size_t const maxbits = bitset->len * BITWIDTH;

    size_t idx;
    for (idx = lo; idx < hi; idx++) {
        if (idx >= maxbits) {
            for (; idx < hi; idx++) {
                lua_pushboolean(L, 0);
                lua_rawseti(L, -2, idx - lo + 1);
            }

            break;
        }

        lua_pushboolean(L,
            (bitset->bits[idx / BITWIDTH] & (JUST_ONE << (idx % BITWIDTH))) > 0);
        lua_rawseti(L, -2, idx - lo + 1);
    }

    return 1;
}


/*** Counts how many bits are set in the bitset.
@function Bitset:count
@treturn num the number of set bits.
*/
static int bs_count(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    size_t sum = 0, i;
    for (i = 0; i < bitset->len; i++) {
        sum += __builtin_popcount(bitset->bits[i]);
    }

    lua_pushinteger(L, (lua_Integer)sum);
    return 1;
}


/*** The intersection of two bitsets. Also available as the `*` operator.
NOTE: `Bitset:intersection` is a _constructive_ operation. That is, it allocates
a new bitset to hold the results. For an in-place intersection, see
@{Bitset:intersection_mut}.

@function Bitset:intersection
@tparam Bitset lhs the left-hand bitset to intersect.
@tparam Bitset rhs the right-hand bitset to intersect.
@treturn Bitset a newly allocated bitset containing the intersection.
@see Bitset:intersection_mut
*/
static int bs_intersection(lua_State *L) {
    Bitset* large = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    Bitset* small = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    if (small->len > large->len) {
        Bitset *const tmp = large;
        large = small;
        small = tmp;
    }

    Bitset *const out = bs_alloc(L, small->len);
    memcpy(small->bits, out->bits, small->len * sizeof(block_t));

    size_t blk;
    for (blk = 0; blk < small->len; blk++) {
        out->bits[blk] &= large->bits[blk];
    }

    return 1;
}


/*** The in-place intersection of two bitsets.
NOTE: `Bitset:intersection_mut` is a _destructive_ operation. That is, it
mutates the left-hand operand passed to it instead of allocating a new bitset.
For a constructive intersection operation, see @{Bitset:intersection}.

@function Bitset:intersection_mut
@tparam Bitset lhs the left-hand bitset to intersect. Is mutated to contain the intersection.
@tparam Bitset rhs the right-hand bitset to intersect.
@treturn Bitset the left-hand bitset is modified in-place, but returned for convenience.
@see Bitset:intersection
*/
static int bs_intersection_mut(lua_State *L) {
    Bitset* lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    Bitset* rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    if (lhs->len > rhs->len) {
        lhs->bits = (block_t*)realloc(lhs->bits, rhs->len * sizeof(block_t));
        lhs->len = rhs->len;

        if (lhs->bits == NULL) {
            lua_pushliteral(L, ERRORMSG_OUT_OF_MEMORY);
            lua_error(L);
        }
    }

    size_t blk;
    for (blk = 0; blk < rhs->len; blk++) {
        lhs->bits[blk] &= rhs->bits[blk];
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** The union of two bitsets. Also available as the `+` operator.
NOTE: `Bitset:union` is a _constructive_ operation. That is, it allocates a new
bitset to hold the results. For an in-place union, see @{Bitset:union_mut}.

@function Bitset:union
@tparam Bitset lhs the left-hand bitset to union.
@tparam Bitset rhs the right-hand bitset to union.
@treturn Bitset a newly allocated bitset containing the union.
@see Bitset:union_mut
*/
static int bs_union(lua_State *L) {
    Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    Bitset* small;
    Bitset* large;

    if (lhs->len >= rhs->len) {
        large = lhs;
        small = rhs;
    } else {
        large = rhs;
        small = lhs;
    }

    Bitset *const out = bs_alloc(L, large->len);
    memcpy(large->bits, out->bits, large->len * sizeof(block_t));

    size_t blk;
    for (blk = 0; blk < small->len; blk++) {
        out->bits[blk] |= small->bits[blk];
    }

    return 1;
}


/*** The in-place union of two bitsets.
NOTE: `Bitset:union_mut` is a _destructive_ operation. That is, it mutates the
left-hand operand passed to it instead of allocating a new bitset. For a
constructive union operation, see @{Bitset:union}.

@function Bitset:union_mut
@tparam Bitset lhs the left-hand bitset to union. Is mutated to contain the union.
@tparam Bitset rhs the right-hand bitset to union.
@treturn Bitset the left-hand bitset is modified in-place, but returned for convenience.
@see Bitset:union
*/
static int bs_union_mut(lua_State *L) {
    Bitset* lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    Bitset* rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    size_t old_len = lhs->len;

    if (lhs->len < rhs->len) {
        lhs->bits = (block_t*)realloc(lhs->bits, rhs->len * sizeof(block_t));
        memcpy(rhs->bits + old_len, lhs->bits + old_len,
            (rhs->len - old_len) * sizeof(block_t));
        lhs->len = rhs->len;

        if (lhs->bits == NULL) {
            lua_pushliteral(L, ERRORMSG_OUT_OF_MEMORY);
            lua_error(L);
        }
    }

    size_t blk;
    for (blk = 0; blk < old_len; blk++) {
        lhs->bits[blk] |= rhs->bits[blk];
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** The asymmetric difference of two bitsets. Also available as the `-` operator.
Returns a newly allocated bitset with bits set iff they are set in the left-hand
operand but not in the right-hand operand.

NOTE: `Bitset:difference` is a _constructive_ operation. That is, it allocates a
new bitset to return the results. For an in-place asymmetric difference
operation, see @{Bitset:difference_mut}.

@function Bitset:difference
@tparam Bitset lhs the source bitset.
@tparam Bitset rhs the bitset to "subtract" from the source.
@treturn Bitset a newly allocated bitset with all bits set that are set in `lhs` but not in `rhs`.
@see Bitset:difference_mut
*/
static int bs_difference(lua_State *L) {
    const Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    Bitset *const out = bs_alloc(L, lhs->len);
    memcpy(lhs->bits, out->bits, lhs->len * sizeof(block_t));

    size_t blk;
    for (blk = 0; blk < lhs->len; blk++) {
        out->bits[blk] &= ~rhs->bits[blk];
    }

    return 1;
}


/*** The in-place asymmetric difference of two bitsets.
Clears any bits in the left-hand operand if they are set in the right-hand.

NOTE: `Bitset:difference_mut` is a _destructive_ operation. That is, it modifies
the left-hand operand in-place instead of allocating a new bitset to hold the
result. For a constructive asymmetric difference operation, see
@{Bitset:difference}.

@function Bitset:difference_mut
@tparam Bitset lhs the bitset to modify.
@tparam Bitset rhs the bitset to "subtract" from the left-hand.
@treturn Bitset the left-hand bitset is modified in place, but returned for convenience.
@see Bitset:difference
*/
static int bs_difference_mut(lua_State *L) {
    Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    const size_t len = (lhs->len > rhs->len) ? rhs->len : lhs->len;

    size_t blk;
    for (blk = 0; blk < len; blk++) {
        lhs->bits[blk] &= ~rhs->bits[blk];
    }

    lua_pushvalue(L, 1);
    return 1;
}


/*** The symmetric difference of two bitsets.
Returns a newly allocated bitset with bits set iff they are set in only the
left-hand operand or the right-hand operand, but not both.

NOTE: `Bitset:symmetric_diff` is a _constructive_ operation. That is, it
allocates a new bitset to hold the result rather than modifying the operands
in-place. For an in-place symmetric difference operation, see
@{Bitset:symmetric_diff_mut}.

@function Bitset:symmetric_diff
@tparam Bitset lhs the left-hand bitset for the symmetric difference operation.
@tparam Bitset rhs the right-hand bitset for the symmetric difference operation.
@treturn Bitset a newly allocated bitset with all bits set that are set in either `lhs` or `rhs`, but not in both.
@see Bitset:symmetric_diff_mut
*/
static int bs_symmetric_diff(lua_State *L) {
    const Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    size_t min_len;
    Bitset* out;

    if (lhs->len > rhs->len) {
        out = bs_alloc(L, lhs->len);

        const size_t len_diff = lhs->len - rhs->len;
        memcpy(lhs->bits + len_diff, out->bits + len_diff,
            len_diff * sizeof(block_t));

        min_len = rhs->len;
    } else {
        out = bs_alloc(L, rhs->len);

        const size_t len_diff = rhs->len - lhs->len;
        memcpy(rhs->bits + len_diff, out->bits + len_diff,
            len_diff * sizeof(block_t));

        min_len = lhs->len;
    }

    size_t blk;
    for (blk = 0; blk < min_len; blk++) {
        out->bits[blk] = lhs->bits[blk] ^ rhs->bits[blk];
    }

    return 1;
}


/*** The in-place symmetric difference of two bitsets.
Modifies the left-hand operand to contain bits that are set in the right-hand
but not in the left-hand, and clears any bits that are set in both.

NOTE: `Bitset:symmetric_diff_mut` is a _destructive_ operation. That is, it
modifies the left-hand operand in-place instead of allocating a new bitset to
hold the result. For a constructive symmetric difference operation, see
@{Bitset:symmetric_diff}.

@function Bitset:symmetric_diff_mut
@tparam Bitset lhs the bitset to modify.
@tparam Bitset rhs the bitset to compare against.
@treturn Bitset the left-hand bitset is modified in place, but returned for convenience.
@see Bitset:symmetric_diff
*/
static int bs_symmetric_diff_mut(lua_State *L) {
    Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    if (rhs->len > lhs->len) {
        lhs->bits = (block_t*)realloc(lhs->bits,
            rhs->len * sizeof(block_t));
        memcpy(rhs->bits + lhs->len, lhs->bits + lhs->len,
            (rhs->len - lhs->len) * sizeof(block_t));
        lhs->len = rhs->len;
    }

    size_t blk;
    for (blk = 0; blk < lhs->len; blk++) {
        lhs->bits[blk] ^= rhs->bits[blk];
    }

    lua_pushvalue(L, 1);
    return 1;
}


static int bs_eq(lua_State *L) {
    const Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    const Bitset* small;
    const Bitset* large;

    if (lhs->len > rhs->len) {
        large = lhs;
        small = rhs;
    } else {
        large = rhs;
        small = lhs;
    }

    size_t blk;
    for (blk = 0; blk < small->len; blk++) {
        if (small->bits[blk] != large->bits[blk]) {
            lua_pushboolean(L, false);
            return 1;
        }
    }

    for (; blk < large->len; blk++) {
        if (large->bits[blk] != 0) {
            lua_pushboolean(L, false);
            return 1;
        }
    }

    lua_pushboolean(L, true);
    return 1;
}


static int bs_subset(lua_State *L) {
    const Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    size_t blk;
    if (lhs->len < rhs->len) {
        for (blk = 0; blk < lhs->len; blk++) {
            if ((lhs->bits[blk] | rhs->bits[blk]) != rhs->bits[blk]) {
                lua_pushboolean(L, false);
                return 1;
            }
        }
    } else {
        for (blk = 0; blk < rhs->len; blk++) {
            if ((lhs->bits[blk] | rhs->bits[blk]) != rhs->bits[blk]) {
                lua_pushboolean(L, false);
                return 1;
            }
        }

        for (; blk < lhs->len; blk++) {
            if (lhs->bits[blk] != 0) {
                lua_pushboolean(L, false);
                return 1;
            }
        }
    }

    lua_pushboolean(L, true);
    return 1;
}


static int bs_strict_subset(lua_State *L) {
    const Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    const Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    bool strict = false;
    size_t blk;
    if (lhs->len < rhs->len) {
        for (blk = 0; blk < lhs->len; blk++) {
            if ((lhs->bits[blk] | rhs->bits[blk]) != rhs->bits[blk]) {
                lua_pushboolean(L, false);
                return 1;
            } else if (lhs->bits[blk] != rhs->bits[blk]) {
                strict = true;
            }
        }

        for (; blk < rhs->len; blk++) {
            if (rhs->bits[blk] != 0) {
                strict = true;
            }
        }
    } else {
        for (blk = 0; blk < rhs->len; blk++) {
            if ((lhs->bits[blk] | rhs->bits[blk]) != rhs->bits[blk]) {
                lua_pushboolean(L, false);
                return 1;
            } else if (lhs->bits[blk] != rhs->bits[blk]) {
                strict = true;
            }
        }

        for (; blk < lhs->len; blk++) {
            if (lhs->bits[blk] != 0) {
                lua_pushboolean(L, false);
                return 1;
            }
        }
    }

    lua_pushboolean(L, strict);
    return 1;
}


static int dump_raw(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    size_t idx = (size_t)int_idx;

    lua_pushinteger(L, bitset->bits[idx]);
    return 1;
}


static int dump_len(lua_State *L) {
    Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_pushinteger(L, bitset->len);
    return 1;
}


static const luaL_reg bs_funcs[] = {
    {"new", bs_new},
    {NULL, NULL},
};


static const luaL_reg bs_methods[] = {
    {"set", bs_set},
    {"set_range", bs_set_range},
    {"clear", bs_clear},
    {"clear_range", bs_clear_range},
    {"get", bs_get},
    {"get_range", bs_get_range},
    {"count", bs_count},
    {"intersection", bs_intersection},
    {"intersection_mut", bs_intersection_mut},
    {"union", bs_union},
    {"union_mut", bs_union_mut},
    {"difference", bs_difference},
    {"difference_mut", bs_difference_mut},
    {"symmetric_diff", bs_symmetric_diff},
    {"symmetric_diff_mut", bs_symmetric_diff_mut},
    {"dump_raw", dump_raw},
    {"dump_len", dump_len},
    {NULL, NULL},
};


LUALIB_API int luaopen_bitset(lua_State *L) {
    luaL_register(L, LUA_BITSET_LIBNAME, bs_funcs);

    // Push a new metatable for the bitset object onto the stack.
    if (luaL_newmetatable(L, LUA_BITSET_TYPENAME) == 0) {
        lua_pushstring(L, "Uh-oh! The string used in the bitset library to \
            identify the bitset metatable is taken in the registry! Sean \
            didn't think this would happen, so you better tell him either \
            through github or email at <sean@errno.com>.");
        lua_error(L);
    }

    static const struct luaL_reg bs_mt[] = {
        {"__gc", bs_gc},
        {"__add", bs_union},
        {"__mul", bs_intersection},
        {"__sub", bs_difference},
        {"__len", bs_count},
        {"__eq", bs_eq},
        {"__lt", bs_strict_subset},
        {"__le", bs_subset},
        {NULL, NULL},
    };

    // Make a new table, and populate it with our methods.
    lua_newtable(L);
    luaL_register(L, NULL, bs_methods);

    // Set the index field of our metatable to the newly populated table.
    lua_setfield(L, -2, "__index");

    // Populate the metatable with the rest of the metafunctions.
    luaL_register(L, NULL, bs_mt);

    // Push some debug info.
    lua_pushstring(L, AUTHOR_STRING);
    lua_setfield(L, -2, "_AUTHOR");

    lua_pushstring(L, VERSION_STRING);
    lua_setfield(L, -2, "_VERSION");

    return 1;
}
