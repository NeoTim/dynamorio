/* **********************************************************
 * Copyright (c) 2017 Google, Inc.  All rights reserved.
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

// Copyright (c) Dhiru Kholia (kholia [at] kth [dot] se)

#include "dr_api.h"
#include "drwrap.h"
#include "drmgr.h"
#include <stdio.h>
#include <stdint.h>

typedef unsigned long snd_pcm_uframes_t;

typedef struct pa_sample_spec {
    int format;
    uint32_t rate;
    uint8_t channels;
} pa_sample_spec_;

#define CHECK(x, msg) do {               \
    if (!(x)) {                          \
        dr_fprintf(STDERR, "%s\n", msg); \
        dr_abort();                      \
    }                                    \
} while (0)

/* global state */
FILE *fp = NULL;
int channels = 0;
int format = 0;
unsigned int rate = 0;


void pre_pa_simple_new(void *wrapcxt, OUT void **user_data)
{
    pa_sample_spec_ *ss;

    ss = (pa_sample_spec_ *) drwrap_get_arg(wrapcxt, 5);
    format = (int) ss->format;
    channels = (int) ss->channels;
    rate = (unsigned int) ss->rate;
    printf("> PCM format is %d\n", format); // PA_SAMPLE_S16LE is 3
    printf("> Channels count is %d\n", channels);
    printf("> Rate is %u\n", rate);
}

void pre_pa_simple_write(void *wrapcxt, OUT void **user_data)
{
    size_t len = (size_t) drwrap_get_arg(wrapcxt, 2);
    unsigned char *buf = (void *) drwrap_get_arg(wrapcxt, 1);
    unsigned long ret = 0;

    fwrite(buf, len, 1, fp);
    fflush(fp);

    drwrap_skip_call(wrapcxt, (void*)ret, 0); // don't call the original "pa_simple_write" function
}

int pa_simple_drain_replacement(void *s, int *error)
{
    return 0;
}

// int snd_pcm_wait(snd_pcm_waitsnd_pcm_t * pcm, int timeout)
int snd_pcm_wait_replacement(void *pcm, int timeout)
{
    return 1; // PCM stream is "always" ready for I/O
}

// int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
int snd_pcm_recover_replacement(void *pcm, int err, int silent)
{
    return 0; // "error" handled succesfully
}

void pre_snd_pcm_hw_params_set_format(void *wrapcxt, OUT void **user_data)
{
    // int snd_pcm_hw_params_set_format (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val)
    // SND_PCM_FORMAT_S16_LE == 2
    size_t val = (size_t) drwrap_get_arg(wrapcxt, 2);
    format = (int) val;
    printf("> PCM format is %d\n", format);
}

static void pre_snd_pcm_hw_params_set_channels(void *wrapcxt, OUT void **user_data)
{
    // int snd_pcm_hw_params_set_channels (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
    unsigned int val = (size_t) drwrap_get_arg(wrapcxt, 2);
    channels = (int) val;
    printf("> Channels count is %d\n", channels);
}

// For a stereo, 16-bit, 44.1 KHz stream,
//
// 'stereo' => number of channels: 2
// 1 analog sample is represented with 16 bits => 2 bytes
// 1 frame represents 1 analog sample from all channels; here we have 2 channels, and so:
// 1 frame size => (num_channels) * (1 sample in bytes) => (2 channels) * (2 bytes (16 bits) per sample) = 4 bytes (32 bits)
void pre_snd_pcm_writei(void *wrapcxt, OUT void **user_data)
{
    // snd_pcm_sframes_t snd_pcm_writei (snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
    snd_pcm_uframes_t size = (snd_pcm_uframes_t) drwrap_get_arg(wrapcxt, 2); // number of frames
    unsigned char *buffer = (void *) drwrap_get_arg(wrapcxt, 1);

    int bytes_per_frame = 0;
    if (format == 2)
        bytes_per_frame = 2;
    else if (format == 10)
        bytes_per_frame = 4; // SND_PCM_FORMAT_S32_LE
    else {
        puts("!");
    }

    fwrite(buffer, bytes_per_frame * size * channels, 1, fp);
    fflush(fp);

    drwrap_skip_call(wrapcxt, (void*)size, 0); // don't call the original "snd_pcm_writei" function
}

void pre_snd_pcm_hw_params_set_rate_near(void *wrapcxt, OUT void **user_data)
{
    // snd_pcm_hw_params_set_rate_near (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
    unsigned int* val = (unsigned int*) drwrap_get_arg(wrapcxt, 2);
    rate = *val;
    printf("> Rate is %u\n", rate);
}

void pre_snd_pcm_hw_params_set_rate(void *wrapcxt, OUT void **user_data)
{
    // snd_pcm_hw_params_set_rate (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
    unsigned int *val = (unsigned int*) drwrap_get_arg(wrapcxt, 2);
    rate = *val;
    printf("> Rate is %u\n", rate);
}

static void module_load_event(void *drcontext, const module_data_t * mod, bool loaded)
{
    bool ok;

    //// ALSA handling ////

    // what is the sample format?
    app_pc snd_pcm_hw_params_set_format_original = (app_pc) dr_get_proc_address(mod->handle, "snd_pcm_hw_params_set_format");
    if (snd_pcm_hw_params_set_format_original != NULL) {
        CHECK(snd_pcm_hw_params_set_format_original!= NULL, "cannot find lib export");
        // ok = drwrap_replace(snd_pcm_hw_params_set_format_original, (app_pc) snd_pcm_hw_params_set_format_replacement, false);
        ok = drwrap_wrap(snd_pcm_hw_params_set_format_original, pre_snd_pcm_hw_params_set_format, NULL);
        CHECK(ok, "> wrapping snd_pcm_hw_params_set_format failed");
    }

    // how many channels are we using?
    app_pc snd_pcm_hw_params_set_channels = (app_pc) dr_get_proc_address(mod->handle, "snd_pcm_hw_params_set_channels");
    if (snd_pcm_hw_params_set_channels != NULL) {
        CHECK(snd_pcm_hw_params_set_channels!= NULL, "cannot find lib export");
        ok = drwrap_wrap(snd_pcm_hw_params_set_channels, pre_snd_pcm_hw_params_set_channels, NULL);
        CHECK(ok, "> wrapping snd_pcm_hw_params_set_channels failed");
    }

    // hook write(s) to the audio device
    app_pc snd_pcm_writei_original = (app_pc) dr_get_proc_address(mod->handle, "snd_pcm_writei");
    if (snd_pcm_writei_original != NULL) {
        CHECK(snd_pcm_writei_original!= NULL, "cannot find lib export");
        ok = drwrap_wrap(snd_pcm_writei_original, pre_snd_pcm_writei, NULL);
        CHECK(ok, "> wrapping snd_pcm_writei failed");
    }

    // this "__" thing is a bit weird
    app_pc snd_pcm_hw_params_set_rate_near_original = (app_pc) dr_get_proc_address(mod->handle, "__snd_pcm_hw_params_set_rate_near");
    if (snd_pcm_hw_params_set_rate_near_original != NULL) {
        CHECK(snd_pcm_hw_params_set_rate_near_original!= NULL, "cannot find lib export");
        ok = drwrap_wrap(snd_pcm_hw_params_set_rate_near_original, pre_snd_pcm_hw_params_set_rate_near, NULL);
        CHECK(ok, "[-] wrapping __snd_pcm_hw_params_set_rate_near failed");
    }

    // let's nop snd_pcm_wait which waits for a PCM to become ready
    app_pc original = (app_pc) dr_get_proc_address(mod->handle, "snd_pcm_wait_");
    if (original != NULL) {
        CHECK(original != NULL, "cannot find lib export");
        ok = drwrap_replace(original, (app_pc) snd_pcm_wait_replacement, false);
        CHECK(ok, "[-] replacing snd_pcm_wait failed");
    }

    // let's nop snd_pcm_recover which causes noisy audible output by touching real audio device!
    original = (app_pc) dr_get_proc_address(mod->handle, "snd_pcm_recover");
    if (original != NULL) {
        CHECK(original != NULL, "cannot find lib export");
        ok = drwrap_replace(original, (app_pc) snd_pcm_recover_replacement, false);
        CHECK(ok, "[-] replacing snd_pcm_recover failed");
    }

    //// PulseAudio handling ////

    original = (app_pc) dr_get_proc_address(mod->handle, "pa_simple_new");
    if (original != NULL) {
        ok = drwrap_wrap(original, pre_pa_simple_new, NULL);
        CHECK(ok, "[-] wrapping pa_simple_new failed");
    }

    original = (app_pc) dr_get_proc_address(mod->handle, "pa_simple_write");
    if (original != NULL) {
        ok = drwrap_wrap(original, pre_pa_simple_write, NULL);
        CHECK(ok, "[-] wrapping pa_simple_write failed");
    }

    // let's nop pa_simple_drain function
    original = (app_pc) dr_get_proc_address(mod->handle, "pa_simple_drain");
    if (original != NULL) {
        CHECK(original != NULL, "cannot find lib export");
        ok = drwrap_replace(original, (app_pc)pa_simple_drain_replacement, false);
        CHECK(ok, "[-] replacing pa_simple_drain failed");
    }
}

DR_EXPORT void dr_init(client_id_t id)
{
    drwrap_init();
    drmgr_init();

    fp = fopen("output.pcm", "wb"); // ffplay -f s16le -ar 48k -ac 2 output.pcm

    drmgr_register_module_load_event(module_load_event);
    // dr_register_module_load_event(module_load_event);
}
