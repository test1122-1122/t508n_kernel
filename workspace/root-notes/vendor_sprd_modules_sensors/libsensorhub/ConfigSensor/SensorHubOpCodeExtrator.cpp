/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>


#include <cutils/log.h>
#include "SensorHubOpCodeExtrator.h"

/*
 *
 *
 *
 * */
#define SENSORHUB_FIRMWARE_PATH_LENGTH 256
#define SENSORHUB_FIRMWARE_PATH "/mnt/vendor/sensorhub/"

#define SENSORHUB_SYSFS_PATH_LENGTH 1024
#define SENSORHUB_SYSFS_PARAM_CALI_PATH "/sys/module/shub_core/parameters"
#define SENSORHUB_SYSFS_PARAM_FW_PATH "/sys/module/firmware_class/parameters"
#define FIRMWARE_LIST_LENGTH  8192

static  char *accelerometer_list;
static  char *gyroscope_list;
static  char *magnetic_list;
static  char *light_list;
static  char *proximity_list;
static  char *pressure_list;


void SensorHubOpcodeRegister(const char *name,
                             sensor_info_t *sensorInfo,
                             opcode_unit *sensorOpcode,
                             uint8_t *cali, int cali_len) {
   char fixed_sysfs_path[SENSORHUB_FIRMWARE_PATH_LENGTH];
   char cali_fixed_sysfs_path[SENSORHUB_FIRMWARE_PATH_LENGTH];
   fwshub_head_t *fw;
   FILE *fdfirm, *fdcali;
   int idx = 0;
   size_t retval = 0;

   snprintf(fixed_sysfs_path,SENSORHUB_FIRMWARE_PATH_LENGTH,"%s%s%s",SENSORHUB_FIRMWARE_PATH, "shub_fw_", name);
   snprintf(cali_fixed_sysfs_path,SENSORHUB_FIRMWARE_PATH_LENGTH,"%s%s%s_cali", SENSORHUB_FIRMWARE_PATH, "shub_fw_", name);

  fdfirm = fopen(fixed_sysfs_path, "wb");
  fdcali = fopen(cali_fixed_sysfs_path, "wb");

  SH_LOG("strlen = %zu %s\n", strlen(name), fixed_sysfs_path);

  fw = (fwshub_head_t *)calloc(1, sizeof(fwshub_head_t));
  if (NULL == fw) {
       SH_ERR("no mem, failed.\n");
       goto Err_alloc;
  }

  if (NULL == fdfirm) {
    SH_ERR("open %s failed %s\n", fixed_sysfs_path, strerror(errno));
    goto Err_firm;
  } else {
    SH_LOG("open %s success\n", fixed_sysfs_path);
  }

  if (NULL == fdcali) {
    SH_ERR("open %s failed %s\n", cali_fixed_sysfs_path, strerror(errno));
    goto Err_cali;
  } else {
    SH_LOG("open %s success\n", cali_fixed_sysfs_path);
  }

  /* Init head */
  memcpy(fw->sensorName, SHUB_SENSOR_NAME, SHUB_SENSOR_NAME_LENGTH);
  // strcpy(fw->sensorName, name);
  fw->sensorType = 1;


  fw->indexOpcode[idx].eCmd = sensorOpcode[idx].eCmd;
  fw->indexOpcode[idx].nShift = 0;
  fw->indexOpcode[idx].nUnits = sensorOpcode[idx].nIIcUnits;

  for (idx = 1; idx < eCmdTotal; idx++) {
    fw->indexOpcode[idx].eCmd = sensorOpcode[idx].eCmd;
    fw->indexOpcode[idx].nShift =
        fw->indexOpcode[idx - 1].nShift + fw->indexOpcode[idx - 1].nUnits;
    fw->indexOpcode[idx].nUnits = sensorOpcode[idx].nIIcUnits;
  }
  /* write head */
  fwrite(fw, sizeof(fwshub_head_t), 1, fdfirm);

  /* write attr */
  fwrite(sensorInfo, sizeof(sensor_info_t), 1, fdfirm);

  /* write body */
  for (idx = 0; idx < eCmdTotal; idx++) {
    retval = fwrite(sensorOpcode[idx].pIIcUnits,
                    sizeof(iic_unit_t),
                    sensorOpcode[idx].nIIcUnits,
                    fdfirm);
  }

  retval = fwrite(cali, cali_len, 1, fdcali);

  fclose(fdcali);
Err_cali:
  fclose(fdfirm);
Err_firm:
  free(fw);
Err_alloc:

  retval = chmod(fixed_sysfs_path, 0777);
  SH_LOG("chmod %s ret = %zu\n", fixed_sysfs_path, retval);

  retval = chmod(cali_fixed_sysfs_path, 0777);
  SH_LOG("chmod %s ret = %zu\n", cali_fixed_sysfs_path, retval);

  return;
}

static int GetFusionMode(void)
{
	int fusion_mode = 0;

#ifdef SENSORHUB_WITH_ACCELEROMETER
	fusion_mode = ACC_FUSION;
#endif
#if ((defined SENSORHUB_WITH_ACCELEROMETER) && (defined SENSORHUB_WITH_GYROSCOPE))
	fusion_mode = ACC_GYRO_FUSION;
#endif
#if ((defined SENSORHUB_WITH_ACCELEROMETER) && (defined SENSORHUB_WITH_MAGNETIC))
	fusion_mode = ACC_MAG_FUSION;
#endif
#if ((defined SENSORHUB_WITH_ACCELEROMETER) && (defined SENSORHUB_WITH_GYROSCOPE) && (defined SENSORHUB_WITH_MAGNETIC))
	fusion_mode = ACC_GYRO_MAG_FUSION;
#endif

	return fusion_mode;
}

static int send_cali_firmware_names(const char *sysfs_path, const char *sysfs_file, unsigned char *in_buf, int size)
{
    int ret;
    FILE *fd;
    char sysfs_param_path[SENSORHUB_SYSFS_PATH_LENGTH];

    snprintf(sysfs_param_path,SENSORHUB_SYSFS_PATH_LENGTH,"%s/%s", sysfs_path, sysfs_file);
    fd = fopen(sysfs_param_path, "w");
    if (NULL == fd) {
        SH_ERR("open %s failed.\n", sysfs_param_path);
        return -1;
    }

    SH_LOG("try to write %s to %s.\n", in_buf, sysfs_param_path);
    ret = fwrite(in_buf, size, 1, fd);
    fclose(fd);
    if (ret == 1)
        return 0;
    else {
        SH_ERR("failed to write %s to %s.\n", in_buf, sysfs_param_path);
        return -1;
    }
}

void SensorHubOpcodeExtrator(void) {
    char    fusion_mode[100];

    accelerometer_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (accelerometer_list == NULL)
        goto Err_acc;

    gyroscope_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (gyroscope_list == NULL)
        goto Err_gyr;

    magnetic_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (magnetic_list == NULL)
        goto Err_mag;

    light_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (light_list == NULL)
        goto Err_light;

    proximity_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (proximity_list == NULL)
        goto Err_pro;

    pressure_list = (char*)malloc(FIRMWARE_LIST_LENGTH);
    if (pressure_list == NULL)
        goto Err_press;

    memset(accelerometer_list, 0, FIRMWARE_LIST_LENGTH);
    memset(gyroscope_list, 0, FIRMWARE_LIST_LENGTH);
    memset(magnetic_list, 0, FIRMWARE_LIST_LENGTH);
    memset(light_list, 0, FIRMWARE_LIST_LENGTH);
    memset(proximity_list, 0, FIRMWARE_LIST_LENGTH);
    memset(pressure_list, 0, FIRMWARE_LIST_LENGTH);

/* the acc config  */
#ifdef SENSORHUB_WITH_ACCELEROMETER_mc3416p
    strcat(accelerometer_list, "accelerometer_mc3416p,");
    SensorHubOpcodeRegisterAccelerometer_mc3416p();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_mc3416p_l3_3c10
    strcat(accelerometer_list, "accelerometer_mc3416p,");
    SensorHubOpcodeRegisterAccelerometer_mc3416p();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bma233
    strcat(accelerometer_list, "accelerometer_bma233,");
    SensorHubOpcodeRegisterAccelerometer_bma233();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bma253
    strcat(accelerometer_list, "accelerometer_bma253,");
    SensorHubOpcodeRegisterAccelerometer_bma253();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bma253_l3_3c10
    strcat(accelerometer_list, "accelerometer_bma253,");
    SensorHubOpcodeRegisterAccelerometer_bma253();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_kxtj3_l3_3c10
    strcat(accelerometer_list, "accelerometer_kxtj3,");
    SensorHubOpcodeRegisterAccelerometer_kxtj3();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_kxtj3
    strcat(accelerometer_list, "accelerometer_kxtj3,");
    SensorHubOpcodeRegisterAccelerometer_kxtj3();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_kxtj3_ud710_20c11
    strcat(accelerometer_list, "accelerometer_kxtj3_ud710_20c11,");
    SensorHubOpcodeRegisterAccelerometer_kxtj3_ud710_20c11();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bmi160
    strcat(accelerometer_list, "accelerometer_bmi160,");
    SensorHubOpcodeRegisterAccelerometer_bmi160();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bmi160_1
    strcat(accelerometer_list, "accelerometer_bmi160_1,");
    SensorHubOpcodeRegisterAccelerometer_bmi160_1();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bmi160_9861s3
    strcat(accelerometer_list, "accelerometer_bmi160_9861s3,");
    SensorHubOpcodeRegisterAccelerometer_bmi160_9861s3();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_bmi160_ud710_20c11
    strcat(accelerometer_list, "accelerometer_bmi160_ud710_20c11,");
    SensorHubOpcodeRegisterAccelerometer_bmi160_ud710_20c11();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_icm20600
    strcat(accelerometer_list, "accelerometer_icm20600,");
    SensorHubOpcodeRegisterAccelerometer_icm20600();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_icm20608
    strcat(accelerometer_list, "accelerometer_icm20608,");
    SensorHubOpcodeRegisterAccelerometer_icm20608();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_icm40607
    strcat(accelerometer_list, "accelerometer_icm40607,");
    SensorHubOpcodeRegisterAccelerometer_icm40607();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_lsm6ds3
    strcat(accelerometer_list, "accelerometer_lsm6ds3,");
    SensorHubOpcodeRegisterAccelerometer_lsm6ds3();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_lsm6ds3tr
    strcat(accelerometer_list, "accelerometer_lsm6ds3tr,");
    SensorHubOpcodeRegisterAccelerometer_lsm6ds3tr();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_lsm6dsl
    strcat(accelerometer_list, "accelerometer_lsm6dsl,");
    SensorHubOpcodeRegisterAccelerometer_lsm6dsl();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_lsm6dsl_ums512
    strcat(accelerometer_list, "accelerometer_lsm6dsl,");
    SensorHubOpcodeRegisterAccelerometer_lsm6dsl();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_lsm6dsl_ud710
    strcat(accelerometer_list, "accelerometer_lsm6dsl_ud710,");
    SensorHubOpcodeRegisterAccelerometer_lsm6dsl_ud710();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_mxc4005
    strcat(accelerometer_list, "accelerometer_mxc4005,");
    SensorHubOpcodeRegisterAccelerometer_mxc4005();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_qma6981
    strcat(accelerometer_list, "accelerometer_qma6981,");
    SensorHubOpcodeRegisterAccelerometer_qma6981();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_sc7a20
    strcat(accelerometer_list, "accelerometer_sc7a20,");
    SensorHubOpcodeRegisterAccelerometer_sc7a20();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_sc7a20_ud710_2c11
    strcat(accelerometer_list, "accelerometer_sc7a20_ud710_2c11,");
    SensorHubOpcodeRegisterAccelerometer_sc7a20_ud710_2c11();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_icm20600_l3_1c10
    strcat(accelerometer_list, "accelerometer_icm20600_l3_1c10,");
    SensorHubOpcodeRegisterAccelerometer_icm20600_l3_1c10();
#endif

#ifdef SENSORHUB_WITH_ACCELEROMETER_stk8ba53
    strcat(accelerometer_list, "accelerometer_stk8ba53,");
    SensorHubOpcodeRegisterAccelerometer_stk8ba53();
#endif

/* the gyro config  */

#ifdef SENSORHUB_WITH_GYROSCOPE_bmi160
    strcat(gyroscope_list, "gyroscope_bmi160,");
    SensorHubOpcodeRegisterGyroscope_bmi160();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_bmi160_1
    strcat(gyroscope_list, "gyroscope_bmi160_1,");
    SensorHubOpcodeRegisterGyroscope_bmi160_1();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_bmi160_9861s3
    strcat(gyroscope_list, "gyroscope_bmi160_9861s3,");
    SensorHubOpcodeRegisterGyroscope_bmi160_9861s3();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_icm20600
    strcat(gyroscope_list, "gyroscope_icm20600,");
    SensorHubOpcodeRegisterGyroscope_icm20600();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_icm20608
    strcat(gyroscope_list, "gyroscope_icm20608,");
    SensorHubOpcodeRegisterGyroscope_icm20608();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_icm40607
    strcat(gyroscope_list, "gyroscope_icm40607,");
    SensorHubOpcodeRegisterGyroscope_icm40607();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_lsm6ds3
    strcat(gyroscope_list, "gyroscope_lsm6ds3,");
    SensorHubOpcodeRegisterGyroscope_lsm6ds3();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_lsm6ds3tr
    strcat(gyroscope_list, "gyroscope_lsm6ds3tr,");
    SensorHubOpcodeRegisterGyroscope_lsm6ds3tr();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_lsm6dsl
    strcat(gyroscope_list, "gyroscope_lsm6dsl,");
    SensorHubOpcodeRegisterGyroscope_lsm6dsl();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_lsm6dsl_ud710
    strcat(gyroscope_list, "gyroscope_lsm6dsl_ud710,");
    SensorHubOpcodeRegisterGyroscope_lsm6dsl_ud710();
#endif

#ifdef SENSORHUB_WITH_GYROSCOPE_icm20600_l3_1c10
    strcat(gyroscope_list, "gyroscope_icm20600_l3_1c10,");
    SensorHubOpcodeRegisterGyroscope_icm20600_l3_1c10();
#endif

/* the  mag cofig
**
**  meanwhile the mag is not good due to the calib bin
**  it is download to cm4 control by the modem_control
*/

#ifdef SENSORHUB_WITH_MAGNETIC_af8133i
    strcat(magnetic_list, "magnetic_af8133i,");
    SensorHubOpcodeRegisterMagnetic_af8133i();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_af9133
    strcat(magnetic_list, "magnetic_af9133,");
    SensorHubOpcodeRegisterMagnetic_af9133();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_akm09911c
    strcat(magnetic_list, "magnetic_akm09911c,");
    SensorHubOpcodeRegisterMagnetic_akm09911c();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_akm09916
    strcat(magnetic_list, "magnetic_akm09916,");
    SensorHubOpcodeRegisterMagnetic_akm09916();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_akm09918
    strcat(magnetic_list, "magnetic_akm09918,");
    SensorHubOpcodeRegisterMagnetic_akm09918();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_akm09918_ums512
    strcat(magnetic_list, "magnetic_akm09918,");
    SensorHubOpcodeRegisterMagnetic_akm09918();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_akm09918_ud710
    strcat(magnetic_list, "magnetic_akm09918_ud710,");
    SensorHubOpcodeRegisterMagnetic_akm09918_ud710();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_lis3mdl
    strcat(magnetic_list, "magnetic_lis3mdl,");
    SensorHubOpcodeRegisterMagnetic_lis3mdl();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_mmc3630
    strcat(magnetic_list, "magnetic_mmc3630,");
    SensorHubOpcodeRegisterMagnetic_mmc3630();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_mmc3630_9861s3
    strcat(magnetic_list, "magnetic_mmc3630_9861s3,");
    SensorHubOpcodeRegisterMagnetic_mmc3630_9861s3();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_mmc35160
    strcat(magnetic_list, "magnetic_mmc35160,");
    SensorHubOpcodeRegisterMagnetic_mmc35160();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_qmc6983mm
    strcat(magnetic_list, "magnetic_qmc6983mm,");
    SensorHubOpcodeRegisterMagnetic_qmc6983mm();
#endif

#ifdef SENSORHUB_WITH_MAGNETIC_mmc5603
    strcat(magnetic_list, "magnetic_mmc5603,");
    SensorHubOpcodeRegisterMagnetic_mmc5603();
#endif

/*the prox config*/

#ifdef SENSORHUB_WITH_PROXIMITY_ltr568als
    strcat(proximity_list, "proximity_ltr568als,");
    SensorHubOpcodeRegisterProximity_ltr568als();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_ltr578als
    strcat(proximity_list, "proximity_ltr578als,");
    SensorHubOpcodeRegisterProximity_ltr578als();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_stk3332
    strcat(proximity_list, "proximity_stk3332,");
    SensorHubOpcodeRegisterProximity_stk3332();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_ap3426
    strcat(proximity_list, "proximity_ap3426,");
    SensorHubOpcodeRegisterProximity_ap3426();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_epl2590
    strcat(proximity_list, "proximity_epl2590,");
    SensorHubOpcodeRegisterProximity_epl2590();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_ltr553als
    strcat(proximity_list, "proximity_ltr553als,");
    SensorHubOpcodeRegisterProximity_ltr553als();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_ltr553iwhale2
    strcat(proximity_list, "proximity_ltr553iwhale2,");
    SensorHubOpcodeRegisterProximity_ltr553iwhale2();
#endif


#ifdef SENSORHUB_WITH_PROXIMITY_pa12200002
    strcat(proximity_list, "proximity_pa12200002,");
    SensorHubOpcodeRegisterProximity_pa12200002();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_rpr0521
    strcat(proximity_list, "proximity_rpr0521,");
    SensorHubOpcodeRegisterProximity_rpr0521();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_stk3338
    strcat(proximity_list, "proximity_stk3338,");
    SensorHubOpcodeRegisterProximity_stk3338();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_stk3338v
    strcat(proximity_list, "proximity_stk3338v,");
    SensorHubOpcodeRegisterProximity_stk3338v();
#endif

#ifdef SENSORHUB_WITH_PROXIMITY_stk3a5x
    strcat(proximity_list, "proximity_stk3a5x,");
    SensorHubOpcodeRegisterProximity_stk3a5x();
#endif

/* the light config*/

#ifdef SENSORHUB_WITH_LIGHT_ltr568als
    strcat(light_list, "light_ltr568als,");
    SensorHubOpcodeRegisterLight_ltr568als();
#endif

#ifdef SENSORHUB_WITH_LIGHT_ltr578als
    strcat(light_list, "light_ltr578als,");
    SensorHubOpcodeRegisterLight_ltr578als();
#endif

#ifdef SENSORHUB_WITH_LIGHT_stk3332
    strcat(light_list, "light_stk3332,");
    SensorHubOpcodeRegisterLight_stk3332();
#endif

#ifdef SENSORHUB_WITH_LIGHT_ap3426
    strcat(light_list, "light_ap3426,");
    SensorHubOpcodeRegisterLight_ap3426();
#endif

#ifdef SENSORHUB_WITH_LIGHT_epl2590
    strcat(light_list, "light_epl2590,");
    SensorHubOpcodeRegisterLight_epl2590();
#endif

#ifdef SENSORHUB_WITH_LIGHT_ltr553als
    strcat(light_list, "light_ltr553als,");
    SensorHubOpcodeRegisterLight_ltr553als();
#endif


#ifdef SENSORHUB_WITH_LIGHT_pa12200002
    strcat(light_list, "light_pa12200002,");
    SensorHubOpcodeRegisterLight_pa12200002();
#endif

#ifdef SENSORHUB_WITH_LIGHT_rpr0521
    strcat(light_list, "light_rpr0521,");
    SensorHubOpcodeRegisterLight_rpr0521();
#endif

#ifdef SENSORHUB_WITH_LIGHT_V2000
    strcat(light_list, "light_V2000,");
    SensorHubOpcodeRegisterLight_V2000();
#endif

#ifdef SENSORHUB_WITH_LIGHT_stk3338v
    strcat(light_list, "light_stk3338v,");
    SensorHubOpcodeRegisterLight_stk3338v();
#endif

#ifdef SENSORHUB_WITH_LIGHT_stk3a5x
    strcat(light_list, "light_stk3a5x,");
    SensorHubOpcodeRegisterLight_stk3a5x();
#endif

/*the pre config*/
#ifdef SENSORHUB_WITH_PRESSURE_lps25hb
   strcat(pressure_list, "pressure_lps25hb,");
   SensorHubOpcodeRegisterPressure_lps25hb();
#endif

#ifdef SENSORHUB_WITH_PRESSURE_bmp280
   strcat(pressure_list, "pressure_bmp280,");
   SensorHubOpcodeRegisterPressure_bmp280();
#endif

    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_FW_PATH, "path", (unsigned char*)SENSORHUB_FIRMWARE_PATH, strlen(SENSORHUB_FIRMWARE_PATH));

    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "acc_firms", (unsigned char*)accelerometer_list, strlen(accelerometer_list));
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "gryo_firms", (unsigned char*)gyroscope_list, strlen(gyroscope_list));
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "mag_firms", (unsigned char*)magnetic_list, strlen(magnetic_list));
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "light_firms", (unsigned char*)light_list, strlen(light_list));
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "prox_firms", (unsigned char*)proximity_list, strlen(proximity_list));
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "pressure_firms", (unsigned char*)pressure_list, strlen(pressure_list));

    memset(fusion_mode, 0, sizeof(fusion_mode));
    sprintf(fusion_mode, "%d", GetFusionMode());
    send_cali_firmware_names(SENSORHUB_SYSFS_PARAM_CALI_PATH, "sensor_fusion_mode", (unsigned char*)fusion_mode, strlen(fusion_mode));

    free(pressure_list);
Err_press:
    free(proximity_list);
Err_pro:
    free(light_list);
Err_light:
    free(magnetic_list);
Err_mag:
    free(gyroscope_list);
Err_gyr:
    free(accelerometer_list);
Err_acc:
    return;
}
