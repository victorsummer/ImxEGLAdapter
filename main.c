#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/select.h>

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

void clock_gettime_diff(
    const struct timespec *time_beg,
    const struct timespec *time_end,
    struct timespec *diff)
{
    diff->tv_sec = time_end->tv_sec - time_beg->tv_sec;
    diff->tv_nsec = time_end->tv_nsec - time_beg->tv_nsec;
}

/***************************************************************************************************
  X11 platform

***************************************************************************************************/
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <pthread.h>

#define EGL_DEVICE_NAME     "Open GL ES 2"
#define EGL_DEVICE_X        0
#define EGL_DEVICE_Y        0
#define EGL_DEVICE_WIDTH    1280
#define EGL_DEVICE_HEIGHT   576

int egl_platform_get_display_type(struct egl_device *device)
{
    XInitThreads();

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

    device->window = (EGLNativeWindowType)XCreateWindow(
        device->type,
        root,
        0,
        0,
        EGL_DEVICE_WIDTH,
        EGL_DEVICE_HEIGHT,
        0,
        visual_info->depth,
        InputOutput,
        visual_info->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &attr);

    XFree(visual_info);

    XSizeHints size_hints;
    size_hints.x = EGL_DEVICE_X;
    size_hints.y = EGL_DEVICE_Y;
    size_hints.width  = EGL_DEVICE_WIDTH;
    size_hints.height = EGL_DEVICE_HEIGHT;
    size_hints.flags = USSize | USPosition;

    XSetNormalHints(device->type, device->window, &size_hints);

    XSetStandardProperties(
        device->type,
        device->window,
        EGL_DEVICE_NAME,
        EGL_DEVICE_NAME,
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

volatile int done = 0;

static void *thread_start(void *arg)
{
    struct egl_device *device = (struct egl_device *)arg;

    int x11_fd = ConnectionNumber(device->type);
    fd_set in_fds;
    struct timeval timer;

    while (!done) {
        FD_ZERO(&in_fds);
        FD_SET(x11_fd, &in_fds);

        timer.tv_usec = 0;
        timer.tv_sec = 1;

        select(x11_fd + 1, &in_fds, 0, 0, &timer);

        while (XPending(device->type)) {
            XEvent event;
            XNextEvent(device->type, &event);
            switch (event.type)
            {
                case Expose:
                    // Redraw
                    break;
                case ConfigureNotify:
                    glViewport(0, 0, (GLint)event.xconfigure.width, (GLint)event.xconfigure.height);
                    break;
                case KeyPress:
                    {
                        XLookupKeysym(&event.xkey, 0);
                        char buf[10];
                        XLookupString(&event.xkey, buf, sizeof(buf), 0, 0);
                        if (buf[0] == 27)
                            done = 1;
                    }
                    break;
                default:
                    break;
            }
        }

    }

    return 0;
}

void egl_platform_run(struct egl_device *device)
{
    system("setterm -cursor off");

    pthread_t thread;
    pthread_create(&thread, 0, &thread_start, (void *)device);

    int d = 0;
    float i = 0.0;

    struct timespec time_beg, time_end, time_diff, time_wait, time_sub;
    clock_gettime(CLOCK_MONOTONIC, &time_beg);

    while (!done) {
        {
            glClearColor(i, i, i, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            eglSwapBuffers(device->display, device->surface);

            if (d)
                i += 0.01;
            else
                i -= 0.01;

            if (i > 1.0 || i < 0.0) {
                if (i > 1.0)
                    i = 1.0;
                if (i < 0.0)
                    i = 0.0;

                d = !d;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &time_end);
        clock_gettime_diff(&time_beg, &time_end, &time_diff);
        time_beg = time_end;

        fprintf(stdout, "%.3Lf fps         \r", 1 / (time_diff.tv_sec + time_diff.tv_nsec / (long double)1000000000));
        fflush(stdout);

        do {
            clock_gettime(CLOCK_MONOTONIC, &time_wait);
            clock_gettime_diff(&time_beg, &time_wait, &time_diff);
            clock_gettime_diff(&time_end, &time_wait, &time_sub);
        } while (time_diff.tv_sec * (uint64_t)1000000000 + time_diff.tv_nsec < 1 / 60.0 * ((uint64_t)990000000 - time_sub.tv_nsec));
    }

    printf("\n");

    pthread_join(thread, 0);

    system("setterm -cursor on");
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

void signal_handler(int signal)
{
}

void egl_run(struct egl_device *device)
{
    egl_platform_run(device);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, &signal_handler);
    signal(SIGTERM, &signal_handler);

    struct egl_device device;
    egl_initialize(&device);

    printf(
        "EGL vendor: %s\n"
        "EGL version: %s\n",
        eglQueryString(device.display, EGL_VENDOR),
        eglQueryString(device.display, EGL_VERSION));

    egl_run(&device);

    egl_uninitialize(&device);

    return 0;
}
