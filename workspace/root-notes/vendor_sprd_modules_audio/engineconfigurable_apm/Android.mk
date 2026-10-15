################################################################################################
#
# @NOTE:
# Audio Policy Engine configurable example for generic device build
#
# Any vendor shall have its own configuration within the corresponding device folder
#
################################################################################################
LOCAL_PATH := $(call my-dir)

ifdef BUILD_AUDIO_POLICY_CONFIGURATION

ifeq ($(BUILD_AUDIO_POLICY_CONFIGURATION),$(filter $(BUILD_AUDIO_POLICY_CONFIGURATION),phone_configurable))

PFW_CORE := external/parameter-framework
#@TODO: upstream new domain generator
#BUILD_PFW_SETTINGS := $(PFW_CORE)/support/android/build_pfw_settings.mk
PFW_DEFAULT_SCHEMAS_DIR := $(PFW_CORE)/upstream/schemas
PFW_SCHEMAS_DIR := $(PFW_DEFAULT_SCHEMAS_DIR)

TOOLS := frameworks/av/services/audiopolicy/engineconfigurable/tools
BUILD_PFW_SETTINGS := $(TOOLS)/build_audio_pfw_settings.mk
PROVISION_CRITERION_TYPES := $(TOOLS)/provision_criterion_types_from_android_headers.mk

PROVISION_STRATEGIES_STRUCTURE := $(TOOLS)/provision_strategies_structure.mk

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_configuration.xml

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE)

LOCAL_REQUIRED_MODULES := \
    audio_policy_engine_product_strategies.xml  \
    audio_policy_engine_stream_volumes.xml \
    audio_policy_engine_default_stream_volumes.xml \
    audio_policy_engine_criteria.xml \
    audio_policy_engine_criterion_types.xml

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_product_strategies.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_stream_volumes.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_default_stream_volumes.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_criteria.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_engine_criterion_types.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_VENDOR_ETC)/primary_audio_policy_configuration.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/r_submix_audio_policy_configuration.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/a2dp_audio_policy_configuration.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/usb_audio_policy_configuration.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/bluetooth_audio_policy_configuration.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/audio_policy_volumes.xml
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_VENDOR_ETC)/default_volume_tables.xml
ANDROID_AUDIO_BASE_HEADER_FILE := system/media/audio/include/system/audio-base.h
AUDIO_POLICY_CONFIGURATION_FILE := $(TARGET_OUT_VENDOR_ETC)/audio_policy_configuration.xml
CRITERION_TYPES_FILE := $(AUDIO_PFW_PATH)/$(LOCAL_MODULE).in

include $(PROVISION_CRITERION_TYPES)
##################################################################
# CONFIGURATION FILES
##################################################################
######### Policy PFW top level file #########

include $(CLEAR_VARS)
LOCAL_MODULE := ParameterFrameworkConfigurationPolicy.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := parameter-framework
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/$(LOCAL_MODULE).in
LOCAL_REQUIRED_MODULES := \
    PolicySubsystem.xml \
    PolicyClass.xml

# external/parameter-framework prevents from using debug interface
AUDIO_PATTERN = @TUNING_ALLOWED@
ifeq ($(TARGET_BUILD_VARIANT),user)
AUDIO_VALUE = false
else
AUDIO_VALUE = true
endif

LOCAL_POST_INSTALL_CMD := $(hide) sed -i -e 's|$(AUDIO_PATTERN)|$(AUDIO_VALUE)|g' $(TARGET_OUT_VENDOR_ETC)/$(LOCAL_MODULE_RELATIVE_PATH)/$(LOCAL_MODULE)

include $(BUILD_PREBUILT)

########## Policy PFW Common Structures #########

include $(CLEAR_VARS)
LOCAL_MODULE := PolicySubsystem.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_REQUIRED_MODULES := \
    PolicySubsystem-CommonTypes.xml \
    ProductStrategies.xml

LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Policy
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := PolicySubsystem-CommonTypes.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Policy
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := PolicyClass.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Policy
LOCAL_SRC_FILES := ../../../../../$(AUDIO_PFW_PATH)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE := ProductStrategies.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Structure/Policy

AUDIO_POLICY_ENGINE_CONFIGURATION_FILE := \
    $(TARGET_OUT_VENDOR_ETC)/audio_policy_engine_configuration.xml
STRATEGIES_STRUCTURE_FILE := $(AUDIO_PFW_PATH)/Structure/$(LOCAL_MODULE).in

include $(PROVISION_STRATEGIES_STRUCTURE)

##################################################################
# CONFIGURATION FILES
##################################################################
########## Policy PFW Structures #########
######### Policy PFW Settings #########
include $(CLEAR_VARS)
LOCAL_MODULE := parameter-framework.policy
LOCAL_MODULE_STEM := PolicyConfigurableDomains.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Settings/Policy
LOCAL_REQUIRED_MODULES := libpolicy-subsystem

PFW_EDD_FILES := $(AUDIO_PFW_EDD_FILES)

PFW_CRITERION_TYPES_FILE := $(TARGET_OUT_VENDOR_ETC)/audio_policy_engine_criterion_types.xml
PFW_CRITERIA_FILE := $(TARGET_OUT_VENDOR_ETC)/audio_policy_engine_criteria.xml
PFW_TOPLEVEL_FILE := $(TARGET_OUT_VENDOR_ETC)/parameter-framework/ParameterFrameworkConfigurationPolicy.xml
PFW_SCHEMAS_DIR := $(PFW_DEFAULT_SCHEMAS_DIR)

include $(BUILD_PFW_SETTINGS)
#ifeq ($(BUILD_AUDIO_POLICY_CONFIGURATION),$(filter $(BUILD_AUDIO_POLICY_CONFIGURATION),phone_configurable))


else

PFW_CORE := external/parameter-framework
BUILD_PFW_SETTINGS := $(PFW_CORE)/support/android/build_pfw_settings.mk
PFW_DEFAULT_SCHEMAS_DIR := $(PFW_CORE)/upstream/schemas
PFW_SCHEMAS_DIR := $(PFW_DEFAULT_SCHEMAS_DIR)
AUDIO_PFW_PATH_TMP :=../../../../../$(AUDIO_PFW_PATH)

##################################################################
# CONFIGURATION FILES
##################################################################
######### Policy PFW top level file #########

include $(CLEAR_VARS)
LOCAL_MODULE := ParameterFrameworkConfigurationPolicy.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/parameter-framework
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH_TMP)/$(LOCAL_MODULE).in

AUDIO_PATTERN = @TUNING_ALLOWED@
ifeq ($(TARGET_BUILD_VARIANT),user)
AUDIO_VALUE = false
else
#AUDIO_VALUE = true
#AUDIO_VALUE = ture will update ParameterFrameworkConfigurationPolicy.xml
#and enable tuning function, create a tcp socket
AUDIO_VALUE = false
endif

LOCAL_POST_INSTALL_CMD := $(hide) sed -i -e 's|$(AUDIO_PATTERN)|$(AUDIO_VALUE)|g' $(LOCAL_MODULE_PATH)/$(LOCAL_MODULE)

include $(BUILD_PREBUILT)


########## Policy PFW Structures #########
include $(CLEAR_VARS)
LOCAL_MODULE := PolicySubsystem-CommonTypes.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/parameter-framework/Structure/Policy
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH_TMP)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := PolicyClass.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC

LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/parameter-framework/Structure/Policy
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH_TMP)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := PolicySubsystem.xml
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC

LOCAL_REQUIRED_MODULES := PolicySubsystem-CommonTypes.xml

LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/parameter-framework/Structure/Policy
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH_TMP)/Structure/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

######### Policy PFW Settings #########

include $(CLEAR_VARS)
LOCAL_MODULE := parameter-framework.policy
LOCAL_MODULE_STEM := PolicyConfigurableDomains.xml
LOCAL_MODULE_CLASS := ETC

LOCAL_MODULE_RELATIVE_PATH := parameter-framework/Settings/Policy
LOCAL_REQUIRED_MODULES := \
        PolicyClass.xml \
        PolicySubsystem.xml \
        ParameterFrameworkConfigurationPolicy.xml

ifeq ($(pfw_rebuild_settings),true)
PFW_CRITERIA_FILE := $(SPRD_PFW_CRITERIA_FILE)
PFW_EDD_FILES := $(SPRD_PFW_EDD_FILES)
PFW_TOPLEVEL_FILE := $(TARGET_OUT_ETC)/parameter-framework/ParameterFrameworkConfigurationPolicy.xml

include $(BUILD_PFW_SETTINGS)
else
# Use the existing file
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH)/Settings/$(LOCAL_MODULE_STEM)
include $(BUILD_PREBUILT)
endif # pfw_rebuild_settings
##################################################################
# CONFIGURATION FILE
##################################################################
# specific management of audio_policy_criteria.conf
include $(CLEAR_VARS)
LOCAL_MODULE := audio_policy_criteria.conf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := $(AUDIO_PFW_PATH_TMP)/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)
endif #ifeq ($(BUILD_AUDIO_POLICY_CONFIGURATION),$(filter $(BUILD_AUDIO_POLICY_CONFIGURATION),phone_configurable))
endif #ifdef BUILD_AUDIO_POLICY_CONFIGURATION

