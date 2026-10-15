/******************************************************************************
 *
 *  $Id: $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#include "timer.h"
//#include "bsp/time.h"
//#include <utils/Log.h>

#define CONVET_NS_TO_MS    (1000000)

#undef LOG_TAG
#define LOG_TAG     "AKMD2"


 //Convert from timespec structure to int64_t value
 //@return int64_t value
 //@param[in] val timespec value
 
//int64_t timespec_to_int64(struct timespec *val)
//{
//    return (((int64_t)val->tv_sec * 1000000000) + (int64_t)val->tv_nsec);
//}

// current time (unit: ms)
int32_t GetCurrentTime(void)
{
    //struct timespec currTime = { 0, 0 }; // Current time 
    int64_t time64_ns = 0;
    int32_t time_ms = 0;

    //Get current time 
   // if (clock_gettime(CLOCK_MONOTONIC, &currTime) < 0) {
        // no need for lastTime and prevHtm_ag
       // ALOGE("%s:%d, clock_gettime error.", __FUNCTION__, __LINE__);
       // return 0;
   // }

  //  time64_ns = (int64_t)timespec_to_int64(&currTime);

    //time_ms = time_nowMs();//(int32_t)(time64_ns / CONVET_NS_TO_MS);

    return time_ms;
}
