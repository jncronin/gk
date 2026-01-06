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

/** @file cy_wps_crypto.c
* @brief AES and SHA hash related functions required for WPS. Implementation here is specific to Mbed TLS library.
*/

#include <string.h>
#include <stdarg.h>

#include "cy_wps_crypto.h"
#include "cyabs_rtos_impl.h"
#include "cyhal.h"
#include "cy_wps_mbedtls_version.h"
/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/

/******************************************************
 *               Function Definitions
 ******************************************************/
extern int aes_cbc_crypt_pad_length_padding( mbedtls_aes_context *ctx, int mode, uint32_t length, const unsigned char iv[16], const unsigned char *input, unsigned char *output );

cy_rslt_t cy_sha256( const unsigned char *input, size_t ilen, unsigned char output[32], int is224 )
{

#if MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_3
    mbedtls_sha256( (uint8_t*) input, ilen, output, is224 );
#elif MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_2
    mbedtls_sha256_ret( (uint8_t*) input, ilen, output, is224 );
#else
error "Unsupported MBEDTLS version"
#endif

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_aes_cbc_encrypt( const unsigned char *input, uint32_t input_length, unsigned char *output, const unsigned char *key, unsigned int keybits, const unsigned char iv[16] )
{
    mbedtls_aes_context ctx;

    /* Encrypt input */
    mbedtls_aes_init( &ctx );
    mbedtls_aes_setkey_enc( &ctx, key, keybits );
    aes_cbc_crypt_pad_length_padding( &ctx, MBEDTLS_AES_ENCRYPT, input_length, iv, input, output );
    mbedtls_aes_free( &ctx );

    return CY_RSLT_SUCCESS;
}

cy_rslt_t cy_aes_cbc_decrypt( const unsigned char *input, uint32_t input_length, unsigned char *output, uint32_t* output_length, const unsigned char *key, unsigned int keybits, const unsigned char iv[16] )
{
    uint32_t result;
    mbedtls_aes_context ctx;

    mbedtls_aes_init( &ctx );
    mbedtls_aes_setkey_dec( &ctx, key, keybits );
    result = aes_cbc_crypt_pad_length_padding( &ctx, MBEDTLS_AES_DECRYPT, input_length, iv, input, output );
    *output_length = result;
    mbedtls_aes_free( &ctx );
    return CY_RSLT_SUCCESS;
}

void cy_sha2_hmac_starts(cy_sha2_hmac_context *ctx, const unsigned char *key, uint32_t keylen,
              int32_t is224)
{
    uint32_t i;
    unsigned char sum[32];

    if (keylen > 64) {
#if MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_3
    mbedtls_sha256(key, keylen, sum, is224);
#elif MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_2
    mbedtls_sha256_ret(key, keylen, sum, is224);
#else
error "Unsupported MBEDTLS version"
#endif

        keylen = (is224) ? 28 : 32;
        key = sum;
    }

    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (unsigned char)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (unsigned char)(ctx->opad[i] ^ key[i]);
    }

    mbedtls_sha256_init(&ctx->ctx);
#if MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_3
    mbedtls_sha256_starts(&ctx->ctx, is224);
    mbedtls_sha256_update(&ctx->ctx, ctx->ipad, 64);
#elif MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_2
    mbedtls_sha256_starts_ret(&ctx->ctx, is224);
    mbedtls_sha256_update_ret(&ctx->ctx, ctx->ipad, 64);
#else
error "Unsupported MBEDTLS version"
#endif

    memset(sum, 0, sizeof(sum));
}

/*
 * SHA-256 HMAC process buffer
 */
void cy_sha2_hmac_update(cy_sha2_hmac_context *ctx, const unsigned char *input, uint32_t ilen)
{
#if MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_3
    mbedtls_sha256_update(&ctx->ctx, input, ilen);
#elif MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_2
    mbedtls_sha256_update_ret(&ctx->ctx, input, ilen);
#else
    error "Unsupported MBEDTLS version"
#endif
}

/*
 * SHA-256 HMAC final digest
 */
void cy_sha2_hmac_finish(cy_sha2_hmac_context * ctx, unsigned char output[32])
{
    int32_t       is224;
    uint32_t      hlen;
    unsigned char tmpbuf[32];

    /*
     * Work Around: Instead of hard-coding is224 to zero, use ctx->ctx.is224;
     * However, mbedtls_sha256_context structure in cy-mbedtls-acceleration(HW Crypto) does not
     * have a is224 field. Verified in entire WPS code that is224 is always used as zero, so
     * it is safe to initialize is224 with 0.
     */
    is224 = 0;
    /* CID 465080: Deadcode. Since is224 is initialized with 0,
       hlen = (is224 == 0) ? 32 : 28 will never evaluate to 28.
       Therefore, initializing hlen to 32 directly.
    */
    hlen = 32;

#if MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_3
    mbedtls_sha256_finish(&ctx->ctx, tmpbuf);
    mbedtls_sha256_starts(&ctx->ctx, is224);
    mbedtls_sha256_update(&ctx->ctx, ctx->opad, 64);
    mbedtls_sha256_update(&ctx->ctx, tmpbuf, hlen);
    mbedtls_sha256_finish(&ctx->ctx, output);
    mbedtls_sha256_free(&ctx->ctx);
#elif MBEDTLS_VERSION_MAJOR == MBEDTLS_MAJOR_VERSION_2
    mbedtls_sha256_finish_ret(&ctx->ctx, tmpbuf);
    mbedtls_sha256_starts_ret(&ctx->ctx, is224);
    mbedtls_sha256_update_ret(&ctx->ctx, ctx->opad, 64);
    mbedtls_sha256_update_ret(&ctx->ctx, tmpbuf, hlen);
    mbedtls_sha256_finish_ret(&ctx->ctx, output);
    mbedtls_sha256_free(&ctx->ctx);
#else
    error "Unsupported MBEDTLS version"
#endif

    memset(tmpbuf, 0, sizeof(tmpbuf));
}

/*
 * Output = HMAC-SHA-256( hmac key, input buffer )
 */
void cy_sha2_hmac(const unsigned char *key, uint32_t keylen,
           const unsigned char *input, uint32_t ilen,
           unsigned char output[32], int32_t is224)
{
    cy_sha2_hmac_context ctx;

    cy_sha2_hmac_starts(&ctx, key, keylen, is224);
    cy_sha2_hmac_update(&ctx, input, ilen);
    cy_sha2_hmac_finish(&ctx, output);

    memset(&ctx, 0, sizeof(cy_sha2_hmac_context));
}
