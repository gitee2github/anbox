
#ifndef ANBOX_PLATFORM_SDL_TOAST_WINDOW_H_
#define ANBOX_PLATFORM_SDL_TOAST_WINDOW_H_

#include "anbox/wm/window.h"
#include "anbox/platform/sdl/sdl_wrapper.h"

class Renderer;

namespace anbox {
namespace platform {
namespace sdl {
class ToastWindow : public std::enable_shared_from_this<ToastWindow>, public wm::Window {
 public:
  ToastWindow(const std::shared_ptr<Renderer> &renderer,
         const graphics::Rect &frame);
  ~ToastWindow();

  void update_state(const wm::WindowState::List &states) override;
  std::uint32_t window_id() const;

 private:
  SDL_Window *window_;
  EGLNativeDisplayType native_display_;
};
} // namespace sdl
} // namespace platform
} // namespace anbox

#endif
