#ifndef __MOCK_GRALLOC_CB_H
#define __MOCK_GRALLOC_CB_H

#include <cutils/native_handle.h>

#define CB_HANDLE_NUM_INTS(nfds) (int)((sizeof(cb_handle_t) - (nfds)*sizeof(int)) / sizeof(int))

struct cb_handle_t : public native_handle {
    cb_handle_t(int p_fd, int p_usage, int p_width, int p_height, int p_frameworkFormat, int p_format) :
            fd(p_fd),
            usage(p_usage),
            width(p_width),
            height(p_height),
            frameworkFormat(p_frameworkFormat),
            format(p_format),
            hostHandle(0)
    {
    }
    static bool validate(const cb_handle_t* hnd) {
        return hnd;
    }

    int fd;
    int usage;
    int width;
    int height;
    int frameworkFormat;
    int format;
    uint32_t hostHandle;
};

#endif  // __MOCK_GRALLOC_CB_H
