LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

#include our sdl config
include $(LOCAL_PATH)/../../config.mk

ifeq ($(SDL_BUILD_MIXER), true)

LOCAL_MODULE := libsdlmixer

LOCAL_CFLAGS := -DANDROID -DWAV_MUSIC -DOGG_USE_TREMOR -DOGG_MUSIC

LOCAL_C_INCLUDES := \
	$(SDL_INCLUDE) \
	external/tremolo
		
LOCAL_CPP_EXTENSION := .cpp

LOCAL_SHARED_LIBRARIES := libvorbisidec

# Note this simple makefile var substitution, you can find even simpler examples in different Android projects
LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))

include $(BUILD_STATIC_LIBRARY)

endif
