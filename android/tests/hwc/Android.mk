LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
	../../hwcomposer/hwcomposer.cpp \
    HostConnection.cpp \
    hwcomposer_gtest.cpp

LOCAL_STATIC_LIBRARIES += libgtest libgtest_main libgmock libgmock_main
LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcutils

LOCAL_CLANG_CFLAGS += -Wno-unused-parameter
LOCAL_C_INCLUDES := \
    $(TOP)/external/gmock/include

LOCAL_MODULE:= htest_hwcomposer
include $(BUILD_EXECUTABLE)
