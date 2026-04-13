/*
 * Copyright 2015 Austin English
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

#if !defined(__OHOS__)
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmplayer);

int __cdecl wmain(int argc, WCHAR *argv[])
{
    int i;

    WINE_FIXME("stub:");
    for (i = 0; i < argc; i++)
        WINE_FIXME(" %s", wine_dbgstr_w(argv[i]));
    WINE_FIXME("\n");

    return 0;
}
#else
#define COBJMACROS

#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <stdio.h>
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmplayer);

/* 全局播放器对象 */
static IMFPMediaPlayer *g_pPlayer = NULL;
static HWND g_hwnd = NULL;

/* 回调接口用于处理媒体事件 */
typedef struct MediaPlayerCallback
{
    IMFPMediaPlayerCallback IMFPMediaPlayerCallback_iface;
    LONG ref_count;
} MediaPlayerCallback;

static inline MediaPlayerCallback *impl_from_IMFPMediaPlayerCallback(IMFPMediaPlayerCallback *iface)
{
    return CONTAINING_RECORD(iface, MediaPlayerCallback, IMFPMediaPlayerCallback_iface);
}

static HRESULT STDMETHODCALLTYPE MediaPlayerCallback_QueryInterface(IMFPMediaPlayerCallback *iface,
                                                                    REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IMFPMediaPlayerCallback) || IsEqualIID(riid, &IID_IUnknown))
    {
        *ppv = iface;
        IMFPMediaPlayerCallback_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE MediaPlayerCallback_AddRef(IMFPMediaPlayerCallback *iface)
{
    MediaPlayerCallback *callback = impl_from_IMFPMediaPlayerCallback(iface);
    return InterlockedIncrement(&callback->ref_count);
}

static ULONG STDMETHODCALLTYPE MediaPlayerCallback_Release(IMFPMediaPlayerCallback *iface)
{
    MediaPlayerCallback *callback = impl_from_IMFPMediaPlayerCallback(iface);
    LONG ref = InterlockedDecrement(&callback->ref_count);
    if (ref == 0)
        HeapFree(GetProcessHeap(), 0, callback);
    return ref;
}

static void STDMETHODCALLTYPE MediaPlayerCallback_OnMediaPlayerEvent(IMFPMediaPlayerCallback *iface,
                                                                     MFP_EVENT_HEADER *pEventHeader)
{
    WINE_TRACE("Media event: %x\n", pEventHeader->eEventType);

    switch (pEventHeader->eEventType)
    {
        case MFP_EVENT_TYPE_MEDIAITEM_CREATED:
            WINE_TRACE("Media item created\n");
            break;

        case MFP_EVENT_TYPE_PLAY:
            WINE_TRACE("Play event\n");
            break;

        case MFP_EVENT_TYPE_PAUSE:
            WINE_TRACE("Pause event\n");
            break;

        case MFP_EVENT_TYPE_STOP:
            WINE_TRACE("Stop event\n");
            break;

        case MFP_EVENT_TYPE_PLAYBACK_ENDED:
            WINE_TRACE("Playback ended\n");
            PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
            break;

        case MFP_EVENT_TYPE_ERROR:
            {
                MFP_ERROR_EVENT *pErrorEvent = (MFP_ERROR_EVENT *)pEventHeader;
                WINE_ERR("Playback error: %lx\n", pErrorEvent->header.hrEvent);
            }
            break;

        default:
            break;
    }
}

static const IMFPMediaPlayerCallbackVtbl MediaPlayerCallback_Vtbl =
{
    MediaPlayerCallback_QueryInterface,
    MediaPlayerCallback_AddRef,
    MediaPlayerCallback_Release,
    MediaPlayerCallback_OnMediaPlayerEvent
};

/* 创建回调对象 */
static HRESULT CreateMediaPlayerCallback(IMFPMediaPlayerCallback **ppCallback)
{
    MediaPlayerCallback *callback;

    callback = HeapAlloc(GetProcessHeap(), 0, sizeof(*callback));
    if (!callback)
        return E_OUTOFMEMORY;

    callback->IMFPMediaPlayerCallback_iface.lpVtbl = &MediaPlayerCallback_Vtbl;
    callback->ref_count = 1;

    *ppCallback = &callback->IMFPMediaPlayerCallback_iface;
    return S_OK;
}

/* 窗口过程 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SIZE:
            if (g_pPlayer)
            {
                /* 更新视频显示 */
                IMFPMediaPlayer_UpdateVideo(g_pPlayer);
            }
            break;

        case WM_KEYDOWN:
            /* 简单的键盘控制 */
            switch (wParam)
            {
                case VK_SPACE:
                    /* 播放/暂停 */
                    if (g_pPlayer)
                    {
                        MFP_MEDIAPLAYER_STATE state;
                        IMFPMediaPlayer_GetState(g_pPlayer, &state);
                        if (state == MFP_MEDIAPLAYER_STATE_PLAYING)
                            IMFPMediaPlayer_Pause(g_pPlayer);
                        else
                            IMFPMediaPlayer_Play(g_pPlayer);
                    }
                    break;

                case VK_ESCAPE:
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    break;
            }
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

/* 初始化窗口 */
static HRESULT InitWindow(HWND *pHwnd)
{
    WNDCLASSEXW wc = {0};
    static const WCHAR class_name[] = {'W','M','P','l','a','y','e','r',0};
    static const WCHAR window_title[] = {'W','i','n','e',' ','M','e','d','i','a',' ','P','l','a','y','e','r',0};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = class_name;

    if (!RegisterClassExW(&wc))
    {
        WINE_ERR("Failed to register window class\n");
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *pHwnd = CreateWindowExW(0, class_name, window_title,
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             800, 600,
                             NULL, NULL, GetModuleHandleW(NULL), NULL);

    if (!*pHwnd)
    {
        WINE_ERR("Failed to create window\n");
        return HRESULT_FROM_WIN32(GetLastError());
    }

    g_hwnd = *pHwnd;
    return S_OK;
}

/* 播放媒体文件 */
static HRESULT PlayMediaFile(const WCHAR *pszURL)
{
    HRESULT hr;
    IMFPMediaPlayerCallback *pCallback = NULL;
    HWND hwnd;

    WINE_TRACE("Playing media file: %s\n", wine_dbgstr_w(pszURL));

    /* 初始化窗口 */
    hr = InitWindow(&hwnd);
    if (FAILED(hr))
    {
        WINE_ERR("InitWindow failed: %lx\n", hr);
        goto cleanup;
    }
    WINE_TRACE("Window created: %p\n", hwnd);

    /* 创建播放器回调 */
    hr = CreateMediaPlayerCallback(&pCallback);
    if (FAILED(hr))
    {
        WINE_ERR("CreateMediaPlayerCallback failed: %lx\n", hr);
        goto cleanup;
    }
    WINE_TRACE("Callback created: %p\n", pCallback);

    /* 创建媒体播放器 - 注意：Wine 的 MFPCreateMediaPlayer 第一个参数是 URL，不是 HWND */
    WINE_TRACE("Calling MFPCreateMediaPlayer with URL: %s, hwnd: %p\n", wine_dbgstr_w(pszURL), hwnd);
    hr = MFPCreateMediaPlayer(pszURL, TRUE, 0, pCallback, hwnd, &g_pPlayer);
    WINE_TRACE("MFPCreateMediaPlayer returned: %lx, player: %p\n", hr, g_pPlayer);
    if (FAILED(hr))
    {
        WINE_ERR("MFPCreateMediaPlayer failed: %lx\n", hr);
        WINE_ERR("Media Foundation may not be properly installed or supported\n");
        goto cleanup;
    }

    WINE_TRACE("Playback started successfully\n");

cleanup:
    if (pCallback)
        IMFPMediaPlayerCallback_Release(pCallback);

    return hr;
}

/* 消息循环 */
static void MessageLoop(void)
{
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

/* 清理资源 */
static void Cleanup(void)
{
    if (g_pPlayer)
    {
        IMFPMediaPlayer_Shutdown(g_pPlayer);
        IMFPMediaPlayer_Release(g_pPlayer);
        g_pPlayer = NULL;
    }
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    HRESULT hr = S_OK;
    const WCHAR *pszURL = NULL;

    /* 检查命令行参数 */
    if (argc < 2)
    {
        WINE_ERR("Usage: %s <media_file_or_url>\n", wine_dbgstr_w(argv[0]));
        WINE_ERR("Example: %s C:\\path\\to\\video.mp4\n", wine_dbgstr_w(argv[0]));
        return 1;
    }

    pszURL = argv[1];
    WINE_ERR("pszURL %s \n", wine_dbgstr_w(pszURL));
    /* 初始化 COM */
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        WINE_ERR("CoInitializeEx failed: %lx\n", hr);
        return 1;
    }

    /* 初始化 Media Foundation */
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (FAILED(hr))
    {
        WINE_ERR("MFStartup failed: %lx\n", hr);
        WINE_ERR("Media Foundation may not be available on this system\n");
        CoUninitialize();
        return 1;
    }

    /* 播放媒体文件 */
    hr = PlayMediaFile(pszURL);
    if (FAILED(hr))
    {
        WINE_ERR("Failed to play media file: %lx\n", hr);
        goto cleanup;
    }

    /* 进入消息循环 */
    MessageLoop();

cleanup:
    /* 清理资源 */
    Cleanup();

    /* 关闭 Media Foundation */
    MFShutdown();

    /* 关闭 COM */
    CoUninitialize();

    return SUCCEEDED(hr) ? 0 : 1;
}
#endif