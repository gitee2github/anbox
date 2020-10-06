#include <gmock/gmock.h>

#include "anbox/wm/multi_window_manager.h"
#include "anbox/wm/window.h"
#include "anbox/platform/base_platform.h"
#include "anbox/platform/null/platform.h"
#include "anbox/bridge/android_api_stub.h"
#include "anbox/application/database.h"

#include <SDL2/SDL.h>
#include <vector>

using namespace ::testing;
namespace anbox{
namespace wm{

using std::vector;

int user_window_event = 0;

class MockPlatform : public platform::NullPlatform{
 public:
    int get_user_window_event() const { return user_window_event; }
};

class MockWindow : public Window{
 public:
  MockWindow(const anbox::wm::Task::Id &task,
               const anbox::graphics::Rect &frame,
               const std::string &title)
        : Window(nullptr, task, frame, title) {}
  MOCK_METHOD1(update_state, void(const WindowState::List &states));
};

TEST(MultiWindowManager, TestInsertAndRemoveTask) {
  std::shared_ptr<Renderer> renderer;
  auto wnd = std::make_shared<Window>(renderer, 0, graphics::Rect(0,0,1024,768), "test");
  Task::Id test_id = 0;

  platform::Configuration config;
  // The default policy will create a dumb window instance when requested
  // from the manager.
  auto platform = platform::create(std::string(""), nullptr, config);
  auto app_db = std::make_shared<application::Database>();
  auto wm = std::make_shared<MultiWindowManager>(platform, nullptr, app_db);

  EXPECT_EQ(nullptr, wm->find_window_for_task(test_id));

  wm->insert_task(test_id, wnd);
  EXPECT_EQ(wnd, wm->find_window_for_task(test_id));

  wm->erase_task(test_id);
  EXPECT_EQ(nullptr, wm->find_window_for_task(test_id));
}

bool InList(SDL_Event event, vector<SDL_Event>& eventList) {
  auto manager_param = (platform::manager_window_param*)(event.user.data1);
  for (auto ele = eventList.begin(); ele != eventList.end(); ) {
    auto manager_param2 = (platform::manager_window_param*)(ele->user.data1);
    if (event.user.code == ele->user.code &&
          manager_param && manager_param2 &&
          manager_param->taskId == manager_param2->taskId &&
          manager_param->rect == manager_param2->rect &&
          manager_param->title == manager_param2->title) {
      delete manager_param2;
      manager_param2 = nullptr;
      eventList.erase(ele);
      return true;
    }
  }
  return false;
}

void TestUpdateWindow(vector<SDL_Event>& eventList) {
  if (eventList.size() == 0) {
    int nCnt = 10;
    while (nCnt--) {
      SDL_Event event;
      if (SDL_WaitEventTimeout(&event, 100) > 0) {
        if (event.type == user_window_event) {
          EXPECT_TRUE(false);
          return;
        }
      }
    }
  }
  int nCnt = 10;
  while (nCnt-- && eventList.size() > 0) {
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, 100) > 0) {
      if (event.type == user_window_event) {
        EXPECT_TRUE(InList(event, eventList));
        if (event.user.data1) {
          delete (platform::manager_window_param*)(event.user.data1);
          event.user.data1 = nullptr;
        }
      }
    }
  }
  EXPECT_EQ(eventList.size(), 0);
}

TEST(MultiWindowManager, TestUpdateNoWindow) {
  user_window_event = SDL_RegisterEvents(1);

  auto first_window = WindowState{
    Display::Id{1},
    true,
    graphics::Rect{0, 0, 1024, 768},
    "org.anbox.foo",
    Task::Id{1},
    Stack::Id::Default, // not a FreeForm
  };

  auto second_window = WindowState{
    Display::Id{2},
    false, // has no surface
    graphics::Rect{300, 400, 1324, 1168},
    "org.anbox.bar",
    Task::Id{2},
    Stack::Id::Freeform,
  };

  auto third_window = WindowState{
    Display::Id{3},
    false,
    graphics::Rect{0, 0, 0, 0}, // width height equal to 0
    "org.anbox.system",
    Task::Id{3},
    Stack::Id::Freeform,
  };

  auto updated = WindowState::List{first_window, second_window, third_window};
  auto removed = WindowState::List{};

  std::shared_ptr<platform::BasePlatform> platform = std::make_shared<MockPlatform>();
  auto app_db = std::make_shared<application::Database>();
  auto wm = std::make_shared<wm::MultiWindowManager>(platform, nullptr, app_db);

  wm->apply_window_state_update(updated, removed);
  vector<SDL_Event> emptyEventList;
  TestUpdateWindow(emptyEventList);
}

void makeEvent(SDL_Event &event, int userCode, Task::Id id, graphics::Rect rect, std::string title) {
    SDL_memset(&event, 0, sizeof(event));
    event.type = user_window_event;
    event.user.code = userCode;
    event.user.data1 = new(std::nothrow) platform::manager_window_param(id, rect, title);
    event.user.data2 = 0;
}

TEST(MultiWindowManager, TestCreateSingleWindow) {
  user_window_event = SDL_RegisterEvents(1);

  auto first_window = WindowState{
    Display::Id{1},
    true,
    graphics::Rect{0, 0, 1024, 768},
    "org.anbox.foo",
    Task::Id{1},
    Stack::Id::Freeform,
  };

  auto updated = WindowState::List{first_window};
  auto removed = WindowState::List{};

  std::shared_ptr<platform::BasePlatform> platform = std::make_shared<MockPlatform>();
  auto app_db = std::make_shared<application::Database>();
  auto wm = std::make_shared<wm::MultiWindowManager>(platform, nullptr, app_db);

  vector<SDL_Event> singleEventList;
  SDL_Event event;
  makeEvent(event, platform::USER_CREATE_WINDOW, Task::Id{1}, graphics::Rect{0, 0, 1024, 768}, "org.anbox.foo");
  singleEventList.push_back(event);

  wm->apply_window_state_update(updated, removed);
  TestUpdateWindow(singleEventList);
}

TEST(MultiWindowManager, TestDestroyWindow) {
  user_window_event = SDL_RegisterEvents(1);

  auto first_window = WindowState{
    Display::Id{1},
    true,
    graphics::Rect{0, 0, 1024, 768},
    "org.anbox.foo",
    Task::Id{1},
    Stack::Id::Freeform,
  };

  auto updated = WindowState::List{}; // no window in updated so that anbox can destroy first_window
  auto removed = WindowState::List{};

  std::shared_ptr<platform::BasePlatform> platform = std::make_shared<MockPlatform>();
  auto app_db = std::make_shared<application::Database>();
  auto wm = std::make_shared<wm::MultiWindowManager>(platform, nullptr, app_db);
  auto window = platform->create_window(first_window.task(), first_window.frame(), first_window.package_name());
  wm->insert_task(first_window.task(), window);

  vector<SDL_Event> singleEventList;
  SDL_Event event;
  makeEvent(event, platform::USER_DESTROY_WINDOW, Task::Id{1}, graphics::Rect{0, 0, 0, 0}, "");
  singleEventList.push_back(event);

  wm->apply_window_state_update(updated, removed);
  TestUpdateWindow(singleEventList);
}

TEST(MultiWindowManager, TestUpdateWindow) {
  user_window_event = SDL_RegisterEvents(1);

  auto first_window = WindowState{
    Display::Id{1},
    true,
    graphics::Rect{0, 0, 1024, 768},
    "org.anbox.foo",
    Task::Id{1},
    Stack::Id::Freeform,
  };

  auto second_window = WindowState{
    Display::Id{1},
    true,
    graphics::Rect{300, 300, 750, 400},
    "org.anbox.foo",
    Task::Id{1}, // task id is the same as first_window to deal update
    Stack::Id::Freeform,
  };

  auto updated = WindowState::List{second_window}; // no window in updated so that anbox can destroy first_window
  auto removed = WindowState::List{};

  std::shared_ptr<Renderer> renderer;
  std::shared_ptr<platform::BasePlatform> platform = std::make_shared<MockPlatform>();
  auto app_db = std::make_shared<application::Database>();
  auto wm = std::make_shared<wm::MultiWindowManager>(platform, nullptr, app_db);
  auto window = std::make_shared<MockWindow>(first_window.task(), first_window.frame(), first_window.package_name());
  wm->insert_task(first_window.task(), window);
  EXPECT_CALL(*(MockWindow*)window.get(), update_state(_)).Times(1);

  wm->apply_window_state_update(updated, removed);

  vector<SDL_Event> emptyEventList;
  TestUpdateWindow(emptyEventList);
}
}
}
