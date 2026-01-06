/*
 * Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
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

#include "wpa3_ext_supp.h"

extern cy_rslt_t cy_wpa3_get_pfn_network( uint8_t * ssid, uint8_t *passphrase, uint8_t *pt );

cy_rslt_t wpa3_crypto_start_pwe_generation(wpa3_supplicant_workspace_t *wksp) {
    int counter = 1;
    int found = 0;
    mbedtls_mpi X, y, ysqr, temp, base, seed, save, randpass, one;
    mbedtls_mpi ycomp, pycomp, py, ysqr_saved;
    cy_rslt_t res;
    int ret;
    unsigned cmp_val = 0, cmp_val_y2 = 0, cmp_val_py2 = 0;
    uint8_t mac_buf[ETH_ADDR_LEN * 2] = { 0 };
    unsigned char base_val[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t pwd_seed[WPA3_PWE_SEED_MAX_LEN] = { 0 };
    uint8_t rand_passwd[WPA3_MAX_PASSPHRASE_LEN] = { 0 };
    uint8_t pointx[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t qr[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t qnr[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t y2_calc[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t y2_buf[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t save_buf[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t py2_calc[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t ysqr_buf[WPA3_SAE_KEYSEED_KEY_LEN * 3] = { 0 };
    uint8_t primebuf[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint32_t start_time = 0;
    uint32_t end_time = 0;
    uint8_t *addr[3];
    size_t len[3];
    size_t num_elem = 0;

    mbedtls_mpi_init(&X);
    mbedtls_mpi_init(&y);
    mbedtls_mpi_init(&ysqr);
    mbedtls_mpi_init(&ysqr_saved);
    mbedtls_mpi_init(&temp);
    mbedtls_mpi_init(&base);
    mbedtls_mpi_init(&seed);
    mbedtls_mpi_init(&save);
    mbedtls_mpi_init(&ycomp);
    mbedtls_mpi_init(&pycomp);
    mbedtls_mpi_init(&py);
    mbedtls_mpi_init(&randpass);
    mbedtls_mpi_init(&one);

    res = wpa3_crypto_get_rand_qr_qnr(&(wksp->wpa3_crypto_ctxt->group), qr, qnr,
            WPA3_SAE_KEYSEED_KEY_LEN);
    if (res != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_get_rand_qr_qnr failed ret = %ld\n", res));
        return res;
    }

    res = wpa3_crypto_get_rand(&(wksp->wpa3_crypto_ctxt->group), &randpass,
            false);
    if (res != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_get_rand failed ret = %ld\n", res));
        return res;
    }

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&randpass, rand_passwd, WPA3_SAE_KEYSEED_KEY_LEN));

    res = wpa3_sta_mac_ap_bssid_buf(
            (uint8_t *) wksp->wpa3_sae_auth_info.sta_mac,
            (uint8_t *) wksp->wpa3_sae_auth_info.ap_bssid, (uint8_t *) mac_buf);
    if (res != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_sta_mac_ap_bssid_buf failed ret = %ld\n", res));
        return res;
    }
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:mac_buf\n"));
    WPA3_EXT_HEX_BUF_DUMP((mac_buf, sizeof(mac_buf)));
    cy_rtos_get_time(&start_time);

    addr[0] = wksp->wpa3_sae_auth_info.passhphrase;
    len[0] = strlen((const char *) wksp->wpa3_sae_auth_info.passhphrase);
    num_elem++;
    addr[num_elem] = (uint8_t *) &counter;
    len[num_elem] = sizeof(uint8_t);
    num_elem++;

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->group.P), primebuf, WPA3_SAE_KEYSEED_KEY_LEN));

    do {
        memset(base_val, 0, sizeof(base_val));
        memset(pwd_seed, 0, sizeof(pwd_seed));
        ret = wpa3_crypto_hmac_sha256(mac_buf, (ETH_ADDR_LEN * 2), num_elem,
                addr, len, base_val);

        if (ret != CY_RSLT_SUCCESS) {
            WPA3_EXT_LOG_MSG(
                    ("WPA3-EXT-SUPP:wpa3_supplicant_hmac_sha256 failed ret = %d\n", ret));
        }

        ret = wpa3_crypto_hmac_sha256_kdf_bits(base_val,
                WPA3_SAE_KEYSEED_KEY_LEN, "SAE Hunting and Pecking", primebuf,
                (size_t) WPA3_SAE_KEYSEED_KEY_LEN, pwd_seed,
                (8 * WPA3_SAE_KEYSEED_KEY_LEN));

        if (ret != CY_RSLT_SUCCESS) {
            WPA3_EXT_LOG_MSG(
                    ("WPA3-EXT-SUPP:wpa3_supplicant_hmac_sha256_kdf failed ret = %d\n", ret));
        }

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_read_binary(&seed, (const unsigned char *)pwd_seed, WPA3_SAE_KEYSEED_KEY_LEN));
        /*
         * Export X into unsigned binary data, big endian
         */
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary(&seed, (unsigned char * )pointx,
                        (sizeof(pointx))));

        /* compared seed to be less than prime in constant time */
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_lt_mpi_ct((const mbedtls_mpi * )&seed,
                        (const mbedtls_mpi * )&(wksp->wpa3_crypto_ctxt->group.P),
                        (unsigned * )&cmp_val));

        if (cmp_val == 1) {
            /* ysqr = seed ^ 3 + a * seed + b; */
            wpa3_crypto_derive_ysqr_from_x(pointx, sizeof(pointx), &ysqr,
                    &(wksp->wpa3_crypto_ctxt->group));

            MBEDTLS_MPI_CHK(
                    mbedtls_mpi_write_binary(&ysqr, ysqr_buf,
                            sizeof(ysqr_buf)));

            wpa3_cyrpto_is_quadratic_residue_blind(qr, qnr, ysqr_buf,
                    sizeof(ysqr_buf), &(wksp->wpa3_crypto_ctxt->group));

            if (found == 0) {
                MBEDTLS_MPI_CHK(
                        mbedtls_mpi_write_binary(&seed,
                                wksp->wpa3_crypto_ctxt->x_buf,
                                sizeof(wksp->wpa3_crypto_ctxt->x_buf)));

                MBEDTLS_MPI_CHK(
                        mbedtls_mpi_read_binary(&base,
                                (const unsigned char * )base_val,
                                sizeof(base_val)));

                /* save = base */
                MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&save, &base));
                MBEDTLS_MPI_CHK(
                        mbedtls_mpi_write_binary(&save, save_buf,
                                sizeof(save_buf)));

                MBEDTLS_MPI_CHK(
                        mbedtls_mpi_mod_mpi(&ysqr, &ysqr,
                                &(wksp->wpa3_crypto_ctxt->group.P)));

                /* derive y from ysqr */
                wpa3_crypto_derive_y_from_ysqr(&(wksp->wpa3_crypto_ctxt->group),
                        &ysqr, &y);
                MBEDTLS_MPI_CHK(
                        mbedtls_mpi_write_binary(&y,
                                wksp->wpa3_crypto_ctxt->y_buf,
                                sizeof(wksp->wpa3_crypto_ctxt->y_buf)));

                ret = wpa3_crypto_check_valid_point_on_ecp_curve(wksp);

                if (ret == CY_RSLT_SUCCESS) {
                    found = 1;
                    wksp->wpa3_crypto_ctxt->pwe_found = true;
                    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:counter=%d\n", counter));
                    addr[0] = rand_passwd;

                    /* save ysqr */
                    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&ysqr_saved, &ysqr));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_write_binary(&ysqr_saved, y2_buf,
                                    sizeof(y2_buf)));

                    /* compute y^2 */
                    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ycomp, &y, &y));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_mod_mpi(&ycomp, &ycomp,
                                    &(wksp->wpa3_crypto_ctxt->group.P)));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_write_binary(&ycomp, y2_calc,
                                    sizeof(y2_calc)));

                    /* compute py and py ^2 */
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_sub_mpi(&py,
                                    &(wksp->wpa3_crypto_ctxt->group.P), &y));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_write_binary(&py,
                                    wksp->wpa3_crypto_ctxt->py_buf,
                                    sizeof(wksp->wpa3_crypto_ctxt->py_buf)));
                    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&pycomp, &py, &py));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_mod_mpi(&pycomp, &pycomp,
                                    &(wksp->wpa3_crypto_ctxt->group.P)));
                    MBEDTLS_MPI_CHK(
                            mbedtls_mpi_write_binary(&pycomp, py2_calc,
                                    sizeof(py2_calc)));

                    cmp_val_y2 = memcmp(y2_calc, y2_buf, sizeof(y2_buf));
                    cmp_val_py2 = memcmp(py2_calc, y2_buf, sizeof(y2_buf));
                    if ((cmp_val_y2 == 0) || (cmp_val_py2 == 0)) {
                        WPA3_EXT_LOG_MSG(
                                ("WPA3-EXT-SUPP:y2 calculated matches with y2 buf\n"));
#ifdef WPA3_EXT_SUPPLICANT_DEBUG
                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x point counter=%d...\n", counter));
                        WPA3_EXT_HEX_BUF_DUMP((wksp->wpa3_crypto_ctxt->x_buf, sizeof(wksp->wpa3_crypto_ctxt->x_buf)));

                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ysqr point counter=%d...\n", counter));
                        WPA3_EXT_HEX_BUF_DUMP((y2_buf, sizeof(y2_buf)));

                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:y point counter=%d...\n", counter));
                        WPA3_EXT_HEX_BUF_DUMP((wksp->wpa3_crypto_ctxt->y_buf, sizeof(wksp->wpa3_crypto_ctxt->y_buf)));

                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:py point counter=%d...\n", counter));
                        WPA3_EXT_HEX_BUF_DUMP((wksp->wpa3_crypto_ctxt->py_buf, sizeof(wksp->wpa3_crypto_ctxt->py_buf)));

                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:computed y2\n"));
                        WPA3_EXT_HEX_BUF_DUMP((y2_calc, sizeof(y2_calc)));

                        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:computed py2\n"));
                        WPA3_EXT_HEX_BUF_DUMP((py2_calc, sizeof(py2_calc)));
#endif
                    }
                }
            }

        }
        counter = counter + 1;
        if (counter > WPA3_MAX_PWE_LOOP) {
            WPA3_EXT_LOG_MSG(
                    ("\n***WPA3-EXT-SUPP:(counter > WPA3_MAX_PWE_LOOP)****\n"));
            break;
        }

    } while ((found == 0) || (counter <= WPA3_MAX_PWE_LOOP));

    cy_rtos_get_time(&end_time);

    WPA3_EXT_LOG_MSG(
            ("WPA3-EXT-SUPP:PWE loop time for 40 iterations computing ysqr, start_time:%ld end_time:%ld  computation time = %ld ms\n", start_time, end_time, (end_time - start_time)));
#ifdef WPA3_EXT_SUPPLICANT_DEBUG
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:Found ysqr as perfect square\n"));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x point counter=%d...\n", counter));
    WPA3_EXT_HEX_BUF_DUMP((wksp->wpa3_crypto_ctxt->x_buf, sizeof(wksp->wpa3_crypto_ctxt->x_buf)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ysqr point counter=%d...\n", counter));
    WPA3_EXT_HEX_BUF_DUMP((y2_buf, sizeof(y2_buf)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:y point counter=%d...\n", counter));
    WPA3_EXT_HEX_BUF_DUMP((wksp->wpa3_crypto_ctxt->y_buf, sizeof(wksp->wpa3_crypto_ctxt->y_buf)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:computed y2"));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ycomp, &y, &y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ycomp, &ycomp, &(wksp->wpa3_crypto_ctxt->group.P)));
    WPA3_EXT_HEX_MPI_DUMP((&ycomp));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:computed py2"));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&py, &(wksp->wpa3_crypto_ctxt->group.P), &y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&pycomp, &py, &py));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&pycomp, &pycomp, &(wksp->wpa3_crypto_ctxt->group.P)));
    WPA3_EXT_HEX_MPI_DUMP((&pycomp));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:base \n"));
    WPA3_EXT_HEX_MPI_DUMP((&save));
#endif

    if (wksp->wpa3_crypto_ctxt->pwe_found == false) {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:PWE generation for HnP failed\n"));
        /* post WPA3_SUPPLICANT_EVENT_DELETE to supplicant */
        /* do cleanup */
        res = WPA3_EXT_PWE_GEN_FAILED;
    }

    /*
     if (lsb(y) == lsb(save))
     {
     PE = (x, y);
     } else {
     PE = (x, p - y);
     }
     */
    if ((save_buf[WPA3_SAE_KEYSEED_KEY_LEN - 1] & 1)
            != (wksp->wpa3_crypto_ctxt->y_buf[WPA3_SAE_KEYSEED_KEY_LEN - 1] & 1)) {
        /* py = p - y */
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_sub_mpi(&py, &(wksp->wpa3_crypto_ctxt->group.P),
                        &y));
        WPA3_EXT_HEX_MPI_DUMP((&py));

        /* y = py */
        MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&y, &py));
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary(&y, wksp->wpa3_crypto_ctxt->y_buf,
                        sizeof(wksp->wpa3_crypto_ctxt->y_buf)));
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x point \n"));
    WPA3_EXT_HEX_BUF_DUMP(
            (wksp->wpa3_crypto_ctxt->x_buf, sizeof(wksp->wpa3_crypto_ctxt->x_buf)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:y point \n"));
    WPA3_EXT_HEX_BUF_DUMP(
            (wksp->wpa3_crypto_ctxt->y_buf, sizeof(wksp->wpa3_crypto_ctxt->y_buf)));

    ret = wpa3_crypto_check_valid_point_on_ecp_curve(wksp);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:xyz point not valid on curve ret=%d\n", ret));
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:xyz point"));
    WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.X));
    WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Y));
    WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Z));

    /* PWE generation done */
    wksp->wpa3_pwe_generation_done = true;

    cleanup: mbedtls_mpi_free(&X);
    mbedtls_mpi_free(&y);
    mbedtls_mpi_free(&ysqr);
    mbedtls_mpi_free(&temp);
    mbedtls_mpi_free(&base);
    mbedtls_mpi_free(&seed);
    mbedtls_mpi_free(&save);
    mbedtls_mpi_free(&ycomp);
    mbedtls_mpi_free(&pycomp);
    mbedtls_mpi_free(&py);
    mbedtls_mpi_free(&ysqr_saved);
    return res;
}

cy_rslt_t wpa3_crypto_derive_pwe_from_pt(wpa3_supplicant_workspace_t *wksp)
{
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    uint8_t pt[WPA3_H2E_PT_XY_SIZE] = {0};
    int len = 1;
    uint8_t mac_buf[ETH_ADDR_LEN * 2] = { 0 };
    uint8_t pwd_seed[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_info_t *md_info;
    mbedtls_mpi pwdval, q1;

    mbedtls_mpi_init(&pwdval);
    mbedtls_mpi_init(&q1);

    /* check if the PT is stored */
    ret = cy_wpa3_get_pfn_network( wksp->wpa3_sae_auth_info.ssid, wksp->wpa3_sae_auth_info.passhphrase, pt);

    if ( ret != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  PT not found \n"));

        /* derive PT */
        ret = wpa3_crypto_derive_pt(wksp, wksp->wpa3_sae_auth_info.ssid, wksp->wpa3_sae_auth_info.passhphrase, pt, sizeof(pt));

        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  PT \n"));
        WPA3_EXT_HEX_BUF_DUMP((pt, sizeof(pt)));

    }
    else
    {
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  PT found \n"));
        WPA3_EXT_HEX_BUF_DUMP((pt, sizeof(pt)));
    }

    /* read point from buffer */
    wpa3_crypto_read_point_from_buffer(wksp, &pt[len], &pt[len + WPA3_SAE_SCALAR_LEN], &wksp->wpa3_crypto_ctxt->sta_pt_element);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:PT xy point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->sta_pt_element.X));
    WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->sta_pt_element.Y));

    /* val = H(0n, MAX(STA-MAC, AP-MAC) || MIN(STA-MAC, AP-MAC)) */


    ret = wpa3_sta_mac_ap_bssid_buf((uint8_t *) wksp->wpa3_sae_auth_info.sta_mac,
                                       (uint8_t *) wksp->wpa3_sae_auth_info.ap_bssid, (uint8_t *) mac_buf);
    if (ret != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sta_mac_ap_bssid_buf failed ret = %ld\n", ret));
        return ret;
    }
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:mac_buf\n"));
    WPA3_EXT_HEX_BUF_DUMP((mac_buf, sizeof(mac_buf)));

    md_info = (mbedtls_md_info_t *) mbedtls_md_info_from_type(md_type);

    if (!md_info)
    {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:md_info failed \n"));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    ret = mbedtls_hkdf_extract((const mbedtls_md_info_t *) md_info,
             (const unsigned char *) NULL, 0,
             (const unsigned char *) mac_buf, (size_t) sizeof(mac_buf),
             (unsigned char *) pwd_seed);

     if (ret != CY_RSLT_SUCCESS)
     {
         WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:mbedtls_hkdf_extract failed ret=%ld \n", ret));
         return WPA3_EXT_CRYPTO_ERROR;
     }
     WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  pwd_seed\n"));
     WPA3_EXT_HEX_BUF_DUMP((pwd_seed, sizeof(pwd_seed)));

     /* pwdval = pwdval modulo (q – 1) + 1 */

     /* q - 1 */
     MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&q1, &(wksp->wpa3_crypto_ctxt->group.N), 1));
     MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&pwdval, (const unsigned char * )pwd_seed,
                                              sizeof(pwd_seed)));

     /* pwdval = pwdval mod (q - 1) */
     ret = mbedtls_mpi_mod_mpi((mbedtls_mpi *) &pwdval, (const mbedtls_mpi *) &pwdval,
                               (const mbedtls_mpi *) &q1);

     if (ret != CY_RSLT_SUCCESS)
     {
         WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:mbedtls_mpi_mod_mpi pwdval failed ret=%ld \n", ret));
         return WPA3_EXT_CRYPTO_ERROR;
     }

     /* pwdval = pwdval + 1 */
     MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&pwdval, &pwdval, 1));

     /* PWE = scalar-op(pwdval, PT) */
     MBEDTLS_MPI_CHK(mbedtls_ecp_mul(&wksp->wpa3_crypto_ctxt->group,
                                     &wksp->wpa3_crypto_ctxt->pwe, &pwdval, &wksp->wpa3_crypto_ctxt->sta_pt_element,
                                     NULL, NULL));

     WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:H2E pwe before inverse"));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.X));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Y));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Z));

     /* sta_commit_element = inverse(sta_commit_element) */
     ret = wpa3_crypto_point_inverse(&wksp->wpa3_crypto_ctxt->group,
             &wksp->wpa3_crypto_ctxt->sta_pt_element, &wksp->wpa3_crypto_ctxt->sta_pt_element);

     if (ret != 0)
     {
         WPA3_EXT_LOG_MSG(( "WPA3-EXT-SUPP:wpa3_crypto_point_inverse failed: ret=%ld\n", ret ));
         goto cleanup;
     }

     WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:H2E STA PWE"));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.X));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Y));
     WPA3_EXT_HEX_MPI_DUMP((&wksp->wpa3_crypto_ctxt->pwe.Z));

     /* check if the point is valid on the ECP curve */
     ret = mbedtls_ecp_check_pubkey(&wksp->wpa3_crypto_ctxt->group, &wksp->wpa3_crypto_ctxt->pwe);

     if (ret != CY_RSLT_SUCCESS)
     {
         WPA3_EXT_LOG_MSG(("\n WPA3-EXT-SUPP:H2E PWE  not valid point on curve ret=%ld\n", ret));
     }
     else
     {
         WPA3_EXT_LOG_MSG(("\n*** WPA3-EXT-SUPP:H2E PWE is a VALID point on ECP CURVE ret=%ld ***\n", ret));
     }

cleanup:
    mbedtls_mpi_free(&pwdval);
    mbedtls_mpi_free(&q1);

    return ret;
}

cy_rslt_t wpa3_crypto_derive_pt(wpa3_supplicant_workspace_t *wksp,
        uint8_t *ssid, uint8_t *passphrase, uint8_t *output, uint8_t outlen)
{
    uint8_t pwd_seed[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t pwd_value[WPA3_KDF_EXPAND_LEN] = { 0 };
    uint8_t xp1[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t yp1[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t xp2[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t yp2[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    mbedtls_md_info_t *md_info;
    mbedtls_md_context_t md_ctx;
    int ret = CY_RSLT_SUCCESS;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_mpi u1, u2;
    mbedtls_ecp_point P1, P2, PT;
    mbedtls_mpi pwdval;
    mbedtls_mpi m;
    wpa3_crypto_context_info_t *crypto_ctxt;

    mbedtls_mpi_init(&u1);
    mbedtls_mpi_init(&u2);
    mbedtls_ecp_point_init(&P1);
    mbedtls_ecp_point_init(&P2);
    mbedtls_ecp_point_init(&PT);
    mbedtls_mpi_init(&m);
    mbedtls_mpi_init(&pwdval);
    mbedtls_md_init(&md_ctx);

    crypto_ctxt = (wpa3_crypto_context_info_t *) wksp->wpa3_crypto_ctxt;

    /* set m = 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&m, 1));

    md_info = (mbedtls_md_info_t *) mbedtls_md_info_from_type(md_type);
    if (!md_info) {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:md_info failed \n"));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    /* pwd-seed = HKDF-Extract(ssid, password [|| identifier]) */
    ret = mbedtls_hkdf_extract((const mbedtls_md_info_t *) md_info,
            (const unsigned char *) ssid, (size_t) strlen((const char *) ssid),
            (const unsigned char *) passphrase,
            (size_t) strlen((const char *) passphrase),
            (unsigned char *) pwd_seed);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_hkdf_extract failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  pwd_seed\n"));
    WPA3_EXT_HEX_BUF_DUMP((pwd_seed, sizeof(pwd_seed)));

    /* pwd-value = HKDF-Expand(pwd-seed, “SAE Hash to Element u1 P1”, len) */

    ret = mbedtls_hkdf_expand((const mbedtls_md_info_t *) md_info,
            (const unsigned char *) pwd_seed, (size_t) sizeof(pwd_seed),
            (const unsigned char *) ("SAE Hash to Element u1 P1"),
            (size_t) strlen("SAE Hash to Element u1 P1"),
            (unsigned char *) pwd_value, (size_t) sizeof(pwd_value));

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_hkdf_expand u1p1 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    /* Perform a modular reduction. R = pwd-value mod p*/
    /* u1 = pwd_value modulo p */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_read_binary(&pwdval, (const unsigned char * )pwd_value,
                    sizeof(pwd_value)));

    ret = mbedtls_mpi_mod_mpi((mbedtls_mpi *) &u1,
            (const mbedtls_mpi *) &pwdval,
            (const mbedtls_mpi *) &(wksp->wpa3_crypto_ctxt->group.P));
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_mpi_mod_mpi u1 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:  pwd_value\n"));
    WPA3_EXT_HEX_BUF_DUMP((pwd_value, sizeof(pwd_value)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: u1 pwdval point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&pwdval));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: u1\n"));
    WPA3_EXT_HEX_MPI_DUMP((&u1));

    /*   P1 = SSWU(u1) */
    ret = wpa3_cyrpto_sswu_algo(wksp, &u1, xp1, yp1);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_cyrpto_sswu_algo pt1 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }
    wpa3_crypto_read_point_from_buffer(wksp, xp1, yp1, &P1);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:xp1 dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((xp1, sizeof(xp1)));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:yp1 dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((yp1, sizeof(yp2)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:P1 xy point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&P1.X));
    WPA3_EXT_HEX_MPI_DUMP((&P1.Y));

    /* check if the point is valid on the ECP curve */
    ret = mbedtls_ecp_check_pubkey(&wksp->wpa3_crypto_ctxt->group, &P1);

    if (ret != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("\n** WPA3-EXT-SUPP:P1 point not valid on curve ret=%d\n", ret));
    }
    else
    {
        WPA3_EXT_LOG_MSG(("\n** WPA3-EXT-SUPP:P1 point VALID on ECP CURVE ret=%d **\n", ret));
    }

    /* pwd-value = HKDF-Expand(pwd-seed, “SAE Hash to Element u2 P2”, len) */
    ret = mbedtls_hkdf_expand((const mbedtls_md_info_t *) md_info,
            (const unsigned char *) pwd_seed, (size_t) sizeof(pwd_seed),
            (const unsigned char *) ("SAE Hash to Element u2 P2"),
            (size_t) strlen("SAE Hash to Element u2 P2"),
            (unsigned char *) pwd_value, (size_t) sizeof(pwd_value));

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_hkdf_expand u2p2 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    /* u2 = pwd-value modulo p */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_read_binary(&pwdval, (const unsigned char * )pwd_value,
                    sizeof(pwd_value)));
    ret = mbedtls_mpi_mod_mpi((mbedtls_mpi *) &u2,
            (const mbedtls_mpi *) &pwdval,
            (const mbedtls_mpi *) &(wksp->wpa3_crypto_ctxt->group.P));

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_mpi_mod_mpi u2 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: u2 pwdval point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&pwdval));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP: u2\n"));
    WPA3_EXT_HEX_MPI_DUMP((&u2));

    ret = wpa3_cyrpto_sswu_algo(wksp, &u2, xp2, yp2);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_cyrpto_sswu_algo pt2 failed ret=%d \n", ret));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:xp2 dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((xp2, sizeof(xp2)));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:yp2 dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((yp2, sizeof(yp2)));

    /*  PT = elem-op(P1, P2); */
    wpa3_crypto_read_point_from_buffer(wksp, xp2, yp2, &P2);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:P2 xy point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&P2.X));
    WPA3_EXT_HEX_MPI_DUMP((&P2.Y));

    /* check if the point is valid on the ECP curve */
    ret = mbedtls_ecp_check_pubkey(&wksp->wpa3_crypto_ctxt->group, &P2);

    if (ret != CY_RSLT_SUCCESS)
    {
        WPA3_EXT_LOG_MSG(("\n** WPA3-EXT-SUPP:P2 point not valid on curve ret=%d\n", ret));
    }
    else
    {
        WPA3_EXT_LOG_MSG(("\n** WPA3-EXT-SUPP:P2 point VALID on ECP CURVE ret=%d **\n", ret));
    }

    /* temp_point =  m * (P1) +  m * (P2) */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_muladd(&crypto_ctxt->group, &PT, &m,
                    (const mbedtls_ecp_point * )&P1, &m,
                    (const mbedtls_ecp_point * )&P2));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:PT xy point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&PT.X));
    WPA3_EXT_HEX_MPI_DUMP((&PT.Y));

    ret = wpa3_crypto_write_point_to_buffer(wksp, &PT, output, outlen);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:output dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((output, outlen));

    if (ret != CY_RSLT_SUCCESS) {
        return WPA3_EXT_CRYPTO_ERROR;
    }

    cleanup:
    mbedtls_mpi_free(&u1);
    mbedtls_mpi_free(&u2);
    mbedtls_ecp_point_free(&P1);
    mbedtls_ecp_point_free(&P2);
    mbedtls_ecp_point_free(&PT);
    mbedtls_mpi_free(&m);
    mbedtls_mpi_free(&pwdval);
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_cyrpto_sswu_algo(wpa3_supplicant_workspace_t *wksp,
                                mbedtls_mpi *u, uint8_t *xpt, uint8_t *ypt)
{
    mbedtls_mpi z, m, z2, u2, u4;
    mbedtls_mpi z2u4, zu2, t, t3, t2, t1, exp;
    mbedtls_mpi za, bza, a, negb, negba, ysqr;
    mbedtls_mpi zero, negbat, x1, gx1, gx2, x1_tmp;
    mbedtls_mpi x2, x3, ax1, ax2, x2_tmp, x2_tmp1, v, x, y, py;
    int l, lsbu, lsby, is_eq;
    int ret = CY_RSLT_SUCCESS;

    mbedtls_mpi_init(&zero);
    mbedtls_mpi_init(&z);
    mbedtls_mpi_init(&z2);
    mbedtls_mpi_init(&u2);
    mbedtls_mpi_init(&u4);
    mbedtls_mpi_init(&z2u4);
    mbedtls_mpi_init(&zu2);
    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&m);
    mbedtls_mpi_init(&t);
    mbedtls_mpi_init(&t1);
    mbedtls_mpi_init(&t2);
    mbedtls_mpi_init(&t3);
    mbedtls_mpi_init(&za);
    mbedtls_mpi_init(&bza);
    mbedtls_mpi_init(&a);
    mbedtls_mpi_init(&negb);
    mbedtls_mpi_init(&negba);
    mbedtls_mpi_init(&negbat);
    mbedtls_mpi_init(&x1);
    mbedtls_mpi_init(&gx1);
    mbedtls_mpi_init(&gx2);
    mbedtls_mpi_init(&x1_tmp);
    mbedtls_mpi_init(&x2_tmp);
    mbedtls_mpi_init(&x2_tmp1);
    mbedtls_mpi_init(&x2);
    mbedtls_mpi_init(&x3);
    mbedtls_mpi_init(&ax1);
    mbedtls_mpi_init(&ax2);
    mbedtls_mpi_init(&v);
    mbedtls_mpi_init(&x);
    mbedtls_mpi_init(&y);
    mbedtls_mpi_init(&py);
    mbedtls_mpi_init(&ysqr);

    /* set z = -10 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&z, 10));
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&z, &(wksp->wpa3_crypto_ctxt->group.P), &z));

    /* set negb = P-B */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&negb, &(wksp->wpa3_crypto_ctxt->group.P), &(wksp->wpa3_crypto_ctxt->group.B)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP negb\n"));
    WPA3_EXT_HEX_MPI_DUMP((&(negb)));

    /* set a = p-3 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&a, &(wksp->wpa3_crypto_ctxt->group.P), 3));

    /* z2 = (z * z) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&z2, &z, &z));

    /* u2 = ( u * u) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u2, u, u));

    /* u4 = ( u2 * u2) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&u4, &u2, &u2));

    /* z2u4 = z2 * u4 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&z2u4, &z2, &u4));

    /*zu2 = z * u2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&zu2, &z, &u2));

    /*  m = (z2 * u4 + z * u2) modulo p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&m, &z2u4, &zu2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&m, &m, &(wksp->wpa3_crypto_ctxt->group.P)));

    /*  l = CEQ(m, 0) */
    l = mbedtls_mpi_cmp_int(&m, 0);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:m l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&m));

    /* t = inverse(m) */
    /* t=  m^(p-2) modulo p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&exp, (const mbedtls_mpi * )&(wksp->wpa3_crypto_ctxt->group.P), 2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&t, &m, &exp, &(wksp->wpa3_crypto_ctxt->group.P), 0));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:t\n"));
    WPA3_EXT_HEX_MPI_DUMP((&t));

    /*  x1 = CSEL(l, (b / (z * a) modulo p), ((-b/a) * (1 + t)) modulo p) */
    /*  za = z * a */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&za, &z, &a));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&za, &za, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP A\n"));
    WPA3_EXT_HEX_MPI_DUMP((&a));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP z\n"));
    WPA3_EXT_HEX_MPI_DUMP((&z));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP za\n"));
    WPA3_EXT_HEX_MPI_DUMP((&za));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP negb\n"));
    WPA3_EXT_HEX_MPI_DUMP((&negb));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP P\n"));
    WPA3_EXT_HEX_MPI_DUMP((&(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP N\n"));
    WPA3_EXT_HEX_MPI_DUMP((&(wksp->wpa3_crypto_ctxt->group.N)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:z, za point\n"));
    WPA3_EXT_HEX_MPI_DUMP((&z));
    WPA3_EXT_HEX_MPI_DUMP((&za));

    /* bza = (b /za) mod p */
    /* 1/za mod P */
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&bza, &za, &(wksp->wpa3_crypto_ctxt->group.P) ));

    /* b/za mod p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&bza, &bza, &(wksp->wpa3_crypto_ctxt->group.B)));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&bza, &bza, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP bza\n"));
    WPA3_EXT_HEX_MPI_DUMP((&(bza)));

    /* 1/a mod P */
    MBEDTLS_MPI_CHK(mbedtls_mpi_inv_mod(&negba, &a, &(wksp->wpa3_crypto_ctxt->group.P) ));
    /* -b * 1/a mod P */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&negba, &negba, &negb));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ECP negba\n"));
    WPA3_EXT_HEX_MPI_DUMP((&(negba)));

    /* t + 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&t, &t, 1));

    /* (-b/a * (t + 1)) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&negbat, &negba, &t));

    /* (-b/a * (t + 1)) mod p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&negbat, &negbat, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:negbat\n"));
    WPA3_EXT_HEX_MPI_DUMP((&negbat));

    /* x1 = CSEL (l, bza, negbat) */
    /* CSEL(x,y,z) operates in constant-time and returns y if x is true and z otherwise.*/
    MBEDTLS_MPI_CHK( mbedtls_mpi_safe_cond_assign( &bza, &negbat, l ) );
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&x1, &bza));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:bza\n"));
    WPA3_EXT_HEX_MPI_DUMP((&bza));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:negbat\n"));
    WPA3_EXT_HEX_MPI_DUMP((&negbat));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x1\n"));
    WPA3_EXT_HEX_MPI_DUMP((&x1));

    /*  gx1 = (x13 + a * x1 + b) modulo p */
    /* x1_tmp = x1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&x1_tmp, &x1));
    /* x2 = x1 * x1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&x2, &x1, &x1));

    /* x3 = x2 * x1_tmp */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&x3, &x2, &x1_tmp));

    /* ax1 = a* x1_tmp */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ax1, &a, &x1_tmp));

    /* ax1 = a* x1_tmp + b */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&ax1, &ax1, &(wksp->wpa3_crypto_ctxt->group.B)));

    /* gx1 = x13 + ax1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&gx1, &x3, &ax1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&gx1, &gx1, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx1\n"));
    WPA3_EXT_HEX_MPI_DUMP((&gx1));

    /*  x2 = (z * u2 * x1) modulo p */
    /* zu2 = ( z * u2 ) */
    /* x2 =  (zu2 * x1_tmp) */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&x2, &zu2, &x1_tmp));
    /* x2 = x2 mod p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&x2, &x2, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x2\n"));
    WPA3_EXT_HEX_MPI_DUMP((&x2));

    //x2_tmp = x2
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&x2_tmp, &x2));

    /* x2 = x2 * x2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&x2_tmp1, &x2, &x2));

    /* x3 = x2 * x2_tmp */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&x3, &x2_tmp1, &x2_tmp));

    /* a * x2_tmp */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ax2, &a, &x2_tmp));

    /* ax2 = a* x2_tmp + b */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&ax2, &ax2, &(wksp->wpa3_crypto_ctxt->group.B)));

    /*  gx2 = (x23 + a * x2 + b) modulo p */
    /* gx2 = x3 + ax1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&gx2, &x3, &ax2));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&gx2, &gx2, &(wksp->wpa3_crypto_ctxt->group.P)));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx2\n"));
    WPA3_EXT_HEX_MPI_DUMP((&gx2));

    /*  t = gx1 is a quadratic residue modulo p */
    /* --> gx1^((p-1)/2) modulo p is zero or one */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&t3, &(wksp->wpa3_crypto_ctxt->group.P), 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&t3, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&t2, &gx1, &t3, &(wksp->wpa3_crypto_ctxt->group.P), 0));

    l = mbedtls_mpi_cmp_int(&t2, 1);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx1^((p-1)/2) modulo p l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&t2));

    /*  v = CSEL(l, gx1, gx2) */
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx1\n"));
    WPA3_EXT_HEX_MPI_DUMP((&gx1));

    /* CSEL(l,gx1,gx2) operates in constant-time and returns gx1 if l is true and gx2 otherwise.*/
    MBEDTLS_MPI_CHK( mbedtls_mpi_safe_cond_assign( &gx1, &gx2, l ) );
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&v, &gx1));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx1\n"));
    WPA3_EXT_HEX_MPI_DUMP((&gx1));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:gx2\n"));
    WPA3_EXT_HEX_MPI_DUMP((&gx2));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:v l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&v));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x1 l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&x1));

    /*  x = CSEL(l, x1, x2) */
    /* CSEL(l,x1,x2) operates in constant-time and returns x1 if l is true and x2 otherwise.*/
    MBEDTLS_MPI_CHK( mbedtls_mpi_safe_cond_assign( &x1, &x2, l ) );
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&x, &x1));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x1 l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&x1));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x2 l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&x2));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x l=%d\n", l));
    WPA3_EXT_HEX_MPI_DUMP((&x));

    /*  y = sqrt(v) */
    /*  t1 = p + 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&t1, &(wksp->wpa3_crypto_ctxt->group.P), 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_div_int(&t1, 0, &t1, 4));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&y, &v, &t1, &(wksp->wpa3_crypto_ctxt->group.P), 0));

    /* derive y from sqrt(v) */
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:y\n"));
    WPA3_EXT_HEX_MPI_DUMP((&y));

    lsbu = mbedtls_mpi_get_bit(u, 0); //== 1;
    lsby = mbedtls_mpi_get_bit(&y, 0); // == 1;

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:lsbu =%d lsby =%d\n", lsbu , lsby));

    /* l = CEQ(lsbu, lsby) */
    is_eq = wpa3_const_time_int_cmp(lsbu, lsby);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:is_eq=%d\n", is_eq));

    /* P = CSEL(l, (x,y), (x, p-y)) */
    /* py = p-y */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&py, &(wksp->wpa3_crypto_ctxt->group.P), &y));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:py\n"));
    WPA3_EXT_HEX_MPI_DUMP((&py));

    MBEDTLS_MPI_CHK( mbedtls_mpi_safe_cond_assign( &y, &py, is_eq ) );

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x is_eq=%d\n", is_eq));
    WPA3_EXT_HEX_MPI_DUMP((&x));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:y is_eq=%d\n", is_eq));
    WPA3_EXT_HEX_MPI_DUMP((&y));

    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ysqr, &y, &y));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&ysqr, &ysqr, &(wksp->wpa3_crypto_ctxt->group.P)));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:ysqr\n"));
    WPA3_EXT_HEX_MPI_DUMP((&ysqr));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:v\n"));
    WPA3_EXT_HEX_MPI_DUMP((&v));

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&x, xpt, WPA3_SAE_KEYSEED_KEY_LEN));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&y, ypt, WPA3_SAE_KEYSEED_KEY_LEN));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:x, y\n"));
    WPA3_EXT_HEX_BUF_DUMP((xpt, WPA3_SAE_KEYSEED_KEY_LEN));
    WPA3_EXT_HEX_BUF_DUMP((ypt, WPA3_SAE_KEYSEED_KEY_LEN));

    cleanup: mbedtls_mpi_free(&zero);
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&z2);
    mbedtls_mpi_free(&u2);
    mbedtls_mpi_free(&u4);
    mbedtls_mpi_free(&z2u4);
    mbedtls_mpi_free(&zu2);
    mbedtls_mpi_free(&exp);
    mbedtls_mpi_free(&m);
    mbedtls_mpi_free(&t);
    mbedtls_mpi_free(&t1);
    mbedtls_mpi_free(&t2);
    mbedtls_mpi_free(&t3);
    mbedtls_mpi_free(&za);
    mbedtls_mpi_free(&bza);
    mbedtls_mpi_free(&a);
    mbedtls_mpi_free(&negb);
    mbedtls_mpi_free(&negba);
    mbedtls_mpi_free(&negbat);
    mbedtls_mpi_free(&x1);
    mbedtls_mpi_free(&gx1);
    mbedtls_mpi_free(&gx2);
    mbedtls_mpi_free(&x1_tmp);
    mbedtls_mpi_free(&x2_tmp);
    mbedtls_mpi_init(&x2_tmp1);
    mbedtls_mpi_free(&x2);
    mbedtls_mpi_free(&x3);
    mbedtls_mpi_free(&ax1);
    mbedtls_mpi_free(&ax2);
    mbedtls_mpi_free(&v);
    mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&y);
    mbedtls_mpi_free(&py);
    mbedtls_mpi_free(&ysqr);
    return ret;
}

cy_rslt_t wpa3_crypto_check_valid_point_on_ecp_curve(
        wpa3_supplicant_workspace_t *wksp) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    uint8_t pwe_buf[WPA3_SAE_KEYSEED_KEY_LEN * 2 + 1] = { 0 };
    uint8_t pwe_buf_len = 0;

    /* read x and y into ecp point pwe */
    pwe_buf[0] = 0x04; /* UNCOMPRESSED POINT */
    pwe_buf_len++;
    memcpy(&pwe_buf[pwe_buf_len], wksp->wpa3_crypto_ctxt->x_buf,
            sizeof(wksp->wpa3_crypto_ctxt->x_buf));
    pwe_buf_len += sizeof(wksp->wpa3_crypto_ctxt->x_buf);
    memcpy(&pwe_buf[pwe_buf_len], wksp->wpa3_crypto_ctxt->y_buf,
            sizeof(wksp->wpa3_crypto_ctxt->y_buf));
    pwe_buf_len += sizeof(wksp->wpa3_crypto_ctxt->y_buf);

    ret = mbedtls_ecp_point_read_binary(&wksp->wpa3_crypto_ctxt->group,
            &wksp->wpa3_crypto_ctxt->pwe, (const unsigned char *) pwe_buf,
            pwe_buf_len);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:mbedtls_ecp_point_read_binary failed ret=%ld\n", ret));
    }

    /* check if the point is valid on the ECP curve */
    ret = mbedtls_ecp_check_pubkey(&wksp->wpa3_crypto_ctxt->group,
            &wksp->wpa3_crypto_ctxt->pwe);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("\n** WPA3-EXT-SUPP:xyz point not valid on curve ret=%ld\n", ret));
    } else {
        WPA3_EXT_LOG_MSG(
                ("\n** WPA3-EXT-SUPP:xyz point VALID on ECP CURVE ret=%ld **\n", ret));
    }
    return ret;
}

cy_rslt_t wpa3_crypto_read_point_from_buffer(wpa3_supplicant_workspace_t *wksp,
        uint8_t *x_buf, uint8_t *y_buf, mbedtls_ecp_point *pt) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    uint8_t pwe_buf[WPA3_SAE_KEYSEED_KEY_LEN * 2 + 1] = { 0 };
    uint8_t pwe_buf_len = 0;

    /* read x and y into ecp point pt */
    pwe_buf[0] = 0x04; /* UNCOMPRESSED POINT */
    pwe_buf_len++;
    memcpy(&pwe_buf[pwe_buf_len], x_buf, WPA3_SAE_KEYSEED_KEY_LEN);
    pwe_buf_len += WPA3_SAE_KEYSEED_KEY_LEN;
    memcpy(&pwe_buf[pwe_buf_len], y_buf, WPA3_SAE_KEYSEED_KEY_LEN);
    pwe_buf_len += WPA3_SAE_KEYSEED_KEY_LEN;

    ret = mbedtls_ecp_point_read_binary(&wksp->wpa3_crypto_ctxt->group, pt,
            (const unsigned char *) pwe_buf, pwe_buf_len);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_read_point_from_buffer failed ret=%ld\n", ret));
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_write_point_to_buffer(wpa3_supplicant_workspace_t *wksp,
        mbedtls_ecp_point *pt, uint8_t *output, uint8_t outlen) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    size_t len = 0;

    ret = mbedtls_ecp_point_write_binary(&wksp->wpa3_crypto_ctxt->group, pt,
            MBEDTLS_ECP_PF_UNCOMPRESSED, &len, (unsigned char *) output,
            (size_t) outlen);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_write_point_to_buffer failed ret=%ld\n", ret));
    }
    return ret;
}

cy_rslt_t wpa3_sta_mac_ap_bssid_buf(uint8_t *sta_mac, uint8_t *ap_bssid,
        uint8_t *output) {
    int i = 0;
    bool sta_gr = false;
    uint8_t null_bssid[ETH_ADDR_LEN] = { 0 };

    if ((memcmp(sta_mac, null_bssid, ETH_ADDR_LEN) == 0)
            || (memcmp(ap_bssid, null_bssid, ETH_ADDR_LEN) == 0)) {
        WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:wpa3_sta_mac_ap_bssid_buf failed\n"));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    for (i = 0; i < ETH_ADDR_LEN; i++) {
        if (sta_mac[i] > ap_bssid[i]) {
            sta_gr = true;
            break;
        } else if (sta_mac[i] < ap_bssid[i]) {
            break;
        }
    }
    if (sta_gr == true) {
        memcpy(output, sta_mac, ETH_ADDR_LEN);
        memcpy(&output[ETH_ADDR_LEN], ap_bssid, ETH_ADDR_LEN);
    } else {
        memcpy(output, ap_bssid, ETH_ADDR_LEN);
        memcpy(&output[ETH_ADDR_LEN], sta_mac, ETH_ADDR_LEN);
    }
    return CY_RSLT_SUCCESS;
}

int wpa3_crypto_mpi_legendre(const mbedtls_mpi *a, const mbedtls_mpi *p) {
    mbedtls_mpi exp, tmp;
    int res = -2;
    int ret;

    mbedtls_mpi_init(&exp);
    mbedtls_mpi_init(&tmp);

    /* exp = (p-1) / 2 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_int(&exp, (const mbedtls_mpi * ) p, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(&exp, 1));
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(&tmp, (const mbedtls_mpi * ) a, &exp, (const mbedtls_mpi * ) p, NULL));

    if ( ret == 0 )
    {
       if (mbedtls_mpi_cmp_int(&tmp, 1) == 0)
       {
           res = 1;
       }
       else if (mbedtls_mpi_cmp_int(&tmp, 0) == 0
            || mbedtls_mpi_cmp_mpi(&tmp, (const mbedtls_mpi *) p) == 0)
       {
            res = 0;
       }
       else
       {
           res = -1;
       }
    }
cleanup:

    mbedtls_mpi_free(&tmp);
    mbedtls_mpi_free(&exp);
    return res;
}

bool wpa3_cyrpto_is_quadratic_residue_blind(uint8_t *qr_buf, uint8_t *qnr_buf,
        uint8_t *ysqr_buf, uint16_t ysqr_buflen, mbedtls_ecp_group *ecp_grp) {
    mbedtls_mpi r, num, qr, qnr, unity, r_tmp, vrsqr;
    mbedtls_mpi ysqr;
    bool ret = false;
    uint8_t tmp[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&ysqr);
    mbedtls_mpi_init(&num);
    mbedtls_mpi_init(&qr);
    mbedtls_mpi_init(&qnr);
    mbedtls_mpi_init(&unity);
    mbedtls_mpi_init(&r_tmp);
    mbedtls_mpi_init(&vrsqr);

    ret = wpa3_crypto_get_rand(ecp_grp, &r, false);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_cyrpto_is_quadratic_residue_blind get_rand failed!!\n"));
        return false;
    }

    /* r_tmp = r */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&r_tmp, &r));

    /* num = (r * r_tmp) mod p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&num, &r, &r_tmp));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&num, &num, &ecp_grp->P));

    /* read ysqr from buffer */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&ysqr, ysqr_buf, (WPA3_SAE_KEYSEED_KEY_LEN * 3)));

    /* vrsqr = ysrq * num */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&vrsqr, &ysqr, &num));
    /* num = (vrsqr) mod p */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_mod_mpi(&num, (const mbedtls_mpi * )&vrsqr,
                    &ecp_grp->P));

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&num, tmp, sizeof(tmp)));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&qr, qr_buf, WPA3_SAE_KEYSEED_KEY_LEN));
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&qnr, qnr_buf, WPA3_SAE_KEYSEED_KEY_LEN));

    /* set unity = 1 */
    mbedtls_mpi_lset(&unity, 1);

    if (wpa3_is_buf_val_odd(tmp, sizeof(tmp)) == true) {
        /*  num = num * qr  */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&num, &num, &qr));

        /*  num = num ^1 mod p */
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_exp_mod(&num, &num, &unity, &ecp_grp->P, NULL));

        if (wpa3_crypto_mpi_legendre(&num, &ecp_grp->P) == 1) {
            return true;
        }
    } else {
        /*  num = num * qnr  */
        MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&num, &num, &qnr));

        /*  num = num ^1 mod p */
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_exp_mod(&num, &num, &unity, &ecp_grp->P, NULL));

        if (wpa3_crypto_mpi_legendre(&num, &ecp_grp->P) == -1) {
            return true;
        }
    }

    cleanup: mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&num);
    mbedtls_mpi_free(&qr);
    mbedtls_mpi_free(&unity);
    mbedtls_mpi_free(&qnr);
    mbedtls_mpi_free(&r_tmp);
    mbedtls_mpi_free(&vrsqr);
    mbedtls_mpi_free(&ysqr);
    return ret;
}

int wpa3_crypto_point_inverse(const mbedtls_ecp_group *grp,
        mbedtls_ecp_point *R, const mbedtls_ecp_point *P) {
    int ret = 0;

    /* Copy */
    if (R != P) {
        MBEDTLS_MPI_CHK(mbedtls_ecp_copy(R, P));
    }
    /* In-place opposite */
    if (mbedtls_mpi_cmp_int(&R->Y, 0) != ret) {
        MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&R->Y, &grp->P, &R->Y));
    }
    cleanup: return (ret);
}

cy_rslt_t wpa3_crypto_derive_ysqr_from_x(uint8_t *x, uint16_t x_len,
        mbedtls_mpi *ysqr, mbedtls_ecp_group *ecp_grp) {
    mbedtls_mpi tmp;
    mbedtls_mpi ax;
    mbedtls_mpi tmp2;
    mbedtls_mpi tmp3;
    mbedtls_mpi x_tmp;
    mbedtls_mpi x_orig;
    mbedtls_mpi ysqr_tmp;
    cy_rslt_t ret = CY_RSLT_SUCCESS;

    mbedtls_mpi_init(&tmp);
    mbedtls_mpi_init(&x_tmp);
    mbedtls_mpi_init(&tmp2);
    mbedtls_mpi_init(&ax);
    mbedtls_mpi_init(&x_orig);
    mbedtls_mpi_init(&tmp3);
    mbedtls_mpi_init(&ysqr_tmp);

    /*x_tmp = x */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&x_tmp, x, x_len));

    /*x_orig = x */
    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(&x_orig, x, x_len));

    /* tmp =  x * x */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&tmp, &x_tmp, &x_tmp));

    /* tmp3 = tmp * x */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&tmp3, &tmp, &x_orig));

    /*ax  = 3 * x */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&ax, 3));
    MBEDTLS_MPI_CHK(mbedtls_mpi_mul_mpi(&ax, &ax, &x_orig));

    /* tmp2 = tmp3 - ax */
    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(&tmp2, &tmp3, &ax));

    /* ysqr_tmp = tmp2 + b */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&ysqr_tmp, &tmp2, &ecp_grp->B));

    /* ysqr = ysqr_tmp */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(ysqr, &ysqr_tmp));

    cleanup: mbedtls_mpi_free(&tmp);
    mbedtls_mpi_free(&x_tmp);
    mbedtls_mpi_free(&tmp2);
    mbedtls_mpi_free(&ax);
    mbedtls_mpi_free(&tmp3);
    mbedtls_mpi_free(&ysqr_tmp);
    mbedtls_mpi_free(&x_orig);

    return ret;
}

cy_rslt_t wpa3_crypto_derive_y_from_ysqr(mbedtls_ecp_group *ecp_grp,
        mbedtls_mpi *ysqr, mbedtls_mpi *y) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    mbedtls_mpi zexp;
    mbedtls_mpi_init(&zexp);

    /* zexp = p + 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_int(&zexp, &ecp_grp->P, 1));

    /* zexp = (p + 1) /4 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_div_int(&zexp, 0, &zexp, 4));

    /* y =  (y2 ^ ((p+1)/4)) mod p */
    MBEDTLS_MPI_CHK(mbedtls_mpi_exp_mod(y, ysqr, &zexp, &ecp_grp->P, 0));

    cleanup: mbedtls_mpi_free(&zexp);

    return ret;
}

int wpa3_crypto_init(wpa3_supplicant_workspace_t *wksp) {
    mbedtls_ecp_group_id grp_id;
    int ret = 0;

    grp_id = MBEDTLS_ECP_DP_SECP256R1;
    wksp->wpa3_crypto_ctxt = malloc(sizeof(wpa3_crypto_context_info_t));

    if (wksp->wpa3_crypto_ctxt == NULL) {
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }

    memset(wksp->wpa3_crypto_ctxt, 0, sizeof(wpa3_crypto_context_info_t));

    mbedtls_ecp_point_init(&wksp->wpa3_crypto_ctxt->sta_commit_element);
    mbedtls_ecp_point_init(&wksp->wpa3_crypto_ctxt->sta_pt_element);
    mbedtls_mpi_init(&wksp->wpa3_crypto_ctxt->sta_scalar);
    mbedtls_mpi_init(&wksp->wpa3_crypto_ctxt->sta_private);
    mbedtls_ecp_point_init(&wksp->wpa3_crypto_ctxt->pwe);
    mbedtls_mpi_init(&wksp->wpa3_crypto_ctxt->sta_sae_rand);
    wksp->wpa3_crypto_ctxt->group_id = WPA3_SAE_ECP_GROUP_ID;

    mbedtls_ctr_drbg_init(&wksp->wpa3_crypto_ctxt->ctr_drbg);
    mbedtls_entropy_init(&wksp->wpa3_crypto_ctxt->entropy);

    /* Initialize ECP group */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_group_load(&(wksp->wpa3_crypto_ctxt->group), grp_id));

    cleanup: return ret;
}

int wpa3_crypto_deinit(wpa3_supplicant_workspace_t *wksp) {
    int ret = 0;

    if (wksp->wpa3_crypto_ctxt == NULL) {
        return WPA3_EXT_SUPP_RSLT_NO_MEM;
    }
    mbedtls_ecp_point_free(&wksp->wpa3_crypto_ctxt->sta_commit_element);
    mbedtls_ecp_point_free(&wksp->wpa3_crypto_ctxt->sta_pt_element);
    mbedtls_mpi_free(&wksp->wpa3_crypto_ctxt->sta_scalar);
    mbedtls_mpi_free(&wksp->wpa3_crypto_ctxt->sta_private);
    mbedtls_ecp_point_free(&wksp->wpa3_crypto_ctxt->pwe);
    mbedtls_mpi_free(&wksp->wpa3_crypto_ctxt->sta_sae_rand);
    mbedtls_ctr_drbg_free(&wksp->wpa3_crypto_ctxt->ctr_drbg);
    mbedtls_entropy_free(&wksp->wpa3_crypto_ctxt->entropy);
    memset(wksp->wpa3_crypto_ctxt, 0, sizeof(wpa3_crypto_context_info_t));

    if (wksp->wpa3_crypto_ctxt != NULL) {
        free(wksp->wpa3_crypto_ctxt);
    }
    return ret;
}

cy_rslt_t wpa3_crypto_get_rand_qr_qnr(mbedtls_ecp_group* ecp_grp, uint8_t* qr,
        uint8_t* qnr, uint16_t buflen) {
    mbedtls_mpi tmp;
    int ret;
    bool qnr_found = false;
    bool qr_found = false;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    while ((qnr_found == false) || (qr_found == false)) {
        mbedtls_mpi_init(&tmp);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);

        ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                (const unsigned char *) "RANDOM_GEN", 10);
        if (ret != 0) {
            mbedtls_mpi_free(&tmp);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            continue;
        }
        mbedtls_ctr_drbg_set_prediction_resistance(&ctr_drbg,
                MBEDTLS_CTR_DRBG_PR_OFF);

        MBEDTLS_MPI_CHK(
                mbedtls_ecp_gen_privkey(ecp_grp, &tmp, mbedtls_ctr_drbg_random,
                        &ctr_drbg));
        ret = wpa3_crypto_mpi_legendre(&tmp, &ecp_grp->P);

        if ((ret == 1) && (qr_found == false)) {
            qr_found = true;
            MBEDTLS_MPI_CHK(
                    mbedtls_mpi_write_binary(&tmp, (unsigned char * )qr,
                            buflen));
        } else if ((ret == -1) && (qnr_found == false)) {
            qnr_found = true;
            MBEDTLS_MPI_CHK(
                    mbedtls_mpi_write_binary(&tmp, (unsigned char * )qnr,
                            buflen));
        }

        if ((qr_found == true) && (qnr_found == true)) {
            mbedtls_mpi_free(&tmp);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            break;
        }
        mbedtls_mpi_free(&tmp);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
    }

    cleanup: mbedtls_mpi_free(&tmp);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_get_rand(mbedtls_ecp_group *ecp_grp, mbedtls_mpi *r,
        bool order) {
    mbedtls_mpi tmp;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    int cmp_val = 0, ret = 0, rand_tries = 5;
    unsigned char buf[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };

    do {
        mbedtls_mpi_init(&tmp);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);

        ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                (const unsigned char *) "RANDOM_GEN", 10);
        if (ret != 0) {
            mbedtls_mpi_free(&tmp);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
            continue;
        }
        mbedtls_ctr_drbg_set_prediction_resistance(&ctr_drbg,
                MBEDTLS_CTR_DRBG_PR_OFF);

        MBEDTLS_MPI_CHK(
                mbedtls_ecp_gen_privkey(ecp_grp, &tmp, mbedtls_ctr_drbg_random,
                        &ctr_drbg));

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary(&tmp, (unsigned char * )buf,
                        sizeof(buf)));

        if (order == true) {
            /* compared tmp to be less than prime in constant time */
            MBEDTLS_MPI_CHK(
                    mbedtls_mpi_lt_mpi_ct((const mbedtls_mpi * )&tmp,
                            (const mbedtls_mpi * )&(ecp_grp->N),
                            (unsigned * )&cmp_val));
        } else {
            /* compared tmp to be less than prime in constant time */
            MBEDTLS_MPI_CHK(
                    mbedtls_mpi_lt_mpi_ct((const mbedtls_mpi * )&tmp,
                            (const mbedtls_mpi * )&(ecp_grp->P),
                            (unsigned * )&cmp_val));
        }
        if (cmp_val == 1) {
            /* r = buf */
            MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(r, buf, sizeof(buf)));
        }

        rand_tries--;

        mbedtls_mpi_free(&tmp);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);

    } while ((cmp_val != 1) && (rand_tries > 0));

    if (cmp_val != 1) {
        return WPA3_EXT_CRYPTO_ERROR;
    }

    cleanup:

    return ret;
}

cy_rslt_t wpa3_crypto_gen_scalar_and_element(
        wpa3_supplicant_workspace_t* workspace) {
    mbedtls_mpi private;
    mbedtls_mpi mask;
    mbedtls_mpi scalar;
    mbedtls_mpi tmp;

    wpa3_crypto_context_info_t *crypto_ctxt = workspace->wpa3_crypto_ctxt;
    mbedtls_ecp_group *grp = &crypto_ctxt->group;

    cy_rslt_t ret = CY_RSLT_SUCCESS;

    mbedtls_mpi_init(&private);
    mbedtls_mpi_init(&mask);
    mbedtls_mpi_init(&scalar);
    mbedtls_mpi_init(&tmp);

    /* generate private and mask */
    /* 1 < private < q   */
    wpa3_crypto_get_rand(&(workspace->wpa3_crypto_ctxt->group), &private, true);

    /* 1 < mask    < q  */
    wpa3_crypto_get_rand(&(workspace->wpa3_crypto_ctxt->group), &mask, true);

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:private"));
    WPA3_EXT_HEX_MPI_DUMP((&private));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:mask"));
    WPA3_EXT_HEX_MPI_DUMP((&mask));

    /* scalar = (private + mask ) mod q */

    /* tmp = private + mask */
    MBEDTLS_MPI_CHK(mbedtls_mpi_add_mpi(&tmp, &private, &mask));

    /* scalar = tmp mod q */
    MBEDTLS_MPI_CHK(mbedtls_mpi_mod_mpi(&scalar, &tmp, &grp->N));

    /* sta_scalar = scalar */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&crypto_ctxt->sta_scalar, &scalar));

    /* sta_private = private */
    MBEDTLS_MPI_CHK(mbedtls_mpi_copy(&crypto_ctxt->sta_private, &private));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:scalar"));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_scalar));

    /* sta_commit_element = scalar-op(mask, pwe) */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_mul(&crypto_ctxt->group,
                    &crypto_ctxt->sta_commit_element, &mask, &crypto_ctxt->pwe,
                    NULL, NULL));
    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:commit element before inverse"));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.X));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.Y));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.Z));

    /* sta_commit_element = inverse(sta_commit_element) */
    ret = wpa3_crypto_point_inverse(&crypto_ctxt->group,
            &crypto_ctxt->sta_commit_element, &crypto_ctxt->sta_commit_element);
    if (ret != 0) {
        WPA3_EXT_LOG_MSG(
                ( "WPA3-EXT-SUPP:wpa3_crypto_point_inverse failed: ret=%ld\n", ret ));
        goto cleanup;
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:commit element"));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.X));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.Y));
    WPA3_EXT_HEX_MPI_DUMP((&crypto_ctxt->sta_commit_element.Z));

    /* check if the point is valid on the ECP curve */
    ret = mbedtls_ecp_check_pubkey(&crypto_ctxt->group,
            &crypto_ctxt->sta_commit_element);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("\n WPA3-EXT-SUPP:Commit Element  not valid point on curve ret=%ld\n", ret));
    } else {
        WPA3_EXT_LOG_MSG(
                ("\n*** WPA3-EXT-SUPP:Commit Element is a VALID point on ECP CURVE ret=%ld ***\n", ret));
    }

    cleanup:

    mbedtls_mpi_free(&private);
    mbedtls_mpi_free(&mask);
    mbedtls_mpi_free(&scalar);
    mbedtls_mpi_free(&tmp);
    return ret;
}

cy_rslt_t wpa3_crypto_get_grp_id_scalar_element(
        wpa3_supplicant_workspace_t* workspace, uint8_t * buf) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    int len = 0;

    if (workspace != NULL) {
        memcpy(buf, &(workspace->wpa3_crypto_ctxt->group_id),
                sizeof(workspace->wpa3_crypto_ctxt->group_id));
        len += sizeof(workspace->wpa3_crypto_ctxt->group_id);

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_scalar), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN ));
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. own Scalar scalar dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_commit_element.X), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN));
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:STA Element dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_commit_element.Y), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. own Element len=%d\n", len));
    }
    cleanup:
    return ret;
}

cy_rslt_t wpa3_crypto_get_grp_id(wpa3_supplicant_workspace_t* workspace,
        uint8_t * buf) {
    int len = 0;

    if (workspace != NULL) {
        memcpy(buf, &(workspace->wpa3_crypto_ctxt->group_id),
                sizeof(workspace->wpa3_crypto_ctxt->group_id));
        len += sizeof(workspace->wpa3_crypto_ctxt->group_id);
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_get_scalar_element(wpa3_supplicant_workspace_t* workspace,
        uint8_t * buf) {
    cy_rslt_t ret = CY_RSLT_SUCCESS;
    int len = 0;

    if (workspace != NULL) {
        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_scalar), (unsigned char *)buf, WPA3_SAE_SCALAR_LEN ));
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. own Scalar scalar dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_commit_element.X), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN));
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:STA Element dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_write_binary( &(workspace->wpa3_crypto_ctxt->sta_commit_element.Y), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. own Element len=%d\n", len));
    }
    cleanup:
    return ret;
}

cy_rslt_t wpa3_crypto_chk_own_peer_scalar_element(wpa3_supplicant_workspace_t* workspace)
{
    mbedtls_mpi scalar_one;
    int ret = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    mbedtls_mpi_init(&scalar_one);

    /* Scalar range check 1 < scalar < q */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&scalar_one, 1));

    ret = mbedtls_mpi_cmp_mpi(
            &(workspace->wpa3_sae_context_info.peer_commit_scalar),
            &scalar_one);

    /* scalar greater than 1*/
    if (ret == 1) {

        ret = mbedtls_mpi_cmp_mpi(
                &(workspace->wpa3_sae_context_info.peer_commit_scalar),
                &workspace->wpa3_crypto_ctxt->group.N);

        /* scalar less than order */
        if (ret == -1) {
            ret = mbedtls_mpi_cmp_mpi(
                    &(workspace->wpa3_crypto_ctxt->sta_scalar),
                    &(workspace->wpa3_sae_context_info.peer_commit_scalar));

            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:mbedtls_mpi_cmp_mpi of sta_scalar and peer commit scalar ret=%d \n", ret));
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:sta_scalar"));
            WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_crypto_ctxt->sta_scalar)));
            WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:peer_commit_scalar"));
            WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_sae_context_info.peer_commit_scalar)));

            /* if scalar is different check element */
            if (ret != 0) {

                ret = mbedtls_ecp_point_cmp(
                                &(workspace->wpa3_crypto_ctxt->sta_commit_element),
                                &(workspace->wpa3_sae_context_info.peer_commit_element));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:mbedtls_ecp_point_cmp of sta_commit_element and peer_commit_element ret=%d \n", ret));

                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:sta_commit_element X"));
                WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_crypto_ctxt->sta_commit_element.X)));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:sta_commit_element Y"));
                WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_crypto_ctxt->sta_commit_element.Y)));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:peer_commit_element X"));
                WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_sae_context_info.peer_commit_element.X)));
                WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:peer_commit_element Y"));
                WPA3_EXT_HEX_MPI_DUMP((&(workspace->wpa3_sae_context_info.peer_commit_element.Y)));

            } else {
                /* scalar is same as peer scalar  */
                result = WPA3_EXT_SUPP_RSLT_SCALAR_ELEMENT_RANGE_ERROR;
            }

            if (ret == 0) {
                /* element is same as peer element */
                result = WPA3_EXT_SUPP_RSLT_SCALAR_ELEMENT_RANGE_ERROR;
            }
        } else {
            /* scalar greater than or equal to order of the curve */
            result = WPA3_EXT_SUPP_RSLT_SCALAR_ELEMENT_RANGE_ERROR;
        }
    } else {
        /* scalar less than 1 */
        result = WPA3_EXT_SUPP_RSLT_SCALAR_ELEMENT_RANGE_ERROR;
    }

    cleanup: mbedtls_mpi_free(&scalar_one);
    return result;
}

cy_rslt_t wpa3_crypto_compute_shared_secret(
        wpa3_supplicant_workspace_t *workspace, uint8_t *k) {
    cy_rslt_t result = CY_RSLT_SUCCESS;
    mbedtls_ecp_point ecp_point;
    wpa3_crypto_context_info_t *crypto_ctxt;
    mbedtls_mpi m;
    int ret;

    if (workspace == NULL) {
        return WPA3_EXT_SUPP_ERROR;
    }
    crypto_ctxt = (wpa3_crypto_context_info_t *) workspace->wpa3_crypto_ctxt;

    mbedtls_mpi_init(&m);
    mbedtls_ecp_point_init(&ecp_point);

    /* set m = 1 */
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&m, 1));

    /*  K = F(scalar-op(private, element-op(peer-Element, scalar-op(peer-scalar, PE)))) */
    /*  If K is identity element (point-at-infinity) then reject */
    /*  k = F(K) (= x coordinate) */

    /* ecp_point = scalar-op(peer-scalar, PE) */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_mul(&crypto_ctxt->group, &ecp_point,
                    &(workspace->wpa3_sae_context_info.peer_commit_scalar),
                    &(crypto_ctxt->pwe), NULL, NULL));

    /* temp_point =  m * (peer-Element) +  m * (ecp_point) */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_muladd(&crypto_ctxt->group, &ecp_point, &m,
                    (const mbedtls_ecp_point * )&(workspace->wpa3_sae_context_info.peer_commit_element),
                    &m, (const mbedtls_ecp_point * )&ecp_point));

    /* ecp_point = scalar-op(private, temp_point) */
    MBEDTLS_MPI_CHK(
            mbedtls_ecp_mul(&crypto_ctxt->group, &ecp_point,
                    &(workspace->wpa3_crypto_ctxt->sta_private), &ecp_point,
                    NULL, NULL));

    /* is point at infinity  check  */
    ret = mbedtls_ecp_is_zero(&ecp_point);

    /* k = F(K) (= x coordinate) */
    if (ret == 1) {
        /* point is at infinity */
        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:computed K is at infinity \n"));
        return WPA3_EXT_CRYPTO_ERROR;
    }

    /* copy the ecp_point to buffer */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary( &(ecp_point.X), (unsigned char *)k, WPA3_SAE_SCALAR_LEN));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:Dump of K \n"));
    WPA3_EXT_HEX_MPI_DUMP((&ecp_point.X));
    WPA3_EXT_HEX_BUF_DUMP((k, WPA3_SAE_KEYSEED_KEY_LEN));

    /*  kck | mk = KDF-n(k, "SAE KCK and PMK") */
    /*  confirm = H(kck | scalar | peer-scalar | Element | Peer-Element | <sender-id>) */

    cleanup: mbedtls_ecp_point_free(&ecp_point);
    mbedtls_mpi_free(&m);

    return result;
}

cy_rslt_t wpa3_crypto_derive_kck_pmk(wpa3_supplicant_workspace_t *workspace,
        uint8_t *k) {
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t zero_key[WPA3_SAE_KEYSEED_KEY_LEN];
    uint8_t keyseed[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t value[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t kckpmk[WPA3_KCK_PMK_LEN] = { 0 };
    mbedtls_mpi temp;
    int ret = 0;
    size_t key_len = WPA3_SAE_KEYSEED_KEY_LEN;
    uint8_t num_elem = 1;

    mbedtls_mpi_init(&temp);

    WPA3_EXT_LOG_MSG(
            ("\nWPA3-EXT-SUPP:wpa3_crypto_derive_kck_pmk() Dump of K \n"));
    WPA3_EXT_HEX_BUF_DUMP((k, WPA3_SAE_KEYSEED_KEY_LEN));
    /* keyseed = H(<0>32, k)
     * KCK + PMK = KDF-512(keyseed, "SAE KCK and PMK", (commit-scalar + peer-commit-scalar) mod r)
     * PMKID = L(( commit-scalar + peer-commit-scalar) mod r, 0, 128)
     */

    memset(zero_key, 0, sizeof(zero_key));
    ret = wpa3_crypto_hmac_sha256(zero_key, WPA3_SAE_KEYSEED_KEY_LEN, num_elem,
            &k, &key_len, keyseed);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_derive_kck_pmk -->wpa3_supplicant_hmac_sha256 failed ret = %d\n", ret));
    }

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:keyseed dump\n "));
    WPA3_EXT_HEX_BUF_DUMP((keyseed, WPA3_SAE_KEYSEED_KEY_LEN));

    /* tmp = commit-scalar + peer-commit-scalar */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_add_mpi(&temp,
                    &(workspace->wpa3_crypto_ctxt->sta_scalar),
                    &(workspace->wpa3_sae_context_info.peer_commit_scalar)));

    /* tmp = tmp mod q */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_mod_mpi(&temp, &temp,
                    &(workspace->wpa3_crypto_ctxt->group.N)));

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&temp, (unsigned char * )value,
                    sizeof(value)));

    WPA3_EXT_LOG_MSG((" WPA3-EXT-SUPP:SAE: PMKID dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((value, WPA3_SAE_KEYSEED_KEY_LEN));

    WPA3_EXT_LOG_MSG(
            (" WPA3-EXT-SUPP:SAE: calling wpa3_crypto_hmac_sha256_kdf KCK!!!\n"));

    ret = wpa3_crypto_hmac_sha256_kdf_bits(keyseed, sizeof(keyseed),
            "SAE KCK and PMK", value, WPA3_SAE_KEYSEED_KEY_LEN, kckpmk,
            WPA3_KCK_PMK_LEN_BITS);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_supplicant_hmac_sha256_kdf failed ret = %d\n", ret));
        result = WPA3_EXT_CRYPTO_ERROR;
        goto cleanup;
    }
    memset(keyseed, 0, sizeof(keyseed));

    memcpy(workspace->wpa3_sae_context_info.kck, kckpmk, WPA3_SAE_KCK_LEN);
    memcpy(workspace->wpa3_sae_context_info.pmk, &kckpmk[WPA3_SAE_KCK_LEN],
            WPA3_SAE_PMK_LEN);
    memcpy(workspace->wpa3_sae_context_info.pmkid, value, WPA3_SAE_PMKID_LEN);

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP: SAE: PMKID 0-128 dump\n"));
    WPA3_EXT_HEX_BUF_DUMP(
            (workspace->wpa3_sae_context_info.pmkid, WPA3_SAE_PMKID_LEN));

    memset(kckpmk, 0, sizeof(kckpmk));
    memset(value, 0, sizeof(value));

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE: KCK dump\n"));
    WPA3_EXT_HEX_BUF_DUMP(
            (workspace->wpa3_sae_context_info.kck, WPA3_SAE_KCK_LEN));

    WPA3_EXT_LOG_MSG(("WPA3-EXT-SUPP:SAE: PMK dump\n"));
    WPA3_EXT_HEX_BUF_DUMP(
            (workspace->wpa3_sae_context_info.pmk, WPA3_SAE_PMK_LEN));

    cleanup: mbedtls_mpi_free(&temp);
    return result;
}

cy_rslt_t wpa3_crypto_get_peer_grp_id_scalar_element(
        wpa3_supplicant_workspace_t* workspace, uint8_t *buf) {
    int ret;
    int len = 0;
    uint8_t ap_scalar_element_buf[WPA3_SAE_KEYSEED_KEY_LEN * 2 + 1] = { 0 };
    uint8_t ap_scalar_element_buf_len = 0;

    if (workspace != NULL) {
        memcpy(&(workspace->wpa3_sae_context_info.peer_group_id), buf,
                sizeof(workspace->wpa3_sae_context_info.peer_group_id));
        len += sizeof(workspace->wpa3_sae_context_info.peer_group_id);
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:wpa3_crypto_get_peer_grp_id_scalar_element() length of group id len=%d\n", len));

        MBEDTLS_MPI_CHK(
                mbedtls_mpi_read_binary( &(workspace->wpa3_sae_context_info.peer_commit_scalar), (unsigned char *)&buf[len], WPA3_SAE_SCALAR_LEN ));
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. peer scalar scalar dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_SCALAR_LEN));
        len += WPA3_SAE_SCALAR_LEN;

        /* read x and y into ecp point ap_scalar_element_buf */
        ap_scalar_element_buf[0] = 0x04; /* UNCOMPRESSED POINT */
        ap_scalar_element_buf_len++;
        memcpy(&ap_scalar_element_buf[ap_scalar_element_buf_len], &buf[len],
                WPA3_SAE_SCALAR_LEN);
        ap_scalar_element_buf_len += WPA3_SAE_SCALAR_LEN;
        len += WPA3_SAE_SCALAR_LEN;
        memcpy(&ap_scalar_element_buf[ap_scalar_element_buf_len], &buf[len],
                WPA3_SAE_SCALAR_LEN);
        len += WPA3_SAE_SCALAR_LEN;
        ap_scalar_element_buf_len += WPA3_SAE_SCALAR_LEN;
        ret = mbedtls_ecp_point_read_binary(
                &(workspace->wpa3_crypto_ctxt->group),
                &(workspace->wpa3_sae_context_info.peer_commit_element),
                (const unsigned char *) ap_scalar_element_buf,
                ap_scalar_element_buf_len);

        if (ret != CY_RSLT_SUCCESS) {
            WPA3_EXT_LOG_MSG(
                    ("WPA3-EXT-SUPP:mbedtls_ecp_point_read_binary of AP element failed ret=%d\n", ret));
        }

        WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:xyz point of peer element"));
        WPA3_EXT_HEX_MPI_DUMP(
                (&(workspace->wpa3_sae_context_info.peer_commit_element.X)));
        WPA3_EXT_HEX_MPI_DUMP(
                (&(workspace->wpa3_sae_context_info.peer_commit_element.Y)));
        WPA3_EXT_HEX_MPI_DUMP(
                (&(workspace->wpa3_sae_context_info.peer_commit_element.Z)));

        /* check if the point is valid on the ECP curve */
        ret = mbedtls_ecp_check_pubkey(&(workspace->wpa3_crypto_ctxt->group),
                &(workspace->wpa3_sae_context_info.peer_commit_element));

        if (ret != CY_RSLT_SUCCESS) {
            WPA3_EXT_LOG_MSG(
                    ("\n** WPA3-EXT-SUPP:peer Element xyz point not valid on curve ret=%d\n", ret));
            return WPA3_EXT_CRYPTO_ERROR;
        } else {
            WPA3_EXT_LOG_MSG(
                    ("\n** WPA3-EXT-SUPP:peer Element xyz point VALID on ECP CURVE ret=%d **\n", ret));
        }
    }
    cleanup: return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_get_send_confirm(wpa3_supplicant_workspace_t* workspace,
        uint8_t *buf) {
    int len = 0;

    if (workspace != NULL) {
        memcpy(&(workspace->wpa3_sae_context_info.rc), buf,
                sizeof(workspace->wpa3_sae_context_info.rc));
        len += sizeof(workspace->wpa3_sae_context_info.rc);
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:wpa3_crypto_get_send_confirm() length of receive confirm id len=%d peer-send-confirm=%d\n", len, workspace->wpa3_sae_context_info.rc));

        memcpy(workspace->wpa3_sae_context_info.peer_confirm,
                (unsigned char *) &buf[len], WPA3_SAE_CONFIRM_LEN);
        WPA3_EXT_LOG_MSG(
                ("\nWPA3-EXT-SUPP:WPA3_EXT_HEX_BUF_DUMP .. peer confirm dump\n"));
        WPA3_EXT_HEX_BUF_DUMP((&buf[len], WPA3_SAE_CONFIRM_LEN));
        len += WPA3_SAE_CONFIRM_LEN;
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_build_send_confirm_handshake(
        wpa3_supplicant_workspace_t * wksp, uint8_t *buffer) {
    /* confirm = HMAC-SHA256(KCK, send-confirm, commit-scalar, COMMIT-ELEMENT,
     *              peer-commit-scalar, PEER-COMMIT-ELEMENT)
     * verifier = HMAC-SHA256(KCK, peer-send-confirm, peer-commit-scalar,
     *               PEER-COMMIT-ELEMENT, commit-scalar, COMMIT-ELEMENT)
     */

    int ret = 0;
    uint8_t confirm_data[WPA3_SAE_CONFIRM_DATA_MSG_LEN] = { 0 };
    uint8_t confirm[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };

    uint8_t *addr[5];
    size_t len[5];
    uint8_t scalar_b1[WPA3_SAE_SCALAR_LEN], scalar_b2[WPA3_SAE_SCALAR_LEN];
    uint8_t element1[WPA3_SAE_ELEMENT_LEN];
    uint8_t element2[WPA3_SAE_ELEMENT_LEN];

    /* confirm = HMAC-SHA256(KCK, send-confirm, commit-scalar, COMMIT-ELEMENT, */
    /*                      peer-commit-scalar, PEER-COMMIT-ELEMENT) */

    WPA3_EXT_LOG_MSG(
            ("\n** WPA3-EXT-SUPP:wpa3_crypto_build_send_confirm_handshake **\n"));

    /* Confirm
     * CN(key, X, Y, Z, ...) =
     *    HMAC-SHA256(key, D2OS(X) || D2OS(Y) || D2OS(Z) | ...)
     * confirm = CN(KCK, send-confirm, commit-scalar, COMMIT-ELEMENT,
     *              peer-commit-scalar, PEER-COMMIT-ELEMENT)
     * verifier = CN(KCK, peer-send-confirm, peer-commit-scalar,
     *               PEER-COMMIT-ELEMENT, commit-scalar, COMMIT-ELEMENT)
     */
    addr[0] = (uint8_t *) &(wksp->wpa3_sae_context_info.sc);
    len[0] = 2;

    WPA3_EXT_LOG_MSG(
            ("\nWPA3-EXT-SUPP: send confirm =%d\n", wksp->wpa3_sae_context_info.sc));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_scalar), scalar_b1, WPA3_SAE_SCALAR_LEN));
    addr[1] = scalar_b1;
    len[1] = WPA3_SAE_SCALAR_LEN;

    /* commit-element */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_commit_element.X), (unsigned char *)element1, WPA3_SAE_SCALAR_LEN));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_commit_element.Y), (unsigned char *)&element1[WPA3_SAE_SCALAR_LEN], WPA3_SAE_SCALAR_LEN));

    addr[2] = element1;
    len[2] = WPA3_SAE_ELEMENT_LEN;

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_scalar), scalar_b2, WPA3_SAE_SCALAR_LEN));
    addr[3] = scalar_b2;
    len[3] = WPA3_SAE_SCALAR_LEN;

    /* commit-element */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_element.X), (unsigned char *)element2, WPA3_SAE_SCALAR_LEN));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_element.Y), (unsigned char *)&element2[WPA3_SAE_SCALAR_LEN], WPA3_SAE_SCALAR_LEN));

    addr[4] = element2;
    len[4] = WPA3_SAE_ELEMENT_LEN;
    wpa3_crypto_hmac_sha256(wksp->wpa3_sae_context_info.kck, WPA3_SAE_KCK_LEN,
            5, addr, len, confirm);

    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_build_send_confirm_handshake failed ret = %d\n", ret));
    }

    WPA3_EXT_HEX_BUF_DUMP((confirm, WPA3_SAE_CONFIRM_LEN));

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:confirm MSG dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((confirm, WPA3_SAE_CONFIRM_LEN));
    memcpy(buffer, confirm, WPA3_SAE_CONFIRM_LEN);
    memcpy(wksp->wpa3_crypto_ctxt->confirm, confirm, WPA3_SAE_CONFIRM_LEN);

    memset(confirm, 0, sizeof(confirm));
    memset(confirm_data, 0, sizeof(confirm_data));

    cleanup: return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_verify_confirm_message(wpa3_supplicant_workspace_t *wksp) {
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint8_t verify[WPA3_SAE_KEYSEED_KEY_LEN] = { 0 };
    uint8_t temp[WPA3_SAE_CONFIRM_DATA_MSG_LEN] = { 0 };
    int ret = 0;

    uint8_t *addr[5];
    size_t len[5];
    uint8_t scalar_b1[WPA3_SAE_SCALAR_LEN], scalar_b2[WPA3_SAE_SCALAR_LEN];
    uint8_t element1[WPA3_SAE_ELEMENT_LEN];
    uint8_t element2[WPA3_SAE_ELEMENT_LEN];

    /* verifier = CN(KCK, peer-send-confirm, peer-commit-scalar,
     *      PEER-COMMIT-ELEMENT, commit-scalar, COMMIT-ELEMENT)
     */

    /* Copy the peer-send-confirm */
    addr[0] = (uint8_t *) &(wksp->wpa3_sae_context_info.rc);
    len[0] = 2;

    WPA3_EXT_LOG_MSG(
            ("\nWPA3-EXT-SUPP:peer send confirm =%d\n", wksp->wpa3_sae_context_info.rc));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_scalar), scalar_b1, WPA3_SAE_SCALAR_LEN));
    addr[1] = scalar_b1;
    len[1] = WPA3_SAE_SCALAR_LEN;

    /* commit-element */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_element.X), (unsigned char *)element1, WPA3_SAE_SCALAR_LEN));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_sae_context_info.peer_commit_element.Y), (unsigned char *)&element1[WPA3_SAE_SCALAR_LEN], WPA3_SAE_SCALAR_LEN));

    addr[2] = element1;
    len[2] = WPA3_SAE_ELEMENT_LEN;

    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_scalar), scalar_b2, WPA3_SAE_SCALAR_LEN));
    addr[3] = scalar_b2;
    len[3] = WPA3_SAE_SCALAR_LEN;

    /* commit-element */
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_commit_element.X), (unsigned char *)element2, WPA3_SAE_SCALAR_LEN));
    MBEDTLS_MPI_CHK(
            mbedtls_mpi_write_binary(&(wksp->wpa3_crypto_ctxt->sta_commit_element.Y), (unsigned char *)&element2[WPA3_SAE_SCALAR_LEN], WPA3_SAE_SCALAR_LEN));

    addr[4] = element2;
    len[4] = WPA3_SAE_ELEMENT_LEN;
    wpa3_crypto_hmac_sha256(wksp->wpa3_sae_context_info.kck, WPA3_SAE_KCK_LEN,
            5, addr, len, verify);
    if (ret != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:wpa3_crypto_build_send_confirm_handshake failed ret = %d\n", ret));
    }

    WPA3_EXT_LOG_MSG(("\nWPA3-EXT-SUPP:verify dump\n"));
    WPA3_EXT_HEX_BUF_DUMP((verify, WPA3_SAE_CONFIRM_LEN));

    result = memcmp(wksp->wpa3_sae_context_info.peer_confirm, verify,
            WPA3_SAE_CONFIRM_LEN);

    memset(verify, 0, sizeof(verify));
    memset(temp, 0, sizeof(temp));

    if (result != CY_RSLT_SUCCESS) {
        WPA3_EXT_LOG_MSG(
                ("WPA3-EXT-SUPP:confirm verify failed result = %ld\n", result));
        return WPA3_EXT_SUPP_CONFIRM_VERIFY_FAILURE;
    }

    cleanup: return result;
}

cy_rslt_t wpa3_crypto_get_send_confirm_handshake(
        wpa3_supplicant_workspace_t* workspace, uint8_t * buf) {
    cy_rslt_t result = CY_RSLT_SUCCESS;
    int len = 0;

    if (workspace != NULL) {
        memcpy(buf, &(workspace->wpa3_sae_context_info.sc),
                sizeof(workspace->wpa3_sae_context_info.sc));
        len += sizeof(uint16_t);

        result = wpa3_crypto_build_send_confirm_handshake(workspace, &buf[len]);
        if (result != CY_RSLT_SUCCESS) {
            WPA3_EXT_LOG_MSG(
                    ("\nWPA3-EXT-SUPP:wpa3_crypto_build_send_confirm_handshake failed result=%ld\n", result));
        }
    }
    return CY_RSLT_SUCCESS;
}

cy_rslt_t wpa3_crypto_hmac_sha256(uint8_t *key, size_t key_len, size_t num_elem,
        uint8_t *addr[], size_t *len, uint8_t *mac) {
    size_t i;
    mbedtls_md_info_t *md_info;
    mbedtls_md_context_t md_ctx;
    int ret = CY_RSLT_SUCCESS;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&md_ctx);

    md_info = (mbedtls_md_info_t *) mbedtls_md_info_from_type(md_type);
    if (!md_info) {
        return WPA3_EXT_CRYPTO_ERROR;
    }

    ret = mbedtls_md_setup(&md_ctx, md_info, 1);
    if (ret != CY_RSLT_SUCCESS) {
        return ret;
    }

    mbedtls_md_hmac_starts(&md_ctx, key, key_len);

    for (i = 0; i < num_elem; i++) {
        mbedtls_md_hmac_update(&md_ctx, addr[i], len[i]);
    }

    mbedtls_md_hmac_finish(&md_ctx, mac);

    mbedtls_md_free(&md_ctx);

    return ret;
}

int wpa3_crypto_hmac_sha256_kdf_bits(uint8_t *key, size_t key_len,
        const char *label, uint8_t *data, size_t data_len, uint8_t *buf,
        size_t buf_len_bits) {
    uint16_t counter = 1;
    size_t pos, plen;
    uint8_t hash[WPA3_ECP_GROUP_P256R1_PRIME_LEN];
    uint8_t *addr[4];
    size_t len[4];
    uint8_t counter_le[2], length_le[2];
    size_t buf_len = (buf_len_bits + 7) / 8;

    addr[0] = counter_le;
    len[0] = 2;
    addr[1] = (uint8_t *) label;
    len[1] = strlen(label);
    addr[2] = data;
    len[2] = data_len;
    addr[3] = length_le;
    len[3] = sizeof(length_le);

    length_le[1] = buf_len_bits >> 8;
    length_le[0] = buf_len_bits & 0xff;

    pos = 0;
    while (pos < buf_len) {
        plen = buf_len - pos;
        counter_le[1] = counter >> 8;
        counter_le[0] = counter & 0xff;
        if (plen >= WPA3_ECP_GROUP_P256R1_PRIME_LEN) {
            if (wpa3_crypto_hmac_sha256(key, key_len, 4, addr, len,
                    &buf[pos]) != CY_RSLT_SUCCESS) {
                return WPA3_EXT_CRYPTO_ERROR;
            }
            pos += WPA3_ECP_GROUP_P256R1_PRIME_LEN;
        } else {
            if (wpa3_crypto_hmac_sha256(key, key_len, 4, addr, len,
                    hash) != CY_RSLT_SUCCESS) {
                return WPA3_EXT_CRYPTO_ERROR;
            }
            memcpy(&buf[pos], hash, plen);
            pos += plen;
            break;
        }
        counter++;
    }

    /*
     * Mask out unused bits in the last octet if it does not use all the
     * bits.
     */
    if (buf_len_bits % 8) {
        uint8_t mask = 0xff << (8 - buf_len_bits % 8);
        buf[pos - 1] &= mask;
    }

    memset(hash, 0, sizeof(hash));

    return 0;
}

CYPRESS_WEAK cy_rslt_t cy_wpa3_get_pfn_network( uint8_t * ssid, uint8_t *passphrase, uint8_t *pt )
{
    return WPA3_EXT_CRYPTO_ERROR;
}
