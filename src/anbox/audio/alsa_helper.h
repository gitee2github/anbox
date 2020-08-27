/*
 * Copyright (C) 2017 Simon Fels <morphis@gravedo.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef ANBOX_AUDIO_ALSA_HELPER_H
#define ANBOX_AUDIO_ALSA_HELPER_H

#include<alsa/asoundlib.h>
#include<time.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/time.h>
#include<malloc.h>
#include<vector>
#include<array>
#include <endian.h>
#include <assert.h>
#include"anbox/logger.h"
#include<string>

#ifdef SND_CHMAP_API_VERSION
const int CONFIG_SUPPORT_CHMAP = 1;
#endif

namespace anbox{
namespace audio{
enum {
    CHANELTYPE_NONE,
    CHANELTYPE_MONO,
    CHANELTYPE_STEREO
};

/**
 * DEFAULT_SAMPLE_RATE is used for recording sample rate.
 * Our audio chip only support 48kHz or 44.1kHz, in order to get high quality recording,
 * so we choose 48kHz. If you want to modify sample rate, please  also modify
 * hardware/libhardware_legacy/audio/audio_policy.conf and DEFAULT_PERIOD_FRAMES.
 * e.g  inputs partion config as follow:
 *   inputs {
 *     primary {
 *       sampling_rates 48000
 *       channel_masks AUDIO_CHANNEL_IN_MONO
 *       formats AUDIO_FORMAT_PCM_16_BIT
 *       devices AUDIO_DEVICE_IN_BUILTIN_MIC
 *      }
 *    }
 */
const int DEFAULT_SAMPLE_RATE = 48000; // record sample rate using 48kHz
const int DEFAULT_PERIOD_FRAMES = 480; // sample rate 48kHz and read 10ms pcm data
const int DEFAULT_SAMPLE_RATE_LOWEST = 2000;
const int DEFAULT_SAMPLE_RATE_HIGHEST = 768000;
const char* const DEFAULT_RECORD_DEVICE = "default";
const std::string DEFAULT_CONFIG_PATH = "/usr/share/kpandroid/audio/audio_cfg";
const int FATAL_ERRORS = 1;
const int AUDIO_ERROR = -1;
// seconds to milliseconds
const int SEC_TO_MSEC = 1000;
// milliseconds to microseconds
const int MSEC_TO_USEC = 1000;
// seconds to nanoseco
const int SEC_TO_NSEC = 1000000;
const int BYTE2BITS = 8;
const int PER_SAMPLE_8BITS = 8;
const int PER_SAMPLE_16BITS = 16;

struct hwparams {
    snd_pcm_format_t format;
    unsigned int channels;
    unsigned int rate;
};

class AlsaHelper {
public:
    AlsaHelper();
    ~AlsaHelper();
    AlsaHelper(const AlsaHelper &) = delete;
    AlsaHelper& operator=(const AlsaHelper&) = delete;
    int open_pcm_device(const char *name, snd_pcm_stream_t stream, int mode);
    int close_pcm_device();
    int set_pcm_params(hwparams params);
    size_t pcm_read(char *data, size_t count);
    snd_pcm_uframes_t get_period_frames() const;
    snd_pcm_uframes_t get_period_frames_bytes() const;
    std::string get_usb_audio_device_name() const;
    bool get_device_config (hwparams& device_config) const;
private:
    const char* pcm_name;
    snd_pcm_stream_t  stream_type;
    snd_pcm_t *mhandle;
    int monotonic {0};
    snd_pcm_uframes_t period_frames;
    int start_delay {0};
    int stop_delay {0};
    size_t significant_bits_per_sample;
    size_t bits_per_sample;
    size_t bits_per_frame;
    hwparams  mhwparams;
    int mmap_flag {0};
    size_t chunk_bytes;  // chunk_bytes formula is period_frames * bits_per_frame / 8

    int set_params();
    int xrun(void);
    int suspend(void);
    void compute_max_peak(char *data, size_t count) const;
};
}
}
#endif
