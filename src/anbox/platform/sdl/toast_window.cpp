#include "anbox/platform/sdl/toast_window.h"
#include "anbox/logger.h"

namespace anbox {
namespace platform {
namespace sdl {

ToastWindow::ToastWindow(const std::shared_ptr<Renderer> &renderer, const graphics::Rect &frame)
    : wm::Window(renderer, 0, frame, "toast"),
    native_display_(0) {
  window_ = SDL_CreateWindow("toast", frame.left(), frame.top(), frame.width(), frame.height(),
                             SDL_WINDOW_BORDERLESS | SDL_WINDOW_TOOLTIP);
  if (!window_) {
    const auto message = utils::string_format("Failed to create window: %s", SDL_GetError());
    return;
  }

  SDL_SysWMinfo info;
  memset(&info, 0, sizeof(struct SDL_SysWMinfo));
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
      break;
  }
  SDL_HideWindow(window_);

  attach();
}

ToastWindow::~ToastWindow() {
  release();
}

void ToastWindow::update_state(const wm::WindowState::List &states) {
  auto state = states[0];
  if (state.stack() == wm::Stack::Id::Default) {
    SDL_HideWindow(window_);
    return;
  }
  if (frame() != state.frame()) {
    SDL_SetWindowSize(window_, state.frame().width(), state.frame().height());
    SDL_SetWindowPosition(window_, state.frame().left(), state.frame().top());
    update_frame(state.frame());
  }
  SDL_ShowWindow(window_);
}

std::uint32_t ToastWindow::window_id() const { return SDL_GetWindowID(window_); }

} // namespace sdl
} // namespace platform
} // namespace anbox
