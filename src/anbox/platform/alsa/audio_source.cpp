/*
 * Copyright (C) 2016 Simon Fels <morphis@gravedo.de>
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

#include "anbox/platform/alsa/audio_source.h"
#include <stdio.h>
#include <time.h>
#include <stdexcept>
#include <string>
#include <boost/throw_exception.hpp>
#include <alsa/asoundlib.h>
#include <sys/select.h>
#include "anbox/logger.h"
#include "anbox/audio/client_info.h"

namespace anbox{
namespace platform{
namespace sdl{

AudioSource::AudioSource()
{
    tid = std::this_thread::get_id();
    pid = getpid();
    is_opened = false;
}

bool AudioSource::connect_audio()
{
    clock_t start = clock();
    int res = 0;
    std::string audioName = malsahelper.get_usb_audio_device_name();
    res = malsahelper.open_pcm_device(audioName.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (res == 0) {
        audio::hwparams params;
        malsahelper.get_device_config(params);
        res = malsahelper.set_pcm_params(params);
        if (res < 0) {
            is_opened = false;
            ERROR("audio source set pcm parameters fail,error code = %d !!!", res);
            malsahelper.close_pcm_device();
            return is_opened;
        }
        is_opened = true;
        DEBUG("audio source open pcm device and set parameters successfully");
    } else {
        is_opened = false;
        ERROR("audio source open pcm device fail, error code = %d  !!!", res);
        return is_opened;
    }
    clock_t end = clock();
    long cost_time = end - start; // time unit is ms
    DEBUG("open pcm device cost times = %d  ms", cost_time);
    return is_opened;
}

int AudioSource::disconnect_audio()
{
    DEBUG("source disconnect audio is called");
    return  malsahelper.close_pcm_device();
}

void AudioSource::set_socket_connection(std::shared_ptr<network::SocketConnection> const& connection)
{
    DEBUG("audio hal request to record pid = %d ,tid = %d", pid, tid);
    mConnection = connection;
}

void AudioSource::process_pcm_data()
{
    std::string fail = "fail";
    if (!is_opened) {
        ERROR("pcm devices isn't opened");
        mConnection->send(fail.c_str(), fail.size());
        return;
    }
    snd_pcm_uframes_t  period_frames = malsahelper.get_period_frames();
    snd_pcm_uframes_t  period_frames_bytes = malsahelper.get_period_frames_bytes();
    char* dataBuf = new (std::nothrow) char [period_frames_bytes];
    if (dataBuf == nullptr) {
        ERROR("get memory fail !!!");
        mConnection->send(fail.c_str(), fail.size());
        return;
    }
    memset(dataBuf, 0, period_frames_bytes);
    const char* tmp = nullptr;
    if ((malsahelper.pcm_read(dataBuf, period_frames)) != period_frames) {
        mConnection->send(fail.c_str(), fail.size());
        ERROR("read pcm data failed !!!");
    } else {
        tmp = dataBuf;
        mConnection->send(tmp, period_frames_bytes);
        tmp = nullptr;
        DEBUG("read pcm data and send them to audio hal successfully!");
    }
    delete[] dataBuf;
}

/**
*  response recording request from android audio hal
*/
void AudioSource::read_data(const std::vector<std::uint8_t> &data)
{
    DEBUG("recording ,pid = %d,tid = %d", pid, tid);
    enum audio::RecordCommand cmd = static_cast<enum audio::RecordCommand>(data[0]); // parse request type from hal
    std::unique_lock<std::mutex> l(lock_);
    int res = 0;
    std::string str;
    switch (cmd) {
        case audio::RecordCommand::StartRecord:
            process_pcm_data();
            break;
        case audio::RecordCommand::StopRecord:
            res = disconnect_audio();
            str = std::to_string(res);
            mConnection->send(str.c_str(), str.length());
            break;
        default:
            // do nothing
            DEBUG("AudioSource read data,unknow cmd pid = %d,tid = %d", pid, tid);
            break;
    }
}

void AudioSource::sleep_ms(unsigned int secs) const
{
    const long secs2usec = 1000000;
    struct timeval tval;
    tval.tv_sec = secs / CLOCKS_PER_SEC;
    tval.tv_usec = (secs * CLOCKS_PER_SEC % secs2usec);
    select(0, NULL, NULL, NULL, &tval);
}
}
}
}
