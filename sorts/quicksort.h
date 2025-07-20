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


#include <stdlib.h>

#ifndef SOL_SORT_TYPE
#error must define SOL_SORT_TYPE
#define SOL_SORT_TYPE int
#endif

#ifndef SOL_SORT_FUNCTION_NAME
#error must define SOL_SORT_FUNCTION_NAME
#define SOL_SORT_FUNCTION_NAME placeholder_quicksort
#endif

#ifndef SOL_SORT_COMPARE_LT
#error must define SOL_SORT_COMPARE_LT(const SOL_SORT_TYPE* a, const SOL_SORT_TYPE* b)
#ifdef SOL_SORT_CONTEXT_TYPE
#define SOL_SORT_COMPARE_LT(A, B, CTX) ((A) < (B))
#else
#define SOL_SORT_COMPARE_LT(A, B) ((A) < (B))
#endif
#endif

/** is a very good idea to set this based on type and expense of comparison */
#ifndef SOL_SORT_BUBBLE_THRESHOLD
#define SOL_SORT_BUBBLE_THRESHOLD 16
#endif

#ifdef SOL_SORT_CONTEXT_TYPE
#define SOL_SORT_COMPARE_LT_CONTEXTUAL(A, B) SOL_SORT_COMPARE_LT(A, B, context)
#else
#define SOL_SORT_COMPARE_LT_CONTEXTUAL(A, B) SOL_SORT_COMPARE_LT(A, B)
#endif

#ifdef SOL_SORT_CONTEXT_TYPE
static void SOL_SORT_FUNCTION_NAME(SOL_SORT_TYPE* data, size_t count, SOL_SORT_CONTEXT_TYPE context)
#else
static void SOL_SORT_FUNCTION_NAME(SOL_SORT_TYPE* data, size_t count)
#endif
{
    struct
    {
        SOL_SORT_TYPE* start;
        SOL_SORT_TYPE* end;
    }
    stack[48];

    size_t stack_size;

    SOL_SORT_TYPE tmp;
    SOL_SORT_TYPE pivot;

    SOL_SORT_TYPE* start;
    SOL_SORT_TYPE* end;
    SOL_SORT_TYPE* iter_forwards;
    SOL_SORT_TYPE* iter_backwards;
    SOL_SORT_TYPE* middle;
    SOL_SORT_TYPE* smallest;

    stack_size = 0;

    start = data;
    end = data + count - 1;

    if(count > SOL_SORT_BUBBLE_THRESHOLD) while(1)
    {
        /* pre-sort start and end of range */

        if(SOL_SORT_COMPARE_LT_CONTEXTUAL(end, start))
        {
            tmp = *start;
            *start = *end;
            *end = tmp;
        }

        middle = start + ((end - start) >> 1);

        /* sort middle relative to start and end, this also also sets the middle(valued) of the 3 as pivot */
        if(SOL_SORT_COMPARE_LT_CONTEXTUAL(end, middle))
        {
            pivot = *end;
            *end = *middle;
            *middle = pivot;
        }
        else if(SOL_SORT_COMPARE_LT_CONTEXTUAL(middle, start))
        {
            pivot = *start;
            *start = *middle;
            *middle = pivot;
        }
        else
        {
            pivot = *middle;
        }

        iter_forwards = start;
        iter_backwards = end;

        while(1)
        {
            /* we want the iters after these while loops to be the first positions that violate pivot sorting */
            /* note: we don't specially handle the pivot, so it may end up on either side of range */
            while(SOL_SORT_COMPARE_LT_CONTEXTUAL((++iter_forwards), (&pivot) ));
            while(SOL_SORT_COMPARE_LT_CONTEXTUAL((&pivot), (--iter_backwards)));

            if(iter_backwards<iter_forwards)
            {
                break;
            }

            tmp = *iter_forwards;
            *iter_forwards = *iter_backwards;
            *iter_backwards = tmp;
        }

        /* any chunk (either side of pivot after above sort) that is smaller than the `SOL_SORT_BUBBLE_THRESHOLD` is "sorted" */
        /* otherwise one or both of these ranges need processing */
        if((iter_backwards - start) < SOL_SORT_BUBBLE_THRESHOLD)
        {
            if((end - iter_forwards) < SOL_SORT_BUBBLE_THRESHOLD)
            {
                /* both parts of this range sufficiently sorted, get another range to sort (if any are left) */
                if(stack_size == 0)
                {
                    break;
                }
                stack_size--;
                start = stack[stack_size].start;
                end = stack[stack_size].end;
            }
            else
            {
                /* after pivot sufficiently sorted, before pivot NOT, so sort that range */
                start = iter_forwards;
            }
        }
        else if((end - iter_forwards) < SOL_SORT_BUBBLE_THRESHOLD)
        {
            /* before pivot sufficiently sorted, after pivot unsorted so sort that range next */
            end = iter_backwards;
        }
        /* otherwise both chunks (before and after the pivot) are unsorted and must be sufficiently sorted */
        /* to avoid overflow of the stack record the larger side to the stack and sort the smaller immediately */
        else if((iter_backwards - start) > (end - iter_forwards))
        {
            stack[stack_size].start = start;
            stack[stack_size].end   = iter_backwards;
            stack_size++;
            start = iter_forwards;
        }
        else
        {
            stack[stack_size].start = iter_forwards;
            stack[stack_size].end   = end;
            stack_size++;
            end = iter_backwards;
        }
    }

    /* find smallest in (at least) first chunk */
    smallest = data;
    /* non-inclusive end */
    end = data + ((SOL_SORT_BUBBLE_THRESHOLD > count) ? count : SOL_SORT_BUBBLE_THRESHOLD);
    for(iter_forwards = data + 1; iter_forwards < end ; iter_forwards++)
    {
        if(SOL_SORT_COMPARE_LT_CONTEXTUAL(iter_forwards, smallest))
        {
            smallest = iter_forwards;
        }
    }
    /* move smallest to start */
    if(data != smallest)
    {
        tmp = *smallest;
        *smallest = *data;
        *data = tmp;
    }

    /* bubble sort the whole list, shouldnt have to move any element more than `SOL_SORT_BUBBLE_THRESHOLD` elements back */
    /* inclusive end */
    end = data + count - 1;
    for(iter_forwards = data+2; iter_forwards <= end; iter_forwards++)
    {
        iter_backwards = iter_forwards - 1;
        if(SOL_SORT_COMPARE_LT_CONTEXTUAL(iter_forwards, iter_backwards))
        {
            tmp = *iter_forwards;
            do
            {
                iter_backwards[1] = iter_backwards[0];
            }
            while(SOL_SORT_COMPARE_LT_CONTEXTUAL((&tmp), (--iter_backwards)));

            iter_backwards[1] = tmp;
        }
    }
}

#undef SOL_SORT_TYPE
#undef SOL_SORT_FUNCTION_NAME
#undef SOL_SORT_BUBBLE_THRESHOLD
#undef SOL_SORT_COMPARE_LT

#undef SOL_SORT_COMPARE_LT_CONTEXTUAL

#ifdef SOL_SORT_CONTEXT_TYPE
#undef SOL_SORT_CONTEXT_TYPE
#endif

