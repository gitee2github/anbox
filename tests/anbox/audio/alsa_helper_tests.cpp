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

#include "anbox/audio/alsa_helper.h"
#include "anbox/logger.h"
#include<string>
#include <gtest/gtest.h>

namespace anbox{
namespace audio{

TEST (AlsaHelper, OpenPcmDevice) {
  AlsaHelper alsahelper;
  EXPECT_EQ(0, alsahelper.open_pcm_device("default", SND_PCM_STREAM_CAPTURE, 0));
  alsahelper.close_pcm_device();

  EXPECT_EQ(0, alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_CAPTURE, 0));
  alsahelper.close_pcm_device();

  EXPECT_EQ(0, alsahelper.open_pcm_device("default", SND_PCM_STREAM_PLAYBACK, 0));
  alsahelper.close_pcm_device();

  EXPECT_EQ(0, alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_PLAYBACK, 0));
  alsahelper.close_pcm_device();

  // pc doesn't has hw:4,0 so open fail
  EXPECT_NE(0, alsahelper.open_pcm_device("hw:4,0", SND_PCM_STREAM_PLAYBACK, 0));
  alsahelper.close_pcm_device();

  EXPECT_NE(0, alsahelper.open_pcm_device("hw:4,0", SND_PCM_STREAM_CAPTURE, 0));
  alsahelper.close_pcm_device();
}

TEST (AlsaHelper, ClosePcmDevice) {
  AlsaHelper alsahelper;
  // not open pcm device first.so close  pcm devices fail
  EXPECT_EQ(0, alsahelper.close_pcm_device());
  alsahelper.open_pcm_device("default", SND_PCM_STREAM_CAPTURE, 0);
  EXPECT_EQ(0, alsahelper.close_pcm_device());
}


TEST (AlsaHelper, SetPcmParams) {
  AlsaHelper alsahelper;
  alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_CAPTURE, 0);
  hwparams mhwparams;
  // rate is correct
  {
    mhwparams.rate = 48000; // 48kHz
    mhwparams.channels = CHANELTYPE_MONO;
    mhwparams.format = SND_PCM_FORMAT_S16_LE;
  }
  EXPECT_EQ(0, alsahelper.set_pcm_params(mhwparams));

  {
    mhwparams.rate = 1500;
    mhwparams.channels = CHANELTYPE_MONO;
    mhwparams.format = SND_PCM_FORMAT_S16_LE;
  }
  EXPECT_NE(0, alsahelper.set_pcm_params(mhwparams));

  // rate is wrong
  {
    mhwparams.rate = 769000; // 80kHz
    mhwparams.channels = CHANELTYPE_MONO;
    mhwparams.format = SND_PCM_FORMAT_S16_LE;
  }
  EXPECT_NE(0, alsahelper.set_pcm_params(mhwparams));

  // channel only support CHANELTYPE_MONO or CHANELTYPE_STEREO(),otherwise is wrong
  {
    mhwparams.rate = 48000; // 48kHz
    mhwparams.channels = CHANELTYPE_MONO;
    mhwparams.format = SND_PCM_FORMAT_S16_LE;
  }
  EXPECT_EQ(0, alsahelper.set_pcm_params(mhwparams));
  // card 2 device 0 doesn't support CHANELTYPE_STEREO record
  mhwparams.channels = CHANELTYPE_STEREO;
  EXPECT_NE(0, alsahelper.set_pcm_params(mhwparams));

  alsahelper.close_pcm_device();
}

TEST (AlsaHelper, PcmRead) {
  AlsaHelper alsahelper;
  alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_CAPTURE, 0);
  hwparams params;
  params.rate = 48000; // 48kHZ
  params.channels = CHANELTYPE_MONO;
  params.format = SND_PCM_FORMAT_S16_LE;
  alsahelper.set_pcm_params(params);
  int period_frames = 480;
  int per_frames_bytes = 2; // SND_PCM_FORMAT_S16_LE has two Bytes
  bool success = false;
  char* dataBuf = new (std::nothrow) char [period_frames * 2];
  if (dataBuf != nullptr) {
    if ((alsahelper.pcm_read(dataBuf, period_frames)) == period_frames) {
      success = true;
    }
  }
  delete[] dataBuf;
  EXPECT_EQ(true,success);
  alsahelper.close_pcm_device();
}

TEST (AlsaHelper, GetPeriodFrames) {
  AlsaHelper alsahelper;
  alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_CAPTURE, 0);
  hwparams params;
  params.rate = 48000; // 48KHz
  params.channels = CHANELTYPE_MONO;
  params.format = SND_PCM_FORMAT_S16_LE;
  alsahelper.set_pcm_params(params);
  int period_frames = 480;
  int res = alsahelper.get_period_frames();
  EXPECT_EQ(period_frames, res);
}

TEST(AlsaHelper,GetPeriodFramesBytes){
  AlsaHelper alsahelper;
  alsahelper.open_pcm_device("hw:2,0", SND_PCM_STREAM_CAPTURE, 0);
  hwparams params;
  params.rate = 48000; // 48kHZ
  params.channels = CHANELTYPE_MONO;
  params.format = SND_PCM_FORMAT_S16_LE;
  alsahelper.set_pcm_params(params);
  int period_frames = 480;
  int expect_res = 960;
  int res = alsahelper.get_period_frames_bytes();
  EXPECT_EQ(period_frames*2, expect_res);
}

TEST(AlsaHelper,GetUsbAudioDeviceName){
    AlsaHelper alsahelper;
    std::string deviceName;
    deviceName = alsahelper.get_usb_audio_device_name();
    EXPECT_EQ(deviceName, "hw:2,0");
}

}
}
