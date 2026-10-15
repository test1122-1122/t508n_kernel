#define LOG_TAG "nxptfa"
#include "Sprd_NxpTfa.h"
#include <audio_utils/resampler.h>
#include <string.h>
#include "dumpdata.h"

struct nxppa_res
{
    struct pcm *pcm_tfa;
    struct resampler_itfe  *resampler;
    char * buffer_resampler;
    pthread_mutex_t lock;
    AUDIO_PCMDUMP_FUN dumppcmfun;
};

static int NxpTfa_Init_Resampler(struct nxppa_res *nxp_res)
{
    int ret;

    if (!nxp_res)
        return -1;
    nxp_res->dumppcmfun =NULL;

    nxp_res->buffer_resampler = malloc(20*1024);
    if (!nxp_res->buffer_resampler ) {
        ALOGE("NxpTfa_Open, malloc for resampler buffer failed!");
        return -1;
    }
    if(nxp_res->resampler == NULL) {
        ret = create_resampler( 44100,
                    48000,
                    2,
                    RESAMPLER_QUALITY_DEFAULT,
                    NULL,
                    &nxp_res->resampler);
        if (ret != 0) {
            ALOGE("NxpTfa_Open,create resampler failed!");
            nxp_res->resampler = NULL;
            free(nxp_res->buffer_resampler);
            nxp_res->buffer_resampler = NULL;
            return -1;
        }
    }

    return 0;
}

extern int get_snd_card_number(const char *card_name);

nxp_pa_handle NxpTfa_Open(struct NxpTpa_Info *info, int need_src, int do_pcm_start)
{
    int card = 0;
    int ret = 0;
    struct nxppa_res * nxp_res;

    if (!info) {
        ALOGE("NxpTfa_Open, info is NULL.");
        return NULL;
    }
    if (!info->pCardName) {
        ALOGE("NxpTfa_Open, card name is NULL.");
        return NULL;
    }

    ALOGE("peter: NxpTfa_Open in. need_src: %d, do_pcm_start: %d", need_src,do_pcm_start);

    nxp_res =malloc(sizeof(struct nxppa_res));
    if(!nxp_res) {
        ALOGE("NxpTfa_Open, malloc for nxppa_res failed!");
        return NULL;
    }
    memset(nxp_res, 0, sizeof(struct nxppa_res));

    pthread_mutex_init(&nxp_res->lock, NULL);

    if (need_src && NxpTfa_Init_Resampler(nxp_res) != 0)
        goto err_free_nxp_res;

    card =  get_snd_card_number(info->pCardName);
    if(card < 0) {
        ALOGE("NxpTfa_Open, get sound card no failed!");
        goto err_deinit_resampler;
    }
    nxp_res->pcm_tfa=pcm_open(card, info->dev , info->pcm_flags, &info->pcmConfig);
    if(!pcm_is_ready(nxp_res->pcm_tfa)) {
        ALOGE("NxpTfa_Open,pcm is not ready!");
        goto err_deinit_resampler;
    }
    if (do_pcm_start && pcm_start(nxp_res->pcm_tfa) != 0) {
        ALOGE("NxpTfa_Open, pcm_start for card(%s), device(%d) failed! error: %s",
              info->pCardName, info->dev, pcm_get_error(nxp_res->pcm_tfa));
    }

    return (nxp_pa_handle)nxp_res;

err_deinit_resampler:
    release_resampler(nxp_res->resampler);
    nxp_res->resampler= NULL;
    free(nxp_res->buffer_resampler);
    nxp_res->buffer_resampler = NULL;
err_free_nxp_res:
    free(nxp_res);
    return NULL;

}

int NxpTfa_Write( nxp_pa_handle handle , const void *data, unsigned int count)
{
    int ret = 0;
    int OutSize;
    size_t out_frames = 0;
    size_t in_frames = count/4;
    int16_t  * buffer = data;
    struct nxppa_res * nxp_res = NULL;

    if(handle == NULL)
        return -1;

    nxp_res = (struct nxppa_res *) handle;

    ALOGV("peter:NxpTfa_Write in");

    pthread_mutex_lock(&nxp_res->lock);

    out_frames = in_frames*2;
    if(nxp_res->resampler && nxp_res->buffer_resampler) {
        //ALOGE("peter 1: NxpTfa_Write in_frames %d,out_frames %d", in_frames, out_frames);
        nxp_res->resampler->resample_from_input(nxp_res->resampler,
                    (int16_t *)buffer,
                    &in_frames,
                    (int16_t *)nxp_res->buffer_resampler,
                    &out_frames);
        //ALOGE("peter 2: NxpTfa_Write in_frames %d,out_frames %d", in_frames, out_frames);
    }

    if(NULL!=nxp_res->dumppcmfun){
        nxp_res->dumppcmfun(nxp_res->buffer_resampler,out_frames*4,DUMP_MUSIC_HWL_BEFOORE_VBC);
    }

    ret = pcm_mmap_write(nxp_res->pcm_tfa, nxp_res->buffer_resampler, out_frames*4);
    //ret = pcm_mmap_write(nxp_res->pcm_tfa, data, count);
    pthread_mutex_unlock(&nxp_res->lock);

    ALOGV("peter:NxpTfa_Write out");

    return ret;
}


void NxpTfa_Close( nxp_pa_handle handle )
{
    struct nxppa_res * nxp_res;

    if(handle == NULL)
        return;

    nxp_res = (struct nxppa_res *) handle;

    pthread_mutex_lock(&nxp_res->lock);

    ALOGE("peter: NxpTfa_Close in");

    if(nxp_res->pcm_tfa) {
        pcm_close(nxp_res->pcm_tfa);
    }

    if(nxp_res->resampler) {
        release_resampler(nxp_res->resampler);
        nxp_res->resampler= NULL;
    }
    if(nxp_res->buffer_resampler) {
        free(nxp_res->buffer_resampler);
        nxp_res->buffer_resampler = NULL;
    }
    pthread_mutex_unlock(&nxp_res->lock);

    free(nxp_res);
}

void register_nxp_pcmdump(nxp_pa_handle handle,void *fun){
    struct nxppa_res * nxp_res = NULL;

    if(handle == NULL)
        return ;

    nxp_res = (struct nxppa_res *) handle;
    nxp_res->dumppcmfun=fun;
    ALOGI("register_nxp_pcmdump");
}
