#ifndef _POST_PROCESSING_PARAMETER_H_
#define  _POST_PROCESSING_PARAMETER_H_

#include <stdbool.h>
#include "audio_param.h"


typedef void* recordproc_handle;

recordproc_handle recordproc_init();
int recordproc_setparameter(recordproc_handle handle, struct audio_record_proc_param * data);
int recordproc_enable(recordproc_handle handle);
int recordproc_disable(recordproc_handle handle);
void recordproc_deinit(recordproc_handle handle);

#endif //endif _PRE_PROCESSING_PARAMETER_H_

