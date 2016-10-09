package.path  = './spec/?.lua;./lib/?.lua;;' .. package.path

require 'bitset'
local pretty = require 'pl.pretty'

describe('bitset', function()
    it('should exist', function()
        assert.is_not_nil(bitset)
        assert.is_not_nil(bitset.new)
    end)

    it('should have instances with metatables', function()
        local a = bitset.new()

        local mt = getmetatable(a)

        assert.is_not_nil(mt)
    end)

    it('should allocate, set, clear, and get correctly', function()
        local a = bitset.new()

        assert.is_false(a:get(0))
        assert.are_equal(0, a:dump_raw(0))

        a:set(0)

        assert.are_equal(1, a:dump_raw(0))

        a:set(7)

        assert.is_true(a:get(0))
        assert.is_true(a:get(7))
        assert.is_false(a:get(13))
        assert.is_false(a:get(34))

        a:set(34)

        assert.is_true(a:get(34))
        assert.is_false(a:get(143))

        a:clear(143)

        assert.is_false(a:get(143))

        a:set(254)
        a:set(255)
        a:set(256)

        assert.is_true(a:get(254))
        assert.is_true(a:get(255))
        assert.is_true(a:get(256))

        a:clear(255)

        assert.is_true(a:get(254))
        assert.is_false(a:get(255))
        assert.is_true(a:get(256))
    end)

    it('should copy correctly', function()
        local a = bitset.new(512)

        for i=0,384 do
            a:set(i)
        end

        local b = bitset.new(a)

        for i=0,511 do
            assert.are_equal(a:get(i), b:get(i))
        end
    end)

    it('should initialize with true bits correctly', function()
        local a = bitset.new(100, true)

        for i=0,99 do
            assert.is_true(a:get(i))
        end

        for i=100,127 do
            assert.is_false(a:get(i))
        end
    end)

    it('should count set bits correctly', function()
        local a = bitset.new()


        for i=0,1024 do
            a:set(i)
            assert.are_equal(i + 1, a:count())
        end

        for i=1024,0,-1 do
            a:clear(i)
            assert.are_equal(i, a:count())
        end

        local total = 0
        for i = 0,1024 do
            local idx = math.random(0, 1024)
            local set_or_clear = math.random(0, 1)
            if set_or_clear == 0 then
                if a:get(i) then
                    total = total - 1
                end
                a:clear(i)
            else
                if not a:get(i) then
                    total = total + 1
                end
                a:set(i)
            end
            assert.are_equal(total, a:count())
        end
    end)

    it('should set ranges of bits local to one raw correctly', function()
        local a = bitset.new()

        a:set_range(5, 23)

        assert.are_equal(18, a:count())

        for i=0,4 do
            assert.is_false(a:get(i))
        end

        for i=5,22 do
            assert.is_true(a:get(i))
        end

        for i=23,31 do
            assert.is_false(a:get(i))
        end
    end)

    it('should set ranges of bits across raws correctly', function()
        local a = bitset.new()

        a:set_range(14, 87)

        assert.are_equal(73, a:count())

        for i=0,13 do
            assert.is_false(a:get(i))
        end

        for i=14,86 do
            assert.is_true(a:get(i))
        end

        for i=87,95 do
            assert.is_false(a:get(i))
        end
    end)

    it('should clear ranges of bits local to one raw correctly', function()
        local a = bitset.new()

        a:set_range(5, 23)
        a:clear_range(8, 15)

        assert.are_equal(11, a:count())

        for i=0,4 do
            assert.is_false(a:get(i))
        end

        for i=5,7 do
            assert.is_true(a:get(i))
        end

        for i=8,14 do
            assert.is_false(a:get(i))
        end

        for i=15,22 do
            assert.is_true(a:get(i))
        end

        for i=23,31 do
            assert.is_false(a:get(i))
        end
    end)

    it('should clear ranges of bits non-local to a single raw correctly', function()
        local a = bitset.new()

        a:set_range(47, 124)
        a:set_range(666, 777)

        a:clear_range(48, 677)

        assert.are_equal(101, a:count())

        for i=0,46 do
            assert.is_false(a:get(i))
        end

        assert.is_true(a:get(47))

        for i=48,676 do
            assert.is_false(a:get(i))
        end

        for i=677,776 do
            assert.is_true(a:get(i))
        end

        for i=777,799 do
            assert.is_false(a:get(i))
        end
    end)

    it('should get ranges of bits local to a single raw correctly', function()
        local a = bitset.new()

        local bits = { 1, 2, 3, 5, 10, 13, 18, 21, 22, 23, 24, 25, 31 }

        for _,v in ipairs(bits) do
            a:set(v)
        end

        local set = a:get_range(9, 22)

        assert.is_false(set[1])
        assert.is_true(set[2])
        assert.is_false(set[3])
        assert.is_false(set[4])
        assert.is_true(set[5])

        for i=6,9 do
            assert.falsy(set[i])
        end

        assert.is_true(set[10])

        for i=11,12 do
            assert.falsy(set[i])
        end

        assert.is_true(set[13])

        for i=14,32 do
            assert.falsy(set[i])
        end
    end)

    it('should get ranges of bits non-local to a single raw correctly', function()
        local a = bitset.new()

        local bits = {
            [0414] = true, [0982] = true, [1010] = true, [0394] = true,
            [0301] = true, [0278] = true, [0906] = true, [0979] = true,
            [0607] = true, [0776] = true, [0063] = true, [0371] = true,
            [0656] = true, [0969] = true, [0767] = true, [0104] = true,
            [0180] = true, [0053] = true, [0227] = true, [0151] = true,
            [0439] = true, [0617] = true, [0277] = true, [0844] = true,
            [0292] = true, [0206] = true, [0153] = true, [0786] = true,
            [0053] = true, [0822] = true, [0705] = true, [0148] = true,
            [0959] = true, [0827] = true, [0395] = true, [0815] = true,
            [0069] = true, [0699] = true, [0540] = true, [0817] = true,
            [0074] = true, [0527] = true, [0713] = true, [0563] = true,
            [0099] = true, [0666] = true, [0074] = true, [0204] = true,
            [0824] = true, [0700] = true, [1016] = true, [0688] = true,
            [0845] = true, [1003] = true, [0375] = true, [0370] = true,
            [0965] = true, [0970] = true, [0837] = true, [0882] = true,
            [0696] = true, [0669] = true, [0556] = true, [0907] = true, }

        for i,_ in pairs(bits) do
            a:set(i)
        end

        local range = a:get_range(324, 768)

        for i,v in ipairs(range) do
            if bits[i + 323] then
                assert.is_true(v)
            else
                assert.is_false(v)
            end
        end
    end)

    it('should union correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,32 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = a:union(b)

        for i=0,127 do
            if a:get(i) or b:get(i) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should union_mut correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,32 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = bitset.new(a)

        c:union_mut(b)

        for i=0,127 do
            if a:get(i) or b:get(i) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should intersect correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,64 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = a:intersection(b)

        for i=0,127 do
            if a:get(i) and b:get(i) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should intersect_mut correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,64 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = bitset.new(a)

        c:intersection_mut(b)

        for i=0,127 do
            if a:get(i) and b:get(i) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should difference correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,80 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = a:difference(b)

        for i=0,127 do
            if b:get(i) then
                assert.is_false(c:get(i))
            else
                assert.are_equal(a:get(i), c:get(i))
            end
        end
    end)

    it('should difference_mut correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,80 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = bitset.new(a)
        c:difference_mut(b)

        for i=0,127 do
            if b:get(i) then
                assert.is_false(c:get(i))
            else
                assert.are_equal(a:get(i), c:get(i))
            end
        end
    end)

    it('should symmetric_diff correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,80 do
            a:set(math.random(0, 127))
            b:set(math.random(0, 127))
        end

        local c = a:symmetric_diff(b)

        for i=0,127 do
            if (a:get(i) and not b:get(i)) or (not a:get(i) and b:get(i)) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should symmetric_diff_mut correctly', function()
        local a = bitset.new(128)
        local b = bitset.new(128)

        for i=1,80 do
            a:set(math.random(0,127))
            b:set(math.random(0,127))
        end

        local c = bitset.new(a)
        c:symmetric_diff_mut(b)

        for i=0,127 do
            if (a:get(i) and not b:get(i)) or (not a:get(i) and b:get(i)) then
                assert.is_true(c:get(i))
            else
                assert.is_false(c:get(i))
            end
        end
    end)

    it('should test for equality correctly', function()
        local a = bitset.new(1024)
        local b = bitset.new(1024)

        local last_set

        for i=1,768 do
            local idx = math.random(0,1023)

            a:set(idx)
            b:set(idx)

            last_set = idx
        end

        assert.are_equal(a, b)

        b:clear(last_set)

        assert.are_not_equal(a, b)
    end)

    it('should test subset-ness correctly', function()
        local a = bitset.new(1024)
        local b = bitset.new(1024)

        for i=1,512 do
            local idx = math.random(0,1023)

            a:set(idx)
            b:set(idx)
        end

        for i=1,256 do
            b:set(math.random(0,1023))
        end

        assert.is_true(a <= b)
    end)

    it('should test strict subset-ness correctly', function()
        local a = bitset.new(1024)
        local b = bitset.new(1024)

        for i=1,512 do
            local idx = math.random(0,1022)

            a:set(idx)
            b:set(idx)
        end

        b:set(1023)

        assert.is_false(a == b)
        assert.is_true(a <= b)
        assert.is_true(a < b)
    end)
end)
