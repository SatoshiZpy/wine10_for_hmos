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
    memset(buffer, 0, bytes);
}

static int32_t ohos_write_data_callback(
    OH_AudioRenderer *renderer,
    void *userData,
    void *audioData,
    int32_t audioDataSize)
{
    struct ohos_stream *stream = (struct ohos_stream *)userData;
    UINT32 read_bytes = 0;
    UINT32 to_read;
    UINT32 lcl_offs_bytes;
    UINT32 chunk_bytes;

    if (!stream || !audioData || audioDataSize <= 0){
        LOGE("Invalid callback parameters: stream=%{public}p, audioData=%{public}p, size=%{public}d\n", 
            stream, audioData, audioDataSize);
        return -1;
    }

    pthread_mutex_lock(&stream->lock);

    lcl_offs_bytes = stream->lcl_offs_frames * stream->fmt->nBlockAlign;

    while(read_bytes < audioDataSize && stream->held_frames > 0)
    {
        to_read = audioDataSize - read_bytes;
        chunk_bytes = stream->bufsize_frames * stream->fmt->nBlockAlign - lcl_offs_bytes;

        if (to_read > chunk_bytes)
            to_read = chunk_bytes;

        if (to_read > stream->held_frames * stream->fmt->nBlockAlign)
            to_read = stream->held_frames * stream->fmt->nBlockAlign;
        
        if(to_read == 0)
            break;

        memcpy((BYTE *)audioData + read_bytes, stream->local_buffer + lcl_offs_bytes, to_read);

        lcl_offs_bytes += to_read;
        if (lcl_offs_bytes >= stream->bufsize_frames * stream->fmt->nBlockAlign)
            lcl_offs_bytes = 0;

            read_bytes += to_read;
    }

    if(read_bytes > 0){
        UINT32 read_frames = read_bytes / stream->fmt->nBlockAlign;
        if (stream->held_frames >= read_frames)
            stream->held_frames -= read_frames;
        else
            stream->held_frames = 0;
        
        stream->lcl_offs_frames += read_frames;
        stream->lcl_offs_frames %= stream->bufsize_frames;
    }

    pthread_mutex_unlock(&stream->lock);

    if(read_bytes < audioDataSize)
    {
        LOGD("Not enough data in buffer: %{public}u %{public}d (underrun by %{public}d bytes)\n",
            read_bytes, audioDataSize, audioDataSize - read_bytes);
        /* Fill remaining with silence */
        memset((BYTE *)audioData + read_bytes, 0, audioDataSize - read_bytes);
    }   

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
    if(NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, 0, &size,
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
            if (NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, 0, &size,
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

    *params->def_period = def_period;
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