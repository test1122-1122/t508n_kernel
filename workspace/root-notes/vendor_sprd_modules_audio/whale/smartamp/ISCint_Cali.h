/*******************************************************************
**  File Name: ISCint_Cali.h                                         
**  Author:  Fei Dong, Howard Chen,Wei Ji (VA-Sys team)                                        
**  Date: 16/10/2018                                    
**  Copyright: Spreadtrum, Incorporated. All Rights Reserved.
**  Description: Spreadtrum Intelligent Speaker Control (ISC) for Smart AMP
**                   ISCint_BassEn header
********************************************************************
********************************************************************
**  Edit History                                                   
**-----------------------------------------------------------------
**  DATE            NAME            DESCRIPTION                    
**  16/10/2018  Fei Dong        create ISCint_Cali.h
*******************************************************************/
#ifndef _ISCint_Cali_H_
#define _ISCint_Cali_H_
#include "smartamp.h"
#include <stdbool.h>

#define Qks 26
#define Qkc 22
#define Qkiv Qks 
#define NDECIM_COEF 64
#define NTABLE_PT 1024
#define NBLOCK_SIG 20
#define NBLOCK_SIG_INTERRUPT        240
#define NPEAK_SECTIONS_PT           1
#define NPEAK_STATE_PT              (4*NPEAK_SECTIONS_PT)
#define NDECIM                      4
#define NBLOCK_CTRL                 (NBLOCK_SIG/NDECIM)
#define NBLOCK_IV_CTRL              (NBLOCK_IV/NDECIM)

#define SMULLS_ISC(x,w,s)   ((int32)((((int64)(x))*(w))>>(s)))

typedef unsigned char           BOOLEAN;
typedef unsigned char           uint8;
typedef unsigned short          uint16;
typedef unsigned long int       uint32;
typedef unsigned int            uint;
typedef signed char             int8;
typedef signed short            int16;
typedef signed long int         int32;
typedef long long               int64;
//typedef __int64               int64;
typedef struct {
     int32 re;
     int32 im;
 } complex_int32 ;

#define MALLOC_ISC(size)                malloc((size))
#define MEMSET_ISC(des, val, size)      memset((des), (val), (size))
#define MEMCPY_ISC(des,src,size)        memcpy((des),(src),(size))
#define MEMMOVE_ISC(des,src,size)       memmove((des),(src),(size))
#define FREE_ISC(des)                   free((des) )

extern const int32 peak_pt_coef[5*NPEAK_SECTIONS_PT];
extern const int32 decim_coef[NDECIM_COEF];
extern const int16 pilot_tone_coef[NTABLE_PT];
extern const int32 qlog2[1025];
extern const int32 invTabF[2049];


typedef struct {
    int32 samplerate;
    int16 pt_timelength;
    int16 pilot_tone_amp_cali;
    int16 I_FullScale;
    int16 V_FullScale;
    int16 R0_max_offset;
    int16 R0_normal_offset;
    int16 Re_T_cali;
    int16 R0_update_frame_num;
    int16 R0_update_block_num;
    int32 mag_thrd;
}ISC_Cali_para_input;

typedef struct{
    int32 pt_samples;    
    int16 R0_update_flag;
    int16 R0_init;
    int16 R0_calc;
    int16 R0_calc_update;
    int16 Re_T_cali;
    int16 R0_maxup_thrd;
    int16 R0_maxdown_thrd;
    int16 R0_normalup_thrd;
    int16 R0_normaldown_thrd;
    int16 postfilter_flag;

    int16 R0_update_block_cnt;
    int16 R0_update_frame_cnt;
    int16 R0_update_frame_num;
    int16 R0_update_block_num;
    int16 R0_update_frame_num_rec;

    int16 pilot_tone_pos;

    int16 I_FullScale;
    int16 V_FullScale;

    int32 R0_sum;
    
    int16 ndecim_coef;
   
    int32  sum_i_pt;
    int32  sum_v_pt;
    
    int32 decim_i_state[NDECIM_COEF+NBLOCK_IV];
    int32 decim_v_state[NDECIM_COEF+NBLOCK_IV];    
    int32 i_decim[NBLOCK_IV_CTRL];
    int32 v_decim[NBLOCK_IV_CTRL];
    

    int32 peak_pt_coef[5*NPEAK_SECTIONS_PT];
    int32 peak_state_i[NPEAK_STATE_PT];
    int32 peak_state_v[NPEAK_STATE_PT];
 //   int32 i_decim_pt[NBLOCK_IV_CTRL];
//    int32 v_decim_pt[NBLOCK_IV_CTRL];
    int16 pilot_tone_coef[NTABLE_PT];
    int32 mag_thrd;
}ISC_Cali_para;

typedef struct {
    ISC_Cali_para Cali_para;
    ISC_Cali_para_input Cali_input_para;
    bool cali_passed;
}ISC_Cali_handler;


int32 creat_pilot(ISC_Cali_para *Cali_para, int16 *output);
int32 init_Calc_R0_ISC(ISC_Cali_para **Cali_para_ptr,ISC_Cali_para_input* Cali_para_input);
int32 Calibration_R0_ISC(int32 *input_cur,int32 *input_vol, ISC_Cali_para * Cali_para);
int32 Calc_R0_ISC(int32 *input_cur,int32 *input_vol,ISC_Cali_para *cali_para);
int32 fir_decim_ISC_C(const int32 *input,  int32 *output, const int32 *coeffs, int32 *state, int32 n_out,int32 n_coeffs,int32 n_decim,int32 shift);
int32 veccmlt_ISC_C(int32 *x,int32 y,int32 *out, int32 N, int32 Qk);
int32 veccmlt_ISC_16_C(int16 *x,int16 y, int16 *out,int16 N, int16 Qk);
int32 biquad_vec_64_ISC_C(const int32* input, int32* output,const int32* coeffs,int32* state,int32 n_input,int32 n_sections,int32 shift);
int32 limiter_ISC_C(int32 x, int32 Qk);
int32 sumabs_ISC_C(int32 *parray,  int32 N,  int32 shift);
int32 vecvadd_ISC_C(int32 *x,  int32 *y, int32 *out,  int32 N);
int32 fast_log2_ISC_C(int32 x);//output has 6 bits precsion
int32 log2_ISC_C(int32 x); //output has 18 bits precsion  //input:int, output: Q18
int32 div32fs_ISC_C(int32 x,int32 r,int32 s);

#endif

