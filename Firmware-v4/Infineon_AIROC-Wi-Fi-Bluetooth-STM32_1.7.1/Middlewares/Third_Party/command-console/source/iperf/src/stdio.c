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

/*---------------------------------------------------------------
 * Copyright (c) 1999,2000,2001,2002,2003
 * The Board of Trustees of the University of Illinois
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software (Iperf) and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the names of the University of Illinois, NCSA,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 * National Laboratory for Applied Network Research
 * National Center for Supercomputing Applications
 * University of Illinois at Urbana-Champaign
 * http://www.ncsa.uiuc.edu
 * ________________________________________________________________
 *
 * stdio.c
 * by Mark Gates <mgates@nlanr.net>
 * and Ajay Tirumalla <tirumala@ncsa.uiuc.edu>
 * -------------------------------------------------------------------
 * input and output numbers, converting with kilo, mega, giga
 * ------------------------------------------------------------------- */

/* IPERF_MODIFIED Start */
/* As build system looks for all the header files with the name and adding include paths. It includes mutex.h present in some other component
 * Hence files are renamed by appending iperf.
 */
#include "iperf_util.h"
/* IPERF_MODIFIED End */
#include "headers.h"

#ifdef __cplusplus
extern "C" {
#endif

const long kKilo_to_Unit = 1024;
const long kMega_to_Unit = 1024 * 1024;
const long kGiga_to_Unit = 1024 * 1024 * 1024;

const long kkilo_to_Unit = 1000;
const long kmega_to_Unit = 1000 * 1000;
const long kgiga_to_Unit = 1000 * 1000 * 1000;

/* -------------------------------------------------------------------
 * byte_atof
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */

double byte_atof( const char *inString ) {
    /* IPERF_MODIFIED Start */
    double theNum = 0.0;
    /* IPERF_MODIFIED End */

    assert( inString != NULL );

    /* scan the number and any suffices */
    /* IPERF_MODIFIED Start */
    sscanf( inString, "%lf", &theNum );
    /* IPERF_MODIFIED End */

    /* convert according to [Gg Mm Kk] */
    /* IPERF_MODIFIED Start */
    while ( *inString != '\0' ) {
        switch ( *inString ) {
            case 'G':  return theNum * kGiga_to_Unit;
            case 'M':  return theNum * kMega_to_Unit;
            case 'K':  return theNum * kKilo_to_Unit;
            case 'g':  return theNum * kgiga_to_Unit;
            case 'm':  return theNum * kmega_to_Unit;
            case 'k':  return theNum * kkilo_to_Unit;
            default: break;
        }
        inString++;
    }
    /* IPERF_MODIFIED End */
    return theNum;
} /* end byte_atof */

/* -------------------------------------------------------------------
 * byte_atoi
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */
intmax_t byte_atoi( const char *inString ) {
	double theNum = 0.0;

    assert( inString != NULL );

    /* scan the number and any suffices */
    /* IPERF_MODIFIED Start */
    sscanf( inString, "%lf", &theNum );
    /* IPERF_MODIFIED End */

    /* convert according to [Gg Mm Kk] */
    /* IPERF_MODIFIED Start */
    while ( *inString != '\0' ) {
        switch ( *inString ) {
            case 'G':  return (max_size_t)( theNum * kGiga_to_Unit );
            case 'M':  return (max_size_t)( theNum * kMega_to_Unit );
            case 'K':  return (max_size_t)( theNum * kKilo_to_Unit );
            case 'g':  return (max_size_t)( theNum * kgiga_to_Unit );
            case 'm':  return (max_size_t)( theNum * kmega_to_Unit );
            case 'k':  return (max_size_t)( theNum * kkilo_to_Unit );
            default: break;
        }
        inString++;
    /* IPERF_MODIFIED End */
    }

    return (intmax_t) theNum;
} /* end byte_atof */

/* -------------------------------------------------------------------
 * bitorbyte_atoi
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */
unsigned int bitorbyte_atoi( const char *inString ) {
    double theNum;
    char suffix = '\0';

    assert( inString != NULL );

    /* scan the number and any suffices */
    sscanf( inString, "%lf%c", &theNum, &suffix );

    /* convert according to [Gg Mm Kk] */
    switch ( suffix ) {
    case 'G':  theNum *= (kgiga_to_Unit * 8.0);  break;
    case 'M':  theNum *= (kmega_to_Unit * 8.0);  break;
    case 'K':  theNum *= (kkilo_to_Unit * 8.0);  break;
    case 'g':  theNum *= kgiga_to_Unit;  break;
    case 'm':  theNum *= kmega_to_Unit;  break;
    case 'k':  theNum *= kkilo_to_Unit;  break;
    default: break;
    }
    return (unsigned int) theNum;
} /* end byte_atof */

/* -------------------------------------------------------------------
 * bitorbyte_atof
 *
 * Given a string of form #x where # is a number and x is a format
 * character listed below, this returns the interpreted integer.
 * Gg, Mm, Kk are giga, mega, kilo respectively
 * ------------------------------------------------------------------- */
double bitorbyte_atof( const char *inString ) {
    double theNum;
    char suffix = '\0';

    assert( inString != NULL );

    /* scan the number and any suffices */
    sscanf( inString, "%lf%c", &theNum, &suffix );

    /* convert according to [Gg Mm Kk] */
    switch ( suffix ) {
    case 'G':  theNum *= (kgiga_to_Unit * 8.0);  break;
    case 'M':  theNum *= (kmega_to_Unit * 8.0);  break;
    case 'K':  theNum *= (kkilo_to_Unit * 8.0);  break;
    case 'g':  theNum *= kgiga_to_Unit;  break;
    case 'm':  theNum *= kmega_to_Unit;  break;
    case 'k':  theNum *= kkilo_to_Unit;  break;
    default: break;
    }
    return theNum;
} /* end byte_atof */

/* -------------------------------------------------------------------
 * constants for byte_printf
 * ------------------------------------------------------------------- */

/* used as indices into kConversion[], kLabel_Byte[], and kLabel_bit[] */
enum {
    kConv_Unit,
    kConv_Kilo,
    kConv_Mega,
    kConv_Giga,
    kConv_Tera,
    kConv_Peta
};

/* factor to multiply the number by */
const double kConversion[] =
{
    1.0,                                    /* unit */
    1.0 / 1024,                             /* kilo */
    1.0 / 1024 / 1024,                      /* mega */
    1.0 / 1024 / 1024 / 1024,               /* giga */
    1.0 / 1024 / 1024 / 1024 / 1024,        /* tera */
    1.0 / 1024 / 1024 / 1024 / 1024 / 1024  /* peta */
};

/* factor to multiply the number by for bits*/
const double kConversionForBits[] =
{
    1.0,                                    /* unit */
    1.0 / 1000,                             /* kilo */
    1.0 / 1000 / 1000,                      /* mega */
    1.0 / 1000 / 1000 / 1000,               /* giga */
    1.0 / 1000 / 1000 / 1000 / 1000,        /* tera */
    1.0 / 1000 / 1000 / 1000 / 1000/ 1000   /* peta */
};


/* labels for Byte formats [KMG] */
const char* kLabel_Byte[] =
{
    "Byte",
    "KByte",
    "MByte",
    "GByte",
    "TByte",
    "PByte"
};

/* labels for bit formats [kmg] */
const char* kLabel_bit[]  =
{
    "bit",
    "Kbit",
    "Mbit",
    "Gbit",
    "Tbit",
    "Pbit"
};

/* -------------------------------------------------------------------
 * byte_snprintf
 *
 * Given a number in bytes and a format, converts the number and
 * prints it out with a bits or bytes label.
 *   B, K, M, G, A, P, T for Byte, Kbyte, Mbyte, Gbyte, Tbyte, Pbyte adaptive byte
 *   b, k, m, g, a, p, t for bit,  Kbit,  Mbit,  Gbit, Tbit, Pbit, adaptive bit
 * adaptive picks the "best" one based on the number.
 * outString should be at least 11 chars long
 * (4 digits + space + 5 chars max + null)
 * ------------------------------------------------------------------- */

void byte_snprintf( char* outString, int inLen,
                    double inNum, char inFormat ) {
    int conv = 0;
    const char* suffix;
    const char* format;

    /* convert to bits for [bkmga] */
    if ( ! isupper( (int)inFormat ) ) {
        inNum *= 8;
    }

    switch ( toupper( (int)inFormat) ) {
        case 'B': conv = kConv_Unit; break;
        case 'K': conv = kConv_Kilo; break;
        case 'M': conv = kConv_Mega; break;
        case 'G': conv = kConv_Giga; break;
        case 'T': conv = kConv_Tera; break;
        case 'P': conv = kConv_Peta; break;

        default:
        case 'A': {
	    double tmpNum = inNum;
	    conv = kConv_Unit;

	    if ( isupper((int)inFormat) ) {
		while ( tmpNum >= 1024.0  &&  conv < kConv_Peta ) {
		    tmpNum /= 1024.0;
		    conv++;
		}
	    } else {
		while ( tmpNum >= 1000.0  &&  conv < kConv_Peta ) {
		    tmpNum /= 1000.0;
		    conv++;
		}
	    }
	    break;
	}
    }

    if ( ! isupper ((int)inFormat) ) {
        inNum *= kConversionForBits[ conv ];
        suffix = kLabel_bit[conv];
    } else {
        inNum *= kConversion [conv];
        suffix = kLabel_Byte[ conv ];
    }

    /* print such that we always fit in 4 places */
    if ( inNum < 9.995 ) {          /* 9.995 would be rounded to 10.0 */
        format = "%4.2f %s";        /* #.## */
    } else if ( inNum < 99.95 ) {   /* 99.95 would be rounded to 100 */
        format = "%4.1f %s";        /* ##.# */
    } else if ( inNum < 999.5 ) {   /* 999.5 would be rounded to 1000 */
	format = "%4.0f %s";        /*  ### */
    } else {                        /* 1000-1024 fits in 4 places
				     * If not using Adaptive sizes then
				     * this code will not control spaces*/
        format = "%4.0f %s";        /* #### */
    }
    snprintf( outString, inLen, format, inNum, suffix );
} /* end byte_snprintf */

/* IPERF_MODIFIED Start */
#ifndef NO_FILE_IO
/* IPERF_MODIFIED End */
/* -------------------------------------------------------------------
 * redirect
 *
 * redirect the stdout into a specified file
 * return: none
 * ------------------------------------------------------------------- */

void redirect(const char *inOutputFileName) {
#ifdef WIN32

    FILE *fp;

    if ( inOutputFileName == NULL ) {
        fprintf(stderr, "should specify the output file name.\n");
        return;
    }

    fp = freopen(inOutputFileName, "a+", stdout);
    if ( fp == NULL ) {
        fprintf(stderr, "redirect stdout failed!\n");
        return;
    }

#endif

    return;
}
/* IPERF_MODIFIED Start */
#endif /* NO_FILE_IO */
/* IPERF_MODIFIED End */


#ifdef __cplusplus
} /* end extern "C" */
#endif
