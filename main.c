#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

struct egl_device {
    EGLNativeDisplayType type;
    EGLDisplay display;
    EGLConfig config;
    EGLNativeWindowType window;
    EGLSurface surface;
    EGLContext context;
    const EGLint *attr_config;
    const EGLint *attr_context;
};

/***************************************************************************************************
  X11 platform

***************************************************************************************************/
#include <X11/Xlib.h>

int egl_platform_get_display_type(struct egl_device *device)
{
    device->type = (EGLNativeDisplayType)XOpenDisplay(0);
    return 0;
}

int egl_platform_create_window(struct egl_device *device)
{
    static EGLint attr_config[] = {
        EGL_RED_SIZE,           1,
        EGL_GREEN_SIZE,         1,
        EGL_BLUE_SIZE,          1,
        EGL_DEPTH_SIZE,         1,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    Window root = RootWindow(device->type, DefaultScreen(device->type));

    EGLConfig config;
    EGLint config_count;
    eglChooseConfig(device->display, attr_config, &config, 1, &config_count);
    assert(config);
    assert(config_count > 0);

    EGLint visual_id;
    eglGetConfigAttrib(device->display, config, EGL_NATIVE_VISUAL_ID, &visual_id);

    XVisualInfo visual_templ;
    visual_templ.visualid = visual_id;

    int visual_count;
    XVisualInfo *visual_info = XGetVisualInfo(device->type, VisualIDMask, &visual_templ, &visual_count);

    XSetWindowAttributes attr = { 0 };
    attr.colormap = XCreateColormap(device->type, root, visual_info->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;

    int x = 0, y = 0, width = 1280, height = 576;
    const char name[] = "Open GL ES 2.0";

    device->window = (EGLNativeWindowType)XCreateWindow(
        device->type,
        root,
        0,
        0,
        width,
        height,
        0,
        visual_info->depth,
        InputOutput,
        visual_info->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &attr);

    XFree(visual_info);

    XSizeHints size_hints;
    size_hints.x = x;
    size_hints.y = y;
    size_hints.width  = width;
    size_hints.height = height;
    size_hints.flags = USSize | USPosition;

    XSetNormalHints(device->type, device->window, &size_hints);

    XSetStandardProperties(
        device->type,
        device->window,
        name,
        name,
        None,
        (char **)NULL,
        0,
        &size_hints);

    XMapWindow(device->type, device->window);

    return 0;
}

int egl_platform_destroy_window(struct egl_device *device)
{
    XDestroyWindow(device->type, device->window);
    XCloseDisplay(device->type);

    return 0;
}

/**************************************************************************************************/

int egl_initialize(struct egl_device *device)
{
    egl_platform_get_display_type(device);

    device->display = eglGetDisplay(device->type);
    assert(eglGetError() == EGL_SUCCESS);

    eglInitialize(device->display, NULL, NULL);
    assert(eglGetError() == EGL_SUCCESS);

    eglBindAPI(EGL_OPENGL_ES_API);
    assert(eglGetError() == EGL_SUCCESS);

    static EGLint attr_config[] = {
        EGL_SAMPLES,        0,
        EGL_RED_SIZE,       8,
        EGL_GREEN_SIZE,     8,
        EGL_BLUE_SIZE,      8,
        EGL_ALPHA_SIZE,     EGL_DONT_CARE,
        EGL_DEPTH_SIZE,     0,
        EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
        EGL_NONE
    };

    EGLint config_count = 0;
    eglChooseConfig(device->display, attr_config, &device->config, 1, &config_count);
    assert(eglGetError() == EGL_SUCCESS);
    assert(config_count == 1);

    device->attr_config = attr_config;

    egl_platform_create_window(device);

    device->surface = eglCreateWindowSurface(device->display, device->config, device->window, NULL);
    assert(eglGetError() == EGL_SUCCESS);

    static EGLint attr_context[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    device->context = eglCreateContext(device->display, device->config, EGL_NO_CONTEXT, attr_context);
    assert(eglGetError() == EGL_SUCCESS);

    device->attr_context = attr_context;

    eglMakeCurrent(device->display, device->surface, device->surface, device->context);
    assert(eglGetError() == EGL_SUCCESS);

    return 0;
}

int egl_uninitialize(struct egl_device *device)
{
    eglMakeCurrent(device->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    assert(eglGetError() == EGL_SUCCESS);

    eglDestroyContext(device->display, device->context);
    assert(eglGetError() == EGL_SUCCESS);
    device->context = (EGLContext)0;

    eglDestroySurface(device->display, device->surface);
    assert(eglGetError() == EGL_SUCCESS);
    device->surface = (EGLSurface)0;

    egl_platform_destroy_window(device);
    device->window = (EGLNativeWindowType)0;

    eglTerminate(device->display);
    assert(eglGetError() == EGL_SUCCESS);
    device->display = (EGLDisplay)0;

    eglReleaseThread();
    assert(eglGetError() == EGL_SUCCESS);

    return 0;
}

void egl_run(struct egl_device *device)
{
    sleep(3);
}

int main(int argc, char *argv[])
{
    struct egl_device device;
    egl_initialize(&device);
    egl_run(&device);
    egl_uninitialize(&device);

    return 0;
}
