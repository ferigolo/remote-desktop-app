#include "ScreencastPortal.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QCoreApplication>
#include <QDebug>
#include <QUuid>
#include <unistd.h>

ScreencastPortal::ScreencastPortal(QObject *parent)
    : QObject(parent), waitingForResponse(false), lastResponseCode(1)
{
}

bool ScreencastPortal::callAndAwaitResponse(const QString &method, const QVariantMap &options)
{
    // Create the method call message
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        method);

    // If it's not CreateSession, we need to pass the session handle as the first argument
    if (method != "CreateSession")
        msg << QVariant::fromValue(QDBusObjectPath(sessionPath));

    // Start requires a parent window string before the options dictionary
    if (method == "Start")
        msg << QString(""); // Empty string for parent window

    msg << options;

    // Send the call asynchronously using QDBusConnection::call
    // This blocks until the method returns a request path
    QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().call(msg);

    if (!reply.isValid())
    {
        qCritical() << "❌ Failed to call" << method << ":" << reply.error().message();
        return false;
    }

    QString requestPath = reply.value().path();
    qInfo() << "🐧 [ScreencastPortal]" << method << "request path:" << requestPath;

    // Connect to the Response signal for this specific request path
    bool connected = QDBusConnection::sessionBus().connect(
        "org.freedesktop.portal.Desktop",
        requestPath,
        "org.freedesktop.portal.Request",
        "Response",
        this,
        SLOT(onPortalResponse(uint, QVariantMap)));

    if (!connected)
    {
        qCritical() << "❌ Failed to connect to Response signal for" << requestPath;
        return false;
    }

    // Block using QEventLoop until the Response signal fires
    waitingForResponse = true;
    lastResponseCode = 1; // 1 means user cancelled by default
    lastResults.clear();

    // Use an event loop to wait without blocking the whole thread
    QEventLoop loop;
    while (waitingForResponse)
    {
        loop.processEvents(QEventLoop::WaitForMoreEvents);
    }

    // Disconnect so we don't leak connections
    QDBusConnection::sessionBus().disconnect(
        "org.freedesktop.portal.Desktop",
        requestPath,
        "org.freedesktop.portal.Request",
        "Response",
        this,
        SLOT(onPortalResponse(uint, QVariantMap)));

    return (lastResponseCode == 0);
}

void ScreencastPortal::onPortalResponse(uint responseCode, const QVariantMap &results)
{
    lastResponseCode = responseCode;
    lastResults = results;
    waitingForResponse = false;
}

void ScreencastPortal::createSession()
{
    // Generate unique tokens for this session
    QString handleToken = "remotedesktop_req_" + QUuid::createUuid().toString(QUuid::Id128);
    QString sessionToken = "remotedesktop_ses_" + QUuid::createUuid().toString(QUuid::Id128);

    // CreateSession
    QVariantMap createOptions;
    createOptions["handle_token"] = handleToken;
    createOptions["session_handle_token"] = sessionToken;

    qInfo() << "🐧 [ScreencastPortal] Requesting CreateSession...";
    if (!callAndAwaitResponse("CreateSession", createOptions))
    {
        qCritical() << "❌ CreateSession canceled or failed";
        return;
    }

    sessionPath = lastResults["session_handle"].toString();
    if (sessionPath.isEmpty())
    {
        qCritical() << "❌ No session handle returned.";
        return;
    }
    qInfo() << "✅ Session created at:" << sessionPath;
}

void ScreencastPortal::selectSources()
{
    QVariantMap selectOptions;
    // We can specify multiple monitor types, cursor modes, etc here later
    // selectOptions["types"] = QVariant::fromValue(uint(1)); // 1 = Monitor, 2 = Window

    qInfo() << "🐧 [ScreencastPortal] Requesting SelectSources...";
    if (!callAndAwaitResponse("SelectSources", selectOptions))
    {
        qCritical() << "❌ SelectSources canceled or failed";
        return;
    }
    qInfo() << "✅ SelectSources successful.";
}

bool ScreencastPortal::negotiateScreencast()
{
    createSession();
    selectSources();

    QVariantMap startOptions;
    qInfo() << "🐧 [ScreencastPortal] Requesting Start (Waiting for user input)...";
    if (!callAndAwaitResponse("Start", startOptions))
    {
        qWarning() << "❌ User canceled or Start failed";
        return false;
    }

    // Parse the node_id from the streams array
    QVariant streamsVar = lastResults["streams"];
    qInfo() << "🐧 [ScreencastPortal] lastResults keys:" << lastResults.keys();
    qInfo() << "🐧 [ScreencastPortal] streamsVar typeName:" << streamsVar.typeName() << "isNull:" << streamsVar.isNull();

    // The response is an array of structs: a(ua{sv})
    // In QtDBus, this often comes out as QDBusArgument
    if (streamsVar.canConvert<QDBusArgument>())
    {
        qInfo() << "🐧 [ScreencastPortal] streamsVar can convert to QDBusArgument";
        const QDBusArgument &arg = streamsVar.value<QDBusArgument>();

        arg.beginArray();
        while (!arg.atEnd())
        {
            arg.beginStructure();
            uint nodeId = 0; // Tells which screen or window the user chose
            QVariantMap streamDetails;
            arg >> nodeId >> streamDetails;
            arg.endStructure();

            finalResult.node_id = nodeId;
            qInfo() << "🎉 [ScreencastPortal] User accepted screen share. Node ID:" << nodeId;
            break; // Just take the first stream for now
        }
        arg.endArray();
    }
    else
    {
        qInfo() << "❌ [ScreencastPortal] streamsVar COULD NOT convert to QDBusArgument";
    }

    return finalResult.node_id != 0;
}

// Requests a File Descriptor to DBus
bool ScreencastPortal::openPipeWireRemote()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        "OpenPipeWireRemote");

    msg << QVariant::fromValue(QDBusObjectPath(sessionPath));
    msg << QVariantMap();

    qInfo() << "🐧 [ScreencastPortal] Requesting PipeWire File Descriptor...";

    QDBusReply<QDBusUnixFileDescriptor> reply = QDBusConnection::sessionBus().call(msg);

    if (!reply.isValid())
    {
        qCritical() << "❌ Failed to OpenPipeWireRemote:" << reply.error().message();
        return false;
    }

    finalResult.fd = dup(reply.value().fileDescriptor());
    qInfo() << "🔑 [ScreencastPortal] Success! Received PipeWire File Descriptor:" << finalResult.fd;
    finalResult.success = (finalResult.node_id != 0 && finalResult.fd != -1);

    return finalResult.success;
}