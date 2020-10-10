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

#include "anbox/platform/sdl/window.h"
#include "anbox/wm/window_state.h"
#include "anbox/graphics/density.h"
#include "anbox/logger.h"
#include "anbox/system_configuration.h"

#include <boost/throw_exception.hpp>
#include <SDL2/SDL_image.h>

#if defined(MIR_SUPPORT)
#include <mir_toolkit/mir_client_library.h>
#endif

namespace {
constexpr const int window_resize_border{5};
constexpr const int button_size{42};
constexpr const int button_margin{0};
constexpr const int button_padding{0};
}

namespace anbox {
namespace platform {
namespace sdl {

static const std::uint32_t HIDE_BACK     = 0x01;
static const std::uint32_t HIDE_MINIMIZE = 0x02;
static const std::uint32_t HIDE_MAXIMIZE = 0x04;
static const std::uint32_t HIDE_CLOSE    = 0x08;
static const std::uint32_t SHOW_ALL      = 0x00;

const std::map<std::string, std::uint32_t> Window::property_map = {
  {"腾讯视频", SHOW_ALL},
  {"爱奇艺HD", SHOW_ALL}
};

Window::Id Window::Invalid{ -1 };

Window::Observer::~Observer() {}

Window::Window(const std::shared_ptr<Renderer> &renderer,
               const Id &id, const wm::Task::Id &task,
               const std::shared_ptr<Observer> &observer,
               const graphics::Rect &frame,
               const std::string &title,
               bool resizable)
    : wm::Window(renderer, task, frame, title),
      id_(id),
      lastClickTime(0),
      observer_(observer),
      native_display_(0),
      visible_property(SHOW_ALL) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);

  // NOTE: We don't furce GL initialization of the window as this will
  // be take care of by the Renderer when we attach to it. On EGL
  // initializing GL here will cause a surface to be created and the
  // renderer will attempt to create one too which will not work as
  // only a single surface per EGLNativeWindowType is supported.
  std::uint32_t flags = SDL_WINDOW_BORDERLESS;
  auto property_itr = property_map.find(title);
  if (property_itr != property_map.end()) {
    visible_property = property_itr->second;
  } else {
    visible_property = HIDE_MAXIMIZE;
  }
  if (!(visible_property & HIDE_MAXIMIZE) && resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }

  window_ = SDL_CreateWindow(title.c_str(),
                             frame.left(), frame.top(),
                             frame.width(), frame.height(),
                             flags);
  if (!window_) {
    const auto message = utils::string_format("Failed to create window: %s", SDL_GetError());
    BOOST_THROW_EXCEPTION(std::runtime_error(message));
  }

  if (utils::get_env_value("ANBOX_NO_SDL_WINDOW_HIT_TEST", "false") == "false")
    if (SDL_SetWindowHitTest(window_, &Window::on_window_hit, this) < 0)
      BOOST_THROW_EXCEPTION(std::runtime_error("Failed to register for window hit test"));

  std::string strPath = SystemConfiguration::instance().resource_dir() + "/ui/logo.png";

  SDL_Surface *icon = IMG_Load(strPath.c_str());
  SDL_SetWindowIcon(window_, icon);
  SDL_FreeSurface(icon);

  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  SDL_GetWindowWMInfo(window_, &info);
  switch (info.subsystem) {
#if defined(X11_SUPPORT)
    case SDL_SYSWM_X11:
      native_display_ = static_cast<EGLNativeDisplayType>(info.info.x11.display);
      set_native_handle(static_cast<EGLNativeWindowType>(info.info.x11.window));
      break;
#endif
#if defined(WAYLAND_SUPPORT)
    case SDL_SYSWM_WAYLAND:
      native_display_ = reinterpret_cast<EGLNativeDisplayType>(info.info.wl.display);
      set_native_handle(reinterpret_cast<EGLNativeWindowType>(info.info.wl.surface));
      break;
#endif
#if defined(MIR_SUPPORT)
    case SDL_SYSWM_MIR: {
      native_display_ = static_cast<EGLNativeDisplayType>(mir_connection_get_egl_native_display(info.info.mir.connection));
      auto buffer_stream = mir_surface_get_buffer_stream(info.info.mir.surface);
      set_native_handle(reinterpret_cast<EGLNativeWindowType>(mir_buffer_stream_get_egl_native_window(buffer_stream)));
      break;
    }
#endif
    default:
      ERROR("Unknown subsystem (%d)", info.subsystem);
      BOOST_THROW_EXCEPTION(std::runtime_error("SDL subsystem not supported"));
  }

  if (get_system_time(last_update_time)) {
    lastClickTime = last_update_time;
  } else {
    ERROR("get system time error!");
  }

  if (window_) {
    SDL_ShowWindow(window_);
  } else {
    ERROR("window null point!");
  }
}

Window::~Window() {}

bool Window::get_system_time(long long &time) {
  struct timeval now;
  memset(&now, 0, sizeof(struct timeval));
  if (gettimeofday(&now, NULL) >= 0) {
    time = USEC_PER_SEC * (now.tv_sec) + now.tv_usec;
    return true;
  }
  return false;
}

void Window::destroy_window() {
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = NULL;
  }
}

bool Window::title_event_filter(int x, int y) {
  const auto top_drag_area_height = graphics::dp_to_pixel(button_size + (button_margin << 1));
  return is_title_enable_ && !fullscreen_ && y <= top_drag_area_height;
}

SDL_HitTestResult Window::on_window_hit(SDL_Window *window, const SDL_Point *pt, void *data) {
  auto platform_window = reinterpret_cast<Window*>(data);
  if (platform_window == nullptr) {
    return SDL_HITTEST_NORMAL;
  }
  platform_window->set_closing(false);

  if (platform_window->get_fullscreen()) {
    return SDL_HITTEST_NORMAL;
  }

  int w = 0, h = 0;
  SDL_GetWindowSize(window, &w, &h);

  // left and right, two margins
  const auto button_area_width = graphics::dp_to_pixel(button_size + (button_margin << 1));

  if (!platform_window->initialized.load()) {
    INFO("window initialized by resize");
    platform_window->initialized = true;
  }
  if (platform_window->title_event_filter(pt->x, pt->y)) {
    if (!platform_window->initialized.load()) {
      INFO("window initialized by click top");
      platform_window->initialized = true;
    }

    if (pt->x > 0 && pt->x < button_area_width &&
            ((platform_window->get_property() & HIDE_BACK) != HIDE_BACK)) {
      std::shared_ptr<anbox::platform::sdl::Window::Observer> observer = platform_window->observer_;
      if (observer) {
        platform_window->set_closing(true);
        observer->window_wants_focus(platform_window->id());
        observer->input_key_event(SDL_SCANCODE_AC_BACK, 1);
        observer->input_key_event(SDL_SCANCODE_AC_BACK, 0);
      }
      return SDL_HITTEST_NORMAL;
    }
    int btn_cnt = 1;  //button count from right of the titlebar
    if ((platform_window->get_property() & HIDE_CLOSE) != HIDE_CLOSE) {
      if (pt->x > w - button_area_width * btn_cnt &&
              pt->x < w - button_area_width * (btn_cnt - 1)) {
        platform_window->set_closing(true);
        platform_window->close();
        return SDL_HITTEST_NORMAL;
      }
      ++btn_cnt;
    }
    if ((platform_window->get_property() & HIDE_MAXIMIZE) != HIDE_MAXIMIZE) {
      if (pt->x > w - button_area_width * btn_cnt &&
              pt->x < w - button_area_width * (btn_cnt - 1)) {
        platform_window->switch_window_state();
        return SDL_HITTEST_NORMAL;
      }
      ++btn_cnt;
    }
    if ((platform_window->get_property() & HIDE_MINIMIZE) != HIDE_MINIMIZE) {
      if (pt->x > w - button_area_width * btn_cnt &&
              pt->x < w - button_area_width * (btn_cnt - 1)) {
        SDL_MinimizeWindow(platform_window->window_);
        return SDL_HITTEST_NORMAL;
      }
    }
    if (((platform_window->get_property() & HIDE_MAXIMIZE) != HIDE_MAXIMIZE) &&
               platform_window->check_db_clicked(pt->x, pt->y)) {
      platform_window->switch_window_state();
      return SDL_HITTEST_NORMAL;
    } else {
      return SDL_HITTEST_DRAGGABLE;
    }
  }

  return SDL_HITTEST_NORMAL;
}

bool Window::check_point_same_as_last(int x, int y) {
  return x == last_point_x_ && y == last_point_y_;
}

bool Window::check_window_no_move(int wnd_x, int wnd_y) {
  return wnd_x == last_wnd_x_ && wnd_y == last_wnd_y_;
}

bool Window::check_db_clicked(int x, int y) {
  bool result = false;
  long long current_time = 0;
  if (!get_system_time(current_time)) {
    return false;
  }

  int wnd_x = 0;
  int wnd_y = 0;
  SDL_GetWindowPosition(window_, &wnd_x, &wnd_y);
  if (check_point_same_as_last(x, y) && 
      check_window_no_move(wnd_x, wnd_y) &&
      current_time - lastClickTime <= timespan_db_click) {
    lastClickTime = current_time - timespan_db_click;
    result = true;
  } else {
    lastClickTime = current_time;
  }

  last_point_x_ = x;
  last_point_y_ = y;
  last_wnd_x_ = wnd_x;
  last_wnd_y_ = wnd_y;

  return result;
}

void Window::close() {
  if (observer_)
    observer_->window_deleted(id_);
}

void Window::switch_window_state() {
  const auto flags = SDL_GetWindowFlags(window_);
  if (flags & SDL_WINDOW_MAXIMIZED)
    SDL_RestoreWindow(window_);
  else
    SDL_MaximizeWindow(window_);
}

void Window::process_event(const SDL_Event &event) {
  switch (event.window.event) {
    case SDL_WINDOWEVENT_FOCUS_GAINED:
      if (observer_) {
        observer_->window_wants_focus(id_);
      }
      break;
    case SDL_WINDOWEVENT_FOCUS_LOST:
      break;
    // Not need to listen for SDL_WINDOWEVENT_RESIZED here as the
    // SDL_WINDOWEVENT_SIZE_CHANGED is always sent.
    case SDL_WINDOWEVENT_SIZE_CHANGED:
      if (observer_) {
        if (get_system_time(last_resize_time_)) {
          resizing_ = true;
        }
        observer_->window_resized(id_, event.window.data1, event.window.data2);
      }
      break;
    case SDL_WINDOWEVENT_MOVED:
      if (observer_) {
        if (get_system_time(last_resize_time_)) {
          resizing_ = true;
        }
        observer_->window_moved(id_, event.window.data1, event.window.data2);
      }
      break;
    case SDL_WINDOWEVENT_SHOWN:
      break;
    case SDL_WINDOWEVENT_HIDDEN:
      break;
    case SDL_WINDOWEVENT_CLOSE:
      if (observer_)
        observer_->window_deleted(id_);
      close();
      break;
    default:
      break;
  }
}

Window::Id Window::id() const { return id_; }

std::uint32_t Window::window_id() const { return SDL_GetWindowID(window_); }

bool Window::checkResizeable() {
  long long time_now = 0;
  if (!get_system_time(time_now)) {
    return false;
  }
  if (resizing_ && time_now - last_resize_time_ > RESIZE_TIMESPAN) {
    last_frame_ = frame();
    resizing_ = false;
  }
  return resizing_;
}

void Window::setResizing(bool resizing) {
  last_frame_ = frame();
  resizing_ = resizing;
}

void Window::update_state(const wm::WindowState::List &states) {
  bool is_title_enable = true;
  const auto top_drag_area_height = graphics::dp_to_pixel(button_size + (button_margin << 1));

  for (int i = states.size() - 1; i >= 0; --i) {
    auto ws = states[i];
    // 0 means window_frame has no titlebar, set it disabled.
    if (ws.flags() == 0 && (ws.frame().top() - frame().top() < top_drag_area_height)) {
      is_title_enable = false;
      break;
    }
    if (ws.flags() != 0) {
      break;
    }
  }

  is_title_enable_ = is_title_enable;
  bool need_fullscreen = false;
  for (auto ws : states)
  {
    if (ws.videofullscreen()) {
      need_fullscreen = true;
      break;
    }
  }

  if (!fullscreen_ && need_fullscreen) {
    SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
  } else if (fullscreen_ && !need_fullscreen) {
    SDL_SetWindowFullscreen(window_, 0);
  }

  fullscreen_ = need_fullscreen;
  if (fullscreen_) {
    // if window is in fullscreen state, ignore position and size change
    return;
  }

  if (!initialized.load() && !states.empty()) {
    int w = 0;
    int h = 0;
    int x = 0;
    int y = 0;
    SDL_GetWindowSize(window_, &w, &h);
    SDL_GetWindowPosition(window_, &x, &y);

    graphics::Rect rect = frame();
    int area = w * h;
    for (auto ws : states)
    {
      int temp = ws.frame().width() * ws.frame().height();
      if (temp >= area) {
        rect = ws.frame();
	area = temp;
      }
    }

    if (w == rect.width() &&
        h == rect.height() &&
        x == rect.left() &&
        y == rect.top()) {
      return;
    }
    long long current_time = 0;
    if (!get_system_time(current_time)) {
      return;
    }
    if (current_time - last_update_time >= APP_START_MAX_TIME) {
      INFO("window initialized by timeout");
      initialized = true;
      return;
    }

    last_update_time = current_time;
    SDL_SetWindowSize(window_, rect.width(), rect.height());
    SDL_SetWindowPosition(window_, rect.left(), rect.top());
    update_frame(rect);
  }
}

void Window::restore_window() {
  if (window_ == nullptr) {
    ERROR("window not initialized!");
    return;
  }
  SDL_ShowWindow(window_);
  SDL_RaiseWindow(window_);
  auto flags = SDL_GetWindowFlags(window_);
  if (flags & SDL_WINDOW_MINIMIZED) {
    SDL_RestoreWindow(window_);
  }
}

void Window::set_focus_from_android(bool just_set) {
  if (observer_ && observer_->get_focus_window_id() != window_id()) {
    if (just_set || observer_->is_focus_window_closing()) {
      observer_->set_focus_window_id(window_id());
    } else {
      restore_window();
    }
  }
}

} // namespace sdl
} // namespace platform
} // namespace anbox
