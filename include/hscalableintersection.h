/*
 * Schemes inspired or lifted from
 * http://highlyscalable.wordpress.com/2012/06/05/fast-intersection-sorted-lists-sse/
 */

#ifndef HSCALABLEINTERSECTION_H_
#define HSCALABLEINTERSECTION_H_

namespace highlyscalablewordpresscom {
/**
 * Taken almost verbatim from http://highlyscalable.wordpress.com/2012/06/05/fast-intersection-sorted-lists-sse/
 * (just for comparison)
 */
size_t cardinality_intersect_scalar(const uint32_t *A, const size_t s_a,
        const uint32_t *B, const size_t s_b) {
    size_t i_a = 0, i_b = 0;
    size_t counter = 0;

    while (i_a < s_a && i_b < s_b) {
        if (A[i_a] < B[i_b]) {
            i_a++;
        } else if (B[i_b] < A[i_a]) {
            i_b++;
        } else {
            counter++;
            i_a++;
            i_b++;
        }
    }
    return counter;
}

/**
 * Taken almost verbatim from http://highlyscalable.wordpress.com/2012/06/05/fast-intersection-sorted-lists-sse/
 */
static __m128i shuffle_mask[16]; // precomputed dictionary


int getBit(int value, int position) {
    return ((value & (1 << position)) >> position);
}

// a simple implementation, we don't care about performance here
void prepare_shuffling_dictionary() {
    for (int i = 0; i < 16; i++) {
        int counter = 0;
        char permutation[16];
        memset(permutation, 0xFF, sizeof(permutation));
        for (char b = 0; b < 4; b++) {
            if (getBit(i, b)) {
                permutation[counter++] = 4 * b;
                permutation[counter++] = 4 * b + 1;
                permutation[counter++] = 4 * b + 2;
                permutation[counter++] = 4 * b + 3;
            }
        }
        __m128i mask = _mm_loadu_si128((const __m128i *) permutation);
        shuffle_mask[i] = mask;
    }
}

/**
 * Taken almost verbatim from http://highlyscalable.wordpress.com/2012/06/05/fast-intersection-sorted-lists-sse/
 */
size_t cardinality_intersect_SIMD(const uint32_t *A, const size_t s_a,
        const uint32_t *B, const size_t s_b) {
    size_t count = 0;
    size_t i_a = 0, i_b = 0;

    // trim lengths to be a multiple of 4
    size_t st_a = (s_a / 4) * 4;
    size_t st_b = (s_b / 4) * 4;

    while (i_a < st_a && i_b < st_b) {
        //[ load segments of four 32-bit elements
        __m128i v_a = _mm_load_si128((__m128i *) &A[i_a]);
        __m128i v_b = _mm_load_si128((__m128i *) &B[i_b]);
        //]

        //[ move pointers
        const uint32_t a_max = A[i_a + 3];
        const uint32_t b_max = B[i_b + 3];
        i_a += (a_max <= b_max) * 4;
        i_b += (a_max >= b_max) * 4;
        //]

        //[ compute mask of common elements
        const uint32_t cyclic_shift = _MM_SHUFFLE(0, 3, 2, 1);
        __m128i cmp_mask1 = _mm_cmpeq_epi32(v_a, v_b); // pairwise comparison
        v_b = _mm_shuffle_epi32(v_b, cyclic_shift); // shuffling
        __m128i cmp_mask2 = _mm_cmpeq_epi32(v_a, v_b); // again...
        v_b = _mm_shuffle_epi32(v_b, cyclic_shift);
        __m128i cmp_mask3 = _mm_cmpeq_epi32(v_a, v_b); // and again...
        v_b = _mm_shuffle_epi32(v_b, cyclic_shift);
        __m128i cmp_mask4 = _mm_cmpeq_epi32(v_a, v_b); // and again.
        __m128i cmp_mask = _mm_or_si128(_mm_or_si128(cmp_mask1, cmp_mask2),
                _mm_or_si128(cmp_mask3, cmp_mask4)); // OR-ing of comparison masks
        // convert the 128-bit mask to the 4-bit mask
        const int mask = _mm_movemask_ps((__m128 ) cmp_mask);
        //]

        //[ copy out common elements
        //__m128i p = _mm_shuffle_epi8(v_a, shuffle_mask[mask]);
        //_mm_storeu_si128((__m128i*)&C[count], p);
        count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
        //]
    }

    // intersect the tail using scalar intersection
    while (i_a < s_a && i_b < s_b) {
        if (A[i_a] < B[i_b]) {
            i_a++;
        } else if (B[i_b] < A[i_a]) {
            i_b++;
        } else {
            count++;
            i_a++;
            i_b++;
        }
    }

    return count;
}

/**
 * This is a version of cardinality_intersect_SIMD partially optimized
 * by D. Lemire.
 */
size_t opti_cardinality_intersect_SIMD(const uint32_t *A, const size_t s_a,
        const uint32_t *B, const size_t s_b) {
    size_t count = 0;
    size_t i_a = 0, i_b = 0;
    const static uint32_t cyclic_shift = _MM_SHUFFLE(0, 3, 2, 1);
    // trim lengths to be a multiple of 4
    size_t st_a = (s_a / 4) * 4;
    size_t st_b = (s_b / 4) * 4;
    if (i_a < st_a && i_b < st_b) {
        __m128i v_a, v_b;
        v_a = _mm_load_si128((__m128i *) &A[i_a]);
        v_b = _mm_load_si128((__m128i *) &B[i_b]);
        while (true) {
            const __m128i cmp_mask1 = _mm_cmpeq_epi32(v_a, v_b); // pairwise comparison
            __m128i cycledv_b = _mm_shuffle_epi32(v_b, cyclic_shift); // shuffling
            const __m128i cmp_mask2 = _mm_cmpeq_epi32(v_a, cycledv_b); // again...
            __m128i cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            const __m128i cmp_mask3 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            const __m128i cmp_mask4 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            const int mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]

            //[ copy out common elements
            //__m128i p = _mm_shuffle_epi8(v_a, shuffle_mask[mask]);
            //_mm_storeu_si128((__m128i*)&C[count], p);
            //]
            const uint32_t a_max = A[i_a + 3];
            //const uint32_t b_max = B[i_b + 3];
            if (a_max <= B[i_b + 3]) {
                i_a += 4;
                if (i_a >= st_a)
                    break;
                v_a = _mm_load_si128((__m128i *) &A[i_a]);
            }
            if (a_max >= B[i_b + 3]) {
                i_b += 4;
                if (i_b >= st_b)
                    break;
                v_b = _mm_load_si128((__m128i *) &B[i_b]);
            }

        }
    }

    // intersect the tail using scalar intersection
    while (i_a < s_a && i_b < s_b) {
        if (A[i_a] < B[i_b]) {
            i_a++;
        } else if (B[i_b] < A[i_a]) {
            i_b++;
        } else {
            count++;
            i_a++;
            i_b++;
        }
    }

    return count;
}

size_t opti8_cardinality_intersect_SIMD(const uint32_t *A, const size_t s_a,
        const uint32_t *B, const size_t s_b) {
    const size_t BlockSize = 8;
    const size_t HalfBlockSize = 4;

    size_t count = 0;
    size_t i_a = 0, i_b = 0;
    const static uint32_t cyclic_shift = _MM_SHUFFLE(0, 3, 2, 1);
    // trim lengths to be a multiple of 4
    size_t st_a = (s_a / BlockSize) * BlockSize;
    size_t st_b = (s_b / BlockSize) * BlockSize;

    if (i_a < st_a && i_b < st_b) {
        /*while (A[i_a + BlockSize - 1] < B[i_b]) {
         i_a += BlockSize;
         if (i_a >= st_a)
         goto end;
         }
         while (B[i_b + BlockSize - 1] < A[i_a]) {
         i_b += BlockSize;
         if (i_b >= st_b)
         goto end;
         };*/

        __m128i v_a, v_b, v_aa, v_bb;
        v_a = _mm_load_si128((__m128i *) &A[i_a]);
        v_aa = _mm_load_si128((__m128i *) &A[i_a + HalfBlockSize]);
        v_b = _mm_load_si128((__m128i *) &B[i_b]);
        v_bb = _mm_load_si128((__m128i *) &B[i_b + HalfBlockSize]);

        while (true) {
            __m128i cycledv_b;
            __m128i cmp_mask1, cmp_mask2, cmp_mask3, cmp_mask4;
            __m128i cmp_mask;
            int mask;

            cmp_mask1 = _mm_cmpeq_epi32(v_a, v_b); // pairwise comparison
            cycledv_b = _mm_shuffle_epi32(v_b, cyclic_shift); // shuffling
            cmp_mask2 = _mm_cmpeq_epi32(v_a, cycledv_b); // again...
            cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask3 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask4 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]

            //[ copy out common elements
            //__m128i p = _mm_shuffle_epi8(v_a, shuffle_mask[mask]);
            //_mm_storeu_si128((__m128i*)&C[count], p);
            //]

            cmp_mask1 = _mm_cmpeq_epi32(v_aa, v_bb); // pairwise comparison
            cycledv_b = _mm_shuffle_epi32(v_bb, cyclic_shift); // shuffling
            cmp_mask2 = _mm_cmpeq_epi32(v_aa, cycledv_b); // again...
            cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask3 = _mm_cmpeq_epi32(v_aa, cycledv_b); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask4 = _mm_cmpeq_epi32(v_aa, cycledv_b); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]


            cmp_mask1 = _mm_cmpeq_epi32(v_aa, v_b); // pairwise comparison
            cycledv_b = _mm_shuffle_epi32(v_b, cyclic_shift); // shuffling
            cmp_mask2 = _mm_cmpeq_epi32(v_aa, cycledv_b); // again...
            cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask3 = _mm_cmpeq_epi32(v_aa, cycledv_b); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask4 = _mm_cmpeq_epi32(v_aa, cycledv_b); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]


            cmp_mask1 = _mm_cmpeq_epi32(v_a, v_bb); // pairwise comparison
            cycledv_b = _mm_shuffle_epi32(v_bb, cyclic_shift); // shuffling
            cmp_mask2 = _mm_cmpeq_epi32(v_a, cycledv_b); // again...
            cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask3 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            cycledv_b = _mm_shuffle_epi32(cycledv_b, cyclic_shift);
            cmp_mask4 = _mm_cmpeq_epi32(v_a, cycledv_b); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]


            const uint32_t a_max = A[i_a + BlockSize - 1];
            //const uint32_t b_max = B[i_b + 3];
            if (a_max <= B[i_b + BlockSize - 1]) {
                //do {
                i_a += BlockSize;
                if (i_a >= st_a)
                    goto end;
                //} while(A[i_a + BlockSize -1] < B[i_b ]);
                v_a = _mm_load_si128((__m128i *) &A[i_a]);
                v_aa = _mm_load_si128((__m128i *) &A[i_a + HalfBlockSize]);
            }
            if (a_max >= B[i_b + BlockSize - 1]) {
                //do {
                i_b += BlockSize;
                if (i_b >= st_b)
                    goto end;
                //} while(B[i_b + BlockSize - 1]<A[i_a]);
                v_b = _mm_load_si128((__m128i *) &B[i_b]);
                v_bb = _mm_load_si128((__m128i *) &B[i_b + HalfBlockSize]);

            }

        }

    }
    end:

    // intersect the tail using scalar intersection
    while (i_a < s_a && i_b < s_b) {
        if (A[i_a] < B[i_b]) {
            i_a++;
        } else if (B[i_b] < A[i_a]) {
            i_b++;
        } else {
            count++;
            i_a++;
            i_b++;
        }
    }

    return count;
}

size_t opti2_cardinality_intersect_SIMD(const uint32_t *A, const size_t s_a,
        const uint32_t *B, const size_t s_b) {
    size_t count = 0;
    size_t i_a = 0, i_b = 0;
    const static uint32_t cyclic_shift1 = _MM_SHUFFLE(0, 3, 2, 1);
    const static uint32_t cyclic_shift2 = _MM_SHUFFLE(1, 0, 3, 2);
    const static uint32_t cyclic_shift3 = _MM_SHUFFLE(2, 1, 0, 3);

    // trim lengths to be a multiple of 4
    size_t st_a = (s_a / 4) * 4;
    size_t st_b = (s_b / 4) * 4;
    if (i_a < st_a && i_b < st_b) {
        __m128i v_a, v_b;
        v_a = _mm_load_si128((__m128i *) &A[i_a]);
        v_b = _mm_load_si128((__m128i *) &B[i_b]);
        while (true) {
            const __m128i cmp_mask1 = _mm_cmpeq_epi32(v_a, v_b); // pairwise comparison
            const __m128i cmp_mask2 = _mm_cmpeq_epi32(v_a,
                    _mm_shuffle_epi32(v_b, cyclic_shift1)); // again...
            __m128i cmp_mask = _mm_or_si128(cmp_mask1, cmp_mask2);
            const __m128i cmp_mask3 = _mm_cmpeq_epi32(v_a,
                    _mm_shuffle_epi32(v_b, cyclic_shift2)); // and again...
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask3);
            const __m128i cmp_mask4 = _mm_cmpeq_epi32(v_a,
                    _mm_shuffle_epi32(v_b, cyclic_shift3)); // and again.
            cmp_mask = _mm_or_si128(cmp_mask, cmp_mask4);
            // convert the 128-bit mask to the 4-bit mask
            const int mask = _mm_movemask_ps((__m128 ) cmp_mask);
            count += _mm_popcnt_u32(mask); // a number of elements is a weight of the mask
            //]

            //[ copy out common elements
            //__m128i p = _mm_shuffle_epi8(v_a, shuffle_mask[mask]);
            //_mm_storeu_si128((__m128i*)&C[count], p);
            //]
            const uint32_t a_max = A[i_a + 3];
            //const uint32_t b_max = B[i_b + 3];
            if (a_max <= B[i_b + 3]) {
                i_a += 4;
                if (i_a >= st_a)
                    break;
                v_a = _mm_load_si128((__m128i *) &A[i_a]);
            }
            if (a_max >= B[i_b + 3]) {
                i_b += 4;
                if (i_b >= st_b)
                    break;
                v_b = _mm_load_si128((__m128i *) &B[i_b]);
            }

        }
    }

    // intersect the tail using scalar intersection
    while (i_a < s_a && i_b < s_b) {
        if (A[i_a] < B[i_b]) {
            i_a++;
        } else if (B[i_b] < A[i_a]) {
            i_b++;
        } else {
            count++;
            i_a++;
            i_b++;
        }
    }

    return count;
}

}

#endif /* HSCALABLEINTERSECTION_H_ */