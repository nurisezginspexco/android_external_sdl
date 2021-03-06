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

#define LOG_TAG "SDLImpl-JNI"

#include "SDLRuntime.h"
#include <SDL_androidvideo.h>
#include <SDL.h>
#include <SDL_video.h>
#include <utils/Log.h>

// ----------------------------------------------------------------------------

using namespace android;

// ----------------------------------------------------------------------------
static const char* const kClassPathName = "android/sdl/impl/SDLImpl";

struct fields_t {
    jfieldID    mMajor;
	jfieldID    mMinor;
	jfieldID    mPatch;
};
static fields_t version_fields;

extern jobject
android_sdl_SDLSurface_create(SDL_Surface* surface);

extern SDL_Surface* 
android_sdl_SDLSurface_getNativeStruct(JNIEnv* env, jobject thiz);

// ----------------------------------------------------------------------------

static jint
android_sdl_impl_SDLImpl_init(JNIEnv *env, jobject thiz, jlong value)
{
    return SDL_Init((Uint32)value);
}

static jint
android_sdl_impl_SDLImpl_initSubSystem(JNIEnv *env, jobject thiz, jlong value)
{
    return SDL_InitSubSystem((Uint32)value);
}

static void
android_sdl_impl_SDLImpl_quitSubSystem(JNIEnv *env, jobject thiz, jlong value)
{
    SDL_QuitSubSystem((Uint32)value);
}

static jlong
android_sdl_impl_SDLImpl_wasInit(JNIEnv *env, jobject thiz, jlong value)
{
    return (jlong)SDL_WasInit((Uint32)value);
}

static void
android_sdl_impl_SDLImpl_quit(JNIEnv *env, jobject thiz)
{
    SDL_Quit();
}

static jobject
android_sdl_impl_SDLImpl_setVideoMode(JNIEnv *env, jobject thiz, jint width, jint height, jint bpp, jlong flags)
{
    SDL_Surface* surface = SDL_SetVideoMode(width, height, bpp, (Uint32)flags);
    return android_sdl_SDLSurface_create(surface);
}

static jint
android_sdl_impl_SDLImpl_flip(JNIEnv *env, jobject thiz, jobject surface)
{
	SDL_Surface* screen = android_sdl_SDLSurface_getNativeStruct(env, surface);
    return (jint)SDL_Flip(screen);
}

static jobject
android_sdl_impl_SDLImpl_getLinkedVersion(JNIEnv *env, jobject thiz)
{
    jobject obj = NULL;
    const SDL_version* version;
	
    jclass clazz = env->FindClass("android/sdl/SDLVersion");
    if (clazz == NULL) {
        goto end;
    }
	
    if((obj = SDLRuntime::createObject(env, clazz)) == NULL) {
	goto end;
    }
	
    version = SDL_Linked_Version();
	
    env->SetShortField(obj, version_fields.mMajor, (jshort)version->major);
    env->SetShortField(obj, version_fields.mMinor, (jshort)version->minor);
    env->SetShortField(obj, version_fields.mPatch, (jshort)version->patch);
	
//    free((void *)version);
end:
    return obj;
}

static JNINativeMethod gMethods[] = {
    {"init",                 "(J)I",                              (void *)android_sdl_impl_SDLImpl_init},
    {"initSubSystem",        "(J)I",                              (void *)android_sdl_impl_SDLImpl_initSubSystem},
    {"quitSubSystem",        "(J)V",                              (void *)android_sdl_impl_SDLImpl_quitSubSystem},
    {"wasInit",              "(J)J",                              (void *)android_sdl_impl_SDLImpl_wasInit},
    {"quit",                 "()V",                               (void *)android_sdl_impl_SDLImpl_quit},
    {"setVideoMode",         "(IIIJ)Landroid/sdl/SDLSurface;",    (void *)android_sdl_impl_SDLImpl_setVideoMode},
    {"flip",                 "(Landroid/sdl/SDLSurface;)I",       (void *)android_sdl_impl_SDLImpl_flip},
    {"getLinkedVersion",     "()Landroid/sdl/SDLVersion;",        (void *)android_sdl_impl_SDLImpl_getLinkedVersion},
};

namespace android {
	
// This function only registers the native methods
int register_android_sdl_impl_SDLImpl(JNIEnv *env)
{
	jclass clazz = env->FindClass("android/sdl/SDLVersion");
    if (clazz == NULL) {
        goto load;
    }
	version_fields.mMajor = env->GetFieldID(clazz, "mMajor", "S");
	version_fields.mMinor = env->GetFieldID(clazz, "mMinor", "S");
	version_fields.mPatch = env->GetFieldID(clazz, "mPatch", "S");
	
load:
    return SDLRuntime::registerNativeMethods(env,
                kClassPathName, gMethods, NELEM(gMethods));
}
	
} // end of android namespace
