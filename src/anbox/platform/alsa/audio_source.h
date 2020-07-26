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
#ifndef ANBOX_PLATFORM_SDL_SOURCE_H
#define ANBOX_PLATFORM_SDL_SOURCE_H

#include <stdlib.h>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include <thread>
#include <unistd.h>
#include "anbox/audio/source.h"
#include "anbox/audio/alsa_helper.h"
#include "anbox/network/socket_connection.h"
#include "anbox/graphics/buffer_queue.h"

namespace anbox {
namespace platform {
namespace sdl {
class AudioSource:public audio::Source {
public:
    AudioSource();
    ~AudioSource() {}
    void read_data(const std::vector<std::uint8_t> &data) override;
    void set_socket_connection(std::shared_ptr<network::SocketConnection> const& connection) override;
    bool connect_audio() override;
    void sleep_ms(unsigned int secs) const;

private:
    std::thread::id tid;
    pid_t pid;
    bool is_opened;
    std::mutex lock_;
    anbox::audio::AlsaHelper malsahelper;
    std::shared_ptr<network::SocketConnection>  mConnection;

    int disconnect_audio();
    void process_pcm_data();
};
} // alsa
} // platform
} // anbox
#endif
