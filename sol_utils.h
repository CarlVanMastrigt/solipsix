/**
Copyright 2025 Carl van Mastrigt

This file is part of solipsix.

solipsix is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

solipsix is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with solipsix.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#pragma once

#ifndef _GNU_SOURCE
static inline char* sol_strdup(const char * in)
{
    if(!in) return NULL;
    size_t len=(strlen(in)+1)*sizeof(char);
    char * out=malloc(len);
    return memcpy(out,in,len);
}

static inline void sol_sincosf(float angle, float* sin, float* cos)
{
    *sin = sinf(angle);
    *cos = cosf(angle);
}
#else
#define sol_sincosf(a,s,c) sincosf(a,s,c)
#define sol_strdup(s) strdup(s)
#endif


#define SOL_U32_INVALID 0xFFFFFFFF
#define SOL_U16_INVALID 0xFFFF

#define SOL_MAX(A,B) (((A)>(B))?(A):(B))
#define SOL_MIN(A,B) (((A)<(B))?(A):(B))
#define SOL_CLAMP(X,MIN,MAX) ((X)>(MAX)?(MAX):(((X)<(MIN))?(MIN):(X)))

static inline uint64_t sol_u64_align(uint64_t size, uint64_t alignment)
{
    return (size+alignment-1) & ~(alignment-1);
}
static inline uint32_t sol_u32_align(uint32_t size, uint32_t alignment)
{
    return (size+alignment-1) & ~(alignment-1);
}


//exp is nearest power of 2 relative to some value
static inline uint64_t sol_u64_ctz      (uint64_t v){ return (v < 1) ? 64 :    __builtin_ctzl(v  ); }
static inline uint64_t sol_u64_clz      (uint64_t v){ return (v < 1) ? 64 :    __builtin_clzl(v  ); }
static inline uint64_t sol_u64_exp_gte  (uint64_t v){ return (v < 2) ? 0  : 64-__builtin_clzl(v-1); }
static inline uint64_t sol_u64_exp_lte  (uint64_t v){ return (v < 1) ? 0  : 63-__builtin_clzl(v  ); }
static inline uint64_t sol_u64_exp_gt   (uint64_t v){ return (v < 1) ? 0  : 64-__builtin_clzl(v  ); }
static inline uint64_t sol_u64_exp_lt   (uint64_t v){ return (v < 2) ? 0  : 63-__builtin_clzl(v-1); }
static inline uint64_t sol_u64_bit_count(uint64_t v){ return              __builtin_popcountl(v  ); }

static inline uint32_t sol_u32_ctz      (uint32_t v){ return (v < 1) ? 32 :    __builtin_ctz(v  ); }
static inline uint32_t sol_u32_clz      (uint32_t v){ return (v < 1) ? 32 :    __builtin_clz(v  ); }
static inline uint32_t sol_u32_exp_gte  (uint32_t v){ return (v < 2) ? 0  : 32-__builtin_clz(v-1); }
static inline uint32_t sol_u32_exp_lte  (uint32_t v){ return (v < 1) ? 0  : 31-__builtin_clz(v  ); }
static inline uint32_t sol_u32_exp_gt   (uint32_t v){ return (v < 1) ? 0  : 32-__builtin_clz(v  ); }
static inline uint32_t sol_u32_exp_lt   (uint32_t v){ return (v < 2) ? 0  : 31-__builtin_clz(v-1); }
static inline uint32_t sol_u32_bit_count(uint32_t v){ return              __builtin_popcount(v  ); }

#define SOL_CONCATENATE_MACRO(A,B) A##B
#define SOL_CONCATENATE(A,B) SOL_CONCATENATE_MACRO(A,B)

#define SOL_SWAP(A,B) { typeof(A) swap_tmp = A; A = B; B = swap_tmp; }
