/*
**
** Copyright 2010, Petr Havlena
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "SDLImpl-JNI"

#include "SDLRuntime.h"
#include <SDL_androidvideo.h>
#include <SkBitmap.h>
#include <SkCanvas.h>
#include <SkMatrix.h>
#include <utils/Log.h>

// ----------------------------------------------------------------------------

using namespace android;

// ----------------------------------------------------------------------------

struct fields_t {
	jfieldID    surface;
    /* actually in android.view.Surface XXX */
    jfieldID    surface_native;
    jmethodID   post_event;
};
static fields_t fields;

static const char* const kClassPathName = "android/sdl/SDLVideo";

//-- helper methods for creating java classes representing SDL native structs

extern jobject
android_sdl_SDLPixelFormat_create(SDL_PixelFormat* pformat);

extern jobject
android_sdl_SDLVideoDevice_create(SDL_VideoDevice* device);

extern jobject
android_sdl_SDLSurface_create(SDL_Surface* surface);

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// ref-counted object for callbacks
class JNISDLVideoDriverListener: public SDLVideoDriverListener
{
public:
    JNISDLVideoDriverListener(JNIEnv* env, jobject thiz, jobject weak_thiz);
    ~JNISDLVideoDriverListener();
    void                    notify(int what, int arg1, int arg2, void* data);
private:
    void                    updateScreen(SkBitmap *bitmap);
    jclass                  mClass;     // Reference to MediaPlayer class
    jobject                 mObject;    // Weak ref to MediaPlayer Java object to call on
    Surface*                mSurface;   // Android surface class
};

JNISDLVideoDriverListener::JNISDLVideoDriverListener(JNIEnv* env, jobject thiz, jobject weak_thiz)
{
    // Hold onto the MediaPlayer class for use in calling the static method
    // that posts events to the application thread.
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        LOGE("Can't find android/sdl/SDLVideo");
		SDLRuntime::doThrow(env, "java/lang/Exception", NULL);
        return;
    }
    mClass = (jclass)env->NewGlobalRef(clazz);
	
    // We use a weak reference so the MediaPlayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    mObject = env->NewGlobalRef(weak_thiz);
}

JNISDLVideoDriverListener::~JNISDLVideoDriverListener()
{
    // remove global references
    JNIEnv *env = SDLRuntime::getJNIEnv();
    env->DeleteGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);
}

/**
  * Method which handles native redrawing screen
  */
void JNISDLVideoDriverListener::updateScreen(SkBitmap* bitmap)
{
	SkBitmap screen;
    Surface::SurfaceInfo surfaceInfo;

    if (!mSurface || !mSurface->isValid()) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "nativeSurface wasn't valid");
        return;
    }

    if (mSurface->lock(&surfaceInfo) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to lock surface");
        return;
    }
	
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "updating screen: %ix%i", surfaceInfo.w, surfaceInfo.h);

    //bitmap which is drawed on android surfaceview
    SDLVideoDriver::setBitmapConfig(&screen, surfaceInfo.format, surfaceInfo.w, surfaceInfo.h);
    screen.setPixels(surfaceInfo.bits);

    SkCanvas canvas(screen);
    SkRect surface_sdl;
    SkRect surface_android;
    SkMatrix matrix;
    surface_android.set(0, 0, screen.width(), screen.height());
    surface_sdl.set(0, 0, bitmap->width(), bitmap->height());
    matrix.setRectToRect(surface_sdl, surface_android, SkMatrix::kFill_ScaleToFit);

    canvas.drawBitmapMatrix(*bitmap, matrix);

    if (mSurface->unlockAndPost() < 0) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to unlock surface");
    }
}

// callback from libsdl which notify us about situation in sdl video driver
void JNISDLVideoDriverListener::notify(int what, int arg1, int arg2, void* data)
{
    JNIEnv *env;
    jobject obj;
	
    if (what == SDL_NATIVE_VIDEO_UPDATE_RECTS) {
        updateScreen((SkBitmap*) data);
        return;
    }
	
    env = SDLRuntime::getJNIEnv();
	
    // we create java representation of native structs
    switch (what) {
        case SDL_NATIVE_VIDEO_CREATE_DEVICE:
            obj = android_sdl_SDLVideoDevice_create((SDL_VideoDevice*) data);
            break;
        case SDL_NATIVE_VIDEO_DELETE_DEVICE:
            break;
        case SDL_NATIVE_VIDEO_INIT:
            obj = android_sdl_SDLPixelFormat_create((SDL_PixelFormat*) data);
            break;
        case SDL_NATIVE_VIDEO_SET_SURFACE:
            obj = android_sdl_SDLSurface_create((SDL_Surface*) data);
            break;
        case SDL_NATIVE_VIDEO_PUMP_EVENTS:
            break;
        case SDL_NATIVE_VIDEO_UPDATE_RECTS:
            break;
    }

    // than call java to process class represents sdl struct
	env->CallStaticVoidMethod(mClass, fields.post_event, mObject, what, arg1, arg2, obj);
	
	if(what == SDL_NATIVE_VIDEO_SET_SURFACE) {
		obj = env->GetObjectField(mObject, fields.surface);
		mSurface = (Surface *) env->GetIntField(obj, fields.surface_native);
	}
}

// ----------------------------------------------------------------------------

// This function gets some field IDs, which in turn causes class initialization.
// It is called from a static block, which won't run until the
// first time an instance of this class is used.
static void
android_sdl_SDLVideo_native_init(JNIEnv *env)
{
	LOGV("native_init");
	
    jclass clazz = env->FindClass(kClassPathName);
    if (clazz == NULL) {
        SDLRuntime::doThrow(env, "java/lang/RuntimeException", 
							"Can't find android/sdl/SDLVideo");
        return;
    }

	/*
    fields.context = env->GetFieldID(clazz, "mNativeContext", "I");
    if (fields.context == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Can't find MediaPlayer.mNativeContext");
        return;
    }
	*/
	 
    fields.post_event = env->GetStaticMethodID(clazz, "postEventFromNative",
                                               "(Ljava/lang/Object;IIILjava/lang/Object;)V");
    if (fields.post_event == NULL) {
        SDLRuntime::doThrow(env, "java/lang/RuntimeException", 
							"Can't find SDLVideo.postEventFromNative");
        return;
    }
	
	fields.surface = env->GetFieldID(clazz, "mSurface", "Landroid/view/Surface;");
	if (fields.surface == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Can't find SDLVideo.mSurface");
		return;
	}
	
	jclass surface_clazz = env->FindClass("android/view/Surface");
	if (surface_clazz == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Can't find android/view/Surface");
		return;
	}
	
	fields.surface_native = env->GetFieldID(surface_clazz, "mSurface", "I");
	if (fields.surface_native == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Can't find Surface.mSurface");
		return;
	}
}

// Register us into sdl video driver, so we can handle sdl video driver statuses
static void
android_sdl_SDLVideo_native_setup(JNIEnv *env, jobject thiz, jobject weak_this)
{
    LOGV("native_setup");

    SDLVideoDriver* vd = SDLVideoDriver::getInstance();
    if (vd == NULL) {
        SDLRuntime::doThrow(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }

    // create new listener and give it to MediaPlayer
    JNISDLVideoDriverListener* listener = new JNISDLVideoDriverListener(env, thiz, weak_this);
    vd->registerListener(listener);
}

static void
android_sdl_SDLVideo_native_finalize(JNIEnv *env, jobject thiz)
{
    LOGV("native_finalize");
    //android_sdl_SDLVideo_release(env, thiz);
}

// ----------------------------------------------------------------------------

static JNINativeMethod gMethods[] = {
    {"native_init",         "()V",                              (void *)android_sdl_SDLVideo_native_init},
    {"native_setup",        "(Ljava/lang/Object;)V",            (void *)android_sdl_SDLVideo_native_setup},
    {"native_finalize",     "()V",                              (void *)android_sdl_SDLVideo_native_finalize},
};

namespace android {
	
// This function only registers the native methods
int register_android_sdl_SDLVideo(JNIEnv *env)
{
    return SDLRuntime::registerNativeMethods(env,
                kClassPathName, gMethods, NELEM(gMethods));
}
	
} // end of android namespace
