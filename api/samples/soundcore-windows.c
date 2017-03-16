/* **********************************************************
* Copyright (c) 2011-2014 Google, Inc.  All rights reserved.
* **********************************************************/

/*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
*   this list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* * Neither the name of Google, Inc. nor the names of its contributors may be
*   used to endorse or promote products derived from this software without
*   specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*/

// Tested with 	mpg123-1.24.0-static-x86.zip (win32)

#include "dr_api.h"
#include "drwrap.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef WINDOWS

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma warning(disable:4055)

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
    dr_fprintf(STDERR, "%s\n", msg); \
    dr_abort();                      \
    }                                    \
} while (0)

#ifdef WINDOWS
# define DISPLAY_STRING(msg) dr_messagebox(msg)
#else
# define DISPLAY_STRING(msg) dr_printf("%s\n", msg);
#endif

/* global state */
FILE *fp = NULL;
HANDLE known_event= NULL;
WAVEHDR dummy;

// MMRESULT waveOutOpen(LPHWAVEOUT phwo, UINT_PTR uDeviceID, LPWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwCallbackInstance, DWORD fdwOpen);
void pre_waveOutOpen(void *wrapcxt, OUT void **user_data)
{
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD wBitsPerSample;
    LPWAVEFORMATEX pwfx = (LPWAVEFORMATEX) drwrap_get_arg(wrapcxt, 2);
    DWORD fdwOpen = 0;

    nChannels = pwfx->nChannels;
    nSamplesPerSec = pwfx->nSamplesPerSec;
    nAvgBytesPerSec = pwfx->nAvgBytesPerSec;
    wBitsPerSample = pwfx->wBitsPerSample;
    fdwOpen = (DWORD) drwrap_get_arg(wrapcxt, 5);

    if (fdwOpen & CALLBACK_EVENT) {
        known_event = (HANDLE)drwrap_get_arg(wrapcxt, 3);
    } else if (fdwOpen & CALLBACK_FUNCTION) {
        dr_fprintf(STDERR, ">>> got a CALLBACK_FUNCTION registration which not supported!\n");
        exit(-1);
    }

    // dr_fprintf(STDERR, "> waveOutOpen %p %d %d %d\n", known_event, nChannels, wBitsPerSample / 8, nSamplesPerSec);
}

// MMRESULT waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
void pre_waveOutWrite(void *wrapcxt, OUT void **user_data)
{
    LPWAVEHDR pwh = (LPWAVEHDR) drwrap_get_arg(wrapcxt, 1);

    fwrite(pwh->lpData, pwh->dwBufferLength, 1, fp);
    fflush(fp);

    pwh->dwFlags = WHDR_DONE | pwh->dwFlags;
    memcpy(&dummy, pwh, sizeof(WAVEHDR));
    drwrap_set_arg(wrapcxt, 1, (void*)&dummy);  // disabling me results in normal playback

    drwrap_skip_call(wrapcxt, (void *)0, 12); // 12 for 32-bit
}

void pre_waitForSingleObject(void *wrapcxt, OUT void **user_data)
{
    // HANDLE n = (HANDLE)drwrap_get_arg(wrapcxt, 0);

    // dr_fprintf(STDERR, "> waitForSingleObject known %p got %p\n", known_event, n);
}

static void module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc original;
    bool ok;

    original = (app_pc) dr_get_proc_address(mod->handle, "waveOutWrite");
    if (original != NULL) {
        CHECK(original!= NULL, "cannot find waveOutWrite lib export");
        ok = drwrap_wrap(original, pre_waveOutWrite, NULL);
        CHECK(ok, "> hooking waveOutWrite failed");
    }

    original = (app_pc) dr_get_proc_address(mod->handle, "waveOutOpen");
    if (original != NULL) {
        CHECK(original!= NULL, "cannot find waveOutOpen lib export");
        ok = drwrap_wrap(original, pre_waveOutOpen, NULL);
        CHECK(ok, "> hooking waveOutOpen failed");
    }

    original = (app_pc) dr_get_proc_address(mod->handle, "waitForSingleObject");
    if (original != NULL) {
        CHECK(original!= NULL, "cannot find waitForSingleObject lib export");
        ok = drwrap_wrap(original, pre_waitForSingleObject, NULL);
        CHECK(ok, "> hooking waitForSingleObject failed");
    }
}

DR_EXPORT void
    dr_init(client_id_t id)
{
    dr_set_client_name("DynamoRIO Sample Client 'wrap'", "http://dynamorio.org/issues");
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, LOG_ALL, 1, "Client 'wrap' initializing\n");
    /* also give notification to stderr */
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
# ifdef WINDOWS
        /* ask for best-effort printing to cmd window.  must be called in dr_init(). */
        dr_enable_console_printing();
# endif
        dr_fprintf(STDERR, "Client wrap is running\n");
    }
#endif
    drwrap_init();
    // dr_register_exit_event(event_exit);
    dr_register_module_load_event(module_load_event);

    fp = fopen("output.pcm", "wb");
}

#endif
