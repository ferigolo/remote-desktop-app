#include "CoreEngine.hpp"
#include <print>
#include <exception>
#include <cstdio>

#include <QCoreApplication>
#include <QtGlobal>
#include <cstdio>

void qtStdoutMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    fprintf(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout, "%s\n", msg.toLocal8Bit().constData());
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    // Disable stdout buffering so prints are sent immediately over the IPC pipe to Rust
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    // Redirects Qt logs to stdout
    qInstallMessageHandler(
        [](QtMsgType type, const QMessageLogContext &context, const QString &msg)
        {
            fprintf(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout,
                    "%s\n",
                    msg.toLocal8Bit().constData());
            fflush(stdout);
        });

    // QCoreApplication is required for QEventLoop and QtDBus to function
    QCoreApplication app(argc, argv);

    try
    {
        std::println(" [Engine] Starting standalone MediaEngine...");

        MediaEngine engine;
        if (!engine.initialize())
        {
            std::println(stderr, "❌ [Engine] Failed to initialize MediaEngine");
            return 1;
        }

        std::println("✅ [Engine] Engine terminated gracefully.");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::println(stderr, "❌ [Engine] Fatal error: {}", e.what());
        return 1;
    }
}
