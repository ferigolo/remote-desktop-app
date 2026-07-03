#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusObjectPath>

struct ScreencastResult
{
  bool success = false;
  uint32_t node_id = 0;
  int fd = -1;
};

class ScreencastPortal : public QObject
{
  Q_OBJECT

public:
  explicit ScreencastPortal(QObject *parent = nullptr);
  bool negotiateScreencast();
  bool openPipeWireRemote();
  const ScreencastResult& getResult() const { return finalResult; }

private slots:
  // Slot that receives the Response signal from the Portal
  void onPortalResponse(uint responseCode, const QVariantMap &results);

private:
  QString sessionPath;

  // State variables for waiting on responses
  bool waitingForResponse;
  uint lastResponseCode;
  QVariantMap lastResults;
  ScreencastResult finalResult{};

  // Helper to send a method call and wait for its associated Response signal
  bool callAndAwaitResponse(const QString &method, const QVariantMap &options);

  // Negotiates the entire Screencast flow and returns the PipeWire Node ID
  void createSession();
  void selectSources();
};
