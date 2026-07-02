#include "LinuxCapturer.hpp"

bool LinuxCapturer::start(std::function<void(const VideoFrame &)> on_frame_received)
{

  std::println("🐧 [LinuxCapturer] Starting Wayland capture process...");

  // TODO: 1. Comunicar com D-Bus para abrir o Pop-up de permissão
  requestScreencastSession();
  // TODO: 2. Conectar ao PipeWire usando o File Descriptor recebido
  // TODO: 3. Iniciar o loop de receção de frames e chamar on_frame_received()

  return true;
}

void LinuxCapturer::stop()
{
  std::println("🐧 [LinuxCapturer] Capture stoped");
}

void LinuxCapturer::handleDBusError(DBusError *error, const char *context)
{
  if (dbus_error_is_set(error))
  {
    std::println(stderr, "❌ DBus error at [{}]: {}: {}", context, error->name, error->message);
    dbus_error_free(error);
  }
}

void LinuxCapturer::requestScreencastSession()
{
  DBusError error;
  dbus_error_init(&error);

  conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (!conn)
  {
    handleDBusError(&error, "Connection");
    return;
  }

  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop",    // Destiny
      "/org/freedesktop/portal/desktop",   // Path
      "org.freedesktop.portal.ScreenCast", // Interface
      "CreateSession"                      // Method
  );

  if (!msg)
  {
    std::println(stderr, "❌ [LinuxCapture] Failed allocating memory for DBus message");
    return;
  }

  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

  // Helper lambda for adding {string: variant<string>} to dict
  auto append_dict_string = [](DBusMessageIter *dict, const char *key, const char *val)
  {
    DBusMessageIter entry, variant;
    dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &val);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(dict, &entry);
  };

  // Apps running outside Flatpak/Snap MUST provide these tokens so the portal can track the Request object
  append_dict_string(&dict_iter, "session_handle_token", "remotedesktop_session");
  append_dict_string(&dict_iter, "handle_token", "remotedesktop_request");

  dbus_message_iter_close_container(&iter, &dict_iter);
  std::println("🐧 [LinuxCapturer] D-Bus: Requesting new ScreenCast session...");
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1,
                                                                 &error);
  dbus_message_unref(msg);

  if (!reply)
  {
    handleDBusError(&error, "CreateSession");
    return;
  }

  const char *sessionHandlePath = nullptr;
  if (dbus_message_get_args(reply, &error,
                            DBUS_TYPE_OBJECT_PATH, &sessionHandlePath,
                            DBUS_TYPE_INVALID))
    std::println("✅ Sucessfully created session at: {}",
                 sessionHandlePath);
  else
    handleDBusError(&error, "Reading response from CreateSession");

  dbus_message_unref(reply);
}

void LinuxCapturer::selectSources()
{
  DBusError error;
  dbus_error_init(&error);

  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop",
      "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.ScreenCast",
      "SelectSources");

  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &sessionHandlePath);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
  dbus_message_iter_close_container(&iter, &dict_iter);

  std::println("🐧 [LinuxCapturer] D-Bus: Requesting SelectSources...");
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  dbus_message_unref(msg);

  if (!reply)
    handleDBusError(&error, "SelectSources");
  else
  {
    std::println("✅ SelectSources finished successfully");
    dbus_message_unref(reply);
  }
}

void LinuxCapturer::startScreencast()
{
  DBusError error;
  dbus_error_init(&error);

  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop",
      "/org/freedesktop/portal/desktop",
      "org.freedesktop.portal.ScreenCast",
      "Start");

  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &sessionHandlePath);
  const char *parent_window = "";
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent_window);

  // Options
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
  dbus_message_iter_close_container(&iter, &dict_iter);

  std::println("🐧 [LinuxCapturer] D-Bus: Sending start command...");

  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  dbus_message_unref(msg);

  if (!reply)
    handleDBusError(&error, "StartScreencast");
  else
  {
    std::println("✅ Start command sent. Waiting user input...");
    dbus_message_unref(reply);
  }
}
