#ifndef __WINE_OHAUDIO_H
#define __WINE_OHAUDIO_H

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audio_common.h>

struct ring_buffer
{
    BYTE *data;
    UINT32 size;
    UINT32 read_pos;
    UINT32 write_pos;
    UINT32 available;
};

struct ohos_stream
{   
    OH_AudioRenderer *renderer;
    OH_AudioStreamBuilder *builder;

    WAVEFORMATEX *fmt;
    DWORD flags;
    AUDCLNT_SHAREMODE share;
    EDataFlow flow;
    HANDLE event;

    BOOL started, please_quit;
    REFERENCE_TIME period;
    UINT32 bufsize_frames, period_frames;
    UINT32 lcl_offs_frames, held_frames, wri_offs_frames;
    UINT64 written_frames;

    BYTE *local_buffer;
    float *vols;

    struct ring_buffer ring_buffer;
    pthread_mutex_t ring_lock;
    pthread_cond_t ring_cond;

    pthread_mutex_t lock;
};

#endif /* __WINE_OHAUDIO_H */
