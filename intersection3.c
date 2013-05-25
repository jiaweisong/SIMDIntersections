/* TODO */

// Add support for 1, 2, 4, 8, 12, 16 vectors
// Make more explicitly a template file (split off)
// Create assembly.S template also
// Add support for different size vectors?



#include <stdint.h>
#include <stddef.h>
#include <strings.h>

#ifdef IACA
#include </opt/intel/iaca-lin32/include/iacaMarks.h>
#endif  

#include <immintrin.h>
#include <smmintrin.h>


// May help choose order of branches, but mostly comments to reader
// NOTE: for icc, seems like these prevent use of conditional moves
#define usually(x)    __builtin_expect((x),1)
#define likely(x)     __builtin_expect((x),1)
#define often(x)      __builtin_expect((x),1)
#define sometimes(x)  __builtin_expect((x),0)
#define unlikely(x)   __builtin_expect((x),0)
#define rarely(x)     __builtin_expect((x),0)

#include <stdlib.h>
#include <stdio.h>

#define VECTYPE __m128i

#define CHUNKINTS (NUMVECS * (sizeof(VECTYPE)/sizeof(uint32_t))) 

// #define COMPILER_BARRIER asm volatile("" ::: "memory")
#define COMPILER_BARRIER

#define ASSERT(x) // do nothing

#define MATCH(destreg, matchreg) _mm_cmpeq_epi32(destreg, matchreg)
#define OR(destreg, otherreg)  _mm_or_si128(destreg, otherreg)
#define LOAD(ptr) _mm_load_si128(ptr)
#define TESTZERO(reg) _mm_testz_si128(reg, reg)
#define SETALL(int32)  _mm_set1_epi32(int32)

#define VOR(dest, other) asm volatile("por %1, %0" : "=x" (dest) : "x" (other) );
#define VLOAD(dest, ptr) asm volatile("movdqu %1, %0" : "=x" (dest) : "*m" (ptr) );
#define VMOVE(dest, src) asm volatile("movdqa %1, %0" : "=x" (dest) : "x" (src) );
#define VMATCH(dest, other) asm volatile("pcmpeqd %1, %0" : "=x" (dest) : "x" (other) );

//  __asm__ __volatile__ ("psllq %0, %0" : "=x" (vec.full128b) : "x"  (vec.full128b));
// asm volatile("movdqu %0, %%xmm0" :  : "m" (xmm_reg) : "%xmm0"  );


size_t finish_scalar(const uint32_t *A, size_t lenA,
                     const uint32_t *B, size_t lenB) {

    size_t count = 0;
    if (lenA == 0 || lenB == 0) return count;
    
    const uint32_t *endA = A + lenA;
    const uint32_t *endB = B + lenB;
    
    
    while (1) {
        while (*A < *B) {
        SKIP_FIRST_COMPARE:
            if (++A == endA) return count; 
        }
        while (*A > *B) {
            if (++B == endB) return count; 
        }
        if (*A == *B) { 
            count++;
            if (++A == endA || ++B == endB) return count;
        }
        else {
            goto SKIP_FIRST_COMPARE;
        }
    }
    
    return count; // NOTREACHED
}

#if ! defined(LOOKAHEAD) || LOOKAHEAD > 4
#error LOOKAHEAD must be defined and one of {0,1,2,3,4}
#endif

#if LOOKAHEAD > 0
#define SAFESPACE (LOOKAHEAD * CHUNKINTS - 1)
#else 
#define SAFESPACE (CHUNKINTS - 1)
#endif

size_t search_chunks(const uint32_t *freq, size_t lenFreq,
                     const uint32_t *rare, size_t lenRare) {
    
    size_t count = 0;
    if (lenFreq == 0 || lenRare == 0) {
        return 0;
    }

    const uint32_t *lastRare = &rare[lenRare];
    const uint32_t *lastFreq = &rare[lenRare];
    const uint32_t *stopFreq = lastFreq - SAFESPACE;
    
    // skip straight to scalar if not enough room to load vectors
    if (rarely(freq >= stopFreq)) {
        goto FINISH_SCALAR;
    }

    uint32_t maxChunk = freq[CHUNKINTS - 1];
    uint32_t nextMatch = *rare;
    VECTYPE Match;

    while (sometimes(maxChunk < nextMatch)) {
    RELOAD_FREQ:
        if (rarely(freq + CHUNKINTS >= stopFreq)) {
            goto FINISH_SCALAR;
        }
        freq += CHUNKINTS;
        maxChunk = freq[CHUNKINTS - 1];
    }

    VECTYPE M0 = LOAD((VECTYPE *)freq + 0); 
#if NUMVECS > 1 
    VECTYPE M1 = LOAD((VECTYPE *)freq + 1); 
#if NUMVECS > 2 
    VECTYPE M2 = LOAD((VECTYPE *)freq + 2); 
    VECTYPE M3 = LOAD((VECTYPE *)freq + 3); 
#if NUMVECS > 4 
    VECTYPE M4 = LOAD((VECTYPE *)freq + 4); 
    VECTYPE M5 = LOAD((VECTYPE *)freq + 5); 
    VECTYPE M6 = LOAD((VECTYPE *)freq + 6); 
    VECTYPE M7 = LOAD((VECTYPE *)freq + 7); 
#if NUMVECS > 8
    VECTYPE M8 = LOAD((VECTYPE *)freq + 4); 
    VECTYPE M9 = LOAD((VECTYPE *)freq + 5); 
    VECTYPE M10 = LOAD((VECTYPE *)freq + 6); 
    VECTYPE M11 = LOAD((VECTYPE *)freq + 7); 
#if NUMVECS > 12  // must be 16
    VECTYPE M12 = LOAD((VECTYPE *)freq + 6); 
    VECTYPE M13 = LOAD((VECTYPE *)freq + 7); 
    // NOTE: M14 and M15 will be simulated
#endif // 12
#endif // 8
#endif // 4
#endif // 2
#endif // 1
        
#if (LOOKAHEAD >= 2)
    uint32_t maxChunk2 = freq[2 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 3)
    uint32_t maxChunk3 = freq[3 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 4)
    uint32_t maxChunk4 = freq[4 * CHUNKINTS - 1]; 
#endif // 4
#endif // 3
#endif // 2

 CHECK_NEXT_RARE:
    Match = SETALL(nextMatch);
    ASSERT(maxChunk >= nextMatch);
    if (usually(rare < lastRare)) { 
        nextMatch = rare[1];     
    }
    // NOTE: safe to leave nextMatch set to *rare on last iteration
    // FUTURE: skip over freq jumps for last iteration? No, would be less efficient.

    // FUTURE: clearer to use nextFreq below here?
#if (LOOKAHEAD >= 1)
    uint32_t jump = 0;  // convince compiler we really want a cmov
    if (sometimes(nextMatch > maxChunk)) { // PROFILE: verify cmov
        jump = CHUNKINTS;
    }        
    freq += jump;
#if (LOOKAHEAD >= 2)
    jump = 0;
    if (unlikely(nextMatch > maxChunk2)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    freq += jump;
#if (LOOKAHEAD >= 3)
    jump = 0;
    if (rarely(nextMatch > maxChunk3)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    freq += jump;
#if (LOOKAHEAD >= 4)
    jump = 0;
    if (rarely(nextMatch > maxChunk4)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    freq += jump;
#endif // 4
#endif // 3
#endif // 2
#endif // 1

    maxChunk = freq[CHUNKINTS - 1];
#if (LOOKAHEAD >= 2)
    maxChunk2 = freq[2 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 3)
    maxChunk3 = freq[3 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 4)
    maxChunk4 = freq[4 * CHUNKINTS - 1]; 
#endif // 4
#endif // 3
#endif // 2

#if NUMVECS == 1
    VECTYPE F0; // final
    F0 = _mm_cmpeq_epi32(M0, Match);
    M0 = _mm_load_si128((VECTYPE *)freq);
    if (! TESTZERO(F0)) {
        count += 1;
    }
#elif NUMVECS == 2
    VECTYPE F0; // final
    M0 = _mm_cmpeq_epi32(M0, Match);
    M1 = _mm_cmpeq_epi32(M1, Match);
    F0 = _mm_or_si128(M0, M1);
    M0 = _mm_load_si128((VECTYPE *) freq + 0);
    M1 = _mm_load_si128((VECTYPE *) freq + 1);
    if (! TESTZERO(F0)) {
        count += 1;
    }
#elif NUMVECS == 4
    VECTYPE S0, S1; // semis
    VECTYPE F0; // final

    COMPILER_BARRIER;

    M0 = _mm_cmpeq_epi32(M0, Match);
    M1 = _mm_cmpeq_epi32(M1, Match);
    M2 = _mm_cmpeq_epi32(M2, Match);

    COMPILER_BARRIER;

    M3 = _mm_cmpeq_epi32(M3, Match);
    S0 = _mm_or_si128(M0, M1);
    M0 = _mm_load_si128((VECTYPE *) freq + 0);
    M1 = _mm_load_si128((VECTYPE *) freq + 1);

    COMPILER_BARRIER;

    S1 = _mm_or_si128(M2, M3);
    M2 = _mm_load_si128((VECTYPE *) freq + 2);
    M3 = _mm_load_si128((VECTYPE *) freq + 3);

    COMPILER_BARRIER;

    F0 = _mm_or_si128(S0, S1);

    if (! TESTZERO(F0)) {
        count += 1;
    }

#elif NUMVECS == 8

    M0 = _mm_cmpeq_epi32(M0, Match);
    M1 = _mm_cmpeq_epi32(M1, Match);

    COMPILER_BARRIER;

    M2 = _mm_cmpeq_epi32(M2, Match);
    M3 = _mm_cmpeq_epi32(M3, Match);
    M1 = _mm_or_si128(M0, M1);
    M0 = _mm_load_si128((VECTYPE *) freq + 0);

    COMPILER_BARRIER;

    M4 = _mm_cmpeq_epi32(M4, Match);
    M5 = _mm_cmpeq_epi32(M5, Match); 
    M3 = _mm_or_si128(M3, M2);
    M2 = _mm_load_si128((VECTYPE *) freq + 2);

    COMPILER_BARRIER;

    M6 = _mm_cmpeq_epi32(M6, Match);
    M7 = _mm_cmpeq_epi32(M7, Match);
    M3 = _mm_or_si128(M3, M1)
    M1 = _mm_load_si128((VECTYPE *) freq + 1);

    COMPILER_BARRIER;

    M5 = _mm_or_si128(M5, M4);
    M7 = _mm_or_si128(M7, M6);
    M4 = _mm_load_si128((VECTYPE *) freq + 4);
    M6 = _mm_load_si128((VECTYPE *) freq + 6);

    COMPILER_BARRIER;

    M7 = _mm_or_si128(M7, M5);
    M5 = _mm_load_si128((VECTYPE *) freq + 5);

    COMPILER_BARRIER;

    M7 = _mm_or_si128(M7, M3);
    M3 = _mm_load_si128((VECTYPE *) freq + 3);

    COMPILER_BARRIER;

    if (! TESTZERO(M7)) {
        count += 1;             // PROFILE: verify cmov
    }
    M7 = _mm_load_si128((VECTYPE *) freq + 7);

#elif NUMVECS == 12

    M0 = _mm_cmpeq_epi32(M0, Match);
    M1 = _mm_cmpeq_epi32(M1, Match);

    COMPILER_BARRIER;

    M2 = _mm_cmpeq_epi32(M2, Match);
    M3 = _mm_cmpeq_epi32(M3, Match);
    M1 = _mm_or_si128(M0, M1);
    M0 = _mm_load_si128((VECTYPE *) freq + 0);

    COMPILER_BARRIER;

    M4 = _mm_cmpeq_epi32(M4, Match);
    M5 = _mm_cmpeq_epi32(M5, Match); 
    M3 = _mm_or_si128(M3, M2);
    M2 = _mm_load_si128((VECTYPE *) freq + 2);

    COMPILER_BARRIER;

    M6 = _mm_cmpeq_epi32(M6, Match);
    M7 = _mm_cmpeq_epi32(M7, Match);
    M3 = _mm_or_si128(M3, M1);
    M1 = _mm_load_si128((VECTYPE *) freq + 1);

    COMPILER_BARRIER;

    M8 = _mm_cmpeq_epi32(M8, Match);
    M9 = _mm_cmpeq_epi32(M9, Match);
    M5 = _mm_or_si128(M5, M4);
    M4 = _mm_load_si128((VECTYPE *) freq + 4);

    COMPILER_BARRIER;

    M10 = _mm_cmpeq_epi32(M10, Match);
    M11 = _mm_cmpeq_epi32(M11, Match);
    M7 = _mm_or_si128(M7, M6);
    M6 = _mm_load_si128((VECTYPE *) freq + 6);

    COMPILER_BARRIER;
    
    M7 = _mm_or_si128(M7, M3);
    M9 = _mm_or_si128(M9, M8);
    M11 = _mm_or_si128(M11, M10);
    M3 = _mm_load_si128((VECTYPE *) freq + 3);
    M8 = _mm_load_si128((VECTYPE *) freq + 8);

    COMPILER_BARRIER;

    M11 = _mm_or_si128(M11, M9);
    M7 = _mm_or_si128(M7, M5);
    M5 = _mm_load_si128((VECTYPE *) freq + 5);
    M9 = _mm_load_si128((VECTYPE *) freq + 9);

    COMPILER_BARRIER;

    M11 = _mm_or_si128(M11, M7);
    M7 = _mm_load_si128((VECTYPE *) freq + 7);
    M10 = _mm_load_si128((VECTYPE *) freq + 10);

    if (! TESTZERO(M11)) {
        count += 1;             // PROFILE: verify cmov
    }
    M11 = _mm_load_si128((VECTYPE *) freq + 11);

#elif NUMVECS == 16
    M0 = _mm_cmpeq_epi32(M0, Match);
    M1 = _mm_cmpeq_epi32(M1, Match);

    M2 = _mm_cmpeq_epi32(M2, Match);
    M3 = _mm_cmpeq_epi32(M3, Match);
    M0 = _mm_or_si128(M0, M1);    

    M1 = _mm_load_si128((VECTYPE *) freq + 14);   // 1 doubles as 14
    M4 = _mm_cmpeq_epi32(M4, Match);
    M5 = _mm_cmpeq_epi32(M5, Match);
    M2 = _mm_or_si128(M2, M3);    

    M3 = _mm_load_si128((VECTYPE *) freq + 15);   // 3 doubles as 15
    M6 = _mm_cmpeq_epi32(M6, Match); 
    M7 = _mm_cmpeq_epi32(M7, Match);
    M2 = _mm_or_si128(M2, M0);    
    M0 = _mm_load_si128((VECTYPE *) freq + 0);

    M8 = _mm_cmpeq_epi32(M8, Match);
    M9 = _mm_cmpeq_epi32(M9, Match);
    M7 = _mm_or_si128(M7, M4);    
    M4 = _mm_load_si128((VECTYPE *) freq + 4);

    M10 = _mm_cmpeq_epi32(M10, Match);
    M11 = _mm_cmpeq_epi32(M11, Match);
    M9 = _mm_or_si128(M9, M5);    
    M5 = _mm_load_si128((VECTYPE *) freq + 5);

    M12 = _mm_cmpeq_epi32(M12, Match);
    M13 = _mm_cmpeq_epi32(M13, Match);
    M9 = _mm_or_si128(M9, M7);    
    M7 = _mm_load_si128((VECTYPE *) freq + 7);

    M1 = _mm_cmpeq_epi32(M1, Match);  // MATCH("14",M) (at least 4 cycles after load)
    M13 = _mm_or_si128(M13, M10);   
    M10 = _mm_load_si128((VECTYPE *) freq + 10);
    M9 = _mm_or_si128(M9, M2);     
    M2 = _mm_load_si128((VECTYPE *) freq + 2);

    M3 = _mm_cmpeq_epi32(M3, Match); // MATCH("15",M) (at least 4 cycles after load)
    M12 = _mm_or_si128(M12, M1);   // OR(12,"14");
    M1 = _mm_load_si128((VECTYPE *) freq + 1);  // Done with "14", reload original 1
    M13 = _mm_or_si128(M13, M9);   
    M9 = _mm_load_si128((VECTYPE *) freq + 9); 

    M8 = _mm_or_si128(M8, M3);    //  OR(8,"15");
    M3 = _mm_load_si128((VECTYPE *) freq + 3);  // Done with "15", reload original 3
    M11 = _mm_or_si128(M11, M6);   
    M6 = _mm_load_si128((VECTYPE *) freq + 6);

    M11 = _mm_or_si128(M11, M8);   
    M8 = _mm_load_si128((VECTYPE *) freq + 8);
    M13 = _mm_or_si128(M13, M12);  
    M12 = _mm_load_si128((VECTYPE *) freq + 12);

    M13 = _mm_or_si128(M13, M11);  
    M11 = _mm_load_si128((VECTYPE *) freq + 11);

    if (! TESTZERO(M13)) {
        count += 1;
    }
    M13 = _mm_load_si128((VECTYPE *) freq + 13);
#else
#error NUMVECS must be one of {1, 2, 4, 8, 12, 16}
#endif
    // NOTE: best effort has been made so that vectors are reloaded for next use 
    //       if maxChunk >= nextMatch, they are loaded correctly.  If not, advance freq.

    // completely done if we have already checked lastRare
    if (rarely(rare >= lastRare)) {
        ASSERT(rare == lastRare);
        return count; // no scalar finish after lastRare done
    }
    rare += 1;  

    if (rarely(freq >= stopFreq)) {
        // FUTURE: try to add one more pass for preloaded vectors?
        goto FINISH_SCALAR;
    }        

    if (usually(maxChunk >= nextMatch)) {
        goto CHECK_NEXT_RARE; // preload worked and vectors are ready
    } 

    goto RELOAD_FREQ;  // need to scan farther along in freq 
    
 FINISH_SCALAR:
    lenFreq = lastFreq - freq + 1;
    lenRare = lastRare - rare + 1;
    return count + finish_scalar(freq, lenFreq, rare, lenRare);
}



  
