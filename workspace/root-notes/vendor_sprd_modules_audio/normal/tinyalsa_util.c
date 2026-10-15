/*
 * Copyright (C) 2012 The Android Open Source Project
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

//#define ALOG_NDEBUG 0
#define LOG_TAG "audio_hw:Alsa_Util"
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <utils/Log.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <sys/mman.h>
#include <sys/time.h>
#include <limits.h>

#include <linux/ioctl.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

static volatile int log_level = 3;
#define LOG_V(...)  ALOGV_IF(log_level >= 5,__VA_ARGS__);
#define LOG_D(...)  ALOGD_IF(log_level >= 4,__VA_ARGS__);
#define LOG_I(...)  ALOGI_IF(log_level >= 3,__VA_ARGS__);
#define LOG_W(...)  ALOGW_IF(log_level >= 2,__VA_ARGS__);
#define LOG_E(...)  ALOGE_IF(log_level >= 1,__VA_ARGS__);


#define ALSA_FOLDER_PATH  "/proc/asound"
#define CARDS_FILE_PATH   "/proc/asound/cards"
#define DEVICES_FILE_PATH  "/proc/asound/devices"
#define USBCARDKEYSTR      "at usb-"
#define SPLIT "\n"
#define PLAYBACKKEYSTR     "audio playback"


#define ALSA_DEVICE_DIRECTORY "/dev/snd/"

#define SND_FILE_CONTROL	ALSA_DEVICE_DIRECTORY "controlC%i"

struct snd_ctl_card_info_t {
	int card;			/* card number */
	int pad;			/* reserved for future (was type) */
	unsigned char id[16];		/* ID of card (user selectable) */
	unsigned char driver[16];	/* Driver name */
	unsigned char name[32];		/* Short name of soundcard */
	unsigned char longname[80];	/* name + info text about soundcard */
	unsigned char reserved_[16];	/* reserved for future (was ID of mixer) */
	unsigned char mixername[80];	/* visual mixer identification */
	unsigned char components[128];	/* card components / fine identification, delimited with one space (AC97 etc..) */
};

static int get_snd_card_name(int card, char *name);
void tinymix_detail_control(struct mixer *mixer, const char *control,
                                   int print_all,char * buf, unsigned int  *bytes);

static char *mystrstr(char *s1 , char *s2)
{
     if (*s1 == 0) {
        if (*s2) return(char*)NULL;
            return (char*)s1;
    }
    while (*s1) {
        int i = 0;
        while (1) {
            if (s2[i] == 0) return s1;
            if (s2[i] != s1[i]) break;
            i++;
        }
        s1++;
    }
    return (char*)NULL;
}


int usb_card_parse(char * namebuf, int namebuf_len) {
    FILE *pfile;
    char *line = NULL, *line2= NULL;
    char *substr1 = NULL,*substr2 = NULL;
    int buffer_size=2048;
    int cardnum = -1;
    char *token = NULL;
    char *subline = NULL;
    char filename[100] = {0};
    char * buffer = NULL;
    bool card_find = false;
    char *cardname = NULL;

    buffer = (char *)malloc(buffer_size);
    if (NULL == buffer) {
        LOG_E("%s malloc  failed\n",__FUNCTION__);
        return -1;
    }
    memset(buffer, 0, buffer_size);
    if ((pfile = (FILE *)fopen(CARDS_FILE_PATH, "rt")) == NULL) {
        free(buffer);
        return -1;
    }
    fread(buffer, 1, buffer_size, pfile);
    fclose(pfile);
    line = strtok_r(buffer, SPLIT, &substr1);
    while (line && substr1) {
        token = strtok_r(line," ", &subline);
        if (token) {
            cardnum = strtoul(token, NULL, 0);
            card_find = true;
            token = strtok_r(subline, ":", &cardname);
            if (cardname) {
                cardname += strspn(cardname, " ");
            }
        }
        line2 = strtok_r(substr1, SPLIT, &substr2);
        if (card_find && mystrstr(line2, (char *)USBCARDKEYSTR)) {
            snprintf(filename, sizeof(filename), "/proc/asound/card%d/usbbus", cardnum);
            if ((pfile = (FILE *)fopen(filename, "r")) != NULL) {
                fclose(pfile);
                if (namebuf_len >= (strlen(cardname) + 1)) {
                    memcpy(namebuf, cardname, strlen(cardname));
                    namebuf[strlen(cardname)] = '\0';
                }
                else {
                    LOG_E("cardname :%s is longer then namebuf",cardname);
                }
                LOG_I("usb_card_parse usb card check ok:num:%d, name:%s", cardnum, cardname);
                free(buffer);
                return cardnum;
            }
            card_find = false;
        }
        if (substr1 && substr2) {
            line = strtok_r(substr2, SPLIT, &substr1);
        } else {
            break;
        }
    }
    free(buffer);
    LOG_I(" usb card check not foundbuffer:%s",buffer);
    return -1;
}

int usb_pdev_parse(int32_t card) {
    FILE *pfile;
    char *line = NULL;
    char *substr = NULL;
    int buffer_size=2048;
    int devicenum = -1;
    int cardnum = -1;
    char *token = NULL;
    char *subline = NULL;
    char filename[100] = {0};
    char *buffer = NULL;

    buffer = (char *)malloc(buffer_size);
    if (NULL == buffer) {
        printf("%s malloc :%d size failed\n", __FUNCTION__, buffer_size);
        return -1;
    }
    memset(buffer, 0, buffer_size);
    if ((pfile = (FILE *)fopen(DEVICES_FILE_PATH, "rt")) == NULL){
        free(buffer);
        return -1;
    }
    fread(buffer, 1, buffer_size, pfile);
    fclose(pfile);
    line = strtok_r(buffer, SPLIT, &substr);
    while (line) {
        if (!mystrstr(line, (char *)PLAYBACKKEYSTR)) {
            line = strtok_r(substr, SPLIT, &substr);
            continue;
        }
        token = strtok_r(line, "[", &subline);
        if (!subline) {
            line = strtok_r(substr, SPLIT, &substr);
            continue;
        }
        token = strtok_r(subline, "-", &subline);
        if (!token) {
            line = strtok_r(substr, SPLIT, &substr);
            continue;
        }
        cardnum = strtoul(token, NULL, 0);
        if (cardnum != card) {
            line = strtok_r(substr, SPLIT, &substr);
            continue;
        }
        token = strtok_r(subline," ", &subline);
        token = strtok_r(subline,"]", &subline);
        if(!token) {
            line = strtok_r(substr, SPLIT, &substr);
            continue;
        }
        devicenum = strtoul(token, NULL, 0);
        snprintf(filename,sizeof(filename),"/proc/asound/card%d/pcm%dp", card, devicenum);
        if ((pfile=(FILE *)fopen(filename, "r")) != NULL){
            fclose(pfile);
            LOG_I("usb_pdev_parse usb device check ok:%d", devicenum);
            free(buffer);
            return devicenum;
        }

        line = strtok_r(substr, SPLIT, &substr);
    }
    LOG_I("card:%d,usb_pdev_parse:no usb device", card);
    free(buffer);
    return -1;
}


int get_snd_card_number(const char *card_name)
{
    int i = 0;
    int ret = 0;
    char cur_name[64] = {0};

    //loop search card number, which is in the ascending order.
    for (i = 0; i < 32; i++) {
        ret = get_snd_card_name(i, &cur_name[0]);
        if (ret < 0)
            continue;
        if (strcmp(cur_name, card_name) == 0) {
            ALOGI("Search Completed, cur_name is %s, card_num=%d", cur_name, i);
            return i;
        }
    }
    ALOGE("There is no one matched to <%s>.", card_name);
    return -1;
}

static int get_snd_card_name(int card, char *name)
{
    int fd;
    struct snd_ctl_card_info_t info;
    char control[sizeof(SND_FILE_CONTROL) + 10] = {0};
    sprintf(control, SND_FILE_CONTROL, card);

    fd = open(control, O_RDONLY);
    if (fd < 0) {
        ALOGE("open snd control failed.");
        return -1;
    }
    if (ioctl(fd, SNDRV_CTL_IOCTL_CARD_INFO, &info) < 0) {
        ALOGE("SNDRV_CTL_IOCTL_CARD_INFO failed.");
        close(fd);
        return -1;
    }
    close(fd);
    ALOGI("card name is %s, query card=%d", info.name, card);
    //get card name
    if (name) strcpy(name, (char *)info.name);
    return 0;
}

 void tinymix_list_controls(struct mixer *mixer,char * buf, unsigned int  *bytes)
{
    struct mixer_ctl *ctl;
    const char *name, *type;
    unsigned int num_ctls, num_values;
    unsigned int i;
    unsigned int size = *bytes;
    unsigned int cur=0;
    unsigned int cur1=0;
    unsigned cur_len = 0;

    num_ctls = mixer_get_num_ctls(mixer);

    ALOGD("Number of controls: %d\n", num_ctls);
    cur = snprintf(buf,size,"Number of controls: %d\n", num_ctls);
    ALOGD("ctl\ttype\tnum\t%-40s value\n", "name");
    cur += snprintf(buf+cur,size,"ctl\ttype\tnum\t%-40s value\n", "name");
    for (i = 0; i < num_ctls; i++) {
        ctl = mixer_get_ctl(mixer, i);

        name = mixer_ctl_get_name(ctl);
        type = mixer_ctl_get_type_string(ctl);
        num_values = mixer_ctl_get_num_values(ctl);
        cur1 = snprintf(buf+cur,size,"%d\t%s\t%d\t%-40s", i, type, num_values, name);
        cur_len = size - (cur+cur1);
        tinymix_detail_control(mixer, name, 0,buf+cur+cur1,&cur_len);
        cur += (cur1 +cur_len);
    }
    *bytes = cur;
}

 void tinymix_print_enum(struct mixer_ctl *ctl, int print_all,char * buf, unsigned int  *bytes)
{
    unsigned int num_enums;
    unsigned int i;
    const char *string;
    unsigned int size = *bytes;
    unsigned int cur=0;

    num_enums = mixer_ctl_get_num_enums(ctl);

    for (i = 0; i < num_enums; i++) {
        string = mixer_ctl_get_enum_string(ctl, i);
        if (print_all)
            ALOGD("\t%s%s", mixer_ctl_get_value(ctl, 0) == (int)i ? ">" : "",
                   string);
        else if (mixer_ctl_get_value(ctl, 0) == (int)i) {
            cur += snprintf(buf+cur,size - cur," %-s", string);
        }
    }
    *bytes = cur;
}

 void tinymix_detail_control(struct mixer *mixer, const char *control,
                                   int print_all,char * buf, unsigned int  *bytes)
{
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_values;
    unsigned int i;
    int min, max;
    unsigned int size = *bytes;
    unsigned int cur=0;
    unsigned cur_len = 0;

    if (isdigit(control[0]))
        ctl = mixer_get_ctl(mixer, atoi(control));
    else
        ctl = mixer_get_ctl_by_name(mixer, control);

    if (!ctl) {
        ALOGE("Invalid mixer control\n");
        return;
    }

    type = mixer_ctl_get_type(ctl);
    num_values = mixer_ctl_get_num_values(ctl);

    if (print_all)
        ALOGD("%s:", mixer_ctl_get_name(ctl));

    for (i = 0; i < num_values; i++) {
        switch (type)
        {
        case MIXER_CTL_TYPE_INT:
            cur += snprintf(buf+cur,size-cur," %d", mixer_ctl_get_value(ctl, i));
            break;
        case MIXER_CTL_TYPE_BOOL:
            cur += snprintf(buf+cur,size-cur," %s", mixer_ctl_get_value(ctl, i) ? "On" : "Off");
            break;
        case MIXER_CTL_TYPE_ENUM:
            cur_len = size - cur;
            tinymix_print_enum(ctl, print_all,buf + cur, &cur_len);
            cur += cur_len;
            break;
         case MIXER_CTL_TYPE_BYTE:
            cur += snprintf(buf+cur,size-cur," 0x%02x", mixer_ctl_get_value(ctl, i));
            break;
        default:
            cur += snprintf(buf+cur,size-cur," unknown");
            break;
        };
    }

    if (print_all) {
        if (type == MIXER_CTL_TYPE_INT) {
            min = mixer_ctl_get_range_min(ctl);
            max = mixer_ctl_get_range_max(ctl);
            ALOGD(" (range %d->%d)", min, max);
        }
    }
    cur += snprintf(buf+cur,size-cur,"\n");
    *bytes = cur;
}

 void tinymix_set_value(struct mixer *mixer, const char *control,
                              char **values, unsigned int num_values)
{
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_ctl_values;
    unsigned int i;

    if (isdigit(control[0]))
        ctl = mixer_get_ctl(mixer, atoi(control));
    else
        ctl = mixer_get_ctl_by_name(mixer, control);

    if (!ctl) {
        ALOGE("Invalid mixer control\n");
        return;
    }

    type = mixer_ctl_get_type(ctl);
    num_ctl_values = mixer_ctl_get_num_values(ctl);

    if (isdigit(values[0][0])) {
        if (num_values == 1) {
            /* Set all values the same */
            int value = atoi(values[0]);

            for (i = 0; i < num_ctl_values; i++) {
                if (mixer_ctl_set_value(ctl, i, value)) {
                    ALOGE("Error: invalid value\n");
                    return;
                }
            }
        } else {
            /* Set multiple values */
            if (num_values > num_ctl_values) {
                ALOGE(
                        "Error: %d values given, but control only takes %d\n",
                        num_values, num_ctl_values);
                return;
            }
            for (i = 0; i < num_values; i++) {
                if (mixer_ctl_set_value(ctl, i, atoi(values[i]))) {
                    ALOGE("Error: invalid value for index %d\n", i);
                    return;
                }
            }
        }
    } else {
        if (type == MIXER_CTL_TYPE_ENUM) {
            if (num_values != 1) {
                ALOGE("Enclose strings in quotes and try again\n");
                return;
            }
            if (mixer_ctl_set_enum_by_string(ctl, values[0]))
                ALOGE("Error: invalid enum value\n");
        } else {
            ALOGE("Error: only enum types can be set with strings\n");
        }
    }
}


