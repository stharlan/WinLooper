// looper.cpp : Defines the entry point for the application.
//

#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>
#include <stdio.h>
#include "framework.h"
#include "looper.h"

// TODO free the device id's when program is done

#define MAX_LOADSTRING 100

#define BASE_REC_DEVICE_ITEM_ID 65000L
#define BASE_PBK_DEVICE_ITEM_ID 65100L

#define MENU_ID_REC 1
#define MENU_ID_PLAY 2

#define ACTION_REC 1
#define ACTION_PBK 2

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

HANDLE g_hStopEvent = NULL;
HANDLE g_hCaptureThread = NULL;
BOOL g_isInitialFrame = TRUE;
UINT64 g_devPosInitial = 0;

typedef struct _device_context {
    IMMDevice* g_lpDevice = NULL;
    IAudioClient* g_lpClient = NULL;
    UINT g_NumDevices = 0;
    HANDLE g_hAudioEvent = NULL;
    BYTE* frameBuffer = NULL;
    UINT32 frameBufferOffset = 0;
    UINT32 frameBufferSize = 0;
    BYTE* loopBuffer = NULL;
    UINT32 loopBufferOffset = 0;
    UINT32 loopBufferSize = 0;
    WAVEFORMATEX* g_lpWfex = NULL;
} DEVICE_CONTEXT;

DEVICE_CONTEXT g_RecContext;
DEVICE_CONTEXT g_PbkContext;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AllocConsole();
    FILE* blah = NULL;
    freopen_s(&blah, "CONOUT$", "w", stdout);
    wprintf(L"Ready...\n");

    // TODO: Place code here.
    UNREFERENCED_PARAMETER(CoInitialize(NULL));
    g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LOOPER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LOOPER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if(g_hStopEvent) CloseHandle(g_hStopEvent);

    CoUninitialize();

    FreeConsole();

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LOOPER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_LOOPER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void load_device_menu_items(HWND hWnd, int menuId)
{

    IMMDeviceEnumerator* lpDevEnum = NULL;
    IMMDeviceCollection* lpDevColl = NULL;
    IMMDevice* lpDevice = NULL;
    HMENU windowMenu = GetMenu(hWnd);
    HMENU deviceMenu = GetSubMenu(windowMenu, menuId);
    LPWSTR deviceId = NULL;
    IPropertyStore* pProps = NULL;
    MENUITEMINFO mii;
    PROPVARIANT dfn;
    BOOL bResult = FALSE;
    EDataFlow dataFlow = eCapture;
    UINT32 baseDeviceIndex = BASE_REC_DEVICE_ITEM_ID;
    DEVICE_CONTEXT* ctx = &g_RecContext;

    if (menuId == MENU_ID_PLAY) {
        dataFlow = eRender;
        baseDeviceIndex = BASE_PBK_DEVICE_ITEM_ID;
        ctx = &g_PbkContext;
    }

    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_DATA;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID*)&lpDevEnum)))
    {
        goto done;
    }

    if (FAILED(lpDevEnum->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &lpDevColl)))
    {
        goto done;
    }

    if (FAILED(lpDevColl->GetCount(&ctx->g_NumDevices)))
    {
        goto done;
    }

    for (UINT d = 0; d < ctx->g_NumDevices; d++) {
        if (!FAILED(lpDevColl->Item(d, &lpDevice)))
        {
            if (!FAILED(lpDevice->GetId(&deviceId)))
            {
                if (!FAILED(lpDevice->OpenPropertyStore(STGM_READ, &pProps)))
                {
                    if (!FAILED(pProps->GetValue(PKEY_Device_FriendlyName, &dfn)))
                    {
                        bResult = AppendMenu(deviceMenu, MF_STRING,
                            (UINT_PTR)(baseDeviceIndex + d), dfn.pwszVal);
                        mii.dwItemData = (ULONG_PTR)deviceId;
                        bResult = SetMenuItemInfo(deviceMenu, baseDeviceIndex + d, FALSE, &mii);
                    }
                }
                deviceId = NULL;
            }
            lpDevice->Release();
        }
    }

    DeleteMenu(deviceMenu, 0, MF_BYPOSITION);

    done:

    if (lpDevEnum) lpDevEnum->Release();
    if (lpDevColl) lpDevColl->Release();
    if (lpDevice) lpDevice->Release();
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    load_device_menu_items(hWnd, MENU_ID_REC);
    load_device_menu_items(hWnd, MENU_ID_PLAY);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void initialize_selected_device(HWND hWnd, UINT itemId, int type)
{
    IMMDeviceEnumerator* lpDevEnum = NULL;
    HMENU windowMenu = GetMenu(hWnd);
    HMENU deviceMenu = GetSubMenu(windowMenu, 1);
    MENUITEMINFO mii;
    REFERENCE_TIME defaultTime = 0;
    REFERENCE_TIME minimumTime = 0;
    UINT32 baseDeviceIndex = BASE_REC_DEVICE_ITEM_ID;
    DEVICE_CONTEXT* ctx = &g_RecContext;
    DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    EnableWindow(hWnd, FALSE);
    SetWindowText(hWnd, L"looper (Initializing device...)");

    wprintf(L"========================================\n");
    if (type == ACTION_PBK) {
        baseDeviceIndex = BASE_PBK_DEVICE_ITEM_ID;
        deviceMenu = GetSubMenu(windowMenu, 2);
        ctx = &g_PbkContext;
        streamFlags = 0;
        wprintf(L"Init'ing playback device\n");
    }
    else
    {
        wprintf(L"Init'ing recording device\n");
    }

    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_DATA;

    if (ctx->g_hAudioEvent != NULL) {
        CloseHandle(ctx->g_hAudioEvent);
        ctx->g_hAudioEvent = NULL;
    }
    if (ctx->g_lpClient) ctx->g_lpClient->Release();
    ctx->g_lpClient = NULL;
    if (ctx->g_lpDevice) ctx->g_lpDevice->Release();
    ctx->g_lpDevice = NULL;
    if (ctx->frameBuffer) free(ctx->frameBuffer);
    ctx->frameBuffer = NULL;
    if (ctx->loopBuffer) free(ctx->loopBuffer);
    ctx->loopBuffer = NULL;

    // uncheck all menu items
    for (UINT mi = 0; mi < ctx->g_NumDevices; mi++) {
        CheckMenuItem(deviceMenu, baseDeviceIndex + mi, MF_BYCOMMAND | MF_UNCHECKED);
    }

    if (FALSE == GetMenuItemInfo(deviceMenu, itemId, FALSE, &mii))
    {
        MessageBox(hWnd, L"ERROR: Failed to get device from menu.", L"ERROR", MB_OK);
        goto done;
    }

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_INPROC_SERVER, _uuidof(IMMDeviceEnumerator), (LPVOID*)&lpDevEnum)))
    {
        MessageBox(hWnd, L"ERROR: Failed to create device enum.", L"ERROR", MB_OK);
        goto done;
    }

    if (FAILED(lpDevEnum->GetDevice((LPWSTR)mii.dwItemData, &ctx->g_lpDevice)))
    {
        MessageBox(hWnd, L"ERROR: Failed to get device.", L"ERROR", MB_OK);
        goto done;
    }

    if (FAILED(ctx->g_lpDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, (void**)&ctx->g_lpClient)))
    {
        MessageBox(hWnd, L"ERROR: Failed to activate device.", L"ERROR", MB_OK);
        goto done;
    }

    if (FAILED(ctx->g_lpClient->GetDevicePeriod(&defaultTime, &minimumTime)))
    {
        MessageBox(hWnd, L"ERROR: Failed to get device period.", L"ERROR", MB_OK);
        goto done;
    }

    wprintf(L"========================================\n");
    wprintf(L"default time = %lli\n", defaultTime);
    wprintf(L"minimum time = %lli\n", minimumTime);

    if (FAILED(ctx->g_lpClient->GetMixFormat(&ctx->g_lpWfex)))
    {
        MessageBox(hWnd, L"ERROR: Failed to get device mix format.", L"ERROR", MB_OK);
        goto done;
    }

    wprintf(L"Format Tag        = %i\n", ctx->g_lpWfex->wFormatTag);
    wprintf(L"Channels          = %i\n", ctx->g_lpWfex->nChannels);
    wprintf(L"Samples Per Sec   = %i\n", ctx->g_lpWfex->nSamplesPerSec);
    wprintf(L"Avg Bytes Per Sec = %i\n", ctx->g_lpWfex->nAvgBytesPerSec);
    wprintf(L"Block Align       = %i\n", ctx->g_lpWfex->nBlockAlign);
    wprintf(L"Bits Per Sample   = %i\n", ctx->g_lpWfex->wBitsPerSample);
    wprintf(L"Size              = %i\n", ctx->g_lpWfex->cbSize);

    if (ctx->g_lpWfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        wprintf(L"This is an extensible format\n");
        WAVEFORMATEXTENSIBLE* wfext = (WAVEFORMATEXTENSIBLE*)ctx->g_lpWfex;
        wprintf(L"Valid Bits Per Sample = %i\n", wfext->Samples.wValidBitsPerSample);
        wprintf(L"Samples Per Block     = %i\n", wfext->Samples.wSamplesPerBlock);
        wprintf(L"Channel Mask          = %i\n", wfext->dwChannelMask);

        if (IsEqualGUID(wfext->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
        {
            wprintf(L"Extensible subtype is PCM\n");
        }
        else if (IsEqualGUID(wfext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
        {
            wprintf(L"Extensible subtype is IEEE Float\n");
        }

    }
    else if (ctx->g_lpWfex->wFormatTag == WAVE_FORMAT_PCM) {
        wprintf(L"This is a PCM format\n");
    }

    if (FAILED(ctx->g_lpClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        defaultTime,
        0,
        ctx->g_lpWfex,
        NULL)))
    {
        MessageBox(hWnd, L"ERROR: Failed to initialize device.", L"ERROR", MB_OK);
        goto done;
    }

    if(FAILED(ctx->g_lpClient->GetBufferSize(&ctx->frameBufferSize)))
    {
        MessageBox(hWnd, L"ERROR: Failed to get buffer size frames.", L"ERROR", MB_OK);
        goto done;
    }

    wprintf(L"buffer size frames = %i\n", ctx->frameBufferSize);
    ctx->frameBufferOffset = 0;
    ctx->frameBuffer = (BYTE*)malloc(ctx->frameBufferSize * ctx->g_lpWfex->nBlockAlign);
    ZeroMemory(ctx->frameBuffer, ctx->frameBufferSize * ctx->g_lpWfex->nBlockAlign);

    ctx->loopBuffer = (BYTE*)malloc(4 * ctx->g_lpWfex->nSamplesPerSec * ctx->g_lpWfex->nBlockAlign);
    ZeroMemory(ctx->loopBuffer, 4 * ctx->g_lpWfex->nSamplesPerSec * ctx->g_lpWfex->nBlockAlign);
    ctx->loopBufferSize = 4 * ctx->g_lpWfex->nSamplesPerSec;
    ctx->loopBufferOffset = 0;

    if (type == ACTION_REC) {
        ctx->g_hAudioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (ctx->g_hAudioEvent == NULL) {
            MessageBox(hWnd, L"ERROR: Failed to create audio event.", L"ERROR", MB_OK);
            goto done;
        }

        if (FAILED(ctx->g_lpClient->SetEventHandle(ctx->g_hAudioEvent)))
        {
            MessageBox(hWnd, L"ERROR: Failed to set audio event handle.", L"ERROR", MB_OK);
            CloseHandle(ctx->g_hAudioEvent);
            ctx->g_hAudioEvent = NULL;
            goto done;
        }
    }
    else {
        ctx->g_hAudioEvent = NULL;
    }

    // create a frame buffer

    CheckMenuItem(deviceMenu, itemId, MF_BYCOMMAND | MF_CHECKED);

done:
    if (lpDevEnum) lpDevEnum->Release();

    EnableWindow(hWnd, TRUE);
    SetWindowText(hWnd, L"looper");
}

void process_buffers(IAudioCaptureClient* lpCaptureClient, IAudioRenderClient* lpRenderClient)
{
    BYTE* pCapData = NULL;
    BYTE* pRenData = NULL;
    UINT32 NumFramesToRead = 0;
    DWORD bufferFlags = 0;
    UINT64 devPos = 0;
    UINT64 qpcPos = 0;
    UINT32 renderPadding = 0;
    UINT32 frameBufferAvailable = 0;
    UINT32 loopBufferAvailable = 0;
    size_t sizeToWrite = 0;
    HRESULT hr = 0;

    //lpCaptureClient->GetBuffer(&pCapData, &NumFramesToRead, &bufferFlags, &devPos, &qpcPos);

    //sizeToWrite = (size_t)NumFramesToRead * (size_t)g_RecContext.g_lpWfex->nBlockAlign;
    //size_t pbkOffset = 4 * g_PbkContext.g_lpWfex->nSamplesPerSec * g_PbkContext.g_lpWfex->nBlockAlign;
    //if ((g_PbkContext.bufferOffset + pbkOffset + sizeToWrite) > g_PbkContext.bufferSize)
    //{
    //    size_t leftover = g_PbkContext.bufferSize - (g_PbkContext.bufferOffset + pbkOffset);
    //    memcpy(g_PbkContext.g_p8bitData + (g_PbkContext.bufferOffset + pbkOffset), pCapData, leftover);
    //    size_t newSizeToWrite = sizeToWrite - leftover;
    //    memcpy(g_PbkContext.g_p8bitData + pbkOffset, pCapData + leftover, newSizeToWrite);
    //}
    //else {
    //    memcpy(g_PbkContext.g_p8bitData + (g_PbkContext.bufferOffset + pbkOffset), pCapData, sizeToWrite);
    //}

    //lpCaptureClient->ReleaseBuffer(NumFramesToRead);

    //if (TRUE == g_isInitialFrame) {
    //    g_isInitialFrame = FALSE;
    //    g_devPosInitial = devPos;
    //}
    ////wprintf(L"%2i min %2i sec %6lli frame\r",
    //    //(int)((double)(devPos - g_devPosInitial) / ((double)g_RecContext.g_lpWfex->nSamplesPerSec * 60.0)),
    //    //(int)((double)(devPos - g_devPosInitial) / (double)g_RecContext.g_lpWfex->nSamplesPerSec) % 60,
    //    //(devPos - g_devPosInitial) % g_RecContext.g_lpWfex->nSamplesPerSec
    ////);

    // capture
    if (!FAILED(lpCaptureClient->GetBuffer(&pCapData, &NumFramesToRead, &bufferFlags, &devPos, &qpcPos)))
    {
        if (NumFramesToRead > 0) {
            loopBufferAvailable = g_RecContext.loopBufferSize - g_RecContext.loopBufferOffset;
            if (loopBufferAvailable > NumFramesToRead)
            {

                void* src = g_RecContext.loopBuffer + (g_RecContext.loopBufferOffset * g_RecContext.g_lpWfex->nBlockAlign);
                void* dest = pCapData;
                size_t sz = NumFramesToRead * g_RecContext.g_lpWfex->nBlockAlign;

                memcpy(src, dest, sz);
                g_RecContext.loopBufferOffset += NumFramesToRead;

            }
            else {
                size_t framesAtEnd = g_RecContext.loopBufferSize - g_RecContext.loopBufferOffset;

                memcpy(g_RecContext.loopBuffer + (g_RecContext.loopBufferOffset * g_RecContext.g_lpWfex->nBlockAlign),
                    pCapData,
                    framesAtEnd * g_RecContext.g_lpWfex->nBlockAlign);

                wprintf(L"update pbk buffer\n");
                memcpy(g_PbkContext.loopBuffer,
                    g_RecContext.loopBuffer,
                    g_RecContext.loopBufferSize * g_RecContext.g_lpWfex->nBlockAlign);

                size_t framesAtBegin = NumFramesToRead - framesAtEnd;
                if (framesAtBegin > 0) {
                    memcpy(g_RecContext.loopBuffer,
                        pCapData + (framesAtEnd * g_RecContext.g_lpWfex->nBlockAlign),
                        framesAtBegin * g_RecContext.g_lpWfex->nBlockAlign);
                }
                g_RecContext.loopBufferOffset = framesAtBegin;
            }
            lpCaptureClient->ReleaseBuffer(NumFramesToRead);
            wprintf(L"captured %i frames; lbofst %i - ", NumFramesToRead, g_RecContext.loopBufferOffset);
        }
        else {
            wprintf(L"INFO: no frames available - ");
        }
    }
    else {
        wprintf(L"ERROR: Failed to get capture buffer - ");
    }

    // render
    if (!FAILED(g_PbkContext.g_lpClient->GetCurrentPadding(&renderPadding)))
    {
        frameBufferAvailable = g_PbkContext.frameBufferSize - renderPadding;
        if (frameBufferAvailable > 0) {
            if (!FAILED(lpRenderClient->GetBuffer(frameBufferAvailable, &pRenData)))
            {
                loopBufferAvailable = g_PbkContext.loopBufferSize - g_PbkContext.loopBufferOffset;
                if (loopBufferAvailable > frameBufferAvailable)
                {
                    memcpy(pRenData,
                        g_PbkContext.loopBuffer + (g_PbkContext.loopBufferOffset * g_PbkContext.g_lpWfex->nBlockAlign),
                        frameBufferAvailable * g_PbkContext.g_lpWfex->nBlockAlign);
                    g_PbkContext.loopBufferOffset += frameBufferAvailable;
                }
                else {
                    size_t framesAtEnd = g_PbkContext.loopBufferSize - g_PbkContext.loopBufferOffset;
                    memcpy(pRenData,
                        g_PbkContext.loopBuffer + (g_PbkContext.loopBufferOffset * g_PbkContext.g_lpWfex->nBlockAlign),
                        framesAtEnd * g_PbkContext.g_lpWfex->nBlockAlign);
                    size_t framesAtBegin = frameBufferAvailable - framesAtEnd;
                    if (framesAtBegin > 0) {
                        memcpy(pRenData + (framesAtEnd * g_PbkContext.g_lpWfex->nBlockAlign),
                            g_PbkContext.loopBuffer,
                            framesAtBegin * g_PbkContext.g_lpWfex->nBlockAlign);
                    }
                    g_PbkContext.loopBufferOffset = framesAtBegin;
                }
                lpRenderClient->ReleaseBuffer(frameBufferAvailable, 0);
                wprintf(L"rendered %i frames; lbofst %i\n", frameBufferAvailable, g_PbkContext.loopBufferOffset);
            }
            else {
                wprintf(L"ERROR: Failed to get render buffer for %i frames\n", frameBufferAvailable);
            }
        }
        else {
            wprintf(L"INFO: No frames available\n");
        }
    }
    else {
        wprintf(L"ERROR: Failed to get current padding\n");
    }
}

DWORD WINAPI CaptureThread(LPVOID lpParm)
{

    IAudioCaptureClient* lpCaptureClient = NULL;
    IAudioRenderClient* lpRenderClient = NULL;
    BOOL bDone = FALSE;
    DWORD waitResult = 0;
    HANDLE hEvents[2] = {
        g_RecContext.g_hAudioEvent, g_hStopEvent
    };
    HRESULT hr = 0;
    BYTE* pData = NULL;

    g_isInitialFrame = TRUE;

    if (FAILED(g_RecContext.g_lpClient->GetService(__uuidof(IAudioCaptureClient), (void**)&lpCaptureClient)))
    {
        wprintf(L"ERROR: Failed to get capture service\n");
        return 0;
    }

    if (FAILED(g_PbkContext.g_lpClient->GetService(__uuidof(IAudioRenderClient), (void**)&lpRenderClient)))
    {
        wprintf(L"ERROR: Failed to get render service\n");
        return 0;
    }

    if (!FAILED(lpRenderClient->GetBuffer(g_PbkContext.frameBufferSize, &pData)))
    {
        if (FAILED(lpRenderClient->ReleaseBuffer(g_PbkContext.frameBufferSize, AUDCLNT_BUFFERFLAGS_SILENT)))
        {
            wprintf(L"ERROR: Failed to write initial silence to render buffer\n");
            return 0;
        }
    }
    else {
        wprintf(L"ERROR: Failed to write initial silence to render buffer\n");
        return 0;
    }

    hr = g_RecContext.g_lpClient->Start();
    if(FAILED(hr))
    {
        wprintf(L"ERROR: Failed to start capture client 0x%x\n", hr);
        return 0;
    }

    hr = g_PbkContext.g_lpClient->Start();
    if (FAILED(hr))
    {
        g_RecContext.g_lpClient->Stop();
        wprintf(L"ERROR: Failed to start render client 0x%x\n", hr);
        return 0;
    }

    while (!bDone)
    {
        waitResult = WaitForMultipleObjects(2, hEvents, FALSE, 1000);
        switch (waitResult) {
        case WAIT_OBJECT_0:
            process_buffers(lpCaptureClient, lpRenderClient);
            break;
        case (WAIT_OBJECT_0 + 1):
            bDone = TRUE;
            ResetEvent(g_hStopEvent);
            break;
        case WAIT_TIMEOUT:
            wprintf(L"wait timeout\n");
            break;
        case WAIT_FAILED:
            wprintf(L"Wait for frames failed. Quitting...\n");
            bDone = TRUE;
            break;
        case WAIT_ABANDONED:
            wprintf(L"wait abandoned\n");
            break;
        }
    }

    if (FAILED(g_RecContext.g_lpClient->Stop()))
    {
        wprintf(L"ERROR: Failed to stop capture client\n");
    }

    if (FAILED(g_PbkContext.g_lpClient->Stop()))
    {
        wprintf(L"ERROR: Failed to stop render client\n");
    }

    if (lpCaptureClient) lpCaptureClient->Release();
    if (lpRenderClient) lpRenderClient->Release();

    wprintf(L"\nCapture thread is done.\n");

    return 0;
}

BOOL start_looper(HWND hWnd)
{

    if (g_RecContext.g_lpClient == NULL) {
        MessageBox(hWnd, L"ERROR: No recording client chosen.", L"ERROR", MB_OK);
        return FALSE;
    }

    if (g_PbkContext.g_lpClient == NULL) {
        MessageBox(hWnd, L"ERROR: No playback client chosen.", L"ERROR", MB_OK);
        return FALSE;
    }

    if (FAILED(g_RecContext.g_lpClient->Reset())) {
        MessageBox(hWnd, L"ERROR: Failed to reset capture client.", L"ERROR", MB_OK);
        return FALSE;
    }
    if (FAILED(g_PbkContext.g_lpClient->Reset())) {
        MessageBox(hWnd, L"ERROR: Failed to reset render client.", L"ERROR", MB_OK);
        return FALSE;
    }

    if (g_RecContext.g_lpWfex->nSamplesPerSec != g_PbkContext.g_lpWfex->nSamplesPerSec)
    {
        MessageBox(hWnd, L"ERROR: Both devices must have the same sampling rate.", L"ERROR", MB_OK);
        return FALSE;
    }

    if (g_RecContext.frameBufferSize != g_PbkContext.frameBufferSize)
    {
        MessageBox(hWnd, L"ERROR: Both devices must have the same frame buffer size.", L"ERROR", MB_OK);
        return FALSE;
    }

    g_hCaptureThread = CreateThread(NULL, 0, CaptureThread, NULL, 0, NULL);
    if (NULL == g_hCaptureThread) {
        MessageBox(hWnd, L"ERROR: Failed to start capture thread.", L"ERROR", MB_OK);
        return FALSE;
    }

    wprintf(L"looper is started\n");

    return TRUE;
}

BOOL stop_looper()
{

    SetEvent(g_hStopEvent);

    if (g_hCaptureThread) {
        WaitForSingleObject(g_hCaptureThread, INFINITE);
        CloseHandle(g_hCaptureThread);
        g_hCaptureThread = NULL;
    }

    wprintf(L"looper is stopped\n");

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            UINT wmId = LOWORD(wParam);
            // Parse the menu selections:
            if (wmId > BASE_REC_DEVICE_ITEM_ID - 1 && wmId < BASE_REC_DEVICE_ITEM_ID + g_RecContext.g_NumDevices)
            {
                // user is selecting a recording device
                initialize_selected_device(hWnd, wmId, ACTION_REC);
            }
            else if (wmId > BASE_PBK_DEVICE_ITEM_ID - 1 && wmId < BASE_PBK_DEVICE_ITEM_ID + g_PbkContext.g_NumDevices)
            {
                // user is selecting a playback device
                initialize_selected_device(hWnd, wmId, ACTION_PBK);
            }
            else {
                switch (wmId)
                {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                case ID_FILE_START:
                    if(TRUE == start_looper(hWnd))
                    {
                        HMENU windowMenu = GetMenu(hWnd);
                        HMENU deviceMenu = GetSubMenu(windowMenu, 0);
                        EnableMenuItem(deviceMenu, ID_FILE_START, MF_DISABLED|MF_GRAYED);
                        EnableMenuItem(deviceMenu, ID_FILE_STOP, MF_ENABLED);
                        
                    }
                    break;
                case ID_FILE_STOP:
                    if(TRUE == stop_looper()) {
                        HMENU windowMenu = GetMenu(hWnd);
                        HMENU deviceMenu = GetSubMenu(windowMenu, 0);
                        EnableMenuItem(deviceMenu, ID_FILE_STOP, MF_DISABLED | MF_GRAYED);
                        EnableMenuItem(deviceMenu, ID_FILE_START, MF_ENABLED);                        
                    }
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
                }
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

