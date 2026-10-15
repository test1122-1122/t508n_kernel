#include "akm_cal.h"


int main(void)
{
			    int32_t input_data[4];
    float output_data[3];
    float output_offset[3];
    int8_t accuracy;
	int k=1;

    akm_cal_init();

  while(1)
  {

            int32_t mag[4];
			input_data[0]=mag[0];
			input_data[1]=mag[1];
			input_data[2]=mag[2];
			input_data[3]=mag[3];
			akm_calibrate(input_data, INPUT_TYPE_MAG, output_data, output_offset, &accuracy);


  }
}