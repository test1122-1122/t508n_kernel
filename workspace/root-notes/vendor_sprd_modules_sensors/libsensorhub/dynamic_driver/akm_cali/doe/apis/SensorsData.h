#ifndef SENSORSDATA_C
#define SENSORSDATA_C

// Include files for SmartCompass Library.
#include "AKCompass.h"
#include "AKCommon.h"
#include "Measure.h"

void AKM_SetMagData(
    AKSCPRMS    *prms,
    const int32_t mx,
    const int32_t my,
    const int32_t mz,
    const int16_t st2,
    const int16_t mdt
);

// grx, gry, grz: [rad]
// currTime
void AKM_SetGyroData(
	AKSCPRMS	*prms,
	const float grx,
	const float gry,
	const float grz,
	const int16_t gdt
);

// [m/s^2]
void AKM_SetAccData(
    AKSCPRMS	*prms,
    const float ix,
    const float iy,
    const float iz
);

#endif
