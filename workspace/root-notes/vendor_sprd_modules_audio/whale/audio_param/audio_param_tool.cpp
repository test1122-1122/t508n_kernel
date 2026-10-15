/*
* Copyright (C) 2010 The Android Open Source Project
* Copyright (C) 2012-2015, The Linux Foundation. All rights reserved.
*
* Not a Contribution, Apache license notifications and license are retained
* for attribution purposes only.
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
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>

#include "audio_xml_utils.h"
#include "audio_param.h"
#include <sys/stat.h>


#include "fcntl.h"
#include "aud_proc.h"
#include "audio_xml_utils.h"
#include "audio_param.h"
#include "tinyalsa_util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "tinyxml.h"
#include <tinyalsa/asoundlib.h>
#include "stdint.h"
#include "audio_debug.h"

#define SHELL_DBG(...)   fprintf(stdout, ##__VA_ARGS__)

#define TOOL_LOG_D  SHELL_DBG
#define TOOL_LOG_E  SHELL_DBG
int log_level = 3;

int load_xml_handle(struct xml_handle *xmlhandle, const char *xmlpath)
{
    TOOL_LOG_D("load_xml_handle:%s",xmlpath);
    TiXmlElement *root=NULL;
    xmlhandle->param_doc = XML_open_param(xmlpath);
    if (xmlhandle->param_doc == NULL) {
        TOOL_LOG_E("load xml handle failed (%s)", xmlpath);
        return -1;
    }

    xmlhandle->param_root =  XML_get_root_param(xmlhandle->param_doc);
    root = (TiXmlElement *)xmlhandle->param_root;
    if (root != NULL) {
        root = (TiXmlElement *)xmlhandle->param_root;
        xmlhandle->first_name = strdup(root->Value());
    }
    return 0;
}

void release_xml_handle(struct xml_handle *xmlhandle)
{
    if (xmlhandle->param_doc) {
        XML_release_param(xmlhandle->param_doc);
        xmlhandle->param_doc = NULL;
        xmlhandle->param_root = NULL;
    }
    if (xmlhandle->first_name) {
        free(xmlhandle->first_name);
        xmlhandle->first_name = NULL;
    }
}

#if 0
int _load_audio_param_from_bin(AUDIOVBCEQ_PARAM_T *param,
                               TiXmlElement *param_group, bool root)
{
    const char *tmp = NULL;
    int id = -1, offset = -1, bits = -1, val = 0, values = 0;
    const char *visible;
    int pre_val = 0;

    TiXmlElement *group = (TiXmlElement *)param_group;
    TiXmlElement *parent_group = NULL;
    int size = 0;
    int mask = 0;
    int i = 0;
    char val_str[32] = {0};

    while(NULL != group) {
        // SHELL_DBG("%s\n",group->Value());
        tmp = group->Attribute(VALUE);
        if (tmp == NULL) {
            // SHELL_DBG("\n");
            _load_audio_param_from_bin(param, group->FirstChildElement(), false);
        } else {
            //        SHELL_DBG("%s ",group->Values());
            //SHELL_DBG("%s %d\n",__func__,__LINE__);

            tmp = group->Attribute(OFFSETS);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -2;
            }
            offset = strtoul(tmp, NULL, 16);

            tmp = group->Attribute(BITS);
            if(NULL == tmp) {
                return -3;
            }
            bits = strtoul(tmp, NULL, 16);

            tmp = group->Attribute(ID);

            if(NULL == tmp) {
                parent_group = (TiXmlElement *)group->Parent();
                if(NULL == parent_group) {
                    SHELL_DBG("set_ele_value find parent_group failed\n");
                    return -2;
                }

                tmp = parent_group->Attribute(TYPE);
                if(NULL == tmp) {
                    SHELL_DBG("%s %d\n", __func__, __LINE__);
                    return -4;
                }

                tmp = parent_group->Attribute(ID);
                if(NULL == tmp) {
                    SHELL_DBG("%s %d\n", __func__, __LINE__);
                    return -1;
                }
                id = strtoul(tmp, NULL, 16);
            } else {
                tmp = group->Attribute(TYPE);
                if(NULL == tmp) {
                    SHELL_DBG("%s %d\n", __func__, __LINE__);
                    return -4;
                }

                tmp = group->Attribute(ID);
                if(NULL == tmp) {
                    SHELL_DBG("%s %d\n", __func__, __LINE__);
                    return -1;
                }
                id = strtoul(tmp, NULL, 16);
            }

            if(strcmp(tmp, U32) == 0) {
                size = sizeof(int32_t);
            } else if(strcmp(tmp, U16) == 0) {
                size = sizeof(int16_t);
            } else if(strcmp(tmp, U8) == 0) {
                size = sizeof(uint8_t);
            }

            values = 0;

            //SHELL_DBG("%s bits:%d offset:%d\n",group->Value(),bits,offset);
            memcpy(&values, param->data + offset, size);



            for(i = 0; i < bits; i++) {
                mask |= 1 << i;
            }

            val = (values >> offset) & (mask);


            tmp = group->Attribute(VALUE);
            if(NULL == tmp) {
                return -3;
            }
            pre_val = strtoul(tmp, NULL, 16);

            if(pre_val != val) {
                SHELL_DBG("%s SetAttribute :0x%x to 0x%x\n", group->Value(), pre_val, val);
            }

            sprintf(val_str, "0x%x", val);

            group->SetAttribute(VALUE, val_str);

        }

        if (root) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            break;
        }

        group = group->NextSiblingElement();
    }
    return 0;
}
#else
int _load_audio_param_from_bin(AUDIOVBCEQ_PARAM_T *param,
                               TiXmlElement *param_group, bool root)
{
    const char *tmp = NULL;
    int id = -1, offset = -1, bits = -1, val = 0, values = 0;
    int pre_val = 0;

    TiXmlElement *group = (TiXmlElement *)param_group;
    TiXmlElement *parent_group = NULL;
    TiXmlElement *type_ele = NULL;
    int size = 0;
    int mask = 0;
    int i = 0;
    char val_str[32] = {0};

    while(NULL != group) {
        tmp = group->Attribute(VALUE);
        if (tmp == NULL) {
            _load_audio_param_from_bin(param, group->FirstChildElement(), false);
        } else {

            tmp = group->Attribute(TYPE);
            if(NULL == tmp) {

                parent_group = (TiXmlElement *)group->Parent();
                if(NULL == parent_group) {
                    SHELL_DBG("set_ele_value find parent_group failed\n");
                    return -2;
                }

                type_ele = parent_group;
            } else {
                type_ele = group;
            }

            tmp = type_ele->Attribute(TYPE);
            if(strcmp(tmp, U32) == 0) {
                size = sizeof(int32_t);
            } else if(strcmp(tmp, U16) == 0) {
                size = sizeof(int16_t);
            } else if(strcmp(tmp, U8) == 0) {
                size = sizeof(uint8_t);
            }

            tmp = type_ele->Attribute(ID);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -1;
            }
            id = string_to_value(tmp);


            tmp = group->Attribute(OFFSETS);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -2;
            }
            offset = string_to_value(tmp);

            tmp = group->Attribute(BITS);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -3;
            }
            bits = string_to_value(tmp);

            values = 0;

            memcpy(&values, param->data + offset, size);

            for(i = 0; i < bits; i++) {
                mask |= 1 << i;
            }

            val = (values >> offset) & (mask);


            tmp = group->Attribute(VALUE);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -3;
            }
            pre_val = string_to_value(tmp);

            if(pre_val != val) {
                SHELL_DBG("%s SetAttribute :0x%x to 0x%x values:0x%x bits:%x offset:%x\n",
                          group->Value(), pre_val, val,
                          values, bits, offset);
            }

            sprintf(val_str, "0x%x", val);

            group->SetAttribute(VALUE, val_str);

        }

        if (root) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            break;
        }

        group = group->NextSiblingElement();
    }
    return 0;
}

#endif

TiXmlElement *private_get_param(param_group_t group, const char *param_name);
int private_crate_group(param_group_t group, const char *ele_name,param_group_t 
new_group);


static int _clone_audio_param(TiXmlElement *group, bool root,
                              TiXmlElement *src_group)
{
    const char *tmp = NULL;
    TiXmlElement *Element = (TiXmlElement *)group;
    TiXmlElement *parent_group = NULL;

    while(NULL != Element) {
        tmp = Element->Attribute("mode");
        if(tmp != NULL) {
            _clone_audio_param(Element->FirstChildElement(), false, src_group);
        } else {

            if(src_group != Element) {
                parent_group = (TiXmlElement *)Element->Parent();
                if(NULL == parent_group) {
                    SHELL_DBG("set_ele_value find parent_group failed\n");
                    return -4;
                }

                parent_group->ReplaceChild(Element, *src_group);
            }
        }

        if(root) {
            break;
        }
        Element = Element->NextSiblingElement();
    }

    return 0;
}

int clone_audio_param(const char *src_xml, const char *dst_xml,
                      const char *src_name , const char *dst_name)
{
    struct xml_handle src_xml_handle;
    struct xml_handle dst_xml_handle;

    TiXmlElement *group = NULL;

    TiXmlElement *src_group = NULL;
    TiXmlElement *dst_group = NULL;
    TiXmlElement *parent_group = NULL;
    TiXmlElement * clone_src = NULL;

    TiXmlElement *root = NULL;

    const char *tmp = NULL;

    int ret = 0;
    memset(&src_xml_handle,0,sizeof(src_xml_handle));
    memset(&dst_xml_handle,0,sizeof(dst_xml_handle));

    ret = load_xml_handle(&src_xml_handle, src_xml);
    if (ret != 0) {
        SHELL_DBG("load xml handle failed (%s)", src_xml);
        return ret;
    }

    group = (TiXmlElement *)src_xml_handle.param_root;
    if(NULL == group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -1;
    }


    src_group = (TiXmlElement *)private_get_param(src_xml_handle.param_root, src_name);
    if(NULL == src_group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -2;
    }

    if(0 == strncmp(dst_name, "all", strlen("all"))) {
        _clone_audio_param((TiXmlElement *)src_xml_handle.param_root, true, src_group);
        memcpy(&dst_xml_handle,&src_xml_handle, sizeof(struct xml_handle));
    } else {
        const char *mode_str=NULL;
        ret = load_xml_handle(&dst_xml_handle, dst_xml);
        if (ret != 0) {
            SHELL_DBG("load xml handle failed (%s)", dst_xml);
            return ret;
        }

        if(NULL == dst_xml_handle.param_root) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            return -1;
        }

        dst_group = (TiXmlElement *)private_get_param(dst_xml_handle.param_root, dst_name);
        if(NULL == dst_group) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            return -3;
        }

        parent_group = (TiXmlElement *)dst_group->Parent();
        if(NULL == parent_group) {
            SHELL_DBG("set_ele_value find parent_group failed\n");
            return -4;
        }

        clone_src= new TiXmlElement(*src_group);
        if(NULL==clone_src){
            TOOL_LOG_D("_clone_xml_audioparam clone failed\n");
            ret=-3;
        }

        mode_str = dst_group->Attribute("mode");
        if(NULL!=mode_str){
            clone_src->SetAttribute("mode",mode_str);
        }
        clone_src->SetValue(dst_group->Value());

        SHELL_DBG("value:%s mode:%s\n",clone_src->Value(),clone_src->Attribute("mode"));
        parent_group->ReplaceChild(dst_group, *clone_src);
    }

    //SHELL_DBG("%s %d\n", __func__, __LINE__);
    if(NULL != dst_xml_handle.param_doc) {

        root = (TiXmlElement *)dst_xml_handle.param_root;
        tmp = root->Attribute(STRUCT_SIZE);
        if(tmp != NULL) {
            root->RemoveAttribute(STRUCT_SIZE);
        }

        tmp = root->Attribute(NUM_MODE);
        if(tmp != NULL) {
            root->RemoveAttribute(NUM_MODE);
        }

        XML_save_param(dst_xml_handle.param_doc,
                       dst_xml);
        SHELL_DBG("\nclone:%s %s %s %s Success\n",src_xml, dst_xml,src_name,dst_name);
    }
    return 0;
}


int Insert_audio_param(const char *src_xml, const char *dst_xml,
                       const char *src_name , const char *dst_name)
{

    AUDIOVBCEQ_PARAM_T param;
    TiXmlElement *group = NULL;

    TiXmlElement *src_group = NULL;
    TiXmlElement *dst_group = NULL;
    TiXmlElement *parent_group = NULL;

    TiXmlElement *root = NULL;

    const char *tmp = NULL;

    int ret = 0;

    memset(&param, 0, sizeof(AUDIOVBCEQ_PARAM_T));

    ret = load_xml_handle(&param.xml, src_xml);
    if (ret != 0) {
        SHELL_DBG("load xml handle failed (%s)", src_xml);
        return ret;
    }

    group = (TiXmlElement *)param.xml.param_root;
    if(NULL == group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -1;
    }

    src_group = (TiXmlElement *)private_get_param(group, src_name);
    if(NULL == src_group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -2;
    }

    dst_group = (TiXmlElement *)private_get_param(group, dst_name);
    if(NULL == dst_group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -3;
    }

    parent_group = (TiXmlElement *)dst_group->Parent();
    if(NULL == parent_group) {
        SHELL_DBG("set_ele_value find parent_group failed\n");
        return -4;
    }

    //SprdReplaceChild(dst_group,src_group);
    parent_group->InsertAfterChild(dst_group, *src_group);

    SHELL_DBG("%s %d\n", __func__, __LINE__);
    if(NULL != param.xml.param_doc) {

        root = (TiXmlElement *)param.xml.param_root;
        tmp = root->Attribute(STRUCT_SIZE);
        if(tmp != NULL) {
            root->RemoveAttribute(STRUCT_SIZE);
        }

        tmp = root->Attribute(NUM_MODE);
        if(tmp != NULL) {
            root->RemoveAttribute(NUM_MODE);
        }

        XML_save_param(param.xml.param_doc,
                       dst_xml);
    }
    return 0;
}

static int _Division_audio_param(TiXmlElement *group, bool root, char *path,
                                 const char *dst_dir)
{
    const char *tmp = NULL;
    int path_len = 0;
    TiXmlElement *Element = (TiXmlElement *)group;

    while(NULL != Element) {
        tmp = Element->Attribute(ID);
        if(tmp != NULL) {
            strcat(path, "_");
            strcat(path, Element->Value());
            _Division_audio_param(Element->FirstChildElement(), false, path, dst_dir);
        } else {
            TiXmlDocument doc;
            TiXmlDeclaration *dec = new TiXmlDeclaration("1.0", "gb2312", "");
            doc.LinkEndChild(dec);
            doc.LinkEndChild(Element);
            strcat(path, ".xml");
            doc.SaveFile(path);
            delete dec;
            path_len = strlen(path) + 1;
            memset(path, 0, path_len);
            strcat(path, dst_dir);
        }

        if(root) {
            break;
        }
        Element = Element->NextSiblingElement();
    }

    return 0;
}

int Division_audio_param(const char *src_xml, const char *dst_dir)
{
    AUDIOVBCEQ_PARAM_T param;
    TiXmlElement *group = NULL;

    char path[1024] = {0};

    int ret = 0;

    memset(&param, 0, sizeof(AUDIOVBCEQ_PARAM_T));

    ret = load_xml_handle(&param.xml, src_xml);
    if (ret != 0) {
        SHELL_DBG("load xml handle failed (%s)", src_xml);
        return ret;
    }

    group = (TiXmlElement *)param.xml.param_root;
    if(NULL == group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -1;
    }

    strcat(path, dst_dir);

    _Division_audio_param(group, true, path, dst_dir);
    return 0;
}

static int _read_param_form_bin(TiXmlElement *group, char *data, bool root)
{
    TiXmlElement *Element = (TiXmlElement *)group;
    TiXmlElement *child = NULL;
    const char *tmp = NULL;
    char val_string[16] = {0};

    while(NULL != Element) {
        tmp = Element->Attribute(ID);
        if(tmp == NULL) {
            _read_param_form_bin(Element->FirstChildElement(), data, false);
        } else {
            int size = 0;
            int addr_offset = 0;
            int values = 0;

            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -1;
            }
            addr_offset = string_to_value(tmp);

            tmp = Element->Attribute(TYPE);
            if(NULL == tmp) {
                SHELL_DBG("%s %d\n", __func__, __LINE__);
                return -1;
            }

            if(strcmp(tmp, U32) == 0) {
                size = sizeof(int32_t);
            } else if(strcmp(tmp, U16) == 0) {
                size = sizeof(int16_t);
            } else if(strcmp(tmp, U8) == 0) {
                size = sizeof(uint8_t);
            }

            values = 0;

            memcpy(&values, data + addr_offset, size);

            child = Element->FirstChildElement();

            if(NULL != child) {
                while(NULL != child) {
                    int bits = 0;
                    int offset = 0;
                    int val = 0;
                    int i = 0;
                    int mask = 0;

                    tmp = child->Attribute(BITS);
                    if(NULL == tmp) {
                        SHELL_DBG("%s %d\n", __func__, __LINE__);
                        return -1;
                    }
                    bits = string_to_value(tmp);

                    tmp = child->Attribute(OFFSETS);
                    if(NULL == tmp) {
                        SHELL_DBG("%s %d\n", __func__, __LINE__);
                        return -1;
                    }
                    offset = string_to_value(tmp);

                    mask=0;
                    for(i = 0; i < bits; i++) {
                        mask |= 1 << i;
                    }
                    val = (values >> offset)&mask;

                    memset(val_string, 0, sizeof(val_string));
                    sprintf(val_string, "0x%x", val);

                    child->SetAttribute(VALUE, val_string);

                    //SHELL_DBG("ele:%s values:0x%x attr_val:%s mask:%x bits:%d\n",child->Value(),values,val_string,mask,bits);

                    child = child->NextSiblingElement();

                }
            } else {
                memset(val_string, 0, sizeof(val_string));
                sprintf(val_string, "0x%x", values);
                Element->SetAttribute(VALUE, val_string);
                //SHELL_DBG("%s values:0x%x :%s\n",Element->Value(),values,val_string);
            }
        }

        if(root) {
            break;
        }

        Element = Element->NextSiblingElement();
    }

    return 0;
}

int read_param_from_bin(const char *src_xml, const char *dst_xml,const char *src_bin)
{

    AUDIOVBCEQ_PARAM_T param;
    TiXmlElement *group = NULL;
    TiXmlElement *root = NULL;

    const char *tmp=NULL;
    int fd = -1;

    int ret = 0;

    memset(&param, 0, sizeof(AUDIOVBCEQ_PARAM_T));

    ret = load_xml_handle(&param.xml, src_xml);
    if (ret != 0) {
        SHELL_DBG("load xml handle failed (%s)", src_xml);
        return ret;
    }

    group = (TiXmlElement *)param.xml.param_root;
    if(NULL == group) {
        SHELL_DBG("%s %d\n", __func__, __LINE__);
        return -1;
    }

    fd = open(src_bin, O_RDONLY);
    if(fd < 0) {
        return fd;
    } else {
        struct vbc_fw_header fw_header;
        int buffer_size = 0;
        memset(&fw_header, 0, sizeof(struct vbc_fw_header));
        ret = read(fd, &fw_header, sizeof(struct vbc_fw_header));

        if(ret != sizeof(struct vbc_fw_header)) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            return ret;
        }

        buffer_size = fw_header.num_mode * fw_header.len;

        param.data = (char *)malloc(buffer_size);
        if(NULL == param.data) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            return -1;
        }

        memset(param.data,0,buffer_size);

        ret = read(fd, param.data, buffer_size);
        if(ret != buffer_size) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            return ret;
        }

        close(fd);
        _read_param_form_bin((TiXmlElement *)param.xml.param_root, param.data, true);
    }

    if(NULL != param.xml.param_doc) {

        root = (TiXmlElement *)param.xml.param_root;
        tmp = root->Attribute(STRUCT_SIZE);
        if(tmp != NULL) {
            root->RemoveAttribute(STRUCT_SIZE);
        }

        tmp = root->Attribute(NUM_MODE);
        if(tmp != NULL) {
            root->RemoveAttribute(NUM_MODE);
        }

        XML_save_param(param.xml.param_doc,
                       dst_xml);
    }

    return 0;
}

#define SPLIT "\n"

int create_audio_param(UNUSED_ATTR const char *src_xml, const char *dst_xml,
                      const char *confile_file)
{
    struct xml_handle src_xml_handle;
    char *config_buffer=NULL;
    int config_buffer_size=38*1024;
    int bytesRead=0;
    FILE *file=NULL;
    char *str1=NULL;
    char *str2=NULL;
    char *str3=NULL;
    char *tmpstr=NULL;
    char *tmpstr1=NULL;
    void *src_group=NULL;
    int ret=0;
    TiXmlDocument new_doc;
    TiXmlElement new_root("root");

    TiXmlDeclaration *dec = new TiXmlDeclaration("1.0", "gb2312", "");
    new_doc.LinkEndChild(dec);
    new_doc.LinkEndChild(&new_root);

    ret = load_xml_handle(&src_xml_handle, src_xml);
    if (ret != 0) {
        SHELL_DBG("load xml handle failed (%s)", src_xml);
        delete dec;
        return -1;
    }


    config_buffer=(char *)malloc(config_buffer_size);
    if(NULL==config_buffer){
        SHELL_DBG("malloc config buffer failed");
        return -1;
    }

    file = fopen(confile_file, "r");
    if(NULL==file){
        SHELL_DBG("open config file:%s failed",confile_file);
        return -2;
    }

     bytesRead = fread(config_buffer, 1, config_buffer_size, file);
     if(bytesRead<=0){
        SHELL_DBG("read config file failed bytesRead:%d",bytesRead);
        fclose(file);
        return -3;
     }

    char *line = strtok_r(config_buffer, SPLIT, &tmpstr);
    while (line != NULL) {
        str3 = (char *)strdup((char *)line);
         str1 = strtok_r(str3, ",",&tmpstr1);
        if(NULL!=str1){
            str2 = strtok_r(NULL, ",",&tmpstr1);
        }

        if((NULL==str1)&&(str2==NULL)){
            free((void *)str3);
            str3=NULL;
            SHELL_DBG("config error");
            fclose(file);
            return -4;
        }

        SHELL_DBG("confing from:(%s) to (%s)\n",str1,str2);
        ALOGI("confing from:(%s) to (%s)",str1,str2);
        src_group = (TiXmlElement *)private_get_param(src_xml_handle.param_root, str1);
        if(NULL == src_group) {
            SHELL_DBG("%s %d\n", __func__, __LINE__);
            fclose(file);
            return -2;
        }

        private_crate_group(&new_root,str2,src_group);
         line = strtok_r(NULL, SPLIT, &tmpstr);
    }

    new_doc.SaveFile(dst_xml);
    fclose(file);
    delete dec;
    return 0;
}

static int parse_bin(char *file_name, char *format){
    struct vbc_fw_header header;
    int bytesRead=0;
    FILE *file;
    char *buffer=NULL;
    int ret=0;
    int format_num=16;
    int addr_offset=0;
    char str_buffer[128]={0};
    int count=0;
    int i=0;

    if(NULL!=format){
        format_num=strtoul(format,NULL,0);
    }

    if(NULL==file_name){
        return -1;
    }

    file = fopen(file_name, "r");
    if(NULL==file){
        SHELL_DBG("open config file:%s failed",file_name);
        ret= -1;
        goto out;
    }

    bytesRead = fread(&header, 1, sizeof(header), file);
    if(bytesRead<=0){
        SHELL_DBG("read vbc_fw_header failed bytesRead:%d",bytesRead);
        ret= -2;
        goto out;

    }

    buffer=(char *)malloc(header.len);
    if(NULL==buffer){
        SHELL_DBG("malloc buffer failed");
        ret= -3;
        goto out;

    }

    SHELL_DBG("magic(%s) num_mode:%d pre size:%d\n",header.magic,
        header.num_mode,header.len);
    while(count<header.num_mode){
        SHELL_DBG("\nmode:%d\n",count++);

        bytesRead = fread(buffer, 1, header.len, file);
        if(bytesRead!=header.len){
            SHELL_DBG("read config file failed bytesRead:%d",bytesRead);
            ret= -4;
            goto out;
        }

        if(format_num==32){
            int *data=(int *)buffer;
            int data_num=header.len/4;
            i=0;
            while(i+8<data_num){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x %08x %08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5],data[i+6],data[i+7]);
                i+=8;
                addr_offset+=8*4;
                SHELL_DBG("%s\n",str_buffer);
                memset(str_buffer,0,sizeof(str_buffer));
            }

            if(data_num==i+7){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x %08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5],data[i+6]);
                i+=7;
                addr_offset+=7*4;
            }

            if(data_num==i+6){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5]);
                i+=6;
                addr_offset+=6*4;
            }

            if(data_num==i+5){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4]);
                i+=5;
                addr_offset+=5*4;
            }

            if(data_num==i+4){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3]);
                i+=4;
                addr_offset+=4*4;
            }

            if(data_num==i+3){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1],data[i+2]);
                i+=3;
                addr_offset+=3*4;
            }

            if(data_num==i+2){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x %08x",
                    addr_offset,
                    data[i],data[i+1]);
                i+=2;
                addr_offset+=2*4;
            }

            if(data_num==i+1){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %08x",
                    addr_offset,
                    data[i]);
                i+=1;
                addr_offset+=1*4;
            }

            SHELL_DBG("%s\n",str_buffer);
            memset(str_buffer,0,sizeof(str_buffer));
        }else{
            short *data=(short *)buffer;
            int data_num=header.len/2;
            i=0;
            while(i+8<data_num){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x %04x %04x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5],data[i+6],data[i+7]);
                i+=8;
                addr_offset+=8*2;
                SHELL_DBG("%s\n",str_buffer);
            }

            if(data_num==i+7){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x %04x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5],data[i+6]);
                i+=7;
                addr_offset+=7*2;
            }

            if(data_num==i+6){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4],data[i+5]);
                i+=6;
                addr_offset+=6*2;
            }

            if(data_num==i+5){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3],
                    data[i+4]);
                i+=5;
                addr_offset+=5*2;
            }

            if(data_num==i+4){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2],data[i+3]);
                i+=4;
                addr_offset+=4*2;
            }

            if(data_num==i+3){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x %04x",
                    addr_offset,
                    data[i],data[i+1],data[i+2]);
                i+=3;
                addr_offset+=3*2;
            }

            if(data_num==i+2){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x %04x",
                    addr_offset,
                    data[i],data[i+1]);
                i+=2;
                addr_offset+=2*2;
            }

            if(data_num==i+1){
                snprintf(str_buffer,sizeof(str_buffer)-1,
                    "%08x %04x",
                    addr_offset,
                    data[i]);
                i+=1;
                addr_offset+=1*2;
            }

            SHELL_DBG("%s\n",str_buffer);
            memset(str_buffer,0,sizeof(str_buffer));
        }
    }

out:
    if(NULL!=file){
        fclose(file);
        file=NULL;
    }

    if(NULL!=buffer){
        free(buffer);
        buffer=NULL;
    }
    return ret;
}

static const struct audio_param_mode_t  audio_param_mode_table[] = {
    { PROFILE_MODE_AUDIO_Handset_NB1,           "Audio\\Handset\\NB1"          },
    { PROFILE_MODE_AUDIO_Handset_NB2,           "Audio\\Handset\\NB2"         },
    { PROFILE_MODE_AUDIO_Handset_WB1,           "Audio\\Handset\\WB1"     },
    { PROFILE_MODE_AUDIO_Handset_WB2,           "Audio\\Handset\\WB2"     },
    { PROFILE_MODE_AUDIO_Handset_SWB1,          "Audio\\Handset\\SWB1"     },
    { PROFILE_MODE_AUDIO_Handset_FB1,           "Audio\\Handset\\FB1"     },
    { PROFILE_MODE_AUDIO_Handset_VOIP1,         "Audio\\Handset\\VOIP1"         },

    { PROFILE_MODE_AUDIO_Handsfree_NB1,         "Audio\\Handsfree\\NB1"        },
    { PROFILE_MODE_AUDIO_Handsfree_NB2,         "Audio\\Handsfree\\NB2"       },
    { PROFILE_MODE_AUDIO_Handsfree_WB1,         "Audio\\Handsfree\\WB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_WB2,         "Audio\\Handsfree\\WB2"   },
    { PROFILE_MODE_AUDIO_Handsfree_SWB1,        "Audio\\Handsfree\\SWB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_FB1,         "Audio\\Handsfree\\FB1"   },
    { PROFILE_MODE_AUDIO_Handsfree_VOIP1,       "Audio\\Handsfree\\VOIP1"       },

    { PROFILE_MODE_AUDIO_Headset4P_NB1,         "Audio\\Headset4P\\NB1"        },
    { PROFILE_MODE_AUDIO_Headset4P_NB2,         "Audio\\Headset4P\\NB2"       },
    { PROFILE_MODE_AUDIO_Headset4P_WB1,         "Audio\\Headset4P\\WB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_WB2,         "Audio\\Headset4P\\WB2"   },
    { PROFILE_MODE_AUDIO_Headset4P_SWB1,        "Audio\\Headset4P\\SWB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_FB1,         "Audio\\Headset4P\\FB1"   },
    { PROFILE_MODE_AUDIO_Headset4P_VOIP1,       "Audio\\Headset4P\\VOIP1"       },

    { PROFILE_MODE_AUDIO_Headset3P_NB1,         "Audio\\Headset3P\\NB1"        },
    { PROFILE_MODE_AUDIO_Headset3P_NB2,         "Audio\\Headset3P\\NB2"       },
    { PROFILE_MODE_AUDIO_Headset3P_WB1,         "Audio\\Headset3P\\WB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_WB2,         "Audio\\Headset3P\\WB2"   },
    { PROFILE_MODE_AUDIO_Headset3P_SWB1,        "Audio\\Headset3P\\SWB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_FB1,         "Audio\\Headset3P\\FB1"   },
    { PROFILE_MODE_AUDIO_Headset3P_VOIP1,       "Audio\\Headset3P\\VOIP1"       },

    { PROFILE_MODE_AUDIO_BTHS_NB1,              "Audio\\BTHS\\NB1"             },
    { PROFILE_MODE_AUDIO_BTHS_NB2,              "Audio\\BTHS\\NB2"            },
    { PROFILE_MODE_AUDIO_BTHS_WB1,              "Audio\\BTHS\\WB1"        },
    { PROFILE_MODE_AUDIO_BTHS_WB2,              "Audio\\BTHS\\WB2"        },
    { PROFILE_MODE_AUDIO_BTHS_SWB1,             "Audio\\BTHS\\SWB1"        },
    { PROFILE_MODE_AUDIO_BTHS_FB1,              "Audio\\BTHS\\FB1"        },
    { PROFILE_MODE_AUDIO_BTHS_VOIP1,            "Audio\\BTHS\\VOIP1"            },

    { PROFILE_MODE_AUDIO_BTHSNREC_NB1,          "Audio\\BTHSNREC\\NB1"         },
    { PROFILE_MODE_AUDIO_BTHSNREC_NB2,          "Audio\\BTHSNREC\\NB2"        },
    { PROFILE_MODE_AUDIO_BTHSNREC_WB1,          "Audio\\BTHSNREC\\WB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_WB2,          "Audio\\BTHSNREC\\WB2"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_SWB1,         "Audio\\BTHSNREC\\SWB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_FB1,          "Audio\\BTHSNREC\\FB1"    },
    { PROFILE_MODE_AUDIO_BTHSNREC_VOIP1,        "Audio\\BTHSNREC\\VOIP1"        },

    { PROFILE_MODE_AUDIO_TYPEC_NB1,             "Audio\\TypeC_Digital\\NB1"         },
    { PROFILE_MODE_AUDIO_TYPEC_NB2,             "Audio\\TypeC_Digital\\NB2"        },
    { PROFILE_MODE_AUDIO_TYPEC_WB1,             "Audio\\TypeC_Digital\\WB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_WB2,             "Audio\\TypeC_Digital\\WB2"    },
    { PROFILE_MODE_AUDIO_TYPEC_SWB1,            "Audio\\TypeC_Digital\\SWB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_FB1,             "Audio\\TypeC_Digital\\FB1"    },
    { PROFILE_MODE_AUDIO_TYPEC_VOIP1,           "Audio\\TypeC_Digital\\VOIP1"        },

    { PROFILE_MODE_AUDIO_HAC_NB1,               "Audio\\Hac\\NB1"         },
    { PROFILE_MODE_AUDIO_HAC_NB2,               "Audio\\Hac\\NB2"        },
    { PROFILE_MODE_AUDIO_HAC_WB1,               "Audio\\Hac\\WB1"    },
    { PROFILE_MODE_AUDIO_HAC_WB2,               "Audio\\Hac\\WB2"    },
    { PROFILE_MODE_AUDIO_HAC_SWB1,              "Audio\\Hac\\SWB1"    },
    { PROFILE_MODE_AUDIO_HAC_FB1,               "Audio\\Hac\\FB1"    },
    { PROFILE_MODE_AUDIO_HAC_VOIP1,             "Audio\\Hac\\VOIP1"        },


    { PROFILE_MODE_MUSIC_Headset_Playback,           "Music\\Headset\\Playback"     },
    { PROFILE_MODE_MUSIC_Headset_Record,             "Music\\Headset\\Record"       },
    { PROFILE_MODE_MUSIC_Headset_UnprocessRecord,     "Music\\Headset\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_Headset_Recognition,     "Music\\Headset\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_Headset_FM,                 "Music\\Headset\\FM"           },

    { PROFILE_MODE_MUSIC_Handsfree_Playback,         "Music\\Handsfree\\Playback"   },
    { PROFILE_MODE_MUSIC_Handsfree_Record,           "Music\\Handsfree\\Record"     },
    { PROFILE_MODE_MUSIC_Handsfree_UnprocessRecord,  "Music\\Handsfree\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_Handsfree_VideoRecord,  "Music\\Handsfree\\VideoRecord"},
    { PROFILE_MODE_MUSIC_Handsfree_Recognition,     "Music\\Handsfree\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_Handsfree_FM,               "Music\\Handsfree\\FM"         },

    { PROFILE_MODE_MUSIC_TYPEC_Playback,         "Music\\TypeC_Digital\\Playback"   },
    { PROFILE_MODE_MUSIC_TYPEC_Record,           "Music\\TypeC_Digital\\Record"     },
    { PROFILE_MODE_MUSIC_TYPEC_Recognition,     "Music\\TypeC_Digital\\VoiceRecognition"},
    { PROFILE_MODE_MUSIC_TYPEC_UnprocessRecord,   "Music\\TypeC_Digital\\UnprocessRecord"},
    { PROFILE_MODE_MUSIC_TYPEC_FM,               "Music\\TypeC_Digital\\FM"         },


    { PROFILE_MODE_MUSIC_Handset_Playback,           "Music\\Handset\\Playback"     },
    { PROFILE_MODE_MUSIC_Headfree_Playback,          "Music\\Headfree\\Playback"    },
    { PROFILE_MODE_MUSIC_Bluetooth_Record,           "Music\\Bluetooth\\Record"     },

    { PROFILE_MODE_LOOP_Handset_Loop1,             "Loopback\\Handset\\Loop1"       },
    { PROFILE_MODE_LOOP_Handsfree_Loop1,           "Loopback\\Handsfree\\Loop1"     },
    { PROFILE_MODE_LOOP_Headset4P_Loop1,           "Loopback\\Headset4P\\Loop1"     },
    { PROFILE_MODE_LOOP_Headset3P_Loop1,           "Loopback\\Headset3P\\Loop1"     },
};

static const char * native_get_audio_param_name(uint8_t param_id){
    for(uint8_t i=0;i<PROFILE_MODE_MAX;i++){
        if(param_id==audio_param_mode_table[i].mode){
            return audio_param_mode_table[i].name;
        }
    }
    return NULL;
}

static int parse_infor(char *file_name){
    struct param_infor infor;

    int bytesRead=0;
    FILE *file;
    int ret=0;
    int i=0;
    const char *param_name=NULL;
    memset(&infor,0,sizeof(infor));

    if(NULL==file_name){
        return -1;
    }

    file = fopen(file_name, "r");
    if(NULL==file){
        SHELL_DBG("open infor file:%s failed\n",file_name);
        ret= -1;
        goto out;
    }

    bytesRead = fread(infor.data, 1, sizeof(infor.data), file);
    if(bytesRead<=0){
        SHELL_DBG("read param_infor failed bytesRead:%d\n",bytesRead);
        ret= -2;
        goto out;

    }

    if(bytesRead!=sizeof(struct param_infor_t)*SND_AUDIO_PARAM_PROFILE_MAX){
        SHELL_DBG("read param_infor failed bytesRead:%d req:%zd\n",
            bytesRead,sizeof(struct param_infor_t)*SND_AUDIO_PARAM_PROFILE_MAX);
        ret= -2;
        goto out;
    }


    SHELL_DBG(" dsp_vbc \t AudioStructure \t CVS \t SmartAmp \t Codec \tName\n");
    SHELL_DBG(" %-8x \t %-8x \t %-8x \t %-8x \t%-8x \tstruct_size\n",
        infor.data[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP].param_struct_size,
        infor.data[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE].param_struct_size,
        infor.data[SND_AUDIO_PARAM_CVS_PROFILE].param_struct_size,
        infor.data[SND_AUDIO_PARAM_SMARTAMP_PROFILE].param_struct_size,
        infor.data[SND_AUDIO_PARAM_CODEC_PROFILE].param_struct_size);

    for(i=0;i<PROFILE_MODE_MAX;i++){
        param_name=native_get_audio_param_name(i);
        if(NULL!=param_name){
            SHELL_DBG(" %-8x \t %-8x \t %-8x \t %-8x \t%-8x \t%s\n",
                infor.data[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP].offset[i],
                infor.data[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_CVS_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_SMARTAMP_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_CODEC_PROFILE].offset[i],
                param_name
                );
        }else{
            SHELL_DBG(" %-8x \t %-8x \t %-8x \t %-8x \t%-8x \tUnknow\n",
                infor.data[SND_AUDIO_PARAM_DSP_VBC_PROFILE_DSP].offset[i],
                infor.data[SND_AUDIO_PARAM_AUDIO_STRUCTURE_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_CVS_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_SMARTAMP_PROFILE].offset[i],
                infor.data[SND_AUDIO_PARAM_CODEC_PROFILE].offset[i]
                );
        }
    }

out:
    if(NULL!=file){
        fclose(file);
        file=NULL;
    }

    return ret;
}

int main(int argc, char *argv[])
{
    if(0 == strncmp(argv[1], "clone", strlen("clone"))) {
        if(argc != 6) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return clone_audio_param(argv[2], argv[3], argv[4], argv[5]);
    } else if(0 == strncmp(argv[1], "Insert", strlen("Insert"))) {
        if(argc != 6) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return Insert_audio_param(argv[2], argv[3], argv[4], argv[5]);
    } else if(0 == strncmp(argv[1], "Division", strlen("Division"))) {
        if(argc != 4) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return Division_audio_param(argv[2], argv[3]);
    }else if(0 == strncmp(argv[1], "Load", strlen("Load"))) {
        if(argc != 5) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return read_param_from_bin(argv[2], argv[3],argv[4]);
    }else if(0 == strncmp(argv[1], "Create", strlen("Create"))) {
        if(argc != 5) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return create_audio_param(argv[2], argv[3],argv[4]);
    }else if(0 == strncmp(argv[1], "parsebin", strlen("parsebin"))) {
        if(argc != 4) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return parse_bin(argv[2], argv[3]);
    }else if(0 == strncmp(argv[1], "parseinfor", strlen("parseinfor"))) {
        if(argc != 3) {
            SHELL_DBG("argc err :%d\n",__LINE__);
            return -1;
        }
        return parse_infor(argv[2]);
    }
    return 0;
}

