#ifndef _AUDIO_NPI_H_
#define _AUDIO_NPI_H_
int testBbatAudioIn(char *buf, int buf_len, char *rsp, int rsplen);
int testBbatAudioOut(char *buf, int buf_len, char *rsp, int rsplen);
int testheadsetplunge(char *buf, int buf_len, char *rsp, int rsplen);
int audiotester_fun(char *received_buf, int buf_len, char *rsp, int rsplen);
int AUDIO_HEADSET_TEST_AT(char *req, char *rsp);
int AUDIO_FM_AT (char *req, char *rsp);
int AUDIO_CPLOOP_AT (char *req, char *rsp);
int AUDIO_PIPE_AT (char *req, char *rsp);
int AUDIO_PLAY_AT (char *req, char *rsp);
int AUDIO_HEADSET_CHECK_AT(char *req, char *rsp);
#if defined(SPRD_AUDIO_HIDL_CLIENT) && defined(AUDIO_WHALE)
int AUDIO_CALIBRATION_AT (char *req, char *rsp);
#endif
#endif
