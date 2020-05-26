/*
 * Copyright (C) 2016 Simon Fels <morphis@gravedo.de>
 *
 * This program is fre
 * e software: you can redistribute it and/or modify it
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
#include "anbox/platform/alsa/audio_source.h"
#include "anbox/logger.h"
#include <gtest/gtest.h>

namespace anbox{
namespace platform{
namespace sdl{
TEST (AudioSource, ConnectAudio) {
    AudioSource source;
    bool res = source.connect_audio();
    EXPECT_EQ(true, res);

}

}
}
}