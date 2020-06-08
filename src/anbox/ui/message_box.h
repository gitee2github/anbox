/*
 *
 * Description: This program is used to create a messagebox window with message when 
 *              session is not running 
 * 
 */

#ifndef ANBOX_UI_MESSAGE_BOX_H_
#define ANBOX_UI_MESSAGE_BOX_H_

#include <thread>

#include <SDL2/SDL.h>

namespace anbox {
namespace ui {
class MessageBox {
public:
  MessageBox();
  ~MessageBox();

  void process_events();

private:
  SDL_Window *window_;
  bool event_thread_running_;
};
} // namespace ui
} // namespace anbox

#endif
