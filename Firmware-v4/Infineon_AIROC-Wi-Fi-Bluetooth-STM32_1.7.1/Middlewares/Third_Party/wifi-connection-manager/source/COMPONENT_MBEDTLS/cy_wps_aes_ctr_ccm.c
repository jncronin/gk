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

/** @file cy_wps_aes_ctr_ccm.c
* @brief AES encryption/decryption functions
*/

/* These functions are derived from the Broadcom WLAN AES-CTR and AES-CCM implementations. */

/* FixMe: This file is specifically used for AES encrypt/decrypt operations in WPS. These can be
 * removed and directly mbedtls AES encrypt/decrypt functions can be used.
 */

#include "mbedtls/aes.h"
#include "cy_wcm_log.h"
#include <stdint.h>
#include <string.h>

#define AES_BLOCK_SZ                      16
#define AES_CTR_MAXBLOCKS                (1 << 16)
#define AES_CCM_AAD_MAX_LEN               0xFEFF
#define AES_CCM_NONCE_LEN                 15
#define AES_CCM_AUTH_LEN                  8
#define AES_CCM_LEN_FIELD_LEN             1
#define AES_CCM_AUTH_FLAGS(iv_len)       (4 * (AES_CCM_AUTH_LEN - 2) + (AES_CCM_NONCE_LEN - 1 - iv_len))
#define AES_CCM_AUTH_AAD_FLAG             0x40
#define AES_CCM_CRYPT_FLAGS(iv_len)      (AES_CCM_NONCE_LEN - 1 - iv_len)

/* NOTE:
 * AES encrypt/decrypt functions are used for WPS functionality. Remaining non-used functions by WPS are kept under
 * AES_SELF_TEST macro to increase the code-coverage of WCM library.
 */

#ifdef AES_SELF_TEST
/* AES-CTR mode encryption/decryption algorithm
 *    - max data_len is (AES_BLOCK_SZ * 2^16)
 *    - nonce must be AES_BLOCK_SZ bytes
 *    - assumes nonce is ready to use as-is (i.e. any
 *        encryption/randomization of nonce/IV is handled by the caller)
 *    - ptxt and ctxt can point to the same location
 *    - returns -1 on error
 */
int aes_crypt_ctr( mbedtls_aes_context *ctx, uint32_t length, const unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    size_t k;
    unsigned char tmp[AES_BLOCK_SZ], ctr[AES_BLOCK_SZ];

    if ( length > ( AES_BLOCK_SZ * AES_CTR_MAXBLOCKS ) )
    {
        return ( -1 );
    }

    memcpy( ctr, iv, AES_BLOCK_SZ );

    while( length >= AES_BLOCK_SZ )
    {
        int i;
        memcpy( tmp, input, 16 );
        mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, ctr, tmp );

        for( i = 0; i < 16; i++ )
        {
            output[i] = (unsigned char)( tmp[i] ^ input[i] );
        }

        ctr[AES_BLOCK_SZ - 1]++;
        if ( !ctr[AES_BLOCK_SZ - 1] )
        {
            ctr[AES_BLOCK_SZ - 2]++;
        }

        input  += 16;
        output += 16;
        length -= 16;
    }

    /* Handle partial block */
    if ( length % AES_BLOCK_SZ )
    {
        mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, ctr, tmp );
        for ( k = 0; k < ( length % AES_BLOCK_SZ ); k++ )
        {
            output[k] = input[k] ^ tmp[k];
        }
    }

    return ( 0 );
}

/* AES-CCM mode MAC calculation
 *    - computes AES_CCM_AUTH_LEN MAC
 *    - nonce must be AES_CCM_NONCE_LEN bytes
 *    - returns -1 on error
 */

int aes_ccm_mac( mbedtls_aes_context *ctx, uint32_t length, uint32_t aad_length, const unsigned char *nonce, int nonce_len, const unsigned char *aad_input, const unsigned char *data_input, unsigned char mac_output[8] )
{
    unsigned char B_0[AES_BLOCK_SZ];
    unsigned char X[AES_BLOCK_SZ];
    size_t j;
    size_t k, len_left;
    int i = 0;

    if ( aad_length > AES_CCM_AAD_MAX_LEN )
    {
        return ( -1 );
    }

    /* B_0 = Flags || iv || l(m) */
    B_0[0] = AES_CCM_AUTH_FLAGS(nonce_len);

    if ( aad_length )
    {
        B_0[0] |= AES_CCM_AUTH_AAD_FLAG;
    }

    memcpy( &B_0[1], nonce, nonce_len );

    for( i = 0, len_left = length; i < AES_CCM_NONCE_LEN - nonce_len; i++, len_left >>= 8 )
           B_0[AES_CCM_NONCE_LEN-i] = (unsigned char)( len_left & 0xFF );

    /* X_1 := E( K, B_0 ) */
    mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, B_0, X );

    /* X_i + 1 := E( K, X_i XOR B_i )  for i = 1, ..., n */

    /* First the AAD */
    if ( aad_length != 0 )
    {
        X[0] ^= (unsigned char)( ( aad_length >> 8 ) & 0xff );
        X[1] ^= (unsigned char)( aad_length & 0xff );
        k = 2;
        j = aad_length;
        while ( j-- )
        {
            X[k] ^= *aad_input++;
            k++;
            if ( k == AES_BLOCK_SZ )
            {
                mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, X, X );
                k = 0;
            }
        }
        /* Handle partial last block */
        if ( k % AES_BLOCK_SZ )
        {
            mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, X, X );
        }
    }

    /* Then the message data */
    for ( k = 0; k < ( length / AES_BLOCK_SZ ); k++ )
    {
        int i;
        for( i = 0; i < 16; i++ )
        {
            X[i] = (unsigned char)( data_input[i] ^ X[i] );
        }

        data_input += AES_BLOCK_SZ;
        mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, X, X );
    }

    /* Last block may be partial, padding is implicit in this xor */
    for ( k = 0; k < ( length % AES_BLOCK_SZ ); k++ )
    {
        X[k] ^= *data_input++;
    }

    if ( length % AES_BLOCK_SZ )
    {
        mbedtls_aes_crypt_ecb( ctx, MBEDTLS_AES_ENCRYPT, X, X );
    }

    /* T := first-M-bytes( X_n+1 ) */
    memcpy( mac_output, X, AES_CCM_AUTH_LEN );

    return ( 0 );
}

/* AES-CCM mode encryption
 *    - computes AES_CCM_AUTH_LEN MAC and then encrypts ptxt and MAC
 *    - nonce must be AES_CCM_NONCE_LEN bytes
 *    - ctxt must have sufficient tailroom for CCM MAC
 *    - ptxt and ctxt can point to the same location
 *    - returns -1 on error
 */
int aes_encrypt_ccm( mbedtls_aes_context *ctx, uint32_t length, uint32_t aad_length, const unsigned char *nonce, uint8_t nonce_len, const unsigned char *aad_input, const unsigned char *plaintext_input, unsigned char *ciphertext_output, unsigned char mac_output[8] )
{
    unsigned char A[AES_BLOCK_SZ], X[AES_BLOCK_SZ];

    /* Initialize counter */
    A[0] = AES_CCM_CRYPT_FLAGS(nonce_len);

    memcpy( &A[1], nonce, nonce_len );
    memset( A + 1 + nonce_len, 0, AES_CCM_NONCE_LEN - nonce_len );

    /* Calculate and encrypt MAC */
    if (aes_ccm_mac( ctx, length, aad_length, nonce, nonce_len, aad_input, plaintext_input, X))
    {
        return (-1);
    }
    if (aes_crypt_ctr( ctx, AES_CCM_AUTH_LEN, A, X, X ))
    {
        return (-1);
    }
    memcpy( mac_output, X, AES_CCM_AUTH_LEN );

    /* Encrypt data */
    A[AES_BLOCK_SZ - 1] = 1;
    if (aes_crypt_ctr( ctx, length, A, plaintext_input, ciphertext_output ))
    {
        return (-1);
    }

    return (0);
}

/* AES-CCM mode decryption
 *    - decrypts ctxt, then computes AES_CCM_AUTH_LEN MAC and checks it against
 *      then decrypted MAC
 *    - the decrypted MAC is included in ptxt
 *    - nonce must be AES_CCM_NONCE_LEN bytes
 *    - ptxt and ctxt can point to the same location
 *    - returns -1 on error
 */
int aes_decrypt_ccm( mbedtls_aes_context *ctx, uint32_t length, uint32_t aad_length,const unsigned char *nonce, uint8_t nonce_len, const unsigned char *aad_input, const unsigned char *ciphertext_input, unsigned char *plaintext_output )
{
    unsigned char A[AES_BLOCK_SZ];
    unsigned char X[AES_BLOCK_SZ];

    /* Initialize counter */
    A[0] = AES_CCM_CRYPT_FLAGS(nonce_len);
    memcpy( &A[1], nonce, nonce_len );
    memset( A + 1 + nonce_len, 0, AES_CCM_NONCE_LEN - nonce_len );
    A[AES_BLOCK_SZ - 1] = 1;

    /* Decrypt data */
    if ( aes_crypt_ctr( ctx, length - AES_CCM_AUTH_LEN, A, ciphertext_input, plaintext_output ) )
    {
        return ( -1 );
    }

    /* Decrypt MAC */
    memset( A + 1 + nonce_len, 0, AES_CCM_NONCE_LEN - nonce_len );

    if ( aes_crypt_ctr( ctx, AES_CCM_AUTH_LEN, A, ciphertext_input + length - AES_CCM_AUTH_LEN, plaintext_output + length - AES_CCM_AUTH_LEN ) )
    {
        return ( -1 );
    }

    /* Calculate MAC */
    if ( aes_ccm_mac( ctx, length - AES_CCM_AUTH_LEN, aad_length, nonce, nonce_len,aad_input, plaintext_output, X ) )
    {
        return ( -1 );
    }

    if ( memcmp(X, plaintext_output + length - AES_CCM_AUTH_LEN, AES_CCM_AUTH_LEN) != 0 )
    {
        return ( -1 );
    }

    return ( 0 );
}
#endif

/* AES-CBC mode encryption/decryption algorithm with padding
 *    - handle partial plaintext blocks with padding
 *    - ptxt and ctxt can point to the same location
 *    - returns -1 on error
 */
int aes_cbc_crypt_pad_length_padding( mbedtls_aes_context *ctx, int mode, uint32_t length, const unsigned char iv[16], const unsigned char *input, unsigned char *output )
{
    int remaining = length & 0x0f;
    uint32_t rounded_length = length & 0xfffffff0;
    unsigned char iv_copy[16];

    if ( length == 0 )
    {
        return -1;
    }

    if ( ( mode == MBEDTLS_AES_DECRYPT ) && ( remaining != 0 ) )
    {
        return -1;
    }

    memcpy( iv_copy, iv, sizeof(iv_copy) );

    mbedtls_aes_crypt_cbc( ctx, mode, rounded_length, iv_copy, input, output );

    if ( mode == MBEDTLS_AES_DECRYPT )
    {
        return (int)(length - output[length - 1]);
    }
    else
    {
        int j;
        uint8_t tmp[AES_BLOCK_SZ];

        for (j = 0; j < remaining; j++)
        {
            tmp[j] = (uint8_t)(input[(int)rounded_length + j] ^ iv_copy[j]);
        }

        for (j = remaining; j < AES_BLOCK_SZ; j++)
        {
            tmp[j] = (uint8_t)(AES_BLOCK_SZ - remaining) ^  iv_copy[j];
        }

        mbedtls_aes_crypt_ecb( ctx, mode, tmp, output + rounded_length );

        return (int)(rounded_length + AES_BLOCK_SZ);
    }
}

#if defined(AES_SELF_TEST)

#include <stdio.h>

#include <unit/aes_vectors.h>

#define TESTBUF    2048

/*
 * Checkup routine
 */
static uint8_t aes_padded_ccm_output[TESTBUF], aes_padded_ccm_input2[TESTBUF];

int32_t aes_padded_ccm_ctr_self_test(int32_t verbose)
{
    /* AES-CBC padded */
    {
        uint32_t k;
        mbedtls_aes_context ctx;

        for (k = 0; k < NUM_CBC_VECTORS; k++)
        {

            int retv;


            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "padded AES CBC Encrypt: %d bytes\n", aes_cbc_vec[k].il);
            }

            mbedtls_aes_init( &ctx );
            mbedtls_aes_setkey_enc( &ctx, aes_cbc_vec[k].key, (uint32_t)( aes_cbc_vec[k].kl * 8 ) );
            retv = aes_cbc_crypt_pad_length_padding( &ctx, MBEDTLS_AES_ENCRYPT, (uint32_t)( aes_cbc_vec[k].il ),
                                                     aes_cbc_vec[k].nonce, aes_cbc_vec[k].input, aes_padded_ccm_output );
            if ( (uint32_t)retv != cbc_padded_ref[k].length)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: pad aes_cbc_encrypt length failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(cbc_padded_ref[k].ref, aes_padded_ccm_output, cbc_padded_ref[k].length) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: pad aes_cbc_encrypt data failed\n", __FUNCTION__);
                }
                return 1;
            }


            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "padded AES CBC Decrypt: %d bytes\n", aes_cbc_vec[k].il);
            }

            mbedtls_aes_setkey_dec( &ctx, aes_cbc_vec[k].key, (uint32_t)( aes_cbc_vec[k].kl * 8 ) );

            retv = aes_cbc_crypt_pad_length_padding( &ctx, AES_DECRYPT, cbc_padded_ref[k].length,
                                                     aes_cbc_vec[k].nonce, cbc_padded_ref[k].ref, aes_padded_ccm_output );

            if ( (uint32_t)retv != aes_cbc_vec[k].il )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: pad aes_cbc_decrypt length failed %d\n", __FUNCTION__, retv);
                }
                return 1;
            }
            if ( memcmp( aes_cbc_vec[k].input, aes_padded_ccm_output, (size_t) aes_cbc_vec[k].il ) != 0 )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: pad aes_cbc_decrypt data failed\n", __FUNCTION__);
                }
                return 1;
            }

            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "short padded AES CBC Encrypt: %d bytes\n", aes_cbc_vec[k].il - 1);
            }

            mbedtls_aes_setkey_enc( &ctx, aes_cbc_vec[k].key, (uint32_t)( aes_cbc_vec[k].kl * 8 ) );
            retv = aes_cbc_crypt_pad_length_padding( &ctx, AES_ENCRYPT, (uint32_t)( aes_cbc_vec[k].il - 1 ),
                                                     aes_cbc_vec[k].nonce, aes_cbc_vec[k].input, aes_padded_ccm_output );

            if ( (uint32_t)retv != cbc_short_padded_ref[k].length )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: short pad aes_cbc_encrypt length failed\n", __FUNCTION__);
                }
                return 1;
            }
            if ( memcmp( cbc_short_padded_ref[k].ref, aes_padded_ccm_output, cbc_short_padded_ref[k].length ) != 0 )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: short pad aes_cbc_encrypt data failed\n", __FUNCTION__);
                }
                return 1;
            }

            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "short padded AES CBC Decrypt: %d bytes\n", aes_cbc_vec[k].il - 1);
            }

            mbedtls_aes_setkey_dec( &ctx, aes_cbc_vec[k].key, (uint32_t)( aes_cbc_vec[k].kl * 8 ) );

            retv = aes_cbc_crypt_pad_length_padding( &ctx, MBEDTLS_AES_DECRYPT, cbc_short_padded_ref[k].length,
                                                     aes_cbc_vec[k].nonce, cbc_short_padded_ref[k].ref, aes_padded_ccm_output );

            if ( (uint32_t)retv != aes_cbc_vec[k].il - 1 )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: short pad aes_cbc_decrypt length failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_cbc_vec[k].input, aes_padded_ccm_output, (size_t)(aes_cbc_vec[k].il - 1)) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: short pad aes_cbc_decrypt data failed\n", __FUNCTION__);
                }
                return 1;
            }

            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "passed\n");
            }

            mbedtls_aes_free( &ctx );
        }

        if( verbose != 0 )
        {
            CY_WPS_DEBUG(( "\n" );
        }
    }


    /*
     * CTR mode
     */

    {
        int retv;
        uint32_t k;
        mbedtls_aes_context ctx;

        mbedtls_aes_init( &ctx );
        /* AES-CTR, full blocks */
        //    dbg(("%s: AES-CTR, full blocks\n", __FUNCTION__));
        for ( k = 0; k < (int)NUM_CTR_VECTORS; k++ )
        {
            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, (uint32_t)( aes_ctr_vec[k].kl * 8 ) );
            retv = aes_crypt_ctr( &ctx,
                                  (uint32_t)aes_ctr_vec[k].il, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].input, aes_padded_ccm_output);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Encrypt: %d bytes\n", aes_ctr_vec[k].il);
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if ( memcmp( aes_padded_ccm_output, aes_ctr_vec[k].ref, (size_t) aes_ctr_vec[k].il ) != 0 )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt encrypt failed\n", __FUNCTION__);
                }
                return 1;
            }

            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, aes_ctr_vec[k].kl * 8);
            retv = aes_crypt_ctr( &ctx,
                                  aes_ctr_vec[k].il, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].ref, aes_padded_ccm_input2);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Decrypt: %d bytes\n", aes_ctr_vec[k].il );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_ctr_vec[k].input, aes_padded_ccm_input2, aes_ctr_vec[k].il) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt decrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
        }

        /* AES-CTR, one partial block */
        //    dbg(("%s: AES-CTR, one partial block\n", __FUNCTION__));
        for (k = 0; k < NUM_CTR_VECTORS; k++)
        {
            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, aes_ctr_vec[k].kl * 8);
            retv = aes_crypt_ctr( &ctx,
                                  k + 1, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].input, aes_padded_ccm_output);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Partial Block Encrypt: %d bytes\n", k + 1 );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if ( memcmp( aes_padded_ccm_output, aes_ctr_vec[k].ref, (size_t)( k + 1 ) ) != 0 )
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt encrypt failed\n", __FUNCTION__);
                }
                return 1;
            }

            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, aes_ctr_vec[k].kl * 8);
            retv = aes_crypt_ctr( &ctx,
                                  k + 1, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].ref, aes_padded_ccm_input2);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Partial Block Decrypt: %d bytes\n", k + 1 );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_ctr_vec[k].input, aes_padded_ccm_input2, k + 1) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt decrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
        }

        /* AES-CTR, multi-block partial */
        //    dbg(("%s: AES-CTR, multi-block partial\n", __FUNCTION__));
        for (k = 0; k < NUM_CTR_VECTORS; k++)
        {
            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, aes_ctr_vec[k].kl * 8);
            retv = aes_crypt_ctr( &ctx,
                                  AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].input, aes_padded_ccm_output);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Partial Multi-Block Encrypt: %d bytes\n",
                       AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1 );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_padded_ccm_output, aes_ctr_vec[k].ref, AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt encrypt failed\n", __FUNCTION__);
                }
                return 1;
            }

            mbedtls_aes_setkey_enc( &ctx, aes_ctr_vec[k].key, aes_ctr_vec[k].kl * 8);
            retv = aes_crypt_ctr( &ctx,
                                  AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, aes_ctr_vec[k].nonce,
                                  aes_ctr_vec[k].ref, aes_padded_ccm_input2);
            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CTR Partial Multi-Block Decrypt: %d bytes\n",
                       AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1 );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_ctr_vec[k].input, aes_padded_ccm_input2,
                       AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ctr_crypt decrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
        }

        mbedtls_aes_free( &ctx );
    }

    /*
     * CCM mode
     */
    {
        unsigned char output[TESTBUF], input2[TESTBUF];
        int retv;
        uint32_t k;
        mbedtls_aes_context ctx;

        mbedtls_aes_init( &ctx );
        /* FIXME: add some failing decrypt cases */
        /* AES-CCM */
        //    dbg(("%s: AES-CCM\n", __FUNCTION__));
        for (k = 0; k < NUM_CCM_VECTORS; k++)
        {
            mbedtls_aes_setkey_enc( &ctx, aes_ccm_vec[k].key, aes_ccm_vec[k].kl * 8);
            retv = aes_ccm_mac( &ctx,
                                aes_ccm_vec[k].il, aes_ccm_vec[k].al,
                                aes_ccm_vec[k].nonce, aes_ccm_vec[k].nonce_len, aes_ccm_vec[k].aad,
                                aes_ccm_vec[k].input, output);

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_mac failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(output, aes_ccm_vec[k].mac, AES_CCM_AUTH_LEN) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_mac failed\n", __FUNCTION__);
                }
                return 1;
            }

            mbedtls_aes_setkey_enc( &ctx, aes_ccm_vec[k].key, aes_ccm_vec[k].kl * 8);
            retv = aes_encrypt_ccm( &ctx,
                                    aes_ccm_vec[k].il, aes_ccm_vec[k].al,
                                    aes_ccm_vec[k].nonce, aes_ccm_vec[k].nonce_len, aes_ccm_vec[k].aad,
                                    aes_ccm_vec[k].input, output, &output[aes_ccm_vec[k].il]);

            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CCM Encrypt: %d bytes\n", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN );
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_encrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(output, aes_ccm_vec[k].ref, aes_ccm_vec[k].il + AES_CCM_AUTH_LEN) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_encrypt failed\n", __FUNCTION__);
                }
                return 1;
            }

            mbedtls_aes_setkey_enc( &ctx, aes_ccm_vec[k].key, aes_ccm_vec[k].kl * 8);
            retv = aes_decrypt_ccm( &ctx,
                                    aes_ccm_vec[k].il + AES_CCM_AUTH_LEN, aes_ccm_vec[k].al,
                                    aes_ccm_vec[k].nonce, aes_ccm_vec[k].nonce_len, aes_ccm_vec[k].aad,
                                    aes_ccm_vec[k].ref, input2);

            if( verbose != 0 )
            {
                cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "AES CCM Decrypt: %d bytes\n", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN);
            }

            if (retv)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_decrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
            if (memcmp(aes_ccm_vec[k].input, input2, aes_ccm_vec[k].il) != 0)
            {
                if( verbose != 0 )
                {
                    cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "%s: aes_ccm_decrypt failed\n", __FUNCTION__);
                }
                return 1;
            }
        }

        mbedtls_aes_free( &ctx );
    }




    if (verbose != 0)
    {
        cy_wcm_log_msg(CYLF_MIDDLEWARE, CY_LOG_DEBUG, "\n");
    }

    return (0);
}

#endif
