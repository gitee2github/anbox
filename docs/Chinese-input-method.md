# Chinese input Method #

This module is used to release Chinese input in anbox.
It is part of the Anbox Android delivery for Android.

## Description ##

This is the first version of the module for android.
the graphics that explains the working principle of this module is as follows:
           ______________                   ______________                         ______________ 
          |              | SDL_TEXTINPUT   |              |    ime_socket         |              |
          |    SDL       |----------------->   Anbox      |----------------------->   Android    |
          |______________|                 |______________|                       |______________|

Chinese input method module is Separated as follows:
```
Create socket "ime_socket" to connect android-framework
get SDL_EVENT to collect Chinese input Characters
send Chinese input Characters to android-framework module
```

## Dependencies ##

This module can not be used alone. It is part of the Anbox android delivery for Android.
It need to connect with local socket server which is start with anbox session-manager.

## Code Detials ##

The most of the code change is in ./src/anbox/platform/sdl/platform.cpp
And the key code is as follows:
```
void Platform::process_events() {
  ...
          case SDL_FINGERMOTION:
            process_input_event(event);
          break;
+         case SDL_TEXTINPUT:
+         WARNING("Input Event TEXT=%s TYPE=%d WINDOWID=%d", event.text.text, event.type, event.text.windowID);
+         if (flag == 0 && ime_fd_) {
+           send(ime_fd_, event.text.text, strlen(event.text.text), 0);
+         }
+         break;
  ...
}
```

## License ##

This module is distributed under the Apache License, Version 2.0 found in the [LICENSE](./LICENSE)file.