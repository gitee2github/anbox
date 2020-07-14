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

#include "anbox/graphics/layer_composer.h"
#include "anbox/graphics/emugl/Renderer.h"
#include "anbox/logger.h"
#include "anbox/wm/manager.h"

namespace anbox {
namespace graphics {
LayerComposer::LayerComposer(const std::shared_ptr<Renderer> renderer, const std::shared_ptr<Strategy> &strategy,
                             const std::shared_ptr<wm::Manager> &wm)
    : renderer_(renderer), strategy_(strategy), wm_(wm) {}

LayerComposer::~LayerComposer() {}

void LayerComposer::submit_layers(const RenderableList &renderables) {
  wm_->lock_window(); // prevent window in win_layer to be released in other thread

  auto win_layers = strategy_->process_layers(renderables);
  for (auto &w : win_layers) {
    renderer_->draw(w.first->native_handle(),
                    Rect{0, 0, w.first->frame().width(), w.first->frame().height()},
                    w.second);
  }

  wm_->unlock_window();
}
}  // namespace graphics
}  // namespace anbox
