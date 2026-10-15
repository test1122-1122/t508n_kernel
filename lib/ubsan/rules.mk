LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/ubsan.cpp

MODULE_OPTIONS := test

GLOBAL_COMPILEFLAGS += -fsanitize=undefined

include make/module.mk
