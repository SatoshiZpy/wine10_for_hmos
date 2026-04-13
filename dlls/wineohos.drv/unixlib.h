
#include "wine/unixlib.h"

enum ohos_funcs
{
    unix_init,
    unix_funcs_count
};

#define OHOS_CALL(func, params) WINE_UNIX_CALL( unix_ ## func, params )

