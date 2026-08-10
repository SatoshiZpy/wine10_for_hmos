/*
 * OHOS host printer discovery interface
 *
 * Copyright 2026 Huawei Device Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef OHOS_ADAPTER_INCLUDE_OHOS_PRINT_H_
#define OHOS_ADAPTER_INCLUDE_OHOS_PRINT_H_

#include <stdint.h>

#include "../export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OHOS_PRINTER_ID_MAX 256
#define OHOS_PRINTER_NAME_MAX 256
#define OHOS_PRINTER_TEXT_MAX 512
#define OHOS_PRINTER_CAPABILITY_MAX 2048

struct ohos_printer_info {
    char id[OHOS_PRINTER_ID_MAX];
    char name[OHOS_PRINTER_NAME_MAX];
    char description[OHOS_PRINTER_TEXT_MAX];
    char make[OHOS_PRINTER_NAME_MAX];
    char uri[OHOS_PRINTER_TEXT_MAX];
    char page_sizes[OHOS_PRINTER_CAPABILITY_MAX];
    char options[OHOS_PRINTER_CAPABILITY_MAX];
    uint32_t status;
    uint32_t color_modes;
    uint32_t duplex_modes;
    uint32_t is_default;
};

/* Returns the host printer count, zero for no printers, or -1 on failure. */
int ADAPTER_EXPORT_API OhosGetPrinters(struct ohos_printer_info *printers, uint32_t capacity);

int ADAPTER_EXPORT_API OhosPrintPostScript(const char *printer_id,
                                           const char *job_name,
                                           const void *data,
                                           uint32_t length);

#ifdef __cplusplus
}
#endif

#endif
