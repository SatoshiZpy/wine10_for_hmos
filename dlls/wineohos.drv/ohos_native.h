#ifndef __WINE_OHOS_NATIVE_H
#define __WINE_OHOS_NATIVE_H

#include "ohos_adapter.h"

enum native_window_perform
{
    NATIVE_WINDOW_LOCK                        = 100,
    NATIVE_WINDOW_UNLOCK_AND_POST
};

struct native_buffer
{
    ohos_window_buffer win_buffer;
    ohos_buffer_handle buffer_handle;
    int width;
    int height;
    int stride;
    int buffer_id;
    int fence_id;
};

#endif /* __WINE_OHOS_NATIVE_H */
