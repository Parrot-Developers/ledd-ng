LOCAL_PATH := $(call my-dir)

################################################################################
# ledd
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ledd-ng
LOCAL_DESCRIPTION := Daemon handling leds behaviour
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	ledd/src/ledd/main.c

LOCAL_LIBRARIES := \
	libledd \
	libulog

include $(BUILD_EXECUTABLE)

################################################################################
# ledd-static
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ledd-static
LOCAL_DESCRIPTION := Daemon handling leds behaviour - static version. This \
	module is more of a tutorial on how to embedd ledd in a static executable, \
	with all the plugins provided by the base distribution of ledd
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	ledd/src/ledd/main.c

LOCAL_WHOLE_STATIC_LIBRARIES := \
	libledd-static

LOCAL_LDFLAGS := -static

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/ledd/include

include $(BUILD_EXECUTABLE)

################################################################################
# libledd
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libledd
LOCAL_DESCRIPTION := Base library of ledd
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	$(call all-c-files-in,ledd/src) \
	$(call all-c-files-in,ledd/src/config)

LOCAL_LIBRARIES := \
	libpomp \
	liblua \
	libutils \
	librs \
	ledd_plugin \
	libulog

LOCAL_LDLIBS := -ldl

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/ledd_plugins/src \
	$(LOCAL_PATH)/ledd/src \
	$(LOCAL_PATH)/ledd/src/config

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/ledd/include

LOCAL_EXPORT_CFLAGS := -DLEDD_SKIP_PLUGINS=false

LOCAL_COPY_FILES := \
	config/20-ledd.rc:etc/boxinit.d/

include $(BUILD_LIBRARY)

################################################################################
# libledd-static
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libledd-static
LOCAL_DESCRIPTION := Base library of ledd, embedds all the plugins provided by \
	the ledd package
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	$(call all-c-files-in,ledd/src) \
	$(call all-c-files-in,ledd/src/config) \
	$(call all-c-files-under,ledd_plugins)

LOCAL_STATIC_LIBRARIES := \
	libutils \
	librs \
	libpomp \
	libulog \
	liblua-static

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/ledd_plugins/include \
	$(LOCAL_PATH)/ledd_plugins/src \
	$(LOCAL_PATH)/ledd/include \
	$(LOCAL_PATH)/ledd/src \
	$(LOCAL_PATH)/ledd/src/config

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/ledd/include

LOCAL_EXPORT_CFLAGS := -DLEDD_SKIP_PLUGINS=true

include $(BUILD_STATIC_LIBRARY)

################################################################################
# ledd_client
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := libledd_client
LOCAL_DESCRIPTION := Library for implementing a ledd clietn
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	$(call all-c-files-in,ledd_client/src)

LOCAL_LIBRARIES := \
	liblua \
	libpomp

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/ledd_client/include

include $(BUILD_LIBRARY)

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

################################################################################
# ldc
################################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := ldc
LOCAL_DESCRIPTION := Command line interface for ldc
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_REQUIRED_MODULES := \
	lua \
	ledd-ng \
	pomp-cli

LOCAL_COPY_FILES := \
	utils/ldc:usr/bin/ldc

include $(BUILD_CUSTOM)

################################################################################
# file_led_driver
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := file_led_driver
LOCAL_DESCRIPTION := Virtual led driver with a file backend
LOCAL_CATEGORY_PATH := tools/ledd/drivers
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/drivers/file_led_driver.c

LOCAL_LIBRARIES := \
	libulog \
	libutils \
	librs \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# pwm_led_driver
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := pwm_led_driver
LOCAL_DESCRIPTION := Driver using the kernel's pwm facility
LOCAL_CATEGORY_PATH := tools/ledd/drivers
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/drivers/pwm_led_driver.c

LOCAL_LIBRARIES := \
	libulog \
	libutils \
	librs \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# socket_led_driver
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := socket_led_driver
LOCAL_DESCRIPTION := Driver sending channel values over a socket
LOCAL_CATEGORY_PATH := tools/ledd/drivers
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/drivers/socket_led_driver.c

LOCAL_LIBRARIES := \
	libulog \
	libutils \
	librs \
	libpomp \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# gpio_led_driver
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := gpio_led_driver
LOCAL_DESCRIPTION := Driver for leds wired to a gpio
LOCAL_CATEGORY_PATH := tools/ledd/drivers
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/drivers/gpio_led_driver.c

LOCAL_LIBRARIES := \
	libulog \
	libutils \
	librs \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# tricolor_led_driver
################################################################################

include $(CLEAR_VARS)

# the zzz_ prefix is used to force this plugin being loaded last, this way, we
# guarantee it can reference any driver loaded before
LOCAL_MODULE := zzz_tricolor_led_driver
LOCAL_DESCRIPTION := Driver for tricolor leds
LOCAL_CATEGORY_PATH := tools/ledd/drivers
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/drivers/tricolor_led_driver.c

LOCAL_LIBRARIES := \
	libulog \
	libutils \
	librs \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# flicker_transition
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := flicker_transition
LOCAL_DESCRIPTION := Ledd animation transition for making flickering a channel
LOCAL_CATEGORY_PATH := tools/ledd/transitions
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/transitions/flicker_transition.c

LOCAL_LIBRARIES := \
	libulog \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# read_hsis
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := read_hsis
LOCAL_DESCRIPTION := Ledd animation transition for making flickering a channel
LOCAL_CATEGORY_PATH := tools/ledd/lua_globals
LOCAL_DESTDIR := usr/lib/ledd-plugins

LOCAL_SRC_FILES := \
	ledd_plugins/lua_globals/read_hsis.c

LOCAL_LIBRARIES := \
	libulog \
	liblua \
	libutils \
	ledd_plugin

include $(BUILD_LIBRARY)

################################################################################
# ledd_client_example
################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE := ledd_client_example
LOCAL_DESCRIPTION := Simple ledd_client usage example
LOCAL_CATEGORY_PATH := tools/ledd

LOCAL_SRC_FILES := \
	ledd_client/example/main.c

LOCAL_LIBRARIES := \
	libioutils \
	libutils \
	libledd_client

include $(BUILD_EXECUTABLE)

