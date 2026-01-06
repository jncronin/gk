/*
 * Copyright 2024, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

/********************************************************************************
 * @file
 * Natural number library
 *
 * This library contains the natural number arithmetic routines that are used in
 * cryptographic algorithms. All natural numbers are stored as arrays of 32-bit
 * unsigned ints, in big-endian order (i.e. if x[N] is an N-word natural number
 * in memory then x[0] is the most significant word of x and x[N-1] is the least
 * significant word. The actual endiannes of the 32-bit number is irrelevant,
 * although on the Leon it is also big endian. This code works on little-endian
 * processors with no modification.
 *******************************************************************************
 */

#include "nn.h"
#include <string.h>

/*
Helpful things for the reader:

First of all, everything in this library is about natural numbers, meaning
that all of them are positive. There's no concept of a negative number and
thus multiplication and division are always unsigned.
You can create a negative number by subtracting a bigger one from a smaller
one, but it is up to you to do something with it - you get a non-zero borrow
from the subtract routine, indicating that you got a negative number (in 2's
complement form), how you deal with it is your problem. As far as the rest of
the library is interested, the result is just a (very big) positive number.

Second, the library is thread-safe, i.e. there's no static data anywhere.
The price you pay for that is that thread-safe allocation of memory to hold
big numbers is your responsibility.

The background of the algorithms in this file can be found at various places.
Most notably, Chapter 14 of the Handbook of Applied Cryptography should be
consulted, as well as the paper titled High Speed RSA Implementation by Cetin
Kaya Koc, downloadable from the RSA website. Further bits and pieces can be
found in various research papers that can be dug up on the Internet. A book
on discrete maths is handy, if you want to dig a bit deeper into the modular
stuff.

Natural numbers are stored in arrays of 32-bit words, in big-endian order.
If k is an N-word long number, then it is stored in an array of k[N].
Then k[i] means the memory word at index i (as in C) but in comments k(i)
represents the i-th 32-bit digit of k. That is, k(i) == k[ N-i-1 ].
Algorithmic descriptions usually refer to the digits of k, the code
comments might refer to both, but the notation is used consistently.

In the comments 2^N means the N-th power of 2, as usual. However, n^-1 is
usually the multiplicative inverse of n (modulo m), that is the number so
that ( n * n^-1 ) % m == 1 where % is the modulo. Similarly, -n is not
the negative of n but its additive inverse (a positive integer) so that
( n + -n ) % m = 0. If n < m then the additive inverse is simply -n = m - n.
If n >= m, then you can use the fact that -n is the same as -(n mod m) and
n^-1 is the same as (n mod m)^-1, i.e. you can always reduce n to be less
than m before starting the calculations.The multiplicative inverse is hard to
calculate, the algorithmic complexity is that of calculating the GCD of the
two numbers (GCD: greatest common denominator).

Here's the maths that you need to work out what goes on:

Basics:

Z: set of all integer numbers
Z+: set of all positive integers, zero included

If p is a prime, then the numbers 0, 1, ..., p-2, p-1 and the arithmetic
operators modulo p form a Galois (i.e. finite) field, usually denoted Zp.
The additive identity element is 0, the multiplicative is 1. The operations
are all performed modulo p:

a + b   :   ( a + b ) mod p
a * b   :   ( a * b ) mod p
a - b   :   ( a + -b ) ) mod p
a / b   :   ( a * b^-1 ) mod p

The b^-1, the multiplicative inverse of b is not that easy to calculate but
it exists for all elements but 0 and can be calculated at the cost of a GCD
calculation. The additive inverse, -x also exists for each x. If x is 0, then
-x is 0 for all other values -x = p-x.

The mod operator has a few interesting features that should be noted:

( a + b ) mod x =  ( ( a mod x ) + ( b mod x ) ) mod x
( a * b ) mod x =  ( ( a mod x ) * ( b mod x ) ) mod x
a mod x = b mod x iff (a - b) = k * x where k is in Z.

The Euler totient function (denoted by capital phi) is interpreted on positive
integers and its value is the number of integers less than the function argument
that are relative primes to the argument. Formally:

PHI(x) = || { n | n < x & gcd(n,x) = 1 } ||

For example, PHI(10) = 3 because from the numbers 2,3,4,5,6,7,8,9 only
3, 7 and 9 are relative primes to 10.
If p is a prime, then PHI(p) = p-1 since p is relative prime to all n < p.
If e is 2N then PHI(e) = e/2 since all odd numbers are relative prime to e.
The 'relative prime' is also called co-prime. Instead of writing that "a
and b are reletive primes" often the gcd(a,b)=1 notation is used.

Fermat's Little Theorem tells you that:

If p is a prime then a^p mod p = a mod p

Its generalisation is the Euler theorem (one of many...):

If GCD( a,n ) = 1 then ( a^PHI(n) ) mod n = 1

All numbers can be written as:

n = q * x + r

where n, x is in Z, q is floor( n / x ) and r is n mod x.

The Montgomery n-residual form of a number x is

y = x * R^-1 mod n

where R^-1 is the multiplicative inverse of R mod n where R is a number so
that R = b^k and b^(k-1) < n < b^k (i.e. R is the smallest power of the base b
that is larger than n. Note that it only exists if n and b are relative primes.
If the modulus is odd, then R can be any even number.
Now the Montgomery multiplication calculates the Montgomery residual of the
multiplication result instead of the simple mod n residual. It does it faster
than a normal modulus could be calculated. Denote the Montgomery multiplication
with #, that is:

a # b = ( a * b * R^-1 ) mod n

Now introduce a transformation, denoted with ' such as

x' = ( x * R ) mod n

then of course

a' # b' = (a*b)'
a' # ( R mod n ) = a'
a' # 1  = a

It can be fairly easily proven that the Montgomery domain (i.e. the x' world)
using Montgomery multiplication and normal addition is equivalent to the
original Galois field with normal multimplication and addition.
Thus, we can do exponentiation inside the Montgomery domain using faster
multiplies if we 1) transform the original number into the Montgomery
domain at the cost of a normal multiply, 2) transform the result back at the
cost of a Montgomery multiply and 3) calculate a 32-bit number, needed for
the Montgomer multiply, at the cost of a single precision division.
In fact, you can transform everything into Montgomery, do all your work
there and transform only the final result back.

If the numbers and the modulus have k digits, then the classical modular
multiplication uses 2k^2 multiplications and k single precision divisions.
The Montgomery method uses 2k^2 + 2k multiplications and no divisions.
Basically, in our case, we trade a 64/32 division and possibly a long
subtraction (plus some algorithmic complexity) for two 32x32->64 multiplies.
The difference is small, mostly because the Sparc core does not have a HW
multiplier, but if you do a lot of multiplies, the accumulated time savings,
in absolute terms, can be substantial. When multiplying 1024 bit numbers,
(on the Leon core) the classical modular multiply finishes in about 195k
clocks while the Montgomery finishes in roughly 167k clocks (~15% faster).

It is not worth using Montgomery to actually multiply numbers. Where the
Montgomery multiply comes handy is when you keep multiplying the same few
numbers and you do not need the interim result, just the final one.
In that case the initial transformation into the Montgomery domain and
the transformation back is negligible compared to the savings you make
on the multiplies. The best known such situation is exponentiation.
*/


/* Note : Only few functions are used by WPS from NN library. Hence adding functions which are not used
 * under ENABLE_UNUSED_NN_FUNCTIONS define to improve the code coverage of WCM asset. Please define ENABLE_UNUSED_NN_FUNCTIONS
 * if you need to use the remaining functions
 */
/****************************************************************************/
/*                  Local definitions and types                             */
/****************************************************************************/

#define MAX_NN              16          // Max. number of NNs

typedef signed int          sint32;     // Signed 32 bit integer
typedef signed long long    sint64;     // Signed 64 bit integer

/****************************************************************************/
/*                          Simple arithmetic                               */
/****************************************************************************/

/*!
******************************************************************************
Clears a number

\return     void
\param[out] m   A natural number that will be set to 0.
*/

void NN_Clr( NN_t* m )
{
    memset(m->num, 0, m->len*sizeof(uint32_t));
}


#ifdef ENABLE_UNUSED_NN_FUNCTIONS
/*!
******************************************************************************
Adds two numbers

Calculates the r = x + y value.

\return     uint32_t  The overflow word
\param[out] r   The result
\param[in]  x   One of the operands
\param[in]  y   The other operand

The result must be distinct (in memory) from the inputs x and y.
The operands can be of any length, the result must be as long as
the longer operand + 1 If the provided result buffer is shorter,
then the result will be truncated and the returned value is the next
word of the sum.

The values x, y and r do not need to be distinct in memory.
*/

uint32_t NN_Add( NN_t* r, const NN_t* x, const NN_t* y )
{
    const uint32_t *xp, *yp;
    uint32_t *rp;
    unsigned long long tr;

    xp = x->num + x->len - 1;
    yp = y->num + y->len - 1;
    rp = r->num + r->len - 1;

    for ( tr = 0; rp >= r->num; tr >>= 32 )
    {
        if ( xp >= x->num )
        {
            tr += cy_hton32( *xp-- );
        }
        if ( yp >= y->num )
        {
            tr += cy_hton32( *yp-- );
        }
        *rp-- = cy_hton32( (uint32_t) tr );
    }

    return ( (uint32_t) tr );
}


/*!
******************************************************************************
Subtracts two numbers

Calculates the r = x - y value.

\return     sint32  The borrow word
\param[out] r   The result
\param[in]  x   One of the operands
\param[in]  y   The other operand

The result must be distinct (in memory) from the inputs x and y.
The operands can be of any length, the result must be as long as
the longer operand If the provided result buffer is shorter,
then the result will be truncated and the returned value is the next
word of the difference.

The values x, y and r do not need to be distinct in memory.
*/

uint32_t NN_Sub( NN_t* r, const NN_t* x, const NN_t* y )
{
const uint32_t  *xp, *yp;
uint32_t *rp;
sint64  tr;

    xp = x->num + x->len - 1;
    yp = y->num + y->len - 1;
    rp = r->num + r->len - 1;

    for ( tr = 0 ; rp >= r->num ; tr >>= 32 )
    {
        if ( xp >= x->num ) tr += *xp--;
        if ( yp >= y->num ) tr -= *yp--;
        *rp-- = tr;
    }

    return( (uint32_t) tr );
}


/*!
******************************************************************************
Multiplies two numbers

Calculates the r = x * y value.

\return     void
\param[out] r   Pointer to the result
\param[in]  x   Pointer to one of the multiplicands
\param[in]  y   Pointer to the other multiplicand

The result must be distinct (in memory) from the inputs x and y.
The multiplicands can be of any length, the result must be as long as
the sum of the length of the multiplicands. If the provided result buffer
is shorter, then the result will be truncated.
*/

void NN_Mul( NN_t* r, const NN_t* x, const NN_t* y )
{
uint32_t  i, j, xi;
uint32_t  *rs, *rp;
const uint32_t *yp;
uint64_t  tr;

    NN_Clr( r );

    for ( rs = r->num + r->len - 1, i = x->len ; i-- ; rs-- )
    {
        xi = x->num[ i ];
        yp = y->num + y->len - 1;
        rp = rs;
        tr = 0;

        for ( j = y->len ; j-- && rp >= r->num ; yp--, rp-- )
        {
            tr  += *rp;
            tr  += NN_Mul32x32u64( xi, *yp );
            *rp  = tr;
            tr >>= 32;
        }

        for ( ; ( rp >= r->num ) && tr ; rp-- )
        {
            tr  += *rp;
            *rp  = tr;
            tr >>= 32;
        }
    }
}
#endif

/*
*   Division
*/
// FIXME: Shoud be written, for the sake of sompleteness, but not used now.

/****************************************************************************/
/*                          Modular arithmetic                              */
/****************************************************************************/

/*!
******************************************************************************
Adds two numbers modulo m

Calculates the r = ( x + y ) % m value.

\return     void
\param[out] r   The result
\param[in]  x   One of the operands
\param[in]  y   The other operand
\param[in]  m   The modulus

The operands must be less than the modulus. The result must be as long as the
modulus.
*/

#ifdef ENABLE_UNUSED_NN_FUNCTIONS
void NN_AddMod( NN_t* r, const NN_t* x, const NN_t* y, const NN_t* m )
{
const uint32_t  *xp, *yp;
uint32_t *rp;
uint64_t  tr;

    xp = x->num + x->len - 1;
    yp = y->num + y->len - 1;
    rp = r->num + r->len - 1;

    for ( tr = 0 ; rp >= r->num ; tr >>= 32 )
    {
        if ( xp >= x->num ) tr += *xp--;
        if ( yp >= y->num ) tr += *yp--;
        *rp-- = tr;
    }

    if ( ! tr ) {

        rp = r->num;
        xp = m->num;
        yp = m->num + m->len;

        for ( ; xp < yp ; xp++, rp++ )
        {
            if ( *rp < *xp ) return;
            if ( *rp > *xp ) break;
        }

        if ( xp == yp ) return;
    }

    rp = r->num + r->len - 1;
    xp = m->num + m->len - 1;

    for ( tr = 0 ; rp >= r->num ; tr >>= 32 )
    {
        tr += *rp;
        tr -= *xp--;
        *rp-- = tr;
        tr = (sint64) tr >> 32;
    }
}


/*!
******************************************************************************
Subtracts two numbers modulo m

Calculates the r = ( x - y ) % m value.

\return     void
\param[out] r   The result
\param[in]  x   One of the operands
\param[in]  y   The other operand
\param[in]  m   The modulus

The operands must be less than the modulus. The result must be as long as the
modulus.
*/

void NN_SubMod( NN_t* r, const NN_t* x, const NN_t* y, const NN_t* m )
{
const uint32_t  *xp, *yp;
uint32_t *rp;
sint64  tr;

    xp = x->num + x->len - 1;
    yp = y->num + y->len - 1;
    rp = r->num + r->len - 1;

    for ( tr = 0 ; rp >= r->num ; tr >>= 32 )
    {
        if ( xp >= x->num ) tr += *xp--;
        if ( yp >= y->num ) tr -= *yp--;
        *rp-- = tr;
    }

    if (tr )
    {

        rp = r->num + r->len - 1;
        xp = m->num + m->len - 1;

        for ( tr = 0 ; rp >= r->num ; tr = (uint64_t) tr >> 32 )
        {
            tr += *rp;
            tr += *xp--;
            *rp-- = tr;
        }
    }
}
#endif

/*!
******************************************************************************
Classical modular multiplication

Calculates the r = ( x * y ) % m value.
If y is not given, then calculates x % m, i.e. it does a modular reduction.

It does a reduction in every step, therefore it does not need extra storage.
The way it works is simple ('digit' here means a 32-bit word):

If x and y are n-digit numbers and they are both less than m, the modulus,
then you can do the following:

r <- 0
for i from n-1 downto 0 do
    r <- r * 2^32 + x * y(i)
    r <- r % m
done
return r

Of course the r must be two words longer than x, y and m (it can be proven that
you do not need anything more). Now the modulus calculation is the one that
we want to speed up, a division is fairly expensive operation. Multiplication
is a lot cheaper, so if we can guess the q = r / m value we can calculate the
modulus using the r % m = r - q * m equation. The way we can guess the quotient
is that we divide the upper 64 bits of r with the upper 32 bits of the modulus.
The result of the division is at most a 33-bit number (yes, it is 33, if you
divide 0xffffffffffffffff by 0x80000000 you get 0x1FFFFFFFF). Treating the 33rd
bit as special case, we can use the rest as 32-bit q. Of course, our estimate is
just an estimate, not the actual precise quotient. However, it can also be shown
that Q <= q <= Q + 2 where Q is the real and q is the estimated quotient.
Thus, we can modify the algorithm accordingly:

r <- 0
for i from n-1 downto 0 do
    r <- r * 2^32 + x * y(i)
    q <- (r(n),r(n-1)) / m(n-1)
    r <- r - m * q
    while r < 0 do
        r <- r + m
    done
done
return r

and we know that the inner loop executes at most 2 times. To assure that the
approximation of q is that good, one has to guarantee that the top bit of the
modulus is set. That can not always be guaranteed. However, it is always
possible to shift the modulus (and the top of 'r') up until the condition
is met. So we still do division, but we trade a long division for a long
multiply and a single precision division - a much cheaper solution overall.

If you look at the algoritms, you will realise that the size of 'x' is
irrelevant, therefore the restirction that 'x' is less than the modulus
can be dropped.

\return     void
\param[out] r   Pointer to the result
\param[in]  x   Pointer to one of the multiplicands
\param[in]  y   Pointer to the other multiplicand
\param[in]  m   Pointer to the modulus

The result must be distinct (in memory) from the inputs x, y and m.
The 'x' multiplicand can be of any length, 'y' must be less than the modulus
and the same length as the modulus or shorter, 'r' must be as large as the
modulus.

If the second multiplicand, 'y' is a NULL pointer then the routine will do
a modular reduction of x. Reduction alone is about twice as fast as a multiply
with a proper y would be.
*/

void NN_MulMod( NN_t* r, const NN_t* x, const NN_t* y, const NN_t* m )
{
uint64_t  mult;
sint64  subs;
uint32_t  prev, mdiv, sure, keep;
uint32_t  i, j, k, n;



    NN_Clr( r );

    // Calculate how many empty bits there are in the modulus

    for ( k = 0, i = m->num[ 0 ] ; (int) i >= 0 ; k++, i <<= 1 );

    if ( ! y )
    {
        // We do a modular reduction

        if ( x->len >= r->len )
        {
            // The number is large enough to actually do reduction. However,
            // the first several steps would achieve nothing else than shifting
            // the number into r. We can do it faster here.

            for ( j = 0 ; j < r->len - 1 ; j++ )
            {
                r->num[ j+1 ] = x->num[ j ];
            }
        }
        else
        {
            // The number is definitely smaller than the modulus, so we
            // simply copy it into the result and return immediately.

            for ( j = 0 ; j < x->len ; j++ )
            {
                r->num[ j + r->len - x->len ] = x->num[ j ];
            }

            return;
        }
    }
    else
    {
        // It is a real multiply so we must start from the beginning of
        // the x operand.

        j = 0;
    }

    // Just a shorthand so we do not have to fetch the length all the time.

    n = m->len;

    for ( i = j ; i < x->len ; i++ )
    {
        prev = 0;
        mult = 0;
        mdiv = x->num[ i ];

        {
            const uint32_t  *p1;
            uint32_t *p2   = r->num + n - 1;

            // Now calculate r = r * 2^32 + y * xi;
            // Due to the adds we must do it from the bottom.
            // Note that r was less than the modulus. So was y. Therefore,
            // even if r and y are both m-1 and xi is all 1s, the largest
            // result is (m-1)*(2^33-1) This means that the result is never
            // more than 33 bit longer than the modulus. This fact is very
            // important for the reduction step. Also note that if y is
            // bigger than the modulus, then the reduction will fail.

            if ( y )
            {
                // We have 2 multiplicands, do the multiply

                for ( p1 = y->num + n - 1, j = n + 1 ; --j ; p1--, p2-- )
                {
                    if ( p1 >= y->num )
                    {
                        mult  += NN_Mul32x32u64( mdiv, *p1 );
                    }

                    prev   = *p2;
                    *p2    = mult;
                    mult >>= 32;
                    mult  += prev;
                }
            }
            else
            {
                // We only have 1 multiplicand, the second one is implicitely 1.

                for ( j = n + 1 ; --j ; p2-- )
                {
                    mult  += mdiv;
                    prev   = *p2;
                    *p2    = mult;
                    mult >>= 32;
                    mult  += prev;
                    mdiv   = 0;
                }
            }
        }

        // If the result is actually smaller than the modulus then
        // we do not have to bother with the rest at all

        if (mult || r->num[ 0 ] >= m->num[ 0 ])
        {
            // Now mult contains the overflow, up to 33 bits long. We also know
            // that it can be at most one bit longer than the length of the most
            // significant word of the modulus.

            do
            {
                subs = 0;
                // If the overflow is more than the top word of the modulus, then
                // we know that there would be a 33-rd bit after the div, so just
                // do the correction immediately

                if ( mult <= m->num[ 0 ] )
                {

                    // If the top bit of the modulus is not set, then we're in
                    // trouble because the division will not be correct. However,
                    // we know that if the modulus' upper word has k zero bits on
                    // the left, then the result also will have k zero bits on the
                    // left. It is simply because we made sure that mult is not
                    // larger than the top word of the modulus. Therefore, for the
                    // division we can shift up both modulus and result by k bits.

                    if ( k )
                    {
                        // We truncate mult as we know that the top word is 0.
                        // The compiler doesn't so it would do 64-bit ops when
                        // 32-bit is sufficient and a lot faster (not that it
                        // matters that much after all the multiplies, actually).

                        prev   = mult;
                        prev   = ( prev << k ) | ( r->num[0] >> ( 32 - k ) );
                        mult   = prev;
                        mult <<= 32;
                        prev   = ( r->num[0] << k ) | ( r->num[1] >> ( 32 - k ) );
                        mult  |= prev;
                        prev   = ( m->num[0] << k ) | ( m->num[1] >> ( 32 - k ) );
                    }
                    else
                    {
                        mult <<= 32;
                        mult  |= r->num[ 0 ];
                        prev   = m->num[ 0 ];
                    }

                    if ( ( mult >> 32 ) < prev ) break;

                    // The quotient would be 33 bit long, do a sub.
                    // Before we do it, we must restore mult.

                    mult = ( (uint32_t ) ( mult >> 32 ) ) >> k;
                }

                // We have to process the 33rd bit.
                // Subtract 2^32*m from the result. The variable 'subs'
                // will be whatever is left after the subtraction, i.e. the
                // 32-bit borrow, sign extended to 64 bits.
                {
                    uint32_t * p1   = r->num + n - 2;
                    const uint32_t *p2   = m->num + n - 1;

                    for ( j = n ; --j ; p1--, p2-- )
                    {
                        subs += *p1;
                        subs -= *p2;
                        *p1 = subs;
                        subs >>= 32;
                    }
                    subs  += mult;
                    subs  -= *p2;
                    mult   = subs;
                    subs >>= 32;
                }

                // If subs is negative, we might as well leave the loop now.
                // We will have to compensate our overestimation and we will
                // not do div/mul at all, so no need to shift mult and prev
                // up for the division..

            } while ( subs >= 0 );

            if ( subs >= 0 )
            {
                // We do a quotient estimation and reduction step.
                // First, we keep the overflow word, it will be needed later
                // and mult will be used for other purposes. We only need the
                // upper word (and even less than that, if the top bit of the
                // modulus is not set).

                keep = ( (uint32_t ) ( mult >> 32 ) ) >> k;

                // Now do a 32 bit division.
                // Since 'mult' was less than the modulus, we do know
                // that the division will result no more than 32 bits.

                for ( mdiv = 0, j = 33 ; --j ; )
                {
                    sure = mult >> 63;
                    mdiv <<= 1;
                    mult <<= 1;

                    if ( sure || ( mult >> 32 ) >= prev )
                    {
                        mdiv += 1;
                        mult -= (uint64_t) prev << 32;
                    }
                }

                // Now mdiv is the quotient. We calculate r = r - mdiv * m;
                // When we finish, subs is the borrow, sign extended.

                subs = 0;
                mult = 0;

                {
                    const uint32_t * p1   = m->num + n - 1;
                    uint32_t * p2   = r->num + n - 1;

                    for ( j = n + 1 ; --j ; p1--, p2-- )
                    {
                        subs  -= mult;
                        subs  += *p2;
                        mult   = NN_Mul32x32u64( mdiv, *p1 );
                        prev   = mult;
                        subs  -= prev;
                        *p2    = subs;
                        subs >>= 32;
                        mult >>= 32;
                    }
                }

                // Make sure that subs is the actual borrow word
                // sign extended to 64 bit.

                subs -= mult;
                subs += keep;
            }

            // Since mdiv might have overestimated the actual value by two,
            // we have to correct the result. If subs is negative, then we
            // have to add the modulus to the result.

            while ( subs < 0 )
            {
                const uint32_t * p1 = m->num + n - 1;
                uint32_t * p2 = r->num + n - 1;
                mult = 0;


                for ( j = n + 1 ; --j ; p1--, p2-- )
                {
                    mult  += *p1;
                    mult  += *p2;
                    *p2    = mult;
                    mult >>= 32;
                }

                subs += mult;
            }
        }
    }
}


/*!
******************************************************************************
Montgomery multiplication

Calculates the r = ( x * y * R^-1 ) % m value, where ^-1 means the
multiplicative inverse modulo m. R here is implicitely set to be the
smallest power of 2^32 that can represent the modulus, i.e. using an
1024 bit modulus, then R is (2^32)^32 = 2^1024.

The parameter m' is -m^-1 mod (2^32), that is, the number for which it
holds true: ( ( 2^32 - m' ) * m ) % ( 2^32 ) == 1; note that even though
the modulus m is a big number, m' is only a 32 bit number. It can be
calculated fairly quickly (roughly the same time as a 32/32 SW division).

Note that the Montgomery multiplication is *NOT* the same as a simple
multiplication modulo m. The extra multiply with the R^-1 mod m tag
makes the result very different from an ordinary multiply. It is not
very useful on its own; you have to convert the multiplicands into the
Montgomery domain and then convert the result back. The former costs
you a normal multiply for each operand and the latter a Montgomery
multiply. Thus, it is obvious that a simple mul-mod can be done much
faster. So why bother then? Well, when you have to do a lot of muls in
succession (for example, during exponentiation) then it becomes a real
timesaver because all the way through you do not have to calculate the
modulo m values, as you transformed the problem from one modulus domain
into an other where certain properties hold.

The running time of this function is about 150*n^2 clocks, that is,
for a 1024 bit modulus it is about 4ms.

The article by Koc is probably the best description of the background into
the whole Montgomery domain business. The short description of the algorithm
(but not the math) is here. The notation used is that k(i) means the i-th
32-bit digit of the n-word value of k, k(0) being the least significant digit
and k(n-1) being the most significant digit. If the (i) is not applied to k,
then it means k in its entirety.

Let x and y be two n-word positive integers, smaller than the modulus.
Let m  be the n-word modulus that MUST be odd.
Let m' be the 32-bit value of -m^-1 mod 2^32.
Let n  be the number of words in each large number.
Let u  be a single word temporary storage
Let r  be the n-word result, r = ( x * y * R^-1 ) mod m.

To calculate r do the following:

r <- 0
for i from 0 to n-1 do
    u <- ( ( r(0) + x(i) * y(0) ) * m' ) % 2^32
    r <- ( r + x(i) * y + u * m ) >> 32
done
if r >= m then r <- r - m
return r

As you can see, the number of multiplications is actually more than it is
with a normal modular multiply (2n^2 + 2n versus 2n^2). However, the single
precision division and the whole mess with the reduction is gone. This gives
the Montgomery multiply a maybe 15% speed advantage when implemented on the
Leon core.

\return     void
\param[out] r   Pointer to the result
\param[in]  x   Pointer to one of the multiplicands
\param[in]  y   Pointer to the other multiplicand
\param[in]  m   Pointer to the modulus
\param[in]  t   The m' = -m^-1 mod 2^32 value

The result must be distinct (in memory) from the inputs x, y and m.
The 'x' multiplicand can be of any length, 'y' must be less than the modulus
but the same length as the modulus, 'r' must be as large as the modulus.
*/

void NN_MulModMont( NN_t* r, const NN_t* x, const NN_t* y, const NN_t* m, uint32_t t )
{
uint32_t  r0, y0, i, j, n;
uint32_t  ui, rn, xi;
uint64_t  pr;
const uint32_t  *cp1;
uint32_t *p2;

    NN_Clr( r );
    n  = m->len - 1;
    y0 = y->num[ n ];
    rn = 0;

    for ( i = x->len ; i-- ; )
    {
        r0 = r->num[ n ];
        xi = x->num[ i ];
        ui = ( r0 + xi * y0 ) * t;

        // This loop calculates the r <- r + x(i) * y bit. The x(i) value
        // is in xi. We go though y(j) and r(j) using the pointers p1 and
        // p2. We use the 64 bit variable p to hold the carry from one
        // digit to the next. Note that since pr is always shifted to
        // the right, the pr = pr + *pr + xi*p1 will not overflow, because
        // 0xffffffff * 0xffffffff = fffffffe00000001 and if you add
        // 2 * 0xffffffff = 0x1fffffffe to that you get 0xffffffffffffffff.

        cp1 = y->num + n;
        p2 = r->num + n;
        pr = 0;

        for ( j = n + 2 ; --j ; cp1--, p2-- )
        {
            pr   += NN_Mul32x32u64( xi, *cp1 ) + *p2;
            *p2   = pr;
            pr  >>= 32;
        }

        // Keep track of the overflow

        rn += pr;

        // Set up the pointers and do the first round of the loop outside
        // of the loop, as we must not store the result (that will be the
        // one that gets discarded by the shift right operation).

        cp1 = m->num + n;
        p2 = r->num + n;
        pr = NN_Mul32x32u64( ui, *cp1-- ) + *p2--;

        // This loop calculates the r <- ( r + u * m ) >> 32 bit. Note that
        // lowest 32 bits or m and r are already processed and the result
        // is in pr. The upper 32 bits, that were not written to r in the
        // previous round are stored in rn while ui contains the value of
        // u. The pointers cp1 and p2 are used to go through the words of
        // m(j) and r(j) where j starts at 1, not zero.

        for ( j = n + 1 ; --j ; cp1--, p2-- )
        {
            pr  >>= 32;
            pr   += NN_Mul32x32u64( ui, *cp1 ) + *p2;
            p2[1] = pr;
        }

        // The final 32 bits go to r(n-1), i.e r[0].

        pr >>= 32;
        pr  += rn;
        r->num[0] = pr;
        rn   = pr >> 32;
    }

    // This is the comparison loop. We go from MSB to LSB. If at any
    // time we see that m > r, we know we're done and return.
    // If we see that m <= r, then we do know that we must subtract m.


    p2 = r->num;
    cp1 = m->num;

    if ( ! rn )
    {
        for ( i = n + 2 ; --i ; p2++, cp1++ )
        {
            if ( *cp1 < *p2 ) break;
            if ( *cp1 > *p2 ) return;
        }
    }
    else
    {
        i = 1;
    }

    if ( ! i )
    {
        // The result is the same as the modulus, so the subtract
        // result would be 0 - we migh as well set it directly, it
        // is faster than subtracting.

        for ( p2 = r->num, i = n + 2 ; --i ; ) *p2++ = 0;
    }
    else
    {
        // This is the subtract loop, nothing special

        p2 = r->num + n;
        cp1 = m->num + n;
        pr = 0;

        for ( i = n + 2 ; --i ; p2--, cp1-- ) {

            pr  = (sint64) pr >> 32;
            pr += *p2;
            pr -= *cp1;
            *p2 = pr;
        }
    }
}


/*!
******************************************************************************
Montgomery exponentiation

This function calculates r = ( x ^ e ) mod m. It is assumed that the modulus
is odd. The exponent can be any long.
The number x must be smaller than the modulus.
The algorithm uses the left-to-right square-and-multiply method.

WARNING: The function destroys x!

\return     void
\param[out] r   The result.
\param[in]  x   The number of which you need the exponent.
\param[in]  e   The exponent.
\param[in]  m   The modulus.
\param[in]  w   Workspace, the same size as r, x or m.
*/

/* TODO: This should be have const parameters x, e, m, w  however the impementation does not lend itself to this */

void NN_ExpModMont( NN_t* r, NN_t* x, NN_t* e, NN_t* m, NN_t* w )
{
    uint32_t eb;
    uint32_t mt;
    uint32_t* ep;
    NN_t* pt1;
    NN_t* pt2;
    NN_t* pt3;
    int     ex, t;
    unsigned int i;

    // Calculate m' = -m^-1 mod 2^32, needed for the Montgomery multiply.

    mt = NN_EmTick( m );

    // Now calculate x' = x * R mod m. It is the same as calculating
    // x * ( R mod m ) mod m. The result goes to r.

    NN_ErModEm( w, m );
    NN_MulMod( r, x, w, m );

    // Set up the pointers. We will switch between them back and forth.
    // Since we do not need x any more (we use x' instead) we use x as
    // workspace, i.e. one of the 2 buffers.

    pt1 = x;
    pt2 = w;

    // Note that we (should have) initialised w to R mod n. Since this
    // corresponds to the unit element in the Montgomery domain, we do not
    // need to actually multiply with it until the first time we multiply
    // with the value now stored in r. Thus, we do not calculate the value
    // nor do we multiply: the first time we would multiply that value with
    // the value stored in 'r', we copy 'r' to the result and make node that
    // from now on we really have to multiply. Since we don't need i any more,
    // we use that as the flag (i!=0 indicates that we must multiply).

    for ( i = 0, t = e->len, ep = e->num ; t-- ; )
    {
        for ( eb = 33, ex = *ep++ ; --eb ; ex <<= 1 )
        {
            if ( i )
            {
                NN_MulModMont( pt2, pt1, pt1, m, mt );
            }
            else
            {
                pt3 = pt2;
                pt2 = pt1;
                pt1 = pt3;
            }

            if ( ex < 0 )
            {
                if ( i )
                {
                    NN_MulModMont( pt1, pt2, r, m, mt );
                }
                else
                {
                    for ( i = 0 ; i < pt1->len ; i++ )
                    {
                        pt1->num[ i ] = r->num[ i ];
                    }
                }
            }
            else
            {
                pt3 = pt2;
                pt2 = pt1;
                pt1 = pt3;
            }
        }
    }

    // This last step will transform the result from the
    // Montgomery domain back to the normal domain.

    NN_Clr( pt2 );
    /* coverity[Overflowed array index write]
       FALSE-POSITIVE:
       structures are defined as follow,
       typedef struct
       {
           uint32_t len;
           uint32_t num[48];
       } wps_NN_t;
       typedef struct
       {
           uint32_t len;
           uint32_t num[1];
       } NN_t;
       And in real application,
       wps_NN_t is type-casted to NN_t.
       This is only a programming technique.
    */
    pt2->num[ pt2->len - 1 ] = 1;
    NN_MulModMont( r, pt1, pt2, m, mt );
}


#ifdef ENABLE_UNUSED_NN_FUNCTIONS
/*!
******************************************************************************
Classical exponentiation

This function calculates r = ( x ^ e ) mod m.
The number x must be smaller than the modulus.
There's no restriction on the length of the exponent.
The algorithm uses the left-to-right square-and-multiply method.

\return     void
\param[out] r   The result.
\param[in]  x   The number of which you need the exponent.
\param[in]  m   The modulus.
\param[in]  e   The exponent.
\param[in]  w   Workspace, the same size as r, x or m.
*/

/* TODO: This should be have const parameters x, e, m, w  however the impementation does not lend itself to this */

void NN_ExpMod( NN_t* r, NN_t* x, NN_t* e, NN_t* m, NN_t* w )
{
    uint32_t  eb;
    uint32_t  *ep;
    NN_t*    pt1;
    NN_t* pt2;
    NN_t* pt3;
    int     ex, t;
    unsigned int i;

    // Set up the pointers. We will switch between them back and forth.

    pt1 = r;
    pt2 = w;

    // The trick of eliminating multiplication by 1 is the same as what
    // is used in the Montgomery exponentiation, see there.
    // In fact, this code fragment is identical to that except that it
    // calls classical mul instead of the Montgomery one.

    for ( i = 0, t = e->len, ep = e->num ; t-- ; )
    {
        for ( eb = 33, ex = *ep++ ; --eb ; ex <<= 1 )
        {
            if ( i )
            {
                NN_MulMod( pt2, pt1, pt1, m );
            }
            else
            {
                pt3 = pt2;
                pt2 = pt1;
                pt1 = pt3;
            }

            if ( ex < 0 )
            {
                if ( i )
                {
                    NN_MulMod( pt1, pt2, x, m );
                }
                else
                {
                    for ( i = 0 ; i < pt1->len ; i++ )
                    {
                        pt1->num[ i ] = x->num[ i ];
                    }
                }
            }
            else
            {
                pt3 = pt2;
                pt2 = pt1;
                pt1 = pt3;
            }
        }
    }

    // The current result is in pt1. If it is not the same as r, then
    // we must copy the result there.

    if ( pt1 != r )
    {
        for ( i = 0 ; i < r->len ; i++ )
        {
            r->num[ i ] = pt1->num[ i ];
        }
    }
}
#endif

/****************************************************************************/
/*                      Auxcilliary functions                               */
/****************************************************************************/

/*!
******************************************************************************
Arithmetic inverse of modular inverse for the fixed modulus 2^32

Calculates the m' = -m^-1 mod 2^32 value for a multiword number.

Note that for modulo 2^32 one only needs to look at the lowest significant
32 bits of the number, the higher order bits simply have no effect.
This algorithm is faster than the GCD method. It uses only basic single
precision arithmetic and a fixed number of iterations. The algorithm is
the following:

let x be a number of which we need the multiplicative inverse mod 2^32:

y = 1;

for i = 2 to 32 do
    if 2^(i-1) < ( x * y ) mod 2^i then
        y = y + 2^(i-1)
    endif
done
return y

It works by observing the the fact that in each step (x*y) mod 2^i = 1
is maintained. The actual code avoids the multiplication, since as y is
calculated, the multiplication result can also be calculated in a separate
variable. In addition, the returned value is the additive inverse of the
modular inverse, rather than the modular inverse itself.

\return     uint32_t  The calculated m' value
\param[in]  mod     The modulus
*/

uint32_t NN_EmTick( const NN_t* mod )
{
uint32_t  y, x, t, b, m;

    x = mod->num[ mod->len - 1 ];
    t = x;
    y = 1;
    b = 1;
    m = 1;

    while ( b ) {

        x <<= 1;
        b <<= 1;
        m  += m + 1;

        if ( b < ( t & m ) ) {

            y += b;
            t += x;
        }
    }

    return( -y );
}


/*!
******************************************************************************
Calculates R mod m

R is a number that is the smallest power of 2^32 that can store the modulus.
That is, if the modulus is say 0x123, then a single 32-bit word can store it
and thus R is 0x100000000 (i.e. 2^32).

If the top bit of the most significant word in the modulus is set, then
R mod m can be calculated trivially, by subtracting the modulus from R.
If the top bit is not set, however, one has to do real calculations, costing
a single-precision division and N single precision multiplications where N is
the length of the modulus (in words).

\return     void
\param[out] r   The (2^32)^k mod m result
\param[in]  m   The modulus
*/

void NN_ErModEm( NN_t* r, const NN_t* m )
{
uint64_t  temp;
const uint32_t  *mp;
uint32_t *rp;
uint32_t  cntr, prev, mdiv, keep;
uint32_t  n;

    n = r->len = m->len;

    NN_Clr( r );

    if ( (sint32) m->num[ 0 ] < 0 )
    {
        // The top bit of the modulus was set, calculate R - m and we're done.

        mp = m->num + n - 1;
        rp = r->num + n - 1;

        for ( temp = 0 ; n-- ; )
        {
            temp  = ( (sint64) temp >> 32 ) - *mp--;
            *rp--  = temp;
        }
    }
    else
    {
        // We have to do proper reduction. We use the classical method.
        // Work out the estimate for the quotient. To get a good estimate,
        // we need all the bits we can get, so we shift everything up until
        // the divisor's top bit gets set.

        for ( cntr = 0, prev = m->num[0] ; (sint32) prev > 0 ; cntr++ )
        {
            prev <<= 1;
        }

        prev |= m->num[ 1 ] >> ( 32 - cntr );
        temp  = (uint64_t) ( 1 << cntr ) << 32;

        // Calculate the quotient estimate, q ~ R / m
        // The result will never be less than the real result Q.
        // In worst case, it will be q = Q + 2 but that happens
        // rather infrequently.

        for ( mdiv = 0, cntr = 33 ; --cntr ; )
        {
            keep = temp >> 63;
            mdiv <<= 1;
            temp <<= 1;

            if ( keep || ( temp >> 32 ) >= prev )
            {
                mdiv += 1;
                temp -= (uint64_t) prev << 32;
            }
        }

        // Multiply and subtract, ie. r = R - q * m

        mp = m->num + n - 1;
        rp = r->num + n - 1;

        for ( temp = 0, cntr = n ; cntr-- ; )
        {
            temp -= NN_Mul32x32u64( mdiv, *mp-- );
            *rp-- = temp;
            temp  = (sint64) temp >> 32;
        }

        keep = temp + 1;

        // If we were so unlucky that q > Q, then we have to add the
        // modulus to the (negative) result. Since Q <= q <= Q + 2 the
        // loop below runs at most twice.

        while ( keep )
        {
            mp = m->num + n - 1;
            rp = r->num + n - 1;

            for ( cntr = n ; cntr-- ; )
            {
                temp  = *rp;
                temp += *mp--;
                *rp-- = temp;
                temp >>= 32;
            }

            keep += temp;
        }
    }
}

/*!
******************************************************************************
Multiplies two 32-bit unsigned numbers and returns the 64-bit result

The old version of gcc that we use is not smart enough to do a 32x32->64
mul when you ask for it. It extends the operands to 64 bit and calls the
64x64->64 multiply instead. Considering that the entire natural number
library relies on the performance of the 32x32->64 multiplication, it
seems like a good idea to speed it up as much as possible. The routine
below does it in about 48 clocks: 40 for the actual mul and a couple more
that the compiler generates as function wrapper.

\return     unsigned long long Multiplication result
\param[in]  a   Multiplicand
\param[in]  b   Multiplicand
*/

uint64_t  NN_Mul32x32u64( uint32_t a, uint32_t b )
{
#ifndef __sparc__
    return( (unsigned long long) a * b );
#else
uint32_t  t;

    if ( a && b ) {

        __asm__ volatile
        (
            "   mov     %[a],%%y            \n"
            "   mov     %[a],%[t]           \n"
            "   andcc   %%g0,%%g0,%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%[b],%[a]      \n"
            "   mulscc  %[a],%%g0,%[a]      \n"
            "   sra     %[b],31,%[b]        \n"
            "   and     %[t],%[b],%[t]      \n"
            "   add     %[t],%[a],%[a]      \n"
            "   mov     %%y,%[b]            \n"
            : [a] "=r" (a), [b] "=r" (b), [t] "=r" (t)
            : "0" (a), "1" (b)
        );
    }
    else
    {
        a = b = 0;
    }

    return( ( (unsigned long long) a << 32 ) | b );
#endif
}

void wps_NN_set( cy_wps_NN_t* m, const uint8_t* buffer )
{
    int a;
    m->len = 48;
    uint32_t* buf32 = (uint32_t*)buffer;
    for (a=0;a < 48; ++a)
    {
        m->num[a] = cy_hton32(buf32[a]);
    }
#ifdef USING_CORTEX_M3
    __asm
    (
        "LDR R0, #48                      "
        "LDR R1, [_m, #4]                 "
        "loop:                            "
        "SUB R0, #1                       "
        "LDR R2, [_buffer, R0, LSL #2]    "
        "REV R2, R2                       "
        "STR R2, [R1, R0, LSL #2]         "
        "CBNZ R0, loop                    "
    )
#endif
}

void wps_NN_get( const cy_wps_NN_t* m, uint8_t* buffer )
{
    int a;
    uint32_t* buf32 = (uint32_t*)buffer;
    for (a=0;a < 48; ++a)
    {
        buf32[a] = cy_hton32(m->num[a]);
    }
}
