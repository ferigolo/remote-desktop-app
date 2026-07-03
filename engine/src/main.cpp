#include "CoreEngine.hpp"
#include <print>
#include <exception>
#include <cstdio>

#include <QCoreApplication>
#include <QtGlobal>
#include <cstdio>

int main(int argc, char *argv[])
{
    // Disable stdout buffering so prints are sent immediately over the IPC pipe to Rust
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // Redirects Qt logs to stdout
    qInstallMessageHandler(
        [](QtMsgType type, const QMessageLogContext &context, const QString &msg)
        {
            (void)context;
            fprintf(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout,
                    "%s\n",
                    msg.toLocal8Bit().constData());
            fflush(stdout);
        });

    // QCoreApplication is required for QEventLoop and QtDBus to function
    QCoreApplication app(argc, argv);

    try
    {
        std::println(" Starting standalone MediaEngine...");

        MediaEngine engine;
        if (!engine.initialize())
        {
            std::println(stderr, "❌ Failed to initialize MediaEngine");
            return 1;
        }

        std::println("✅ Engine terminated gracefully.");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::println(stderr, "❌ Fatal error: {}", e.what());
        return 1;
    }
}
