#define LOG_TAG  "audio_hw_lib"
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include"unistd.h"
#include"sys/types.h"
#include"fcntl.h"
#include"stdio.h"
#include <errno.h>
#ifdef AUDIOHAL_V4
#include <log/log.h>
#else
#include <utils/Log.h>
#endif
#define AUDIO_EXT_CONTROL_PIPE "/dev/pipe/mmi.audio.ctrl"
#ifdef AUDIOHAL_V4
#define AUDIO_EXT_DATA_CONTROL_PIPE "/data/vendor/local/media/mmi.audio.ctrl"
#else
#define AUDIO_EXT_DATA_CONTROL_PIPE "/data/local/media/mmi.audio.ctrl"
#endif
int SendAudioTestCmd(char *cmd, int bytes)
{
    int fd = -1;
    int ret = -1;
    int bytes_to_read = bytes;
    char *write_ptr = NULL;
    int writebytes = 0;
    if (cmd == NULL)
    {
        ALOGW("SendAudioTestCmd cmd is null");
        return -1;
    }

    fd = open(AUDIO_EXT_CONTROL_PIPE, O_WRONLY);
    if (fd < 0)
    {
        fd = open(AUDIO_EXT_DATA_CONTROL_PIPE, O_WRONLY);
        ALOGI("SendAudioTestCmd open %s",AUDIO_EXT_DATA_CONTROL_PIPE);
    }else{
        ALOGI("SendAudioTestCmd open %s success",AUDIO_EXT_CONTROL_PIPE);
    }

    if (fd < 0)
    {
        ALOGW("SendAudioTestCmd open pipe failed errno=%d errr msg:%s",errno,strerror(errno));
        return -1;
    }
    else
    {
        write_ptr = cmd;
        writebytes = 0;
        do
        {
            writebytes = write(fd, write_ptr, bytes);
            if(writebytes == bytes)
            {
                bytes = 0;
                break;
            }
            else
            {
                if (ret > 0)
                {
                    if (writebytes <= bytes)
                    {
                        bytes -= writebytes;
                        write_ptr += writebytes;
                    }
                }
                else if ((!((errno == EAGAIN) || (errno == EINTR))) || (0 == ret))
                {
                    ALOGE("pipe write error %d, bytes read is %d", errno, bytes_to_read - bytes);
                    break;
                }
            }
        }
        while (bytes);
    }

    if (fd > 0)
    {
        close(fd);
    }

    if (bytes == bytes_to_read)
        return ret;
    else
        return (bytes_to_read - bytes);
}

unsigned char *hex_to_string(unsigned char *data, int size)
{
    int  i = 0;
    unsigned char ch;
    unsigned char *out_hex_ptr = NULL;
    unsigned char *hex_ptr = (unsigned char *)malloc(size * 2 + 1);
    if(NULL == hex_ptr)
    {
        return NULL;
    }
    out_hex_ptr = hex_ptr;
    memset(hex_ptr, 0, (size * 2 + 1));

    for(i = 0; i < size; i++)
    {
        ch = (char)((data[i] & 0xf0) >> 4);

        if(ch <= 9)
        {
            *hex_ptr = (char)(ch + '0');
        }
        else
        {
            *hex_ptr = (char)(ch + 'a' - 10);
        }

        hex_ptr++;

        ch = (char)(data[i] & 0x0f);


        if(ch <= 9)
        {
            *hex_ptr = (char)(ch + '0');
        }
        else
        {
            *hex_ptr = (char)(ch + 'a' - 10);

        }
        hex_ptr++;
    }
    return out_hex_ptr;

}

void dump_data(char *tag, char *buf, int len)
{
    int i = len;
    int line = 0;
    int size = 0;
    int j = 0;
    char dump_buf[60] = {0};
    int dump_buf_len = 0;

    char *tmp = (char *) buf;
    line = i / 16 + 1;

    for(i = 0; i < line; i++)
    {
        dump_buf_len = 0;
        memset(dump_buf, 0, sizeof(dump_buf));

        if(i < line - 1)
        {
            size = 16;
        }
        else
        {
            size = len % 16;
        }
        tmp = (char *)buf + i * 16;

        sprintf(dump_buf + dump_buf_len, "%04x: ", i * 16);
        dump_buf_len = 5;

        for(j = 0; j < size; j++)
        {
            sprintf(dump_buf + dump_buf_len, " %02x", tmp[j]);
            dump_buf_len += 3;
        }
        if(NULL != tag)
        {
            ALOGI("%s:%s\n", tag, dump_buf);
        }
        else
        {
            ALOGI("%s\n", dump_buf);
        }
    }
}
