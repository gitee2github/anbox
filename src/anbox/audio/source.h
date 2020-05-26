/*
 * Copyright (C) 2020
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

#ifndef ANBOX_AUDIO_SOURCE_H_
#define ANBOX_AUDIO_SOURCE_H_

#include <cstdint>

#include <vector>
#include"anbox/network/socket_connection.h"

namespace anbox {
namespace audio {
class Source {
 public:
  virtual ~Source() {}

  virtual void read_data (const std::vector<std::uint8_t> &data) = 0;
  virtual void set_socket_connection (std::shared_ptr<network::SocketConnection> const& connection) = 0;
  virtual bool connect_audio() = 0;
};
} // namespace audio
} // namespace anbox

#endif
