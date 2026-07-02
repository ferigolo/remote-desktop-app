#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusObjectPath>

struct ScreencastResult
{
  bool success = false;
  uint32_t node_id = 0;
  // We will add the PipeWire file descriptor later when implementing PipeWire
};

class ScreencastPortal : public QObject
{
  Q_OBJECT

public:
  explicit ScreencastPortal(QObject *parent = nullptr);

  // Negotiates the entire Screencast flow and returns the PipeWire Node ID
  ScreencastResult negotiateScreencast();
  void createSession();
  void selectSources();

private slots:
  // Slot that receives the Response signal from the Portal
  void onPortalResponse(uint responseCode, const QVariantMap &results);

private:
  QString m_sessionPath;

  // State variables for waiting on responses
  bool m_waitingForResponse;
  uint m_lastResponseCode;
  QVariantMap m_lastResults;
  ScreencastResult finalResult{};

  // Helper to send a method call and wait for its associated Response signal
  bool callAndAwaitResponse(const QString &method, const QVariantMap &options);
};
