LOCAL_PATH := $(call my-dir)

################################################################################
# ledd_plugin
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ledd_plugin
LOCAL_DESCRIPTION := Library for the implementation of ledd plugins, drivers or\
	transitions
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := $(call all-c-files-in,ledd_plugins/src)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/ledd_plugins

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/ledd_plugins/include

LOCAL_LIBRARIES := \
	liblua \
	libulog \
	libutils \
	libpomp \
	librs

include $(BUILD_LIBRARY)
