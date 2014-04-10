LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := v4l2-dbg
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../.. \
    $(LOCAL_PATH)/../../include \
    bionic \
    external/stlport/stlport

LOCAL_SHARED_LIBRARIES := libstlport

LOCAL_SRC_FILES := v4l2-dbg.cpp

include $(BUILD_EXECUTABLE)
