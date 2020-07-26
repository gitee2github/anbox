/*
 *  Copyright (C) 2017 Simon Fels <morphis@gravedo.de>
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
#include"alsa_helper.h"
#include<vector>
#include<cstdio>

namespace anbox {
namespace audio {
AlsaHelper::AlsaHelper()
{
    pcm_name = audio::DEFAULT_RECORD_DEVICE;
    stream_type = SND_PCM_STREAM_CAPTURE;
    mhandle = nullptr;
    period_frames = 0;
    significant_bits_per_sample = 0;
    bits_per_sample = 0;
    bits_per_frame = 0;
    mhwparams = {
        .format = SND_PCM_FORMAT_S16_LE,
        .channels = CHANELTYPE_STEREO,
        .rate = DEFAULT_SAMPLE_RATE,
        };
    chunk_bytes = 0; // chunk_bytes formula is  chunk_size * bits_per_frame / 8
}

AlsaHelper::~AlsaHelper()
{
    close_pcm_device();
    pcm_name = nullptr;
}

int AlsaHelper::open_pcm_device(const char *name, snd_pcm_stream_t stream, int open_mode)
{
    int res = -1;
    res = snd_pcm_open(&mhandle, name, stream, open_mode);
    if (res < 0) {
        ERROR("alsa helper open pcm device fail and device name = %s and error code = %d", name, res);
    } else {
        DEBUG("open pcm device success");
    }
    return res;
}

int AlsaHelper::close_pcm_device()
{
    DEBUG("alsa helper close pcm device is called");
    int res = 0;
    if (mhandle != nullptr) {
        res = snd_pcm_close(mhandle);
        mhandle = nullptr;
        if (res < 0) {
            ERROR("close pcm device fail !!!");
        } else {
            DEBUG("close pcm device success");
        }
    }
    return res;
}

int AlsaHelper::set_pcm_params(hwparams params)
{
    DEBUG("alsa helper set pcm params is called");
    int error;
    mhwparams = params;
    error = set_params();
    significant_bits_per_sample = snd_pcm_format_width(mhwparams.format);
    bits_per_sample = snd_pcm_format_physical_width(mhwparams.format);
    bits_per_frame = bits_per_sample * mhwparams.channels;
    if (bits_per_sample != 0) {
        chunk_bytes = period_frames * bits_per_frame / bits_per_sample;
    }
    DEBUG("alsa helper set_pcm_params successful, chunk_bytes = %lu, period_size = %lu", chunk_bytes, period_frames);
    return error;
}

int AlsaHelper::set_params()
{
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t frames;
    int err = -1; // negative indicate error, otherwise success
    unsigned int rate;
    snd_pcm_hw_params_alloca(&params);
    if ((err = snd_pcm_hw_params_any(mhandle, params)) < 0) {
        ERROR("Broken configuration for this PCM: no configurations available");
        return err;
    }
    if ((err = snd_pcm_hw_params_set_access(mhandle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        ERROR("Access type not available");
        return err;
    }
    if ((err = snd_pcm_hw_params_set_format(mhandle, params, mhwparams.format)) < 0) {
        ERROR("Sample format non available");
        return err;
    }
    if ((err = snd_pcm_hw_params_set_channels(mhandle, params, mhwparams.channels)) < 0) {
        ERROR("Channels count non available");
        return err;
    }
    rate = mhwparams.rate;
    if (rate < DEFAULT_SAMPLE_RATE_LOWEST || rate > DEFAULT_SAMPLE_RATE_HIGHEST) {
        return -1;
    }
    err = snd_pcm_hw_params_set_rate_near(mhandle, params, &mhwparams.rate, 0);
    rate = mhwparams.rate;
    /* set period size  to 480 frames. because sample 48KHz and read 10ms data */
    frames = DEFAULT_PERIOD_FRAMES;
    if ((err = snd_pcm_hw_params_set_period_size_near(mhandle, params, &frames, 0)) < 0) {
        ERROR("period size non available");
        return err;
    }
    if ((err = snd_pcm_hw_params(mhandle, params)) < 0) {
        ERROR("Unable to install hw params:");
        return err;
    }
    snd_pcm_hw_params_get_period_size(params, &period_frames, 0);
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (period_frames == buffer_size) {
        ERROR("Can't use period equal to buffer size (%lu == %lu)", period_frames, buffer_size);
        err = -1;
    }
    return err;
}

size_t AlsaHelper::pcm_read(char *readBuf, size_t length)
{
    ssize_t r;
    size_t count = length;
    int error = 0;
    int wait_times = 100;
    while (count > 0) {
        if (mmap_flag) {
            r = snd_pcm_mmap_readi(mhandle, readBuf, count);
        } else {
            r = snd_pcm_readi(mhandle, readBuf, count);
        }
        // handle snd pcm read  result
        if (r == -EAGAIN || (r >= 0 && static_cast<size_t>(r) < count)) {
            DEBUG("pcm read data no complete, has read size= %d", r);
            snd_pcm_wait(mhandle, wait_times);
        } else if (r == -EPIPE) {
            if ((error = xrun()) < 0) {
                return 0;
            }
        } else if (r == -ESTRPIPE) {
            if ((error = suspend()) < 0) {
                return 0;
            }
        } else if (r < 0) {
            ERROR("pcm read has fail,error code = %zu", r);
            return 0;
        }
        if (r > 0) {
            if (mhwparams.channels) {
                compute_max_peak(readBuf, r * mhwparams.channels);
            }
            count -= r;
            readBuf += r * bits_per_frame / BYTE2BITS;
        }
        ERROR("after has read data r = %d count = %d", r, count);
    }
    return length;
}

bool  AlsaHelper::get_device_config(hwparams& device_config) const
{
    //  it wil read from config file
    device_config.format = SND_PCM_FORMAT_S16_LE;
    device_config.channels = CHANELTYPE_MONO;
    device_config.rate = DEFAULT_SAMPLE_RATE;
    return true;
}

/* peak handler */
void AlsaHelper::compute_max_peak(char *data, size_t count) const
{
    const size_t size = 2;
    signed int max, perc[size], max_peak[size];
    static int run = 0;
    int c = 0;
    int vumeter = CHANELTYPE_MONO;
    memset(max_peak, 0, sizeof(max_peak));
    if (bits_per_sample == PER_SAMPLE_16BITS) {
        signed short *valp = reinterpret_cast<signed short *>(data);
        signed short mask = snd_pcm_format_silence_16(mhwparams.format);
        signed short sval = 0;
        count = count >> 1;
        while (count-- > 0) {
            sval = le16toh(*valp);
            sval = abs(sval) ^ mask;
            if (max_peak[c] < sval) {
                max_peak[c] = sval;
            }
            valp++;
            if (vumeter == CHANELTYPE_STEREO) {
                c = !c;
            }
        }
    } else {
        if (run == 0) {
            run = 1;
        }
        return;
    }
    max = 1 << (significant_bits_per_sample - 1);
    if (max <= 0) {
        max = 0x7fffffff;
    }
    int unit = 100;
    int ichans = 1;
    for (c = 0; c < ichans; c++) {
        if (bits_per_sample > PER_SAMPLE_16BITS) {
            perc[c] = max_peak[c] / (max / unit);
        } else {
            perc[c] = max_peak[c] * unit / max;
        }
    }
}

int AlsaHelper::suspend(void)
{
    ERROR("pcm read data has suspend");
    int res = 0;
    while ((res = snd_pcm_resume(mhandle)) == -EAGAIN) {
        sleep(1);
    }
    if (res < 0) {
        if ((res = snd_pcm_prepare(mhandle)) < 0) {
            return res;
        }
    }
    return res;
}

int AlsaHelper::xrun()
{
    snd_pcm_status_t *status = nullptr;
    int res = -1;
    snd_pcm_status_alloca(&status);
    if ((res = snd_pcm_status(mhandle, status)) < 0) {
        return res;
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        if (FATAL_ERRORS == 1) {
            ERROR("it has overrun,directly exit!!! \n");
            return res;
        }
        if (monotonic) {
#ifdef HAVE_CLOCK_GETTIME
            struct timespec now, diff, tstamp;
            clock_gettime(CLOCK_MONOTONIC, &now);
            snd_pcm_status_get_trigger_htstamp(status, &tstamp);
            timermsub(&now, &tstamp, &diff);
            long time =  diff.tv_sec * sec_to_msec + diff.tv_nsec / sec_to_nsec;
            ERROR("overrun !!! (at least %.3f ms long", time);
#else
            ERROR("overrun !!!\n");
#endif
        } else {
            struct timeval now, diff, tstamp;
            gettimeofday(&now, 0);
            snd_pcm_status_get_trigger_tstamp(status, &tstamp);
            timersub(&now, &tstamp, &diff);
            // time unit is millisecond
            long time =  diff.tv_sec  * SEC_TO_MSEC + diff.tv_usec / MSEC_TO_USEC;
            ERROR("overrun !!! (at least %.3f ms long", time);
        }
        if ((res = snd_pcm_prepare(mhandle)) < 0) {
            ERROR("overrun, it couldn't translate to prepare status");
            return res;
        }
        return res; /* ok, data should be accepted again */
    }
    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        if (stream_type == SND_PCM_STREAM_CAPTURE) {
            if ((res = snd_pcm_prepare(mhandle)) < 0) {
            ERROR("overrun, it couldn't translate to prepare status");
            return  res;
        }
            return res;
        }
    }
    return res;
}

snd_pcm_uframes_t AlsaHelper::get_period_frames() const
{
    return period_frames;
}

snd_pcm_uframes_t AlsaHelper::get_period_frames_bytes() const
{
    return period_frames * (bits_per_frame / BYTE2BITS);
}

std::string AlsaHelper::get_usb_audio_device_name() const
{
    std::string deviceName = "default";
    snd_ctl_t *handle = nullptr;
    int  err = -1;
    snd_ctl_card_info_t *info = nullptr;
    snd_ctl_card_info_alloca(&info);
    int card = -1;
    if (snd_card_next(&card) < 0 || card < 0) {
        return deviceName;
    }
    int nameMaxLen = 32;
    while (card >= 0) {
        char name[nameMaxLen];
        snprintf(name, nameMaxLen, "hw:%d", card);
        if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
            if (snd_card_next(&card) < 0) {
                break;
            }
        }
        if ((err = snd_ctl_card_info(handle, info)) < 0) {
            snd_ctl_close(handle);
            if (snd_card_next(&card) < 0) {
                break;
            }
        }
        int dev = -1;
        while (true) {
            snd_ctl_pcm_next_device(handle, &dev);
            if (dev < 0) {
                break;
            }
            if ((snd_ctl_card_info_get_id(info) != nullptr) && (snd_ctl_card_info_get_name(info)!= nullptr)) {
                std::string card_info_id(snd_ctl_card_info_get_id(info));
                std::string card_info_name(snd_ctl_card_info_get_name(info));
                if ((card_info_id == "Audio") && (card_info_name == "USB Audio")) {
                    deviceName = "hw:" + std::to_string(card) + "," + std::to_string(dev);
                    if (validateAudioDevice(deviceName)) {
                        snd_ctl_close(handle);
                        return deviceName;
                    }
            }
            }
        }
        snd_ctl_close(handle);
        if (snd_card_next(&card) < 0) {
            break;
        }
    }
    return deviceName;
}

bool AlsaHelper::validateAudioDevice(std::string name) const
{
    snd_pcm_t *handle = nullptr;
    snd_pcm_hw_params_t *params = nullptr;
    unsigned int rate = 0;
    float lowLimit  = 0.95;
    float upLimit = 1.05;
    hwparams specHwparams = {
        .format = SND_PCM_FORMAT_S16_LE,
        .channels = CHANELTYPE_MONO,
        .rate = DEFAULT_SAMPLE_RATE,
    };
    if (snd_pcm_open(&handle, name.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
        return false;
    }
    snd_pcm_hw_params_alloca(&params);
    if (snd_pcm_hw_params_any(handle, params) < 0) {
        snd_pcm_close(handle);
        return false;
    }
    if (snd_pcm_hw_params_set_format(handle, params, specHwparams.format) < 0) {
        snd_pcm_close(handle);
        return false;
    }
    if (snd_pcm_hw_params_set_channels(handle, params, specHwparams.channels) < 0) {
        snd_pcm_close(handle);
        return false;
    }
    rate = specHwparams.rate;
    snd_pcm_hw_params_set_rate_near(handle, params, &specHwparams.rate, 0);
    if (static_cast<float>(rate * upLimit) < specHwparams.rate
        || static_cast<float>(rate * lowLimit) > specHwparams.rate) {
        snd_pcm_close(handle);
        return false;
    }
    snd_pcm_close(handle);
    return true;
}
}
}
