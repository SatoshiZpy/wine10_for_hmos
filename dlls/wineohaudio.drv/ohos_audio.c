/*
 * OHOS audio driver, with WOW64 marshaling adapted from winealsa.drv.
 *
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
 * Copyright 2011 Andrew Eikum for CodeWeavers
 * Copyright 2022 Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#include <ohaudio/native_audiorenderer.h>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audio_common.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "initguid.h"
#include "mmdeviceapi.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"
#include "wine/logging.h"

#include "mmreg.h"
#include "propkeydef.h"

#include "unixlib.h"
#include "wine/logging.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohaudio);

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
    BYTE *tmp_buffer;
    UINT32 tmp_buffer_frames;
    float *vols;

    INT32 getbuf_last;

    pthread_mutex_t lock;
};

static const WCHAR drv_keyW[]  = {'S','o','f','t','w','a','r','e','\\',
    'W','i','n','e','\\','D','r','i','v','e','r','s','\\',
    'w','i','n','e','o','h','a','u','d','i','o','.','d','r','v'};

/* Buffers exposed to WOW64 callers must fit in their address space. */
static ULONG_PTR zero_bits;

static const REFERENCE_TIME def_period = 100000;
static const REFERENCE_TIME min_period = 50000;

static int muldiv(int a, int b, int c)
{
    LONGLONG ret;

    if (!c) return -1;

    if ((a < 0 && b < 0) || (a >= 0 && b >= 0))
        ret = (((LONGLONG)a * b) + (c / 2)) / c;
    else
        ret = (((LONGLONG)a * b) - (c / 2)) / c;

    if (ret > INT_MAX || ret < INT_MIN) return -1;
    return ret;
}

static void silence_buffer(BYTE *buffer, UINT32 frames, const WAVEFORMATEX *fmt)
{
    UINT32 bytes = frames * fmt->nBlockAlign;
    memset(buffer, fmt->wBitsPerSample == 8 ? 0x80 : 0, bytes);
}

static int32_t ohos_write_data_callback(
    OH_AudioRenderer *renderer,
    void *userData,
    void *audioData,
    int32_t audioDataSize)
{
    struct ohos_stream *stream = userData;
    UINT32 read_frames, first_frames, read_bytes, block_align;

    if (!stream || !audioData || audioDataSize <= 0)
        return -1;

    block_align = stream->fmt->nBlockAlign;
    pthread_mutex_lock(&stream->lock);

    /* Only consume complete frames which the producer has committed. In
     * particular, a short buffer must not be reread to fill the callback. */
    read_frames = audioDataSize / block_align;
    if (read_frames > stream->held_frames)
        read_frames = stream->held_frames;
    first_frames = stream->bufsize_frames - stream->lcl_offs_frames;
    if (first_frames > read_frames)
        first_frames = read_frames;

    memcpy(audioData, stream->local_buffer + stream->lcl_offs_frames * block_align,
           first_frames * block_align);
    memcpy((BYTE *)audioData + first_frames * block_align, stream->local_buffer,
           (read_frames - first_frames) * block_align);

    stream->lcl_offs_frames = (stream->lcl_offs_frames + read_frames) % stream->bufsize_frames;
    stream->held_frames -= read_frames;
    pthread_mutex_unlock(&stream->lock);

    read_bytes = read_frames * block_align;
    /* Unsigned 8-bit PCM has its silence midpoint at 128. Avoid logging in
     * the renderer callback, including when the producer temporarily stalls. */
    if (read_bytes < audioDataSize)
        memset((BYTE *)audioData + read_bytes, stream->fmt->wBitsPerSample == 8 ? 0x80 : 0,
               audioDataSize - read_bytes);
    return 0;
}

static WAVEFORMATEX *clone_format(const WAVEFORMATEX *fmt)
{
    WAVEFORMATEX *ret;
    size_t size;

    if(fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        size = sizeof(WAVEFORMATEXTENSIBLE);
    else
        size = sizeof(WAVEFORMATEX);

    ret = malloc(size);
    if (!ret)
        return NULL;


    memcpy(ret, fmt, size);

    ret->cbSize = size - sizeof(WAVEFORMATEX);

    return ret;
}

static NTSTATUS ohos_not_implemented(void *args)
{   
    TRACE("OHOS audio driver not_implemented\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_midi_notify_wait(void *args)
{
    struct midi_notify_wait_params *params = args;

    if(params && params->quit)
        *params->quit = TRUE;

    return STATUS_SUCCESS;
}

static NTSTATUS ohos_process_attach(void *args)
{
#ifdef _WIN64
    if (NtCurrentTeb()->WowTebOffset)
    {
        SYSTEM_BASIC_INFORMATION info;
        NTSTATUS status;

        status = NtQuerySystemInformation(SystemEmulationBasicInformation, &info, sizeof(info), NULL);
        if (status) return status;
        zero_bits = (ULONG_PTR)info.HighestUserAddress | 0x7fffffff;
    }
#endif
    TRACE("OHOS audio driver process_attach\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_process_detach(void *args)
{
    TRACE("OHOS audio driver process_detach\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_main_loop(void *args)
{
    struct main_loop_params *params = args;
    NtSetEvent(params->event, NULL);
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_timer_loop(void *args)
{
    struct timer_loop_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;
    LARGE_INTEGER delay;
    UINT32 avail;

    TRACE("OHOS audio driver timer_loop\n");

    if(!stream)
        return STATUS_SUCCESS;
    
    delay.QuadPart = -stream->period;
    
    while(!stream->please_quit)
    {
        NtDelayExecution(FALSE, &delay);

        pthread_mutex_lock(&stream->lock);

        avail = stream->bufsize_frames - stream->held_frames;

        pthread_mutex_unlock(&stream->lock);

        if(avail >= stream->period_frames && stream->event)
            NtSetEvent(stream->event, NULL);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS ohos_test_connect(void *args)
{
    struct test_connect_params *params = args;

    TRACE("OHOS audio driver test_connect\n");
    params->priority = Priority_Neutral;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_endpoint_ids(void *args)
{
    static const WCHAR defaultW[] = {'d','e','f','a','u','l','t',0};
    struct get_endpoint_ids_params *params = args;
    unsigned int needed, name_len, device_len, offset;
    struct endpoint *endpoint;

    TRACE("OHOS audio driver get_endpoint_ids for flow %d\n", params->flow);

    if( params->flow != eRender ) {
        params->num = 0;
        params->result = S_OK;
        return STATUS_SUCCESS;
    }

    params->num = 1;
    params->default_idx = 0;

    name_len = wcslen(defaultW) + 1;
    device_len = strlen("default") + 1;
    needed  = sizeof(*params->endpoints) + name_len *sizeof(WCHAR) + ((device_len + 1) & ~1);

    if(needed <= params->size) {
        endpoint = params->endpoints;
        endpoint->name = offset = sizeof(*params->endpoints);
        memcpy((char*)params->endpoints + offset, defaultW, name_len * sizeof(WCHAR));
        offset += name_len * sizeof(WCHAR);
        endpoint->device = offset;
        memcpy((char*)params->endpoints + offset, "default", device_len);
        params->result = S_OK;
    } else {
        params->size = needed;
        params->result = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }
    
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_create_stream(void *args)
{
    struct create_stream_params *params = args;
    struct ohos_stream *stream;
    OH_AudioStream_Result ret;
    OH_AudioRenderer_Callbacks callbacks;
    SIZE_T size;

    TRACE("OHOS audio driver create_stream\n");

    if (params->flow != eRender) {
        params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        return STATUS_SUCCESS;
    }

    stream = calloc(1, sizeof(*stream));
    if(!stream) {
        params->result = E_OUTOFMEMORY;
        return STATUS_SUCCESS;
    }

    stream->fmt = clone_format(params->fmt);

    if(!stream->fmt) {
        free(stream);
        params->result = E_OUTOFMEMORY;
        return STATUS_SUCCESS;
    }

    stream->flags = params->flags;
    stream->share = params->share;
    stream->flow = params->flow;
    stream->period = params->period;

    pthread_mutex_init(&stream->lock, NULL);

    stream->bufsize_frames = muldiv(params->duration, stream->fmt->nSamplesPerSec, 10000000);
    if(stream->bufsize_frames == 0)
        stream->bufsize_frames = stream->fmt->nSamplesPerSec * 2;

    stream->period_frames = muldiv(stream->period, stream->fmt->nSamplesPerSec, 10000000);
    if (stream->period_frames == 0)
        stream->period_frames = 1024;

    TRACE("Buffer size: %u frames, Period: %u frames\n", stream->bufsize_frames, stream->period_frames);
    size = stream->bufsize_frames * stream->fmt->nBlockAlign;
    if(NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, zero_bits, &size,
                               MEM_COMMIT, PAGE_READWRITE)) {
        ERR("Failed to allocate local buffer\n");
        free(stream->fmt);
        free(stream);
        params->result = E_OUTOFMEMORY;
        return STATUS_SUCCESS;
    }

    silence_buffer(stream->local_buffer, stream->bufsize_frames, stream->fmt);

    ret = OH_AudioStreamBuilder_Create(&stream->builder, AUDIOSTREAM_TYPE_RENDERER);
    if (ret != AUDIOSTREAM_SUCCESS) {
        ERR("Failed to create AudioStreamBuilder: %d\n", ret);
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
        free(stream->fmt);
        free(stream);
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }

    ret = OH_AudioStreamBuilder_SetSamplingRate(stream->builder, stream->fmt->nSamplesPerSec);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set sampling rate: %d\n", ret);
    }

    ret = OH_AudioStreamBuilder_SetChannelCount(stream->builder, stream->fmt->nChannels);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set channel count: %d\n", ret);
    }

    switch (stream->fmt->wBitsPerSample) {
        case 8:
            ret = OH_AudioStreamBuilder_SetSampleFormat(stream->builder, AUDIOSTREAM_SAMPLE_U8);
            break;
        case 16:
            ret = OH_AudioStreamBuilder_SetSampleFormat(stream->builder, AUDIOSTREAM_SAMPLE_S16LE);
            break;
        default:
            WARN("Unsupported bits per sample: %u, defaulting to 16-bit\n", stream->fmt->wBitsPerSample);
            ret = OH_AudioStreamBuilder_SetSampleFormat(stream->builder, AUDIOSTREAM_SAMPLE_S16LE);
            break;
    }
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set sample format: %d\n", ret);
    }

    ret = OH_AudioStreamBuilder_SetEncodingType(stream->builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set encoding type: %d\n", ret);
    }

    ret = OH_AudioStreamBuilder_SetRendererInfo(stream->builder, AUDIOSTREAM_USAGE_MUSIC);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set renderer info: %d\n", ret);
    }

    ret = OH_AudioStreamBuilder_SetLatencyMode(stream->builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set latency mode: %d\n", ret);
    }

    ret = OH_AudioStreamBuilder_SetFrameSizeInCallback(stream->builder, 1024);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to set frame size in callback: %d\n", ret);
    }

    memset(&callbacks, 0, sizeof(OH_AudioRenderer_Callbacks));
    callbacks.OH_AudioRenderer_OnStreamEvent = NULL;
    callbacks.OH_AudioRenderer_OnInterruptEvent = NULL;
    callbacks.OH_AudioRenderer_OnError = NULL;
    callbacks.OH_AudioRenderer_OnWriteData = ohos_write_data_callback;

    ret = OH_AudioStreamBuilder_SetRendererCallback(stream->builder, callbacks, stream);
    if (ret != AUDIOSTREAM_SUCCESS) {
        ERR("Failed to set renderer callback: %d\n", ret);
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
        OH_AudioStreamBuilder_Destroy(stream->builder);
        free(stream->fmt);
        free(stream);
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }

    ret = OH_AudioStreamBuilder_GenerateRenderer(stream->builder, &stream->renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        ERR("Failed to generate renderer: %d\n", ret);
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
        OH_AudioStreamBuilder_Destroy(stream->builder);
        free(stream->fmt);
        free(stream);
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }
    *params->stream = (stream_handle)stream;
    *params->channel_count = stream->fmt->nChannels;
    params->result = S_OK;
    
    TRACE("OHOS audio stream created successfully\n");
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_release_stream(void *args)
{
    struct release_stream_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;
    SIZE_T size;

    TRACE("OHOS audio driver release_stream\n");

    if (!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    if (stream->renderer) {
        OH_AudioRenderer_Release(stream->renderer);
    }

    if (stream->builder) {
        OH_AudioStreamBuilder_Destroy(stream->builder);
    }

    if (stream->local_buffer) {
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
    }

    if (stream->tmp_buffer) {
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, &size, MEM_RELEASE);
    }

    free(stream->fmt);
    free(stream->vols);
    pthread_mutex_destroy(&stream->lock);
    free(stream);

    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_start(void *args)
{
    struct start_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;
    OH_AudioStream_Result ret;

    TRACE("OHOS audio driver start\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    ret = OH_AudioRenderer_Start(stream->renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        ERR("Failed to start renderer: %d\n", ret);
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }

    stream->started = TRUE;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_stop(void *args)
{
    struct stop_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;
    OH_AudioStream_Result ret;

    TRACE("OHOS audio driver stop\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    ret = OH_AudioRenderer_Stop(stream->renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to stop renderer: %d\n", ret);
    }

    stream->started = FALSE;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_reset(void *args)
{
    struct reset_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;
    OH_AudioStream_Result ret;

    TRACE("OHOS audio driver reset\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    pthread_mutex_lock(&stream->lock);

    if (stream->started){
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_NOT_STOPPED;
        return STATUS_SUCCESS;
    }

    if (stream->getbuf_last){
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_BUFFER_OPERATION_PENDING;
        return STATUS_SUCCESS;
    }
    ret = OH_AudioRenderer_Flush(stream->renderer);
    if (ret != AUDIOSTREAM_SUCCESS) {
        WARN("Failed to flush renderer: %d\n", ret);
    }

    stream->held_frames = 0;
    stream->lcl_offs_frames = 0;
    stream->wri_offs_frames = 0;
    stream->written_frames = 0;

    pthread_mutex_unlock(&stream->lock);

    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_render_buffer(void *args)
{
    struct get_render_buffer_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream*)params->stream;
    SIZE_T size;

    TRACE("OHOS audio driver get_render_buffer\n");

    pthread_mutex_lock(&stream->lock);

    if (stream->getbuf_last) 
    {
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_OUT_OF_ORDER;
        return STATUS_SUCCESS;
    }

    if (!params->frames)
    {
        pthread_mutex_unlock(&stream->lock);
        *params->data = NULL;
        params->result = S_OK;
        return STATUS_SUCCESS;
    }

    if (stream->held_frames + params->frames > stream->bufsize_frames)
    {
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_BUFFER_TOO_LARGE;
        return STATUS_SUCCESS;
    }
    if (stream->wri_offs_frames + params->frames > stream->bufsize_frames)
    {
        if (stream->tmp_buffer_frames < params->frames)
        {
            if (stream->tmp_buffer)
            {
                size = 0;
                NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, &size, MEM_RELEASE);
                stream->tmp_buffer = NULL;
            }
            size = params->frames * stream->fmt->nBlockAlign;
            if (NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, zero_bits, &size,
                                   MEM_COMMIT, PAGE_READWRITE)) {
                stream->tmp_buffer_frames = 0;
                pthread_mutex_unlock(&stream->lock);
                params->result = E_OUTOFMEMORY;
                return STATUS_SUCCESS;
            }
            stream->tmp_buffer_frames = params->frames;
        }
        *params->data = stream->tmp_buffer;
        stream->getbuf_last = -params->frames;
    }else {
        *params->data = stream->local_buffer + stream->wri_offs_frames * stream->fmt->nBlockAlign;
        stream->getbuf_last = params->frames;
    }

    silence_buffer(*params->data, params->frames, stream->fmt);

    pthread_mutex_unlock(&stream->lock);
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static void ohos_wrap_buffer(struct ohos_stream *stream, BYTE *buffer, UINT32 written_frames)
{
    UINT32 write_offs_frames = stream->wri_offs_frames;
    UINT32 write_offs_bytes = write_offs_frames * stream->fmt->nBlockAlign;
    UINT32 chunk_frames = stream->bufsize_frames - write_offs_frames;
    UINT32 chunk_bytes = chunk_frames * stream->fmt->nBlockAlign;
    UINT32 written_bytes = written_frames * stream->fmt->nBlockAlign;

    if (written_bytes <= chunk_bytes)
    {
        memcpy(stream->local_buffer + write_offs_bytes, buffer, written_bytes);
    }else{
        memcpy(stream->local_buffer + write_offs_bytes, buffer, chunk_bytes);
        memcpy(stream->local_buffer, buffer + chunk_bytes, written_bytes - chunk_bytes);
    }
}

static NTSTATUS ohos_release_render_buffer(void *args)
{
    struct release_render_buffer_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream*)params->stream;
    UINT32 written_frames = params->written_frames;
    BYTE *buffer;

    TRACE("OHOS audio driver release_render_buffer\n");

    pthread_mutex_lock(&stream->lock);

    if (!written_frames)
    {
        stream->getbuf_last = 0;
        pthread_mutex_unlock(&stream->lock);
        params->result = S_OK;
        return STATUS_SUCCESS;
    }

    if (!stream->getbuf_last){
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_OUT_OF_ORDER;
        return STATUS_SUCCESS;
    }

    if (written_frames > (stream->getbuf_last >= 0 ? stream->getbuf_last : -stream->getbuf_last)){
        pthread_mutex_unlock(&stream->lock);
        params->result = AUDCLNT_E_INVALID_SIZE;
        return STATUS_SUCCESS;
    }

    if (stream->getbuf_last >= 0 )
        buffer = stream->local_buffer + stream->wri_offs_frames * stream->fmt->nBlockAlign;
    else
        buffer = stream->tmp_buffer;

    if (params->flags & AUDCLNT_BUFFERFLAGS_SILENT)
        silence_buffer(buffer, written_frames, stream->fmt);

    if (stream->getbuf_last < 0)
        ohos_wrap_buffer(stream, buffer, written_frames);
    
    stream->wri_offs_frames += written_frames;
    stream->wri_offs_frames %= stream->bufsize_frames;
    stream->held_frames += written_frames;
    stream->written_frames += written_frames;
    stream->getbuf_last = 0;

    pthread_mutex_unlock(&stream->lock);
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_is_format_supported(void *args)
{
    struct is_format_supported_params *params = args;

    TRACE("OHOS audio driver is_format_supported\n");
    
    if (params->flow != eRender) {
        params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        return STATUS_SUCCESS;
    }

    if (params->fmt_in->wFormatTag != WAVE_FORMAT_PCM &&
        params->fmt_in->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
        params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        return STATUS_SUCCESS;
    }

    switch (params->fmt_in->nSamplesPerSec) {
        case 8000:
        case 11025:
        case 22050:
        case 44100:
        case 48000:
        case 96000:
            break;
        default:
            TRACE("Unsupported sample rate: %lu\n", params->fmt_in->nSamplesPerSec);
            params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
            return STATUS_SUCCESS;
    }

    if (params->fmt_in->nChannels != 1 && params->fmt_in->nChannels != 2) {
        TRACE("Unsupported channel count: %u\n", params->fmt_in->nChannels);
        params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        return STATUS_SUCCESS;
    }

    if (params->fmt_in->wBitsPerSample != 8 && params->fmt_in->wBitsPerSample != 16) {
        TRACE("Unsupported bits per sample: %u\n", params->fmt_in->wBitsPerSample);
        params->result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        return STATUS_SUCCESS;
    }
    TRACE("OHOS audio driver format supported: %lu Hz, %u ch, %u-bit\n",
         params->fmt_in->nSamplesPerSec, params->fmt_in->nChannels, params->fmt_in->wBitsPerSample);
    params->result = S_OK;
    return STATUS_SUCCESS;
}   

static NTSTATUS ohos_get_mix_format(void *args)
{
    struct get_mix_format_params *params = args;
    WAVEFORMATEXTENSIBLE *fmt = params->fmt;

    TRACE("OHOS audio driver get_mix_format\n");

    memset(fmt, 0, sizeof(*fmt));
    fmt->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    fmt->Format.nChannels = 2;
    fmt->Format.nSamplesPerSec = 44100;
    fmt->Format.wBitsPerSample = 16;
    fmt->Format.nBlockAlign = fmt->Format.nChannels * fmt->Format.wBitsPerSample / 8;
    fmt->Format.nAvgBytesPerSec = fmt->Format.nSamplesPerSec * fmt->Format.nBlockAlign;
    fmt->Format.cbSize = sizeof(*fmt) - sizeof(fmt->Format);
    fmt->Samples.wValidBitsPerSample = 16;
    fmt->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    fmt->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    TRACE("OHOS audio driver mix format: 44100 Hz, 2 ch, 16-bit\n");
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_device_period(void *args)
{
    struct get_device_period_params *params = args;

    TRACE("OHOS audio driver get_device_period\n");

    if (params->def_period)
        *params->def_period = def_period;
    if (params->min_period)
        *params->min_period = min_period;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_buffer_size(void *args)
{
    struct get_buffer_size_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver get_buffer_size\n");

    if (!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    *params->frames = stream->bufsize_frames;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_latency(void *args)
{
    struct get_latency_params *params = args;

    TRACE("OHOS audio driver get_latency\n");

    *params->latency = def_period;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_current_padding(void *args)
{
    struct get_current_padding_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver get_current_padding\n");

    if (!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    *params->padding = stream->held_frames;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_next_packet_size(void *args)
{
    struct get_next_packet_size_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver get_next_packet_size\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    if(stream->started)
        *params->frames = stream->period_frames;
    else
        *params->frames = 0;

    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_position(void *args)
{
    struct get_position_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver get_position\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    *params->pos = stream->written_frames - stream->held_frames;

    if (stream->share == AUDCLNT_SHAREMODE_SHARED)
        *params->pos *= stream->fmt->nBlockAlign;

    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_frequency(void *args)
{
    struct get_frequency_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver get_frequency\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    if (stream->share == AUDCLNT_SHAREMODE_SHARED)
        *params->freq = (UINT64)stream->fmt->nSamplesPerSec * stream->fmt->nBlockAlign;
    else
        *params->freq = stream->fmt->nSamplesPerSec;
    
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_set_event_handle(void *args)
{
    struct set_event_handle_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver set_event_handle\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    stream->event = params->event;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_is_started(void *args)
{
    struct is_started_params *params = args;
    struct ohos_stream *stream = (struct ohos_stream *)params->stream;

    TRACE("OHOS audio driver is_started\n");

    if(!stream) {
        params->result = E_POINTER;
        return STATUS_SUCCESS;
    }

    params->result = stream->started ? S_OK : S_FALSE;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_get_prop_value(void *args)
{
    static const GUID PKEY_AudioEndpoint_GUID = {
        0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}
    };
    static const PROPERTYKEY devicepath_key = {
        {0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 2
    };

    struct get_prop_value_params *params = args;
    const GUID *guid = params->guid;
    PROPVARIANT *out = params->value;
    char path[128];
    int len;
    UINT serial_number;

    TRACE("OHOS audio driver get_prop_value\n");

    serial_number = (guid->Data4[4] << 24) | (guid->Data4[5] << 16) |
                    (guid->Data4[6] << 8) | guid->Data4[7];

    if (IsEqualPropertyKey(*params->prop, devicepath_key)) {

        len = sprintf(path, "{1}.ROOT\\MEDIA\\%04u", 0);

        if (*params->buffer_size < ++len * sizeof(WCHAR)) {
            params->result = E_NOT_SUFFICIENT_BUFFER;
            *params->buffer_size = len * sizeof(WCHAR);
            return STATUS_SUCCESS;
        }

        out->vt = VT_LPWSTR;
        out->pwszVal = params->buffer;

        ntdll_umbstowcs(path, len, out->pwszVal, len);

        params->result = S_OK;
        return STATUS_SUCCESS;
    } else if (IsEqualGUID(&params->prop->fmtid, &PKEY_AudioEndpoint_GUID)) {
        switch (params->prop->pid) {
            case 0:
                out->vt = VT_UI4;
                out->ulVal = (params->flow == eRender) ? Speakers : Microphone;
                params->result = S_OK;
                return STATUS_SUCCESS;
           case 3:
                if (params->flow == eRender) {
                    out->vt = VT_UI4;
                    out->ulVal = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
                    params->result = S_OK;
                    return STATUS_SUCCESS;
                }
                break;
        }
    } 

    params->result = E_NOTIMPL;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] = 
{
    ohos_process_attach,
    ohos_process_detach,
    ohos_main_loop,
    ohos_get_endpoint_ids,
    ohos_create_stream,
    ohos_release_stream,
    ohos_start,
    ohos_stop,
    ohos_reset,
    ohos_timer_loop,
    ohos_get_render_buffer,
    ohos_release_render_buffer,
    ohos_not_implemented, /* get_capture_buffer*/
    ohos_not_implemented, /* release_capture_buffer */
    ohos_is_format_supported,
    ohos_not_implemented, /* get_loopback_capture_device */
    ohos_get_mix_format,
    ohos_get_device_period,
    ohos_get_buffer_size,
    ohos_get_latency,
    ohos_get_current_padding,
    ohos_get_next_packet_size,
    ohos_get_frequency,
    ohos_get_position,
    ohos_not_implemented, /* set_volumes */
    ohos_set_event_handle,
    ohos_not_implemented, /* set_sample_rate */
    ohos_test_connect,
    ohos_is_started,
    ohos_get_prop_value,
    ohos_not_implemented, /* midi_get_driver */
    ohos_not_implemented, /* midi_init */
    ohos_not_implemented, /* midi_release */
    ohos_not_implemented, /* midi_out_message */
    ohos_not_implemented, /* midi_in_message */
    ohos_midi_notify_wait,
    ohos_not_implemented, /* aux_message */
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == funcs_count);

#ifdef _WIN64

typedef UINT PTR32;

static NTSTATUS ohos_wow64_main_loop(void *args)
{
    struct
    {
        PTR32 event;
    } *params32 = args;
    struct main_loop_params params =
    {
        .event = ULongToHandle(params32->event)
    };
    return ohos_main_loop(&params);
}

static NTSTATUS ohos_wow64_get_endpoint_ids(void *args)
{
    struct
    {
        EDataFlow flow;
        PTR32 endpoints;
        unsigned int size;
        HRESULT result;
        unsigned int num;
        unsigned int default_idx;
    } *params32 = args;
    struct get_endpoint_ids_params params =
    {
        .flow = params32->flow,
        .endpoints = ULongToPtr(params32->endpoints),
        .size = params32->size
    };
    ohos_get_endpoint_ids(&params);
    params32->size = params.size;
    params32->result = params.result;
    params32->num = params.num;
    params32->default_idx = params.default_idx;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_create_stream(void *args)
{
    struct
    {
        PTR32 name;
        PTR32 device;
        EDataFlow flow;
        AUDCLNT_SHAREMODE share;
        DWORD flags;
        REFERENCE_TIME duration;
        REFERENCE_TIME period;
        PTR32 fmt;
        HRESULT result;
        PTR32 channel_count;
        PTR32 stream;
    } *params32 = args;
    struct create_stream_params params =
    {
        .name = ULongToPtr(params32->name),
        .device = ULongToPtr(params32->device),
        .flow = params32->flow,
        .share = params32->share,
        .flags = params32->flags,
        .duration = params32->duration,
        .period = params32->period,
        .fmt = ULongToPtr(params32->fmt),
        .channel_count = ULongToPtr(params32->channel_count),
        .stream = ULongToPtr(params32->stream)
    };
    ohos_create_stream(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_release_stream(void *args)
{
    struct
    {
        stream_handle stream;
        PTR32 timer_thread;
        HRESULT result;
    } *params32 = args;
    struct release_stream_params params =
    {
        .stream = params32->stream,
        .timer_thread = ULongToHandle(params32->timer_thread)
    };
    ohos_release_stream(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_render_buffer(void *args)
{
    struct
    {
        stream_handle stream;
        UINT32 frames;
        HRESULT result;
        PTR32 data;
    } *params32 = args;
    BYTE *data = NULL;
    struct get_render_buffer_params params =
    {
        .stream = params32->stream,
        .frames = params32->frames,
        .data = &data
    };
    ohos_get_render_buffer(&params);
    params32->result = params.result;
    *(unsigned int *)ULongToPtr(params32->data) = PtrToUlong(data);
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_is_format_supported(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        AUDCLNT_SHAREMODE share;
        PTR32 fmt_in;
        PTR32 fmt_out;
        HRESULT result;
    } *params32 = args;
    struct is_format_supported_params params =
    {
        .device = ULongToPtr(params32->device),
        .flow = params32->flow,
        .share = params32->share,
        .fmt_in = ULongToPtr(params32->fmt_in),
        .fmt_out = ULongToPtr(params32->fmt_out)
    };
    ohos_is_format_supported(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_mix_format(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        PTR32 fmt;
        HRESULT result;
    } *params32 = args;
    struct get_mix_format_params params =
    {
        .device = ULongToPtr(params32->device),
        .flow = params32->flow,
        .fmt = ULongToPtr(params32->fmt)
    };
    ohos_get_mix_format(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_device_period(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        HRESULT result;
        PTR32 def_period;
        PTR32 min_period;
    } *params32 = args;
    struct get_device_period_params params =
    {
        .device = ULongToPtr(params32->device),
        .flow = params32->flow,
        .def_period = ULongToPtr(params32->def_period),
        .min_period = ULongToPtr(params32->min_period),
    };
    ohos_get_device_period(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_buffer_size(void *args)
{
    struct
    {
        stream_handle stream;
        HRESULT result;
        PTR32 frames;
    } *params32 = args;
    struct get_buffer_size_params params =
    {
        .stream = params32->stream,
        .frames = ULongToPtr(params32->frames)
    };
    ohos_get_buffer_size(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_latency(void *args)
{
    struct
    {
        stream_handle stream;
        HRESULT result;
        PTR32 latency;
    } *params32 = args;
    struct get_latency_params params =
    {
        .stream = params32->stream,
        .latency = ULongToPtr(params32->latency)
    };
    ohos_get_latency(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_current_padding(void *args)
{
    struct
    {
        stream_handle stream;
        HRESULT result;
        PTR32 padding;
    } *params32 = args;
    struct get_current_padding_params params =
    {
        .stream = params32->stream,
        .padding = ULongToPtr(params32->padding)
    };
    ohos_get_current_padding(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_next_packet_size(void *args)
{
    struct
    {
        stream_handle stream;
        HRESULT result;
        PTR32 frames;
    } *params32 = args;
    struct get_next_packet_size_params params =
    {
        .stream = params32->stream,
        .frames = ULongToPtr(params32->frames)
    };
    ohos_get_next_packet_size(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_frequency(void *args)
{
    struct
    {
        stream_handle stream;
        HRESULT result;
        PTR32 freq;
    } *params32 = args;
    struct get_frequency_params params =
    {
        .stream = params32->stream,
        .freq = ULongToPtr(params32->freq)
    };
    ohos_get_frequency(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_position(void *args)
{
    struct
    {
        stream_handle stream;
        BOOL device;
        HRESULT result;
        PTR32 pos;
        PTR32 qpctime;
    } *params32 = args;
    struct get_position_params params =
    {
        .stream = params32->stream,
        .device = params32->device,
        .pos = ULongToPtr(params32->pos),
        .qpctime = ULongToPtr(params32->qpctime)
    };
    ohos_get_position(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_set_event_handle(void *args)
{
    struct
    {
        stream_handle stream;
        PTR32 event;
        HRESULT result;
    } *params32 = args;
    struct set_event_handle_params params =
    {
        .stream = params32->stream,
        .event = ULongToHandle(params32->event)
    };

    ohos_set_event_handle(&params);
    params32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_get_prop_value(void *args)
{
    struct propvariant32
    {
        WORD vt;
        WORD pad1, pad2, pad3;
        union
        {
            ULONG ulVal;
            PTR32 ptr;
            ULARGE_INTEGER uhVal;
        };
    } *value32;
    struct
    {
        PTR32 device;
        EDataFlow flow;
        PTR32 guid;
        PTR32 prop;
        HRESULT result;
        PTR32 value;
        PTR32 buffer; /* caller allocated buffer to hold value's strings */
        PTR32 buffer_size;
    } *params32 = args;
    PROPVARIANT value = {0};
    struct get_prop_value_params params =
    {
        .device = ULongToPtr(params32->device),
        .flow = params32->flow,
        .guid = ULongToPtr(params32->guid),
        .prop = ULongToPtr(params32->prop),
        .value = &value,
        .buffer = ULongToPtr(params32->buffer),
        .buffer_size = ULongToPtr(params32->buffer_size)
    };
    ohos_get_prop_value(&params);
    params32->result = params.result;
    if (SUCCEEDED(params.result))
    {
        value32 = UlongToPtr(params32->value);
        value32->vt = value.vt;
        switch (value.vt)
        {
        case VT_UI4:
            value32->ulVal = value.ulVal;
            break;
        case VT_LPWSTR:
            value32->ptr = params32->buffer;
            break;
        default:
            FIXME("Unhandled vt %04x\n", value.vt);
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_test_connect(void *args)
{
    struct
    {
        PTR32 name;
        enum driver_priority priority;
    } *params32 = args;
    struct test_connect_params params =
    {
        .name = ULongToPtr(params32->name)
    };

    ohos_test_connect(&params);
    params32->priority = params.priority;
    return STATUS_SUCCESS;
}

static NTSTATUS ohos_wow64_midi_notify_wait(void *args)
{
    struct
    {
        PTR32 quit;
        PTR32 notify;
    } *params32 = args;
    struct midi_notify_wait_params params =
    {
        .quit = ULongToPtr(params32->quit)
    };

    /* The OHOS stub only sets quit; it never produces a notification. */
    return ohos_midi_notify_wait(&params);
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    ohos_process_attach,
    ohos_process_detach,
    ohos_wow64_main_loop,
    ohos_wow64_get_endpoint_ids,
    ohos_wow64_create_stream,
    ohos_wow64_release_stream,
    ohos_start,
    ohos_stop,
    ohos_reset,
    ohos_timer_loop,
    ohos_wow64_get_render_buffer,
    ohos_release_render_buffer,
    ohos_not_implemented, /* get_capture_buffer*/
    ohos_not_implemented, /* release_capture_buffer */
    ohos_wow64_is_format_supported,
    ohos_not_implemented, /* get_loopback_capture_device */
    ohos_wow64_get_mix_format,
    ohos_wow64_get_device_period,
    ohos_wow64_get_buffer_size,
    ohos_wow64_get_latency,
    ohos_wow64_get_current_padding,
    ohos_wow64_get_next_packet_size,
    ohos_wow64_get_frequency,
    ohos_wow64_get_position,
    ohos_not_implemented, /* set_volumes */
    ohos_wow64_set_event_handle,
    ohos_not_implemented, /* set_sample_rate */
    ohos_wow64_test_connect,
    ohos_is_started,
    ohos_wow64_get_prop_value,
    ohos_not_implemented, /* midi_get_driver */
    ohos_not_implemented, /* midi_init */
    ohos_not_implemented, /* midi_release */
    ohos_not_implemented, /* midi_out_message */
    ohos_not_implemented, /* midi_in_message */
    ohos_wow64_midi_notify_wait,
    ohos_not_implemented, /* aux_message */
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == funcs_count);

#endif /* _WIN64 */
