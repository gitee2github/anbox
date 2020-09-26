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

#include "anbox/ui/message_box.h"
#include "anbox/system_configuration.h"
#include "anbox/utils.h"
#include "anbox/logger.h"

#include <SDL2/SDL_image.h>

namespace anbox {
namespace ui {
MessageBox::MessageBox(){
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
  // Don't disable compositing
  // Available since SDL 2.0.8
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
    const auto message = utils::string_format("Failed to initialize SDL: %s", SDL_GetError());
    BOOST_THROW_EXCEPTION(std::runtime_error(message));
  }

  const auto width = 320;
  const auto height = 50;
  window_ = SDL_CreateWindow("start android service first!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             width, height, SDL_WINDOW_SHOWN);
  if (!window_) {
    const auto message = utils::string_format("Failed to create window: %s", SDL_GetError());
    BOOST_THROW_EXCEPTION(std::runtime_error(message));
  }

  auto surface = SDL_GetWindowSurface(window_);
  if (!surface)
    BOOST_THROW_EXCEPTION(std::runtime_error("Could not retrieve surface for our window"));

  SDL_FillRect(surface, nullptr, SDL_MapRGB(surface->format, 0xee, 0xee, 0xee));
  SDL_UpdateWindowSurface(window_);

  event_thread_running_ = true;
}

MessageBox::~MessageBox() {
}

void MessageBox::process_events() {
  while (event_thread_running_) {
    SDL_Event event;
    while (event_thread_running_ && SDL_WaitEventTimeout(&event, 100)) {
      switch (event.type) {
        case SDL_QUIT:
          event_thread_running_ = false;
          break;
        default:
          break;
      }
    }
  }
}
} // namespace ui
} // namespace anbox
