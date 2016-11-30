#include "vk_application.h"
using namespace cdsvk;

#include <cstdio>

class TestApplication : public cdsvk::Application {
public:
  explicit TestApplication(Application::CreateInfo &ci) :
      Application(ci) {
    printf("Init'd!\n");
  }
  virtual ~TestApplication() {

  }

  TestApplication(const TestApplication&) = delete;
  const TestApplication& operator=(const TestApplication&) = delete;

  virtual void update(double /*dt*/) override {

  }
  virtual void render() override {

  }

private:
};

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  Application::CreateInfo app_ci = {};

  TestApplication app(app_ci);
  int run_error = app.run();

  return run_error;
}