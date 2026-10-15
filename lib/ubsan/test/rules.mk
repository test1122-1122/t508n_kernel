LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/ubsan_tests.c \
	$(LOCAL_DIR)/ubsan_cpp_tests.cpp

# the float_cast_overflow test needs floating point support
MODULE_OPTIONS := float

MODULE_DEPS += \
	lib/ubsan

# the tests intentionally compute values that trigger undefined behavior
# and then discard them
MODULE_COMPILEFLAGS += -Wno-unused-but-set-variable -Wno-strict-aliasing

include make/module.mk
