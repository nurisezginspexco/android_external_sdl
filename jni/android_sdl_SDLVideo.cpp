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

#include <SDL_androidaudio.h>
#include <SDL_androidvideo.h>

#include <utils/Log.h>
#include <jni.h>

// ----------------------------------------------------------------------------

using namespace android;

// ----------------------------------------------------------------------------

struct fields_t {
    //jfieldID    context;
    //jfieldID    surface;
    /* actually in android.view.Surface XXX */
    //jfieldID    surface_native;

    jmethodID   post_event;
};
static fields_t fields;

static const char* const kClassPathName = "android/sdl/SDLVideo";


//-- helper methods for creating java classes representing SDL native structs

static jobject createObject(JNIEnv* env, jclass clazz) {	
	jmethodID constructor = env->GetMethodID(clazz, "<init>", "()V");
	if (constructor == NULL) {
		SDLRuntime::doThrow(env, "java/lang/RuntimeException", "Can't find constructor");
        return NULL;
    }
	
	jobject obj = env->NewObject(clazz, constructor);
	if (obj == NULL) {
		SDLRuntime::doThrow(env, "java/lang/RuntimeException", "Can't invoke constructor");
        return NULL;
    }
	return obj;
}

static jobject createJavaSDLSurface(SDL_Surface* surface) {
	jobject obj;
	JNIEnv* env = SDLRuntime::getJNIEnv();
	
	jclass clazz = env->FindClass("android/sdl/SDLSurface");
	if (clazz == NULL) {
		SDLRuntime::doThrow(env, "java/lang/RuntimeException", "android/sdl/SDLSurface");
        return NULL;
    }
	
	if((obj = createObject(env, clazz)) == NULL) {
		return NULL;
	}
	
	return obj;
}

static jobject createJavaSDLPixelFormat(SDL_PixelFormat* pformat) {
	jobject obj;
	JNIEnv* env = SDLRuntime::getJNIEnv();
	
	jclass clazz = env->FindClass("android/sdl/SDLPixelFormat");
	if (clazz == NULL) {
		SDLRuntime::doThrow(env, "java/lang/RuntimeException", "android/sdl/SDLPixelFormat");
        return NULL;
    }
	
	if((obj = createObject(env, clazz)) == NULL) {
		return NULL;
	}
	
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mBitsPerPixel", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mBytesPerPixel", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mRloss", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mGloss", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mBloss", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mAloss", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mRshift", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mGshift", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mBshift", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mAshift", "I"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mRmask", "L"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mGmask", "L"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mBmask", "L"),
					 (jint)0);
	env->SetIntField(obj,
					 env->GetFieldID(clazz, "mAmask", "L"),
					 (jint)0);
	return NULL;
}

static jobject createJavaSDLVideoDevice(SDL_VideoDevice* device) {
	jobject obj;
	JNIEnv* env = SDLRuntime::getJNIEnv();
	
	jclass clazz = env->FindClass("android/sdl/SDLVideoDevice");
	if (clazz == NULL) {
		SDLRuntime::doThrow(env, "java/lang/RuntimeException", "android/sdl/SDLVideoDevice");
        return NULL;
    }
	
	if((obj = createObject(env, clazz)) == NULL) {
		return NULL;
	}
	
	return obj;
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// ref-counted object for callbacks
class JNISDLVideoDriverListener: public SDLVideoDriverListener
{
public:
    JNISDLVideoDriverListener(JNIEnv* env, jobject thiz, jobject weak_thiz);
    ~JNISDLVideoDriverListener();
    void notify(int arg0, int arg1, void* data);
private:
    jclass      mClass;     // Reference to MediaPlayer class
    jobject     mObject;    // Weak ref to MediaPlayer Java object to call on
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
    mObject  = env->NewGlobalRef(weak_thiz);
}

JNISDLVideoDriverListener::~JNISDLVideoDriverListener()
{
    // remove global references
    JNIEnv *env = SDLRuntime::getJNIEnv();
    env->DeleteGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);
}

// callback from libsdl which notify us about situation in sdl video driver
void JNISDLVideoDriverListener::notify(int arg0, int arg1, void* data)
{
    JNIEnv *env = SDLRuntime::getJNIEnv();
	jobject obj;
	switch (arg0) {
		case SDL_NATIVE_VIDEO_CREATE_DEVICE:
			obj = createJavaSDLVideoDevice((SDL_VideoDevice*) data);
			break;
		case SDL_NATIVE_VIDEO_DELETE_DEVICE:
			break;
		case SDL_NATIVE_VIDEO_PROCESS_EVENTS:
			break;
		case SDL_NATIVE_VIDEO_INIT:
			obj = createJavaSDLPixelFormat((SDL_PixelFormat*) data);
			break;
		case SDL_NATIVE_VIDEO_SET_SURFACE:
			obj = createJavaSDLSurface((SDL_Surface*) data);
			break;
		case SDL_NATIVE_VIDEO_PUMP_EVENTS:
			break;
		case SDL_NATIVE_VIDEO_UPDATE_RECTS:
			break;
	}
    env->CallStaticVoidMethod(mClass, fields.post_event, mObject, arg0, arg1, obj);
}

// ----------------------------------------------------------------------------

// This function gets some field IDs, which in turn causes class initialization.
// It is called from a static block in MediaPlayer, which won't run until the
// first time an instance of this class is used.
static void
android_sdl_SDLVideo_native_init(JNIEnv *env)
{
    jclass clazz;

    clazz = env->FindClass(kClassPathName);
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
                                               "(IILjava/lang/Object;)V");
    if (fields.post_event == NULL) {
        SDLRuntime::doThrow(env, "java/lang/RuntimeException", 
							"Can't find SDLVideo.postEventFromNative");
        return;
    }

	/*
    fields.surface = env->GetFieldID(clazz, "mSurface", "Landroid/view/Surface;");
    if (fields.surface == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Can't find MediaPlayer.mSurface");
        return;
    }

    jclass surface = env->FindClass("android/view/Surface");
    if (surface == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Can't find android/view/Surface");
        return;
    }

    fields.surface_native = env->GetFieldID(surface, "mSurface", "I");
    if (fields.surface_native == NULL) {
        jniThrowException(env, "java/lang/RuntimeException", "Can't find Surface.mSurface");
        return;
    }
	*/
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

// KTHXBYE