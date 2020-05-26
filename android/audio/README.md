# hardware-audio #

This module contains the Anbox Android Audio HAL source code.
It is part of the Anbox Android delivery for Android.

## Description ##

This is the first version of the module for android.
It generates the library "audio.primary.goldfish.so" used to implement vendor-specific functionalities for the Android default Audio service.

The configuration file  audio_policy.conf should be on the "hardware/libhardware_legacy"  folder. It defines the default configuration states (input, output...) of the driver. More details can be found in   audio_policy.conf.  The  output/Input  parameter in audio_policy.conf should be set as follow:

        outputs {
          primary {
            sampling_rates 44100
            channel_masks AUDIO_CHANNEL_OUT_STEREO
            formats AUDIO_FORMAT_PCM_16_BIT
            devices AUDIO_DEVICE_OUT_SPEAKER
            flags AUDIO_OUTPUT_FLAG_PRIMARY
          }
        }
        inputs {
          primary {
            sampling_rates 48000
            channel_masks AUDIO_CHANNEL_IN_MONO
            formats AUDIO_FORMAT_PCM_16_BIT
            devices AUDIO_DEVICE_IN_BUILTIN_MIC
          }
        }

Warning: Our audio chip only support 48kHz or 44.1kHz, in order to get high quality recording,
so we choose 48kHz.If you want to change inputs sampling_rates, you must modify alsa_helper.h,it should be on the "anbox/src/anbox/audio/" folder.
source codes as follow:
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

Please see the release notes for more details.

## Documentation ##

* The [release notes][] provide information on the release.
* The [distribution package][] provides detailed information on how to use this delivery.

## Dependencies ##

This module can not be used alone. It is part of the Anbox android delivery for Android.
It provides the audio library used by the default implementation of android.   And  it need to  connect with

local socket server  which is start with anbox session-manager.  which has to be added in CMakeLists.txt as follow:

```
include_directories(
    ...
	/usr/include/alsa/
)
add_library(anbox-protobuf
    STATIC
    ...
    target_link_libraries(anbox-protobuf
    ${PROTOBUF_LITE_LIBRARIES}
    /usr/lib/aarch64-linux-gnu/libasound.so
    )
set(SOURCES
    ...
    anbox/audio/alsa_helper.h
    anbox/audio/alsa_helper.cpp
    anbox/platform/alsa/audio_source.h
    anbox/platform/alsa/audio_source.cpp
    )
```

## Contents ##

This directory contains the sources and the associated Android makefile to generate the audio.primary.goldfish.so library.

### Audio Playback details

the graphics that explains the details of  audio playback is as follows:

                                ______________
                               |              |   pthread_mutex_lock(&out->lock)
                               |  out_write   |   pthread_mutex_unlock(&out->lock)
                               |______________|
                                       |
                                       |write to queue head
                                       |
                    |<---live--------->|head
             _______|__________________v________________________
            |       |111111111111111111|                        | Circular Queue
            |_______|___________________________________________|
                    |tail
                    |
                    | read from queue tail
                    |
                    |
                  __V________________
                 |                   |  pthread_mutex_lock(&out->lock)
           thread|  out_write_worker |  pthread_mutex_unlock(&out->lock)
                 |___________________|  write
                    |
                    |local socket connect
                    |
                    |
                  __V________________
                 |                   |
                 |anbox socket server|
                 |___________________|
                    |
                    |
                    |
                  __V________________
                 |                   |
                 |host audio driver  |
                 |___________________|

### Sound recording details

the graphics that explains the details of  audio recording is as follows:

           ______________
          |              |   pthread_mutex_lock(&in->lock)
          |    in_read   |   pthread_mutex_unlock(&in->lock)
          |_____^________|
                |
                | read from queue tail
                |
           tail |<---live--------->|
         _______|__________________v_________________________
        |       |111111111111111111|                         | Circular Queue
        |_______|__________________^_________________________|
                                   |head
                                   |
                                   |write to queue head
                                   |
                                   |
                    _______________|__
                  |                   |  pthread_mutex_lock(&in->lock)
            thread|  in_read_worker   |  pthread_mutex_unlock(&in->lock)
                  |____^______________|  read
                       |
                       |local socket connect
                       |
                       |
                   ___________________
                  |                   |
                  |anbox socket server|
                  |____^______________|
                       |
                       |
                       |
                  _____|_____________
                 |                   |
                 |host audio driver  |
                 |___________________|


## License ##

This module is distributed under the Apache License, Version 2.0 found in the [LICENSE](./LICENSE)file.
