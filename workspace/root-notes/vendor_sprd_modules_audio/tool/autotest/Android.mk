LOCAL_PATH:= $(call my-dir)
ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        tinyxml/tinyxml.cpp \
        tinyxml/tinyxmlparser.cpp \
        tinyxml/tinyxmlerror.cpp \
        tinyxml/tinystr.cpp

LOCAL_CFLAGS+= \
        -Wno-undefined-bool-conversion \
        -Wno-missing-braces \
        -Wno-logical-op-parentheses \
        -Werror

LOCAL_C_INCLUDES += $(LOCAL_PATH)/tinyxml

LOCAL_MODULE:= libtinyxmltool

include $(BUILD_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	src/test_main.cpp \
	src/test_control.cpp \
	src/test_process.cpp \
	src/test_alsa_utils.cpp \
	src/test_xml_utils.cpp

LOCAL_32_BIT_ONLY := true
LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libutils\
	libtinyalsa \
	libmedia \
	libhardware_legacy

ifneq ($(filter $(strip $(PLATFORM_VERSION)),O 8.0.0 8.1.0),)
LOCAL_SHARED_LIBRARIES += libaudioclient
LOCAL_CFLAGS += -DFLAG_VENDOR_ETC
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_MODULE := audio_hardware_test

LOCAL_C_INCLUDES +=  \
	$(LOCAL_PATH)/inc/ \
	external/tinyalsa/include \
	system/media/audio_utils/include \
	system/media/audio_effects/include\

ifneq ($(filter $(strip $(PLATFORM_VERSION)),P 9),)
LOCAL_SHARED_LIBRARIES += libaudioclient
LOCAL_C_INCLUDES += frameworks/av/include
LOCAL_STATIC_LIBRARIES :=libtinyxmltool
LOCAL_C_INCLUDES +=  $(LOCAL_PATH)/tinyxml
else
LOCAL_SHARED_LIBRARIES +=libtinyxml
LOCAL_C_INCLUDES += external/tinyxml
endif

include $(BUILD_EXECUTABLE)
