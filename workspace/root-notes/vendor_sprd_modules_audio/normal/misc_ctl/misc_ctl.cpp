
#define LOG_TAG "audio_hw_mis_ctl:"

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "cutils/list.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <expat.h>
#include <system/thread_defs.h>
#include <cutils/sched_policy.h>
#include <sys/prctl.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/String8.h>

#ifdef VERSION_IS_ANDROID_O
#include <cutils/log.h>
#include <binder/IInterface.h>

#include <vendor/sprd/hardware/power/2.0/IPower.h>
#include <vendor/sprd/hardware/power/2.0/types.h>
using ::android::hidl::base::V1_0::IBase;
using ::vendor::sprd::hardware::power::V2_0::IPower;
using ::vendor::sprd::hardware::power::V2_0::PowerHint;
#elif defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q
#include <vendor/sprd/hardware/power/3.0/IPower.h>
#include <vendor/sprd/hardware/power/3.0/types.h>
#include <log/log.h>
using ::android::hidl::base::V1_0::IBase;
using ::vendor::sprd::hardware::power::V3_0::IPower;
using ::vendor::sprd::hardware::power::V3_0::PowerHint;
#else
#include <cutils/log.h>
#include <binder/IInterface.h>
#include <binder/BinderService.h>
#include <powermanager/IPowerManager.h>
#include <powermanager/PowerManager.h>
#endif


namespace android {

extern "C"{
    void * aud_misc_ctl_open();
    void  aud_misc_ctl_close(void * misc);
    int aud_misc_ctl_force_lowpower(void * misc);
    int aud_misc_ctl_cancel_lowpower(void * misc);
    int aud_misc_ctl_disable_pownerhint(void * misc);
    int aud_misc_ctl_enable_pownerhint(void * misc);
}

#ifdef VERSION_IS_ANDROID_O
::android::sp<::vendor::sprd::hardware::power::V2_0::IPower> gPowerHal;
::android::sp<IBase> lock;
#elif defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q
::android::sp<::vendor::sprd::hardware::power::V3_0::IPower> gPowerHal;
::android::sp<IBase> lock;
#ifdef NORMAL_DISABLE_POWER_HINT
::android::sp<IBase> normal_lock;
#endif
#else
static sp<IPowerManager>       mPowerManager;
static sp<IBinder>             mWakeLockToken;
#endif

typedef enum {
    AUD_CMD_EXIT,
    AUD_CMD_FORCE_LOWPOWER,
    AUD_CMD_CANCEL_LOWPOWER,
    AUD_CMD_CLOSE,
#if defined NORMAL_DISABLE_POWER_HINT || defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q
    AUD_CMD_NORMA_ENABLE_POWERHINT,
    AUD_CMD_NORMA_DISABLE_POWERHINT,
#endif
}AUD_CMD_T;

struct aud_cmd {
    struct listnode node;
    AUD_CMD_T cmd;
    bool is_sync;
    sem_t   sync_sem;
    int ret;
    void* param;
};

struct misc_ctl {
    struct listnode cmd_list;
    pthread_mutex_t  lock;
    pthread_cond_t   cmd_cond;
    pthread_mutex_t  cmd_mutex;
    pthread_t        thread;
    int state;
    int mp3_lowpower;
#if defined NORMAL_DISABLE_POWER_HINT && (defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q)
    bool normal_powerhint;
#endif
};

#if defined VERSION_IS_ANDROID_O || defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q
static void audio_offload_force_low_power() {
#if !defined(NO_POWER_SAVING_MODE)
    if (gPowerHal == 0) {
        gPowerHal = IPower::getService();
    }
    if (gPowerHal != 0) {
        lock = new IBase();
        if (lock == 0) {
            ALOGE("Thread cannot connect to the power manager service");
        } else {
            gPowerHal->acquirePowerHintBySceneId(lock,
                    "audio_mp3",
                    (int32_t)PowerHint::VENDOR_SCREENOFF_MP3_PLAYBACK);
            ALOGI("acquirePowerHintBySceneId()");
        }
    }
#endif
}


static void audio_offload_release_low_power()
{
#if !defined(NO_POWER_SAVING_MODE)
    if(lock != 0){
        if (gPowerHal != 0) {
            ALOGI("releasePowerHintBySceneId()");
            gPowerHal->releasePowerHintBySceneId(lock,(int32_t)PowerHint::VENDOR_SCREENOFF_MP3_PLAYBACK);
        }
        lock.clear();
    }
#endif
}

#if defined NORMAL_DISABLE_POWER_HINT && (defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q)
static void audio_normal_disable_power_hint() {
    if(normal_lock != 0){
        if (gPowerHal != 0) {
            ALOGI("audio_normal_disable_power_hint");
            gPowerHal->releasePowerHintBySceneName(normal_lock,"audio_playback");
        }
        normal_lock.clear();
    }
}

static void audio_normal_enable_power_hint()
{
    if (gPowerHal == 0) {
        gPowerHal = IPower::getService();
    }
    if (gPowerHal != 0) {
        normal_lock = new IBase();
        if (normal_lock == 0) {
            ALOGE("audio_normal_enable_power_hint Thread cannot connect to the power manager service");
        } else {
            gPowerHal->acquirePowerHintBySceneName(normal_lock,
                    "audio_noraml",
                    "audio_playback");
            ALOGI("audio_normal_enable_power_hint");
        }
    }
}
#endif

#else
static void audio_offload_force_low_power() {
#if !defined(NO_POWER_SAVING_MODE)
    if (mPowerManager == 0) {
        // use checkService() to avoid blocking if power service is not up yet
        sp<IBinder> binder =
            defaultServiceManager()->checkService(String16("power"));
        if (binder == 0) {
            ALOGE("Thread cannot connect to the power manager service");
        } else {
            mPowerManager = interface_cast<IPowerManager>(binder);
            //binder->linkToDeath(mDeathRecipient);
        }
    }

    if (mPowerManager != 0) {
        sp<IBinder> binder = new BBinder();
        mPowerManager->acquirePrfmLock(binder,
                String16("audio_mp3"),
                String16("audioserver"),
                POWER_HINT_VENDOR_SCREENOF_MP3_PLAYBACK);
        mWakeLockToken = binder;
        ALOGI("acquireWakeLock_l() - Prfmlock");
    }
#endif
}


static void audio_offload_release_low_power()
{
#if !defined(NO_POWER_SAVING_MODE)
    if (mWakeLockToken != 0) {
        if (mPowerManager != 0) {
            ALOGI("releaseWakeLock_l() - Prfmlock ");
            mPowerManager->releasePrfmLock(mWakeLockToken);
        }
        mWakeLockToken.clear();
    }
#endif
}
#endif

static int aud_signal(struct misc_ctl * misc_ctl)
{
    int ret = 0;
    pthread_cond_signal(&misc_ctl->cmd_cond);
    return ret;
}


static int aud_send_cmd(struct misc_ctl *misc_ctl, AUD_CMD_T command,void * parameter, uint32_t is_sync)
{
    int ret = 0;
    struct aud_cmd *cmd = (struct aud_cmd *)calloc(1, sizeof(struct aud_cmd));

    ALOGI("%s, cmd:%d",
        __func__, command);
    /* add this command to list, then send signal to offload thread to process the command list */
    cmd->cmd = command;
    cmd->is_sync = is_sync;
    cmd->param = parameter;
    sem_init(&cmd->sync_sem, 0, 0);
    pthread_mutex_lock(&misc_ctl->cmd_mutex);
    list_add_tail(&misc_ctl->cmd_list, &cmd->node);
    aud_signal(misc_ctl);
    pthread_mutex_unlock(&misc_ctl->cmd_mutex);
    if(is_sync) {
        sem_wait(&cmd->sync_sem);
        ret = cmd->ret;
        if(cmd->param) {
            free(cmd->param);
        }
        free(cmd);
    }
    return ret;
}


static int aud_cmd_process(struct misc_ctl *misc_ctl, struct aud_cmd  *cmd)
{
    int ret = 0;
    if(cmd == NULL) {
        return 0;
    }
    if(cmd) {
        ALOGI("cmd-cmd is %d",cmd->cmd);
        switch(cmd->cmd) {
            case AUD_CMD_FORCE_LOWPOWER:
                audio_offload_force_low_power();
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
            case AUD_CMD_CANCEL_LOWPOWER:
                audio_offload_release_low_power();
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
#if defined NORMAL_DISABLE_POWER_HINT && (defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q)
            case AUD_CMD_NORMA_ENABLE_POWERHINT:
                audio_normal_enable_power_hint();
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
            case AUD_CMD_NORMA_DISABLE_POWERHINT:
                audio_normal_disable_power_hint();
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
#endif
            case AUD_CMD_CLOSE:
                misc_ctl->state = 1;
                audio_offload_force_low_power();
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
            default:
                if(cmd->is_sync) {
                    cmd->ret = ret;
                    sem_post(&cmd->sync_sem);
                }
                else {
                    free(cmd);
                }
                break;
        }
    }
    return ret;
}


static void *thread_func(void * param)
{
    int ret = 0;
    ALOGE(" thread_func in");
    struct misc_ctl *misc_ctl  = (struct misc_ctl *)param;
    struct aud_cmd  *cmd;
    struct listnode *item;
    prctl(PR_SET_NAME, (unsigned long)"Audio Misc Ctrl", 0, 0, 0);
    if(!misc_ctl) {
            return NULL;
    }
    misc_ctl->state = 0;
    while(!misc_ctl->state) {
        cmd = NULL;
        pthread_mutex_lock(&misc_ctl->cmd_mutex);
        if (list_empty(&misc_ctl->cmd_list)) {
            ret = pthread_cond_wait(&misc_ctl->cmd_cond, &misc_ctl->cmd_mutex);
        }
        if (!list_empty(&misc_ctl->cmd_list)) {
            /* get the command from the list, then process the command */
            item = list_head(&misc_ctl->cmd_list);
            cmd = node_to_item(item, struct aud_cmd, node);
            list_remove(item);
        }
        pthread_mutex_unlock(&misc_ctl->cmd_mutex);

        if(cmd) {
            ret = aud_cmd_process(misc_ctl, cmd);
        }
    }
    ALOGI(" thread_func exit 1");
    while (!list_empty(&misc_ctl->cmd_list)) {
        struct aud_cmd  *cmd;
        struct listnode *item;
        /* get the command from the list, then process the command */
        item = list_head(&misc_ctl->cmd_list);
        cmd = node_to_item(item, struct aud_cmd, node);
        list_remove(item);
        if(cmd->is_sync) {
            cmd->ret = 0;
            sem_post(&cmd->sync_sem);
        }
        else {
            if(cmd->param) {
                free(cmd->param);
            }
            free(cmd);
        }
    }
    ALOGI(" thread_func exit 2");
    return NULL;
}

void * aud_misc_ctl_open()
{
    int ret = 0;
    struct misc_ctl *misc_ctl = NULL;
    misc_ctl = (struct misc_ctl *)calloc(1, sizeof(struct misc_ctl));
	if (!misc_ctl) {
		ALOGE("%s param is aud_misc_ctl_open NULL",__FUNCTION__);
		return NULL;
	}

    memset(misc_ctl,0, sizeof(struct misc_ctl));

    pthread_mutex_init(&misc_ctl->cmd_mutex, NULL);
    pthread_cond_init(&misc_ctl->cmd_cond, NULL);
    pthread_mutex_init(&misc_ctl->lock, NULL);

    list_init(&misc_ctl->cmd_list);
    pthread_attr_t threadattr;
    pthread_attr_init(&threadattr);
    pthread_attr_setdetachstate(&threadattr, PTHREAD_CREATE_JOINABLE);
    ret = pthread_create(&misc_ctl->thread, &threadattr,
    thread_func, (void *)misc_ctl);
    if (ret) {
        ALOGE("bt sco : duplicate thread create fail, code is %d", ret);
        goto error_fail;
    }
    pthread_attr_destroy(&threadattr);
    return (void *)misc_ctl;

error_fail:
    if(misc_ctl) {
        pthread_cond_destroy(&misc_ctl->cmd_cond);
        pthread_mutex_destroy(&misc_ctl->cmd_mutex);
        pthread_mutex_destroy(&misc_ctl->lock);
        free(misc_ctl);
    }
    return NULL;
}


int aud_misc_ctl_force_lowpower(void * misc)
{
    int ret = 0;
    struct misc_ctl *misc_ctl = (struct misc_ctl *)misc;
    if(!misc_ctl) {
        return -1;
    }
    pthread_mutex_lock(&misc_ctl->lock);
    ret = aud_send_cmd(misc_ctl, AUD_CMD_FORCE_LOWPOWER,NULL, 0);
    misc_ctl->mp3_lowpower = 1;
    pthread_mutex_unlock(&misc_ctl->lock);
    return ret;
}

int aud_misc_ctl_cancel_lowpower(void * misc)
{
    int ret = 0;
    struct misc_ctl *misc_ctl = (struct misc_ctl *)misc;
    if(!misc_ctl) {
        return -1;
    }
    pthread_mutex_lock(&misc_ctl->lock);
    if(misc_ctl->mp3_lowpower == 1) {
        misc_ctl->mp3_lowpower = 0;
        ret = aud_send_cmd(misc_ctl, AUD_CMD_CANCEL_LOWPOWER,NULL, 0);
    }
    pthread_mutex_unlock(&misc_ctl->lock);
    return ret;
}

void aud_misc_ctl_close(void * misc)
{
    int ret = 0;
    struct misc_ctl *misc_ctl = (struct misc_ctl *)misc;
    if(!misc_ctl) {
        return;
    }
    do{
        ret = pthread_mutex_trylock(&misc_ctl->lock);
        if(ret) {
            ALOGE("aud_misc_ctl_close but is busy");
            usleep(20000);
        }
    }while(ret);
    aud_send_cmd(misc_ctl, AUD_CMD_CLOSE,NULL,1);

    if(misc_ctl->thread) {
        pthread_join(misc_ctl->thread, (void **) NULL);
    }
    pthread_cond_destroy(&misc_ctl->cmd_cond);
    pthread_mutex_destroy(&misc_ctl->cmd_mutex);
    pthread_mutex_destroy(&misc_ctl->lock);
    return;
}

int aud_misc_ctl_disable_pownerhint(void * misc)
{
#if defined NORMAL_DISABLE_POWER_HINT && (defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q)
    int ret = 0;
    struct misc_ctl *misc_ctl = (struct misc_ctl *)misc;
    if(!misc_ctl) {
        return -1;
    }
    pthread_mutex_lock(&misc_ctl->lock);
    ret = aud_send_cmd(misc_ctl, AUD_CMD_NORMA_DISABLE_POWERHINT,NULL, 0);
    misc_ctl->normal_powerhint = true;
    pthread_mutex_unlock(&misc_ctl->lock);
    return ret;
#else
    return 0;
#endif
}

int aud_misc_ctl_enable_pownerhint(void * misc)
{
#if defined NORMAL_DISABLE_POWER_HINT && (defined VERSION_IS_ANDROID_P || defined VERSION_IS_ANDROID_Q)
    int ret = 0;
    struct misc_ctl *misc_ctl = (struct misc_ctl *)misc;
    if(!misc_ctl) {
        return -1;
    }
    pthread_mutex_lock(&misc_ctl->lock);
    if(misc_ctl->normal_powerhint == true) {
        misc_ctl->normal_powerhint = false;
        ret = aud_send_cmd(misc_ctl, AUD_CMD_NORMA_ENABLE_POWERHINT,NULL, 0);
    }
    pthread_mutex_unlock(&misc_ctl->lock);
    return ret;
#else
    return 0;
#endif
}
}
