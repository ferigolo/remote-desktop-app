#include "ClientApp.hpp"
#include <rtc/rtc.hpp>

int main(int argc, char* argv[]) {
    rtc::InitLogger(rtc::LogLevel::Info);

    std::string url = "ws://127.0.0.1:8080";
    if (argc > 1) {
        url = argv[1];
    }

    ClientApp app;
    if (!app.initialize(url))
        return 1;

    app.run();
    return 0;
}
