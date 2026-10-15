#ifndef LIBAPIS_H
#define LIBAPIS_H

#include "AKCompass.h"

#ifdef AKMD_ENABLE_ORIENTATION
#include "D6D_APIs.h"
#endif

#ifdef AKMD_ENABLE_PG
#ifdef AKMD_PG_FLOAT_POINTS
#include "PseudoGyro_APIs_Float.h"
#else
#include "PseudoGyro_APIs_Fixed.h"
#endif
#endif

size_t LibAPIs_GetBufferSize(void);

void LibAPIs_SetPrmsPointer(AKSCPRMS *prms, size_t prms_sz);

void LibAPIs_Init(
    AKSCPRMS *prms
);

void LibAPIs_SetLevel(
    AKSCPRMS *prms,
    const AKSC_HDST hdst
);

int16 LibAPIs_ExeCalib(
    AKSCPRMS *prms
);

#endif

