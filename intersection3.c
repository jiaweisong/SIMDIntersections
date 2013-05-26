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

// #define MATCH(destreg, matchreg) _mm_cmpeq_epi32(destreg, matchreg)
// #define OR(destreg, otherreg)  _mm_or_si128(destreg, otherreg)
// #define LOAD(ptr) _mm_load_si128(ptr)
#define TESTZERO(reg) _mm_testz_si128(reg, reg)
// #define SETALL(int32)  _mm_set1_epi32(int32)

#define VMOVE(dest, src) asm volatile("movdqa %1, %0" : "=x" (dest) : "x" (src) );

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

// NOTE: Using "asm volatile" does help keep desired ordering
#define VOLATILE volatile

#define VOR(dest, other) \
    asm VOLATILE("por %1, %0" : "+x" (dest) : "x" (other) );

#define VLOAD(dest, ptr, offset)                                        \
    asm VOLATILE("movdqu %c2(%1), %0" : "+x" (dest) : "g" (ptr), "i" (offset * sizeof(VECTYPE)) );

#define VSETALL(dest, int32)                                            \
    asm VOLATILE("movd %1, %0; pshufd $0, %0, %0" : "+x" (dest) : "g" (int32) ) 

// #define VMATCH(dest, other) dest = _mm_cmpeq_epi32(dest, other)
#define VMATCH(dest, other)                                   \
    asm VOLATILE("pcmpeqd %1, %0" : "+x" (dest) : "x" (other) )

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

    while (sometimes(maxChunk < nextMatch)) {
    RELOAD_FREQ:
        if (rarely(freq + CHUNKINTS >= stopFreq)) {
            goto FINISH_SCALAR;
        }
        freq += CHUNKINTS;
        maxChunk = freq[CHUNKINTS - 1];
    }

    register VECTYPE M0 asm("xmm0");
    VLOAD(M0, freq, 0); 
#if NUMVECS > 1 
    register VECTYPE M1 asm("xmm1");
    VLOAD(M1, freq, 1); 
#if NUMVECS > 2 
    register VECTYPE M2 asm("xmm2");
    VLOAD(M2, freq, 2); 
    register VECTYPE M3 asm("xmm3");
    VLOAD(M3, freq,  3); 
#if NUMVECS > 4 
    register VECTYPE M4 asm("xmm4");
    VLOAD(M4, freq, 4); 
    register VECTYPE M5 asm("xmm5");
    VLOAD(M5, freq, 5); 
    register VECTYPE M6 asm("xmm6");
    VLOAD(M6, freq, 6); 
    register VECTYPE M7 asm("xmm7");
    VLOAD(M7, freq, 7); 
#if NUMVECS > 8
    register VECTYPE M8 asm("xmm8");
    VLOAD(M8, freq, 8); 
    register VECTYPE M9 asm("xmm9");
    VLOAD(M9, freq, 9); 
    register VECTYPE M10 asm("xmm10");
    VLOAD(M10, freq, 10); 
    register VECTYPE M11 asm("xmm11");
    VLOAD(M11, freq, 11); 
#if NUMVECS > 12  // must be 16
    register VECTYPE M12 asm("xmm12");
    VLOAD(M12, freq, 12); 
    register VECTYPE M13 asm("xmm13");
    VLOAD(M13, freq, 13);
    //    register VECTYPE M14 asm("xmm14") = {0,0};   // Potential workaround that breaks icc
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

#ifdef IACA
    IACA_START;
#endif

    register VECTYPE Match asm("xmm15");
 CHECK_NEXT_RARE:
    VSETALL(Match, nextMatch);
    ASSERT(maxChunk >= nextMatch);
    if (usually(rare < lastRare)) { 
        nextMatch = rare[1];     
    }
    // NOTE: safe to leave nextMatch set to *rare on last iteration

    const uint32_t *nextFreq = freq; // location to reload next set vectors (possibly unchanged)

#if (LOOKAHEAD >= 1)
    uint32_t jump = 0;  // convince compiler we really want a cmov
    if (sometimes(nextMatch > maxChunk)) { // PROFILE: verify cmov
        jump = CHUNKINTS;
    }        
    nextFreq += jump;
#if (LOOKAHEAD >= 2)
    jump = 0;
    if (unlikely(nextMatch > maxChunk2)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    nextFreq += jump;
#if (LOOKAHEAD >= 3)
    jump = 0;
    if (rarely(nextMatch > maxChunk3)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    nextFreq += jump;
#if (LOOKAHEAD >= 4)
    jump = 0;
    if (rarely(nextMatch > maxChunk4)) { // PROFILE: verify cmov
        jump = CHUNKINTS;  
    }        
    nextFreq += jump;
#endif // 4
#endif // 3
#endif // 2
#endif // 1

    maxChunk = nextFreq[CHUNKINTS - 1];
#if (LOOKAHEAD >= 2)
    maxChunk2 = nextFreq[2 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 3)
    maxChunk3 = nextFreq[3 * CHUNKINTS - 1]; 
#if (LOOKAHEAD >= 4)
    maxChunk4 = nextFreq[4 * CHUNKINTS - 1]; 
#endif // 4
#endif // 3
#endif // 2

#if NUMVECS == 1
    VECTYPE F0; // final
    F0 = _mm_cmpeq_epi32(M0, Match);
    M0 = _mm_load_si128((VECTYPE *)nextFreq);
    if (! TESTZERO(F0)) {
        count += 1;
    }
#elif NUMVECS == 2
    VMATCH(M0, Match);
    VMATCH(M1, Match);
    VOR(M1, M0);
    M0 = _mm_load_si128((VECTYPE *) nextFreq + 0);
    if (! TESTZERO(M0)) {
        count += 1;
    }
    M1 = _mm_load_si128((VECTYPE *) nextFreq + 1);
#elif NUMVECS == 4

    VMATCH(M0, Match);
    VMATCH(M1, Match);

    COMPILER_BARRIER;

    VMATCH(M2, Match);
    VMATCH(M3, Match);
    VOR(M1, M0);
    VLOAD(M0, nextFreq, 0);

    COMPILER_BARRIER;

    VOR(M3, M2);
    VLOAD(M2, nextFreq, 2);

    COMPILER_BARRIER;

    VOR(M3, M1);
    VLOAD(M1, nextFreq, 1);

    if (! TESTZERO(M3)) {
        count += 1;
    }
    VLOAD(M3, nextFreq, 3);

    COMPILER_BARRIER;


#elif NUMVECS == 8

    VMATCH(M0, Match);
    VMATCH(M1, Match);

    COMPILER_BARRIER;

    VMATCH(M2, Match);
    VMATCH(M3, Match);
    VOR(M1, M0);
    VLOAD(M0, nextFreq, 0);

    COMPILER_BARRIER;

    VMATCH(M4, Match);
    VMATCH(M5, Match); 
    VOR(M3, M2);
    VLOAD(M2, nextFreq, 2);

    COMPILER_BARRIER;

    VMATCH(M6, Match);
    VMATCH(M7, Match);
    VOR(M3, M1);
    VLOAD(M1, nextFreq, 1);

    COMPILER_BARRIER;

    VOR(M5, M4);
    VOR(M7, M6);
    VLOAD(M4, nextFreq, 4);
    VLOAD(M6, nextFreq, 6);

    COMPILER_BARRIER;

    VOR(M7, M5);
    VLOAD(M5, nextFreq, 5);

    COMPILER_BARRIER;

    VOR(M7, M3);
    VLOAD(M3, nextFreq, 3);

    COMPILER_BARRIER;

    if (! TESTZERO(M7)) {
        count += 1;             // PROFILE: verify cmov
    }
    VLOAD(M7, nextFreq, 7);

#elif NUMVECS == 12

    VMATCH(M0, Match);
    VMATCH(M1, Match);

    COMPILER_BARRIER;

    VMATCH(M2, Match);
    VMATCH(M3, Match);
    VOR(M1, M0);
    VLOAD(M0, nextFreq, 0);

    COMPILER_BARRIER;

    VMATCH(M4, Match);
    VMATCH(M5, Match); 
    VOR(M3, M2);
    VLOAD(M2, nextFreq, 2);

    COMPILER_BARRIER;

    VMATCH(M6, Match);
    VMATCH(M7, Match);
    VOR(M3, M1);
    VLOAD(M1, nextFreq, 1);

    COMPILER_BARRIER;

    VMATCH(M8, Match);
    VMATCH(M9, Match);
    VOR(M5, M4);
    VLOAD(M4, nextFreq, 4);

    COMPILER_BARRIER;

    VMATCH(M10, Match);
    VMATCH(M11, Match);
    VOR(M7, M6);
    VLOAD(M6, nextFreq, 6);

    COMPILER_BARRIER;
    
    VOR(M7, M3);
    VOR(M9, M8);
    VOR(M11, M10);
    VLOAD(M3, nextFreq, 3);
    VLOAD(M8, nextFreq, 8);

    COMPILER_BARRIER;

    VOR(M11, M9);
    VOR(M7, M5);
    VLOAD(M5, nextFreq, 5);
    VLOAD(M9, nextFreq, 9);

    COMPILER_BARRIER;

    VOR(M11, M7);
    VLOAD(M7, nextFreq, 7);
    VLOAD(M10, nextFreq, 10);

    if (! TESTZERO(M11)) {
        count += 1;             // PROFILE: verify cmov
    }
    VLOAD(M11, nextFreq, 11);

#elif NUMVECS == 16
    VMATCH(M0, Match);
    VMATCH(M1, Match);

    VMATCH(M2, Match);
    VMATCH(M3, Match);
    VOR(M0, M1);    

    VLOAD(M1, nextFreq, 14);   // 1 doubles as 14
    VMATCH(M4, Match);
    VMATCH(M5, Match);
    VOR(M2, M3);    

    VLOAD(M3, nextFreq, 15);   // 3 doubles as 15
    VMATCH(M6, Match); 
    VMATCH(M7, Match);
    VOR(M2, M0);    
    VLOAD(M0, nextFreq, 0);

    VMATCH(M8, Match);
    VMATCH(M9, Match);
    VOR(M7, M4);    
    VLOAD(M4, nextFreq, 4);

    VMATCH(M10, Match);
    VMATCH(M11, Match);
    VOR(M9, M5);    
    VLOAD(M5, nextFreq, 5);

    VMATCH(M12, Match);
    VMATCH(M13, Match);
    VOR(M9, M7);    
    VLOAD(M7, nextFreq, 7);

    VMATCH(M1, Match);  // MATCH("14",M) (at least 4 cycles after load)
    VOR(M13, M10);   
    VLOAD(M10, nextFreq, 10);
    VOR(M9, M2);     
    VLOAD(M2, nextFreq, 2);

    VMATCH(M3, Match); // MATCH("15",M) (at least 4 cycles after load)
    VOR(M12, M1);   // OR(12,"14");
    VLOAD(M1, nextFreq, 1);  // Done with "14", reload original 1
    VOR(M13, M9);   
    VLOAD(M9, nextFreq, 9); 

    VOR(M8, M3);    //  OR(8,"15");
    VLOAD(M3, nextFreq, 3);  // Done with "15", reload original 3
    VOR(M11, M6);   
    VLOAD(M6, nextFreq, 6);

    VOR(M11, M8);   
    VLOAD(M8, nextFreq, 8);
    VOR(M13, M12);  
    VLOAD(M12, nextFreq, 12);

    VOR(M13, M11);  
    VLOAD(M11, nextFreq, 11);

    if (! TESTZERO(M13)) {
        count += 1;
    }
    VLOAD(M13, nextFreq, 13);
#else
#error NUMVECS must be one of {1, 2, 4, 8, 12, 16}
#endif
    // NOTE: best effort has been made so that vectors are reloaded for next use 
    //       if maxChunk >= nextMatch, they are loaded correctly.  If not, advance freq.
    freq = nextFreq;  // PROFILE: verify that freq and nextFreq are the same register

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

#ifdef IACA
    IACA_END;
#endif

    goto RELOAD_FREQ;  // need to scan farther along in freq 

       
    
 FINISH_SCALAR:
    lenFreq = lastFreq - freq + 1;
    lenRare = lastRare - rare + 1;
    return count + finish_scalar(freq, lenFreq, rare, lenRare);
}


#ifdef NOTES

PSHUFD 0|5 1
P(AND|OR)D  0|1|5 1
PSL(L|R)D 1  1
PCMP(EQ|GT)D 0|5 1
PMOVMSKB  0 2
PMOVMSKPS 0 2
POPCNT 1 3
PEXTRD 2*(0|1|5) 2
BS(R|F) 0|1|5 3
CMP 0|1|5 1

// PROFILE: ASSUME 16 cycle penalty for misprediction

.25 cycle/int = 10Bint/s  (16Bint/s theoretical at 4GHz);
.5 cycle/int = 5Bint/s    (8.0Bint/s)
1 cycle/int = 2.5Bint/s   (4.0Bint/s);
2 cycle/int = 1.2Bint/s   (2.0Bint/s);
3 cycle/int = 0.8Bint/s   (1.3Bint/s);
4 cycle/int = 0.6Bint/s   (1.0Bint/s);


#endif // NOTES

  
