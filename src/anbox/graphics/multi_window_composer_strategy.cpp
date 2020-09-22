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

#include "anbox/graphics/multi_window_composer_strategy.h"
#include "anbox/wm/manager.h"
#include "anbox/utils.h"
#include "anbox/logger.h"

namespace anbox {
namespace graphics {
MultiWindowComposerStrategy::MultiWindowComposerStrategy(const std::shared_ptr<wm::Manager> &wm) : wm_(wm) {}

std::map<std::shared_ptr<wm::Window>, RenderableList> MultiWindowComposerStrategy::process_layers(const RenderableList &renderables) {
  WindowRenderableList win_layers;
  bool bGetToast = false;  // if don't have toast frame, tell wm to hide toast_window
  for (const auto &renderable : renderables) {
    // Ignore all surfaces which are not meant for a task
    if (renderable.name() == "Toast") {
      if (bGetToast) {
        WARNING("Toast! toast choosed");
        continue;
      }
      auto w = wm_->update_toast_window(renderable.screen_position());
      if (w) {
        win_layers.insert({w, {renderable}});
      } else {
        ERROR("Toast! get toast window error");
        continue;
      }
      bGetToast = true;
      continue;
    }
    if (!utils::string_starts_with(renderable.name(), "org.anbox.surface.")) {
      continue;
    }

    wm::Task::Id task_id = 0;
    if (sscanf(renderable.name().c_str(), "org.anbox.surface.%d", &task_id) != 1 || !task_id)
      continue;

    auto w = wm_->find_window_for_task(task_id);
    if (!w) continue;
    if (win_layers.find(w) == win_layers.end()) {
      win_layers.insert({w, {renderable}});
      continue;
    }

    win_layers[w].push_back(renderable);
  }
  if (!bGetToast) { // hide toast window
    wm_->update_toast_window(Rect(0, 0, 0, 0));
  }

  for (auto &w : win_layers) {
    const auto &renderables = w.second;
    RenderableList final_renderables;
    auto window_frame = w.first->frame();
    bool resizeable = w.first->checkResizeable();
    auto old_frame = w.first->last_frame();
    int max_area = 0;
    Rect max_rect;
    for (auto &r : renderables) {
      // As we get absolute display coordinates from the Android hwcomposer we
      // need to recalculate all layer coordinates into relatives ones to the
      // window they are drawn into.
      auto rect = Rect{
          r.screen_position().left() - window_frame.left(),
          r.screen_position().top() - window_frame.top(),
          r.screen_position().right() - window_frame.left(),
          r.screen_position().bottom() - window_frame.top()};

      if (rect.width() * rect.height() > max_area) {
        max_area = rect.width() * rect.height();
        max_rect = Rect(r.screen_position().left() - old_frame.left(),
                        r.screen_position().top() - old_frame.top(),
                        r.screen_position().right() - old_frame.left(),
                        r.screen_position().bottom() - old_frame.top());
      }
      auto new_renderable = r;
      new_renderable.set_screen_position(rect);
      final_renderables.push_back(new_renderable);
    }

    bool changed = false;
    if (resizeable) {
      int max_old_area = 0;
      Rect max_old_rect;
      auto it = last_renderables.find(w.first);
      if (it != last_renderables.end()) {
        for (auto &rt : it->second) {
          if (max_old_area <= rt.screen_position().width() * rt.screen_position().height()) {
            max_old_area = rt.screen_position().width() * rt.screen_position().height();
            max_old_rect = rt.screen_position();
          }
        }
      }
      if (max_old_rect == max_rect) {
        w.second = it->second;
        changed = true;
      } else {
        w.first->setResizing(false);
      }
    }

    if(!changed) {
      w.second = final_renderables;
    }
  }
  last_renderables.swap(win_layers);
  return last_renderables;
}
}  // namespace graphics
}  // namespace anbox
