#include "LinuxCapturer.hpp"
#include <string>

bool LinuxCapturer::start(std::function<void(const VideoFrame &)> on_frame_received)
{
  std::println("🐧 [LinuxCapturer] Starting Wayland capture process...");

  // 1. Comunicar com D-Bus para abrir o Pop-up de permissão
  if (!requestScreencastSession())
    return false;

  if (!selectSources())
    return false;

  if (!startScreencast())
    return false;

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

PortalResponse LinuxCapturer::waitForResponse(const std::string &requestPath)
{
  PortalResponse result = {1, "", 0};

  while (true)
  {
    DBusMessage *msg = dbus_connection_pop_message(conn);
    if (!msg)
    {
      dbus_connection_read_write(conn, -1);
      continue;
    }

    if (dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response") &&
        std::string(dbus_message_get_path(msg)) == requestPath)
    {
      DBusMessageIter iter;
      dbus_message_iter_init(msg, &iter);

      if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32)
        dbus_message_iter_get_basic(&iter, &result.response_code);

      if (result.response_code == 0)
      {
        dbus_message_iter_next(&iter);
        if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY)
        {
          DBusMessageIter dict_iter;
          dbus_message_iter_recurse(&iter, &dict_iter);

          while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY)
          {
            DBusMessageIter entry_iter;
            dbus_message_iter_recurse(&dict_iter, &entry_iter);

            const char *key;
            dbus_message_iter_get_basic(&entry_iter, &key);

            DBusMessageIter var_iter;
            dbus_message_iter_next(&entry_iter);
            dbus_message_iter_recurse(&entry_iter, &var_iter);

            int var_type = dbus_message_iter_get_arg_type(&var_iter);
            std::string key_str = key;

            if (key_str == "session_handle" && var_type == DBUS_TYPE_STRING)
            {
              const char *session_handle;
              dbus_message_iter_get_basic(&var_iter, &session_handle);
              result.session_handle = session_handle;
            }
            else if (key_str == "streams" && var_type == DBUS_TYPE_ARRAY)
            {
              DBusMessageIter array_iter;
              dbus_message_iter_recurse(&var_iter, &array_iter);
              if (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRUCT)
              {
                DBusMessageIter struct_iter;
                dbus_message_iter_recurse(&array_iter, &struct_iter);
                if (dbus_message_iter_get_arg_type(&struct_iter) == DBUS_TYPE_UINT32)
                {
                  dbus_message_iter_get_basic(&struct_iter, &result.node_id);
                }
              }
            }
            dbus_message_iter_next(&dict_iter);
          }
        }
      }

      dbus_message_unref(msg);
      break;
    }
    dbus_message_unref(msg);
  }

  return result;
}

bool LinuxCapturer::requestScreencastSession()
{
  DBusError error;
  dbus_error_init(&error);

  conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (!conn)
  {
    handleDBusError(&error, "Connection");
    return false;
  }

  // Get unique D-Bus name (e.g., ":1.123") and format it for the portal path (e.g., "1_123")
  std::string sender = dbus_bus_get_unique_name(conn);
  if (sender.front() == ':')
    sender.erase(0, 1);
  std::replace(sender.begin(), sender.end(), '.', '_');

  // Create a match rule specifically for the application's requests
  const std::string matchRule = "type='signal',interface='org.freedesktop.portal.Request',member = 'Response', path_namespace = '/org/freedesktop/portal/desktop/request/" + sender + "' ";

  dbus_bus_add_match(conn, matchRule.c_str(), nullptr);
  dbus_connection_flush(conn);

  DBusMessage *msg = dbus_message_new_method_call(
      "org.freedesktop.portal.Desktop",    // Destiny
      "/org/freedesktop/portal/desktop",   // Path
      "org.freedesktop.portal.ScreenCast", // Interface
      "CreateSession"                      // Method
  );

  if (!msg)
  {
    std::println(stderr, "❌ [LinuxCapture] Failed allocating memory for DBus message");
    return false;
  }

  DBusMessageIter iter, dict_iter;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

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

  append_dict_string(&dict_iter, "session_handle_token", "remotedesktop_session");
  append_dict_string(&dict_iter, "handle_token", "remotedesktop_request");

  dbus_message_iter_close_container(&iter, &dict_iter);
  std::println("🐧 [LinuxCapturer] D-Bus: Requesting new ScreenCast session...");
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  dbus_message_unref(msg);

  if (!reply)
  {
    handleDBusError(&error, "CreateSession");
    return false;
  }

  char *requestPath = nullptr;
  if (dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &requestPath, DBUS_TYPE_INVALID))
  {
    std::string path(requestPath);
    dbus_message_unref(reply);

    PortalResponse res = waitForResponse(path);
    if (res.response_code == 0 && !res.session_handle.empty())
    {
      this->sessionPath = res.session_handle;
      std::println("✅ Session created at: {}", this->sessionPath);
      return true;
    }
    else
    {
      std::println(stderr, "❌ CreateSession canceled or failed.");
      return false;
    }
  }
  else
  {
    handleDBusError(&error, "Reading response from CreateSession");
    dbus_message_unref(reply);
    return false;
  }
}

bool LinuxCapturer::selectSources()
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
  const char *sp = sessionPath.c_str();
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &sp);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
  dbus_message_iter_close_container(&iter, &dict_iter);

  std::println("🐧 [LinuxCapturer] D-Bus: Requesting SelectSources...");
  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  dbus_message_unref(msg);

  if (!reply)
  {
    handleDBusError(&error, "SelectSources");
    return false;
  }

  char *requestPath = nullptr;
  if (dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &requestPath, DBUS_TYPE_INVALID))
  {
    std::string path = requestPath;
    dbus_message_unref(reply);

    PortalResponse res = waitForResponse(path);
    if (res.response_code == 0)
    {
      std::println("✅ SelectSources finished successfully");
      return true;
    }
    else
    {
      std::println(stderr, "❌ SelectSources canceled or failed.");
      return false;
    }
  }
  else
  {
    handleDBusError(&error, "Reading response from SelectSources");
    dbus_message_unref(reply);
    return false;
  }
}

bool LinuxCapturer::startScreencast()
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
  const char *sp = sessionPath.c_str();
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &sp);
  const char *parent_window = "";
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent_window);

  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);
  dbus_message_iter_close_container(&iter, &dict_iter);

  std::println("🐧 [LinuxCapturer] D-Bus: Sending start command...");

  DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, -1, &error);
  dbus_message_unref(msg);

  if (!reply)
  {
    handleDBusError(&error, "StartScreencast");
    return false;
  }

  char *requestPath = nullptr;
  if (dbus_message_get_args(reply, &error, DBUS_TYPE_OBJECT_PATH, &requestPath, DBUS_TYPE_INVALID))
  {
    std::string path = requestPath;
    dbus_message_unref(reply);

    std::println("✅ Start command sent. Waiting user input...");
    PortalResponse res = waitForResponse(path);
    if (res.response_code == 0)
    {
      std::println("🎉 [LinuxCapturer] User accepted screen sharing");
      std::println("    -> Node ID: {}", res.node_id);
      return true;
    }
    else
    {
      std::println("❌ [LinuxCapturer] User canceled screen sharing or an error occured");
      return false;
    }
  }
  else
  {
    handleDBusError(&error, "Reading response from StartScreencast");
    dbus_message_unref(reply);
    return false;
  }
}
