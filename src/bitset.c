#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "luajit.h"
#include "lauxlib.h"

#define LUA_BITSET_LIBNAME "bitset"
#define LUA_BITSET_TYPENAME "_bitset_ty"

#define AUTHOR_STRING "Sean Leffler <sean@errno.com>"
#define VERSION_STRING "1.0"

#define BITWIDTH 32

struct Bitset {
    lua_Integer *bits;
    unsigned int len;
};


static void error_out_of_memory(lua_State *L) {
    lua_pushliteral(L, "Error: out of memory! Could not allocate bitset.");
    lua_error(L);
}


static struct Bitset* lua_alloc_bitset(lua_State *L, unsigned int sz) {
    struct Bitset *bitset =
        (struct Bitset*)lua_newuserdata(L, sizeof(struct Bitset));

    bitset->bits = (lua_Integer*)calloc(sizeof(lua_Integer), sz);

    if (bitset->bits == NULL) {
        error_out_of_memory(L);
    }

    bitset->len = sz;

    luaL_getmetatable(L, LUA_BITSET_TYPENAME);
    lua_setmetatable(L, -2);

    return bitset;
}


static int bs_gc(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    free(bitset->bits);
    return 0;
}


static int bs_new(lua_State *L) {
    lua_Integer int_sz = luaL_optinteger(L, 1, 0);

    if (int_sz < 0) {
        luaL_argerror(L, 1, "expected positive size");
    }

    // We are assuming Lua 5.1's API/ABI, so we don't have a lua_Unsigned type
    // to work with, unfortunately. `unsigned int` should do the trick in the
    // meantime.
    lua_alloc_bitset(L, (unsigned int)int_sz / BITWIDTH + 1);

    return 1;
}


static int bs_set(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    unsigned int idx = (unsigned int)int_idx;

    // If we're trying to set/clear a bit that's far out of range, we have
    // to reallocate.
    if (idx >= bitset->len * BITWIDTH) {
        unsigned int len_old = bitset->len;
        bitset->len = idx / BITWIDTH + 1;
        bitset->bits = (lua_Integer*)realloc(bitset->bits,
            sizeof(lua_Integer) * bitset->len);

        if (bitset->bits == NULL) {
            error_out_of_memory(L);
        }

        memset(bitset->bits + len_old, 0, (bitset->len - len_old) * sizeof(lua_Integer));
    }

    unsigned int bit = idx % BITWIDTH;
    idx = idx / BITWIDTH;

    bitset->bits[idx] |= ((unsigned int)0x1 << bit);

    lua_pushvalue(L, 1);
    return 1;
}


static int bs_set_range(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_lo = luaL_checkinteger(L, 2);
    lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    unsigned int lo = (unsigned int)int_lo;
    unsigned int hi = (unsigned int)int_hi;

    if (lo > hi) {
        unsigned int tmp = lo;
        lo = hi;
        hi = tmp;
    }

    // If we're trying to set/clear any bits that are far out of range, we have
    // to reallocate.
    if (hi >= bitset->len * BITWIDTH) {
        unsigned int len_old = bitset->len;
        bitset->len = hi / BITWIDTH + 1;
        bitset->bits = (lua_Integer*)realloc(bitset->bits,
            sizeof(lua_Integer) * bitset->len);

        if (bitset->bits == NULL) {
            error_out_of_memory(L);
        }

        memset(bitset->bits + len_old, 0,
            (bitset->len - len_old) * sizeof(lua_Integer));
    }

    unsigned int mask;

    const unsigned int lo_bit = lo % BITWIDTH;
    const unsigned int lo_blk = lo / BITWIDTH;

    const unsigned int hi_bit = hi % BITWIDTH;
    const unsigned int hi_blk = hi / BITWIDTH;

    if (hi_blk > lo_blk) {
        mask = 0xffffffff << lo_bit;

        bitset->bits[lo_blk] |= mask;

        mask = ~(0xffffffff << hi_bit);

        bitset->bits[hi_blk] |= mask;

        unsigned int blk;
        for (blk = lo_blk + 1; blk < hi_blk; blk++) {
            bitset->bits[blk] = 0xffffffff;
        }
    } else {
        mask = (0xffffffff << lo_bit) & ~(0xffffffff << hi_bit);

        bitset->bits[lo_blk] |= mask;
    }

    lua_pushvalue(L, 1);
    return 1;
}


static int bs_clear(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    unsigned int idx = (unsigned int)int_idx;

    // If we're clearing a bit that's out of range, well, who cares. We're
    // clearing it, not setting it. No need to reallocate, just return.
    if (idx < bitset->len * BITWIDTH) {
        unsigned int bit = idx % BITWIDTH;
        idx = idx / BITWIDTH;

        bitset->bits[idx] &= ~(0x1 << bit);
    }

    lua_pushvalue(L, 1);
    return 1;
}


static int bs_clear_range(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_lo = luaL_checkinteger(L, 2);
    lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    unsigned int lo = (unsigned int)int_lo;
    unsigned int hi = (unsigned int)int_hi;

    if (lo > hi) {
        unsigned int tmp = lo;
        lo = hi;
        hi = tmp;
    }

    // No need to reallocate anything. We're just clearing bits here, so we just
    // make sure that `lo` and `hi` are in range. All is well.
    if (lo < bitset->len * BITWIDTH) {
        if (hi >= bitset->len * BITWIDTH) {
            hi = bitset->len * BITWIDTH - 1;
        }

        unsigned int mask;

        const unsigned int lo_bit = lo % BITWIDTH;
        const unsigned int lo_blk = lo / BITWIDTH;

        const unsigned int hi_bit = hi % BITWIDTH;
        const unsigned int hi_blk = hi / BITWIDTH;

        if (hi_blk > lo_blk) {
            mask = ~(0xffffffff << lo_bit);

            bitset->bits[lo_blk] &= mask;

            mask = 0xffffffff << hi_bit;

            bitset->bits[hi_blk] &= mask;

            unsigned int blk;
            for (blk = lo_blk + 1; blk < hi_blk; blk++) {
                bitset->bits[blk] = 0x0;
            }
        } else {
            mask = ~(0xffffffff << lo_bit) | (0xffffffff << hi_bit);

            bitset->bits[lo_blk] &= mask;
        }
    }

    lua_pushvalue(L, 1);
    return 1;
}


static int bs_get(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    unsigned int idx = (unsigned int)int_idx;

    // If we're trying to check a bit that's out of range, no need to actually
    // do the work. We know it's unset, since bits are unset until they're set.
    if (idx >= bitset->len * BITWIDTH) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int is_set = (bitset->bits[idx / BITWIDTH] & (0x1 << (idx % BITWIDTH))) > 0;

    lua_pushboolean(L, is_set);
    return 1;
}


static int bs_get_range(lua_State *L) {
    struct Bitset *const bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    const lua_Integer int_lo = luaL_checkinteger(L, 2);
    const lua_Integer int_hi = luaL_checkinteger(L, 3);

    if (int_lo < 0) {
        luaL_argerror(L, 2, "expected positive lower bound");
    }

    if (int_hi < 0) {
        luaL_argerror(L, 3, "expected positive upper bound");
    }

    unsigned int lo;
    unsigned int hi;

    if (int_lo > int_hi) {
        lo = (unsigned int)int_hi;
        hi = (unsigned int)int_lo;
    } else {
        lo = (unsigned int)int_lo;
        hi = (unsigned int)int_hi;
    }

    lua_createtable(L, hi - lo, 0);

    unsigned int const maxbits = bitset->len * BITWIDTH;

    unsigned int idx;
    for (idx = lo; idx < hi; idx++) {
        if (idx >= maxbits) {
            for (; idx < hi; idx++) {
                lua_pushboolean(L, 0);
                lua_rawseti(L, -2, idx);
            }

            break;
        }

        lua_pushboolean(L,
            (bitset->bits[idx / BITWIDTH] & (0x1 << (idx % BITWIDTH))) > 0);
        lua_rawseti(L, -2, idx);
    }

    return 1;
}


static int bs_count(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    unsigned int sum = 0, i;
    for (i = 0; i < bitset->len; i++) {
        sum += __builtin_popcount(bitset->bits[i]);
    }

    lua_pushinteger(L, (lua_Integer)sum);
    return 1;
}


static int bs_intersection(lua_State *L) {
    struct Bitset* large = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    struct Bitset* small = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    if (small->len > large->len) {
        struct Bitset *const tmp = large;
        large = small;
        small = tmp;
    }

    struct Bitset *const out = lua_alloc_bitset(L, small->len);
    memcpy(small->bits, out->bits, small->len * sizeof(lua_Integer));

    unsigned int blk;
    for (blk = 0; blk < small->len; blk++) {
        out->bits[blk] &= large->bits[blk];
    }

    return 1;
}


static int bs_union(lua_State *L) {
    struct Bitset *const lhs = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);
    struct Bitset *const rhs = luaL_checkudata(L, 2, LUA_BITSET_TYPENAME);

    struct Bitset* small;
    struct Bitset* large;

    if (lhs->len >= rhs->len) {
        large = lhs;
        small = rhs;
    } else {
        large = rhs;
        small = lhs;
    }

    struct Bitset *const out = lua_alloc_bitset(L, large->len);
    memcpy(large->bits, out->bits, large->len * sizeof(lua_Integer));

    unsigned int blk;
    for (blk = 0; blk < small->len; blk++) {
        out->bits[blk] |= small->bits[blk];
    }

    return 1;
}


static int dump_raw(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

    lua_Integer int_idx = luaL_checkinteger(L, 2);

    // A negative index is completely invalid.
    if (int_idx < 0) {
        luaL_argerror(L, 2, "expected positive index");
    }

    unsigned int idx = (unsigned int)int_idx;

    lua_pushinteger(L, bitset->bits[idx]);
    return 1;
}


static int dump_len(lua_State *L) {
    struct Bitset *bitset = luaL_checkudata(L, 1, LUA_BITSET_TYPENAME);

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
    {"union", bs_union},
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
