#ifndef INC_SLZ4_H_
#define INC_SLZ4_H_
/**
@file slz4.h
@author t-sakai
@date 2018/11/21 create

# License
This software is distributed under two licenses, choose whichever you like.

## MIT License
Copyright (c) 2020 Takuro Sakai

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Public Domain
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>

USAGE:
Put '#define SLZ4_IMPLEMENTATION' before including this file to create the implementation.
*/
#ifdef __cplusplus
#    include <cassert>
#    include <cstddef>
#    include <cstdint>
#else
#    include <assert.h>
#    include <stdbool.h>
#    include <stddef.h>
#    include <stdint.h>
#endif

namespace slz4
{
#ifndef SLZ4_NULL
#    ifdef __cplusplus
#        if 201103L <= __cplusplus || 1700 <= _MSC_VER
#            define SLZ4_NULL nullptr
#        else
#            define SLZ4_NULL 0
#        endif
#    else
#        define SLZ4_NULL (void*)0
#    endif
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef ::size_t size_t;
typedef ::ptrdiff_t ptrdiff_t;

#ifdef _DEBUG
#    define SLZ4_ASSERT(exp) assert(exp)
#else
#    define SLZ4_ASSERT(exp)
#endif

/**
* @brief Returns the minimum of two signed 32-bit integers.
* @param x0 The first 32-bit signed integer.
* @param x1 The second 32-bit signed integer.
* @return The minimum value between `x0` and `x1`.
*/
inline s32 minimum(s32 x0, s32 x1)
{
    return x0 < x1 ? x0 : x1;
}

inline u16 minimum(u16 x0, u16 x1)
{
    return x0 < x1 ? x0 : x1;
}

inline const u8* minimum(const u8* x0, const u8* x1)
{
    return x0 < x1 ? x0 : x1;
}

inline const u8* maximum(const u8* x0, const u8* x1)
{
    return x0 < x1 ? x1 : x0;
}

static const u32 MAX_BLOCK_SIZE = 0x7E000000;
static const u16 MAX_DISTANCE = 65535;
static const u32 MIN_MATCH_LENGTH = 4;
static const u32 END_LITERALS = 5;
static const u32 LOOKAHEAD_LENGTH = 12;
static const u32 MAX_IN_TOKEN = 15;

static const u32 DICTIONARY_SIZE = 0x10000U / 4;
static const u32 INVALID_ENTRY = 0xFFFFFFFFU;

struct SLZ4Context
{
    u32 entries_[DICTIONARY_SIZE];
};

struct LZSSMatch
{
    u32 distance_;
    u32 length_;
};

/**
@return The number of bytes written into 'dst' or a negative value if this function fails.
@param capacity ... Byte size of a destination buffer 'dst'. This is at least more than the size which is reterned by the compressBound.
@param dst ... A destination buffer.
@param size ... Byte size of a source data. The maximum supported value for this is MAX_BLOCK_SIZE.
@param src ... A source data.
*/
s32 compress(SLZ4Context& context, u32 capacity, u8* dst, u32 size, const u8* src);

/**
@return The maximum number of bytes that compression may write into a destination buffer.
@param size ... Byte size of a source data. The maximum supported value for this is MAX_BLOCK_SIZE.
*/
s32 compressBound(u32 size);

/**
@return The number of bytes written into 'dst' or a negative value if this function fails.
@param capacity ... Byte size of a destination buffer 'dst'. The maximum supported value for this is MAX_BLOCK_SIZE.
@param dst ... A destination buffer.
@param size ... Byte size of a compressed data 'src'.
@param src ... A source compressed data.
*/
s32 decompress(u32 capacity, u8* dst, u32 size, const u8* src);

} //namespace slz4
#endif //INC_SLZ4_H_

#ifdef SLZ4_IMPLEMENTATION
#include <cstring>
#include <immintrin.h>

//--- wyhash
//---------------------------------------------------------------------
/* The Unlicense
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
For more information, please refer to <http://unlicense.org/>
*/
// This is free and unencumbered software released into the public domain under The Unlicense (http://unlicense.org/)
// main repo: https://github.com/wangyi-fudan/wyhash
// author: 王一 Wang Yi <godspeed_china@yeah.net>
// contributors: Reini Urban, Dietrich Epp, Joshua Haberman, Tommy Ettinger, Daniel Lemire, Otmar Ertl, cocowalla, leo-yuriev, Diego Barrios Romero, paulie-g, dumblob, Yann Collet, ivte-ms, hyb, James Z.M. Gao, easyaspi314 (Devin), TheOneric
// endian macros

#ifndef WYHASH_LITTLE_ENDIAN
#    if defined(_WIN32) || defined(__LITTLE_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#        define WYHASH_LITTLE_ENDIAN 1
#    elif defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#        define WYHASH_LITTLE_ENDIAN 0
#    else
#        warning could not determine endianness! Falling back to little endian.
#        define WYHASH_LITTLE_ENDIAN 1
#    endif
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define wyhash_likely(x) __builtin_expect((x), 1)
#    define wyhash_unlikely(x) __builtin_expect((x), 0)
#else
#    define wyhash_likely(x) (x)
#    define wyhash_unlikely(x) (x)
#endif

#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(__clang__)
#    define wyhash_restrict __restrict__
#elif defined(_MSC_VER)
#    define wyhash_restrict __restrict
#else
#    define wyhash_restrict
#endif

namespace slz4
{
namespace
{
    union U32Bytes
    {
        u8 bytes_[4];
        u32 u32_;
    };

    u32 pack(const u8* ptr)
    {
        U32Bytes bytes;
        bytes.bytes_[0] = ptr[0];
        bytes.bytes_[1] = ptr[1];
        bytes.bytes_[2] = ptr[2];
        bytes.bytes_[3] = ptr[3];
        return bytes.u32_;
    }

    u32 push(u32 code, u32 u)
    {
        return (code >> 8) | (u << 24);
    }

    static inline u64 wyhash_read4(const u8* p)
    {
        u32 v;
        ::memcpy(&v, p, 4);
        return v;
    }

    // 128bit multiply function
    static inline u64 wyhash_rot(u64 x)
    {
        return (x >> 32) | (x << 32);
    }

    static inline void wyhash_mum(u64* wyhash_restrict A, u64* wyhash_restrict B)
    {
#if (1)
        u64 hh = (*A >> 32) * (*B >> 32);
        u64 hl = (*A >> 32) * (u32)*B;
        u64 lh = (u32)*A * (*B >> 32);
        u64 ll = (u64)(u32)*A * (u32)*B;

        *A = wyhash_rot(hl) ^ hh;
        *B = wyhash_rot(lh) ^ ll;

#elif defined(__SIZEOF_INT128__)
        __uint128_t r = *A;
        r *= *B;
        *A = (u64)r;
        *B = (u64)(r >> 64U);

#elif defined(_MSC_VER) && defined(_M_X64)
        *A = _umul128(*A, *B, B);
#else
        u64 ha = *A >> 32;
        u64 hb = *B >> 32;
        u64 la = (u32)*A;
        u64 lb = (u32)*B;
        u64 hi, lo;
        u64 rh = ha * hb;
        u64 rm0 = ha * lb;
        u64 rm1 = hb * la;
        u64 rl = la * lb;
        u64 t = rl + (rm0 << 32);
        u64 c = t < rl;
        lo = t + (rm1 << 32);
        c += lo < t;
        hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
        *A = lo;
        *B = hi;
#endif
    }

    // multiply and xor mix function, aka MUM
    static inline u64 wyhash_mix(u64 A, u64 B)
    {
        wyhash_mum(&A, &B);
        return A ^ B;
    }

    u32 hash(u32 code)
    {
        assert(sizeof(code) == 4);
        static const u64 wyhash_param[4] = {0xA0761D6478BD642FULL, 0xE7037ED1A0B428DBULL, 0x8EBC6AF09C88C6E3ULL, 0x589965CC75374CC3ULL};
        u64 a, b;
        size_t size = sizeof(code);
        const u8* p = reinterpret_cast<const u8*>(&code);
        a = (wyhash_read4(p) << 32) | wyhash_read4(p + ((size >> 3) << 2));
        b = (wyhash_read4(p + size - 4) << 32) | wyhash_read4(p + size - 4 - ((size >> 3) << 2));
        static const u64 seed = 2685821657736338717ULL;
        u64 r = wyhash_mix(wyhash_param[1] ^ size, wyhash_mix(a ^ wyhash_param[1], b ^ seed));
        return static_cast<u32>(r - (r >> 32U));
    }

    //-------------------------------------------------------------------
    LZSSMatch findLongestMatch(SLZ4Context& context, u32 code, const u8* start, u32 position, u32 end)
    {
        SLZ4_ASSERT(MIN_MATCH_LENGTH <= (end - position));

        u32 index = hash(code) & (DICTIONARY_SIZE - 1);
        u32 result = context.entries_[index];
        context.entries_[index] = position;
        if(INVALID_ENTRY == result
           || MAX_DISTANCE < (position - result)
           || code != pack(start + result)) {
            return {0, 0};
        }

        SLZ4_ASSERT(result < position);
        u32 distance = position - result;
        u32 match = result + 4;
        position += 4;
        SLZ4_ASSERT(start[match - 4] == start[position - 4]);
        SLZ4_ASSERT(start[match - 3] == start[position - 3]);
        SLZ4_ASSERT(start[match - 2] == start[position - 2]);
        SLZ4_ASSERT(start[match - 1] == start[position - 1]);

        while(position < end) {
            if(start[match] != start[position]) {
                break;
            }
            ++match;
            ++position;
        }
        return {distance, match - result};
    }

    //-------------------------------------------------------------------
    bool writeLength(u32& pos, u32 capacity, u8* dst, u32 length)
    {
        SLZ4_ASSERT(0 <= length);
        if(length < MAX_IN_TOKEN) {
            return true;
        }
        length -= MAX_IN_TOKEN;

        while(255U <= length) {
            if(capacity <= pos) {
                return false;
            }
            dst[pos++] = 255U;
            length -= 255U;
        }
        if(capacity <= pos) {
            return false;
        }
        dst[pos++] = static_cast<u8>(length);
        return true;
    }

    //-------------------------------------------------------------------
    bool writeLiterals(u32& pos, u32 capacity, u8* dst, const u8* src, u32 literalLength)
    {
        SLZ4_ASSERT(0 <= literalLength);
        if(capacity <= pos) {
            return false;
        }

        //Write a sequence header
        u32 firstLiteralBits = literalLength < MAX_IN_TOKEN ? literalLength : MAX_IN_TOKEN;

        dst[pos++] = static_cast<u8>(firstLiteralBits << 4);

        //Write literal length
        if(!writeLength(pos, capacity, dst, literalLength)) {
            return false;
        }

        //Write literals
        if(capacity <= (pos + literalLength)) {
            return false;
        }
        for(u32 i = 0; i < literalLength; ++i, ++src) {
            dst[pos++] = *src;
        }
        return true;
    }

    //-------------------------------------------------------------------
    bool writeLiterals(u32& pos, u32 capacity, u8* dst, const u8* src, u32 literalLength, const LZSSMatch& match)
    {
        SLZ4_ASSERT(0 <= literalLength);
        SLZ4_ASSERT(0 <= match.distance_ && match.distance_ <= MAX_DISTANCE);
        SLZ4_ASSERT(MIN_MATCH_LENGTH <= match.length_);
        if(capacity <= pos) {
            return false;
        }

        u32 matchLength = match.length_ - MIN_MATCH_LENGTH;

        //Write a sequence header
        u32 firstLiteralBits = literalLength < MAX_IN_TOKEN ? literalLength : MAX_IN_TOKEN;
        u32 firstMatchBits = matchLength < MAX_IN_TOKEN ? matchLength : MAX_IN_TOKEN;

        dst[pos++] = static_cast<u8>((firstLiteralBits << 4) | firstMatchBits);

        //Write literal length
        if(!writeLength(pos, capacity, dst, literalLength)) {
            return false;
        }

        //Write literals
        if(capacity <= (pos + literalLength)) {
            return false;
        }
        for(u32 i = 0; i < literalLength; ++i, ++src) {
            dst[pos++] = *src;
        }

        //Write match offset
        if(capacity <= (pos + 1)) {
            return false;
        }
        dst[pos++] = static_cast<u8>(match.distance_ & 0xFFU);
        dst[pos++] = static_cast<u8>((match.distance_ >> 8) & 0xFFU);
        if(!writeLength(pos, capacity, dst, matchLength)) {
            return false;
        }
        return true;
    }

    //-------------------------------------------------------------------
    u32 decodeLength(const u8*& current, const u8* end)
    {
        u32 length = 0;
        while(current < end) {
            u32 len = current[0];
            length += len;
            ++current;
            if(len < 255U) {
                break;
            }
        }
        return length;
    }

    //-------------------------------------------------------------------
    u32 decodeLength(u32& current, u32 end, const u8* src)
    {
        u32 length = 0;
        while(current < end) {
            u32 len = src[current];
            length += len;
            ++current;
            if(len < 255U) {
                break;
            }
        }
        return length;
    }

    //-------------------------------------------------------------------
    void set(u8* dst, u32 value, u32 size)
    {
        u32 s = size >> 7;
        SLZ4_ASSERT(size == (s << 7));
        __m256i x = _mm256_set1_epi32(value);
        for(u32 i = 0; i < s; ++i) {
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst), x);
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + 32), x);
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + 64), x);
            _mm256_store_si256(reinterpret_cast<__m256i*>(dst + 96), x);
            dst += 128;
        }
    }

    //-------------------------------------------------------------------
    void copy(u8* dst, const u8* src, u32 size)
    {
        static const uintptr_t AlignMask = 3U;
        uintptr_t a0 = reinterpret_cast<uintptr_t>(dst) & AlignMask;
        uintptr_t a1 = reinterpret_cast<uintptr_t>(src) & AlignMask;

        u32 align = (((0 != a0) ? 1 : 0) << 1) | ((0 != a1) ? 1 : 0);
        u32 pos = 0;
        switch(align) {
        case 0: //both are aligned
        {
            u32 t0;
            u32 s = size;
            while(4 < s) {
                t0 = *reinterpret_cast<const u32*>(&src[pos]);
                *reinterpret_cast<u32*>(&dst[pos]) = t0;
                pos += 4;
                s -= 4;
            }
        } break;
        case 1: //dst is aligned
        {
            u32 t0;
            u32 s = size;
            while(4 < s) {
                t0 = pack(&src[pos]);
                *reinterpret_cast<u32*>(&dst[pos]) = t0;
                pos += 4;
                s -= 4;
            }
        } break;
        case 2: //src is aligned
        {
            u32 s = size;
            U32Bytes bytes;
            while(4 < s) {
                bytes.u32_ = *reinterpret_cast<const u32*>(&src[pos]);
                dst[pos + 0] = bytes.bytes_[0];
                dst[pos + 1] = bytes.bytes_[1];
                dst[pos + 2] = bytes.bytes_[2];
                dst[pos + 3] = bytes.bytes_[3];
                pos += 4;
                s -= 4;
            }
        } break;
        default: //both are unaligned
        {
            u32 s = size;
            while(4 < s) {
                dst[pos + 0] = src[pos + 0];
                dst[pos + 1] = src[pos + 1];
                dst[pos + 2] = src[pos + 2];
                dst[pos + 3] = src[pos + 3];
                pos += 4;
                s -= 4;
            }
        }
            break;
        }
        while(pos < size) {
            dst[pos] = src[pos];
            ++pos;
        }
    }

    void copy_overlap(u8* dst, const u8* src, u32 size)
    {
        u32 pos = 0;
        u32 s = size;
        while(4 < s) {
            dst[pos + 0] = src[pos + 0];
            dst[pos + 1] = src[pos + 1];
            dst[pos + 2] = src[pos + 2];
            dst[pos + 3] = src[pos + 3];
            pos += 4;
            s -= 4;
        }
        while(pos < size) {
            dst[pos] = src[pos];
            ++pos;
        }
    }

} // namespace

//-------------------------------------------------------------------
s32 compress(SLZ4Context& context, u32 capacity, u8* dst, u32 size, const u8* src)
{
    SLZ4_ASSERT(0 <= size && size <= MAX_BLOCK_SIZE);
    if(MAX_BLOCK_SIZE < size) {
        return -1;
    }

    u32 dstPos = 0;
    if(size < END_LITERALS) {
        return writeLiterals(dstPos, capacity, dst, src, size)
                   ? dstPos
                   : -1;
    }

    set(reinterpret_cast<u8*>(context.entries_), INVALID_ENTRY, sizeof(u32) * DICTIONARY_SIZE);
    { //Add the first code to our dictionary
        u32 code = pack(src);
        u32 index = hash(code) & (DICTIONARY_SIZE - 1);
        context.entries_[index] = 0;
    }

    u32 end = size;
    u32 endMatch = (LOOKAHEAD_LENGTH < size) ? size - LOOKAHEAD_LENGTH : 0;
    u32 pending = 0;
    u32 position = 4;
    while(position < endMatch) {
        u32 code = pack(src + position);
        LZSSMatch match = findLongestMatch(context, code, src, position, end);

        if(MIN_MATCH_LENGTH <= match.length_) {
            u32 literalLength = position - pending;
            if(!writeLiterals(dstPos, capacity, dst, src + pending, literalLength, match)) {
                return -1;
            }
            position += match.length_;
            pending = position;
            continue;
        }
        ++position;
    }

    if(pending < end) {
        u32 literalLength = end - pending;
        if(!writeLiterals(dstPos, capacity, dst, src + pending, literalLength)) {
            return -1;
        }
    }
    return dstPos;
}

//-------------------------------------------------------------------
s32 compressBound(u32 size)
{
    SLZ4_ASSERT(0 <= size && size <= MAX_BLOCK_SIZE);
    return (size <= MAX_BLOCK_SIZE) ? size + (size / 255) + 16 : -1;
}

//-------------------------------------------------------------------
s32 decompress(u32 capacity, u8* dst, u32 size, const u8* src)
{
    SLZ4_ASSERT(capacity <= MAX_BLOCK_SIZE);
    if(MAX_BLOCK_SIZE < capacity) {
        return -1;
    }
    u32 current = 0;
    u32 end0 = size;
    u32 end1 = END_LITERALS <= size ? size - END_LITERALS : 0;
    u32 dend = capacity;

    u32 d = 0;

    for(;;) {
        if(end0 <= current) {
            break;
        }

        //Decode token
        u32 literalLength = (src[current] >> 4);
        u32 matchLength = (src[current]) & 0xFU;
        ++current;

        //Decode literal length
        if(MAX_IN_TOKEN <= literalLength) {
            literalLength += decodeLength(current, end0, src);
        }

        //Read literals
        if(end0 < current || (end0 - current) < literalLength) {
            return -1;
        }
        if(dend < d || (dend - d) < literalLength) {
            return -1;
        }
        copy_overlap(&dst[d], &src[current], literalLength);
        d += literalLength;
        current += literalLength;

        if(end1 <= current) {
            break;
        }

        //Decode match offset
        u32 offset = static_cast<u32>(src[current]) | (static_cast<u32>(src[current + 1]) << 8);
        current += 2;

        //Decode match length
        if(MAX_IN_TOKEN <= matchLength) {
            matchLength += decodeLength(current, end0, src);
        }

        //Copy match
        if(d < offset) {
            return -1;
        }
        matchLength += MIN_MATCH_LENGTH;
        if(dend < d || (dend - d) < matchLength) {
            return -1;
        }
#if 1
        if(4<=offset && 256<=matchLength){
            copy(&dst[d], &dst[d - offset], matchLength);
        }else{
            copy_overlap(&dst[d], &dst[d - offset], matchLength);
        }
#else
        copy_overlap(&dst[d], &dst[d - offset], matchLength);
#endif
        d += matchLength;
    }
    return static_cast<s32>(d);
}

} //namespace slz4
#endif //SLZ4_IMPLEMENTATION
