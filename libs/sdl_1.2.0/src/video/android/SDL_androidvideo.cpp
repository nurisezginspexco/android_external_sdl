#include "SDL_androidvideo.h"

#define CLASS_PATH "SDL_androidvideo.cpp"

static SDLVideoDriver *thiz = NULL;

SDLVideoDriver::SDLVideoDriver() {
}

SDLVideoDriver::~SDLVideoDriver() {
	unregisterListener();
}

SDLVideoDriver *SDLVideoDriver::getInstance() {
    if(thiz == NULL) thiz = new SDLVideoDriver();
    return thiz;
}

void SDLVideoDriver::registerListener(SDLVideoDriverListener *listener) {
    mListener = listener;
	__android_log_print(ANDROID_LOG_INFO, CLASS_PATH, "listener registred");
}

void SDLVideoDriver::unregisterListener() {
	if(mListener == NULL) {
		return;
	}
    free(mListener);
}

void SDLVideoDriver::setBitmapConfig(SkBitmap *bitmap, int format, int width, int height) {
     switch (format) {
        case PIXEL_FORMAT_RGBA_8888:
            bitmap->setConfig(SkBitmap::kARGB_8888_Config, width, height);
            break;

        case PIXEL_FORMAT_RGBA_4444:
            bitmap->setConfig(SkBitmap::kARGB_4444_Config, width, height);
            break;

        case PIXEL_FORMAT_RGB_565:
            bitmap->setConfig(SkBitmap::kRGB_565_Config, width, height);
            break;

        case PIXEL_FORMAT_A_8:
            bitmap->setConfig(SkBitmap::kA8_Config, width, height);
            break;

        default:
            bitmap->setConfig(SkBitmap::kNo_Config, width, height);
            break;
    }
}

void SDLVideoDriver::initBitmap(int format, int width, int height) {
    //-- set screen bitmap(sdl) config based on format
    setBitmapConfig(&mBitmap, format, width, height);
    mBitmap.setIsOpaque(true);

    //-- alloc array of pixels
    if(!mBitmap.allocPixels()) {
        SDL_SetError("Failed to alloc bitmap pixels");
    }
}

/* Initialization/Query functions */
SDL_VideoDevice *SDLVideoDriver::onCreateDevice(int devindex) {
    // if class isn't yet initzialized, we will do it
    getInstance();

    /* Initialize all variables that we clean on shutdown */
    thiz->device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
    if (thiz->device) {
        SDL_memset(thiz->device, 0, (sizeof *thiz->device));
    }
    if (thiz->device == NULL) {
        SDL_OutOfMemory();
        if (thiz->device ) {
            SDL_free(thiz->device);
        }
        return 0;
    }
	
	__android_log_print(ANDROID_LOG_INFO, CLASS_PATH, "device creating");
	
    /* Set the function pointers */
    thiz->device->VideoInit = SDLVideoDriver::onVideoInit;
    thiz->device->ListModes = SDLVideoDriver::onListModes;
    thiz->device->SetVideoMode = SDLVideoDriver::onSetVideoMode;
    thiz->device->CreateYUVOverlay = NULL;
    thiz->device->SetColors = SDLVideoDriver::onSetColors;
    thiz->device->UpdateRects = SDLVideoDriver::onUpdateRects;
    thiz->device->VideoQuit = SDLVideoDriver::onVideoQuit;
    thiz->device->AllocHWSurface = SDLVideoDriver::onAllocHWSurface;
    thiz->device->CheckHWBlit = NULL;
    thiz->device->FillHWRect = NULL;
    thiz->device->SetHWColorKey = NULL;
    thiz->device->SetHWAlpha = NULL;
    thiz->device->LockHWSurface = SDLVideoDriver::onLockHWSurface;
    thiz->device->UnlockHWSurface = SDLVideoDriver::onUnlockHWSurface;
    thiz->device->FlipHWSurface = NULL;
    thiz->device->FreeHWSurface = SDLVideoDriver::onFreeHWSurface;
    thiz->device->SetCaption = SDLVideoDriver::onSetCaption;
    thiz->device->SetIcon = NULL;
    thiz->device->IconifyWindow = NULL;
    thiz->device->GrabInput = NULL;
    thiz->device->GetWMInfo = NULL;
    thiz->device->InitOSKeymap = SDLVideoDriver::onInitOSKeymap;
    thiz->device->PumpEvents = SDLVideoDriver::onPumpEvents;
    thiz->device->free = SDLVideoDriver::onDeleteDevice;
	
	thiz->mListener->notify(SDL_NATIVE_VIDEO_CREATE_DEVICE, 0, 0, (void*) thiz->device);

    __android_log_print(ANDROID_LOG_INFO, CLASS_PATH, "device created");
	
    return thiz->device;
}

int SDLVideoDriver::onVideoInit(_THIS, SDL_PixelFormat *vformat) {
    /* TODO: we only support RGB565 for now */
    vformat->BitsPerPixel = 16;
    vformat->BytesPerPixel = 2;
	
	thiz->mListener->notify(SDL_NATIVE_VIDEO_INIT, 0, 0, (void*) vformat);
    /* We're done! */
    return 0;	
}

SDL_Rect **SDLVideoDriver::onListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
    return (SDL_Rect **) -1;
}

SDL_Surface *SDLVideoDriver::onSetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags) {
    if(self == NULL)
        return NULL;
	
    /* Allocate the new pixel format for the screen */
    if (!SDL_ReallocFormat(current, bpp, 0, 0, 0, 0)) {
        SDL_SetError("Couldn't allocate new pixel format for requested mode");
        return NULL;
    }

    // create bitmap where will sdl render pixels for android
    thiz->initBitmap(PIXEL_FORMAT_RGB_565, width, height);
	
    /* Set up the new mode framebuffer */
    current->flags = (flags & SDL_FULLSCREEN) | (flags & SDL_DOUBLEBUF);
    current->w = thiz->mBitmap.width();
    current->h = thiz->mBitmap.height();
    current->pitch = current->w * (bpp / 8);
    current->pixels = thiz->mBitmap.getPixels();
	
	thiz->mListener->notify(SDL_NATIVE_VIDEO_SET_SURFACE, 0, 0, (void*) current);
    /* We're done */
    return current;
}

int SDLVideoDriver::onSetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors) {
        return 1;
}

void SDLVideoDriver::onVideoQuit(_THIS) {
}

/* Hardware surface functions */
int SDLVideoDriver::onAllocHWSurface(_THIS, SDL_Surface *surface) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_ALLOC_HW_SURFACE, 0, 0, (void*) surface);
    return(-1);
}

int SDLVideoDriver::onLockHWSurface(_THIS, SDL_Surface *surface) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_LOCK_HW_SURFACE, 0, 0, (void*) surface);
    return 0;
}

void SDLVideoDriver::onUnlockHWSurface(_THIS, SDL_Surface *surface) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_UNLOCK_HW_SURFACE, 0, 0, (void*) surface);
}

void SDLVideoDriver::onFreeHWSurface(_THIS, SDL_Surface *surface) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_FREE_HW_SURFACE, 0, 0, (void*) surface);
}

/* on set window caption */
void SDLVideoDriver::onSetCaption(_THIS, const char *title, const char *icon) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_SET_CAPTION, 0, 0, (void*) title);
}

/* etc. */
void SDLVideoDriver::onUpdateRects(_THIS, int numrects, SDL_Rect *rects) {
    if(thiz->mBitmap.width() == 0 || thiz->mBitmap.height() == 0) {
        __android_log_print(ANDROID_LOG_ERROR, CLASS_PATH, "Bitmap is too small %ix%i",
                thiz->mBitmap.width(), thiz->mBitmap.height());
        return;
    }

	thiz->mListener->notify(SDL_NATIVE_VIDEO_UPDATE_RECTS, 0, 0, (void*) &thiz->mBitmap);
}

/* ANDROID driver bootstrap functions */
int SDLVideoDriver::onAvailable() {
    return 1;
}

void SDLVideoDriver::onDeleteDevice(SDL_VideoDevice *device) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_DELETE_DEVICE, 0, 0, (void*) device);
    SDL_free(thiz->device);
}

void SDLVideoDriver::onPumpEvents(_THIS) {
	thiz->mListener->notify(SDL_NATIVE_VIDEO_PUMP_EVENTS, 0, 0, NULL);
}

void SDLVideoDriver::onInitOSKeymap(_THIS) {
	//thiz->mListener->notify(SDL_NATIVE_VIDEO_INIT_OS_KEYMAP, 0, 0, NULL);
}

extern "C" {

VideoBootStrap ANDROID_bootstrap = {
    SDLVideoDriver::getDriverName(), SDLVideoDriver::getDriverDescription(),
    SDLVideoDriver::onAvailable, SDLVideoDriver::onCreateDevice
};

}//end extern C
