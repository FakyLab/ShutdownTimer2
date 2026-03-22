// shutdowntimer-helper.cpp
// Privileged D-Bus system service for ShutdownTimer.
//
// Runs as root via D-Bus system bus activation. The unprivileged GUI app
// calls methods on this service via D-Bus. Each method checks PolicyKit
// authorization before performing any privileged action.
//
// D-Bus name:  org.fakylab.ShutdownTimerHelper
// Object path: /org/fakylab/ShutdownTimerHelper
//
// Build:
//   g++ -std=c++17 shutdowntimer-helper.cpp \
//       $(pkg-config --cflags --libs gio-2.0 polkit-gobject-1) \
//       -o shutdowntimer-helper

#include <gio/gio.h>
#include <polkit/polkit.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// -- Constants ----------------------------------------------------------------

static const char kBusName[]    = "org.fakylab.ShutdownTimerHelper";
static const char kObjectPath[] = "/org/fakylab/ShutdownTimerHelper";

static const char kActionWrite[] = "org.fakylab.shutdowntimer.write-message";
static const char kActionClear[] = "org.fakylab.shutdowntimer.clear-message";

static const char kIssuePath[]    = "/etc/issue";
static const char kSddmConf[]     = "/etc/sddm.conf.d/shutdown-timer-msg.conf";
static const char kLightDMConf[]  = "/etc/lightdm/lightdm.conf.d/shutdown-timer-msg.conf";

// Sentinel markers that bracket our block inside /etc/issue.
// Must not contain regex characters — plain string match only.
static const char kIssueBegin[] = "# --- ShutdownTimer message begin ---";
static const char kIssueEnd[]   = "# --- ShutdownTimer message end ---";

// -- PolicyKit ----------------------------------------------------------------

static bool checkAuthorization(const char* actionId,
                                GDBusMethodInvocation* invocation)
{
    GError* err = nullptr;
    PolkitAuthority* authority = polkit_authority_get_sync(nullptr, &err);
    if (!authority) {
        fprintf(stderr, "helper: polkit_authority_get_sync: %s\n",
                err ? err->message : "unknown");
        if (err) g_error_free(err);
        return false;
    }

    const char* sender = g_dbus_method_invocation_get_sender(invocation);
    PolkitSubject* subject = polkit_system_bus_name_new(sender);

    PolkitAuthorizationResult* result =
        polkit_authority_check_authorization_sync(
            authority, subject, actionId,
            nullptr,
            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
            nullptr, &err);

    bool authorized = (result != nullptr) &&
                       polkit_authorization_result_get_is_authorized(result);

    if (result)    g_object_unref(result);
    if (subject)   g_object_unref(subject);
    if (authority) g_object_unref(authority);
    if (err)       g_error_free(err);

    return authorized;
}

// -- File helpers -------------------------------------------------------------

static bool makeDirs(const std::string& filePath)
{
    size_t slash = filePath.rfind('/');
    if (slash == std::string::npos) return true;
    std::string dir = filePath.substr(0, slash);
    // Create each component
    for (size_t i = 1; i <= dir.size(); ++i) {
        if (i == dir.size() || dir[i] == '/') {
            std::string part = dir.substr(0, i);
            mkdir(part.c_str(), 0755); // ignore EEXIST
        }
    }
    return true;
}

static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool writeFile(const std::string& path, const std::string& content)
{
    makeDirs(path);
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f << content;
    return f.good();
}

static bool removeFile(const std::string& path)
{
    return (unlink(path.c_str()) == 0) || (errno == ENOENT);
}

// -- /etc/issue block management ---------------------------------------------

// Removes any existing ShutdownTimer block from content and returns the result.
static std::string stripBlock(const std::string& content)
{
    size_t b = content.find(kIssueBegin);
    if (b == std::string::npos) return content;

    size_t e = content.find(kIssueEnd, b);
    if (e == std::string::npos) return content;

    // Advance past the end marker and any trailing newline
    size_t endPos = e + strlen(kIssueEnd);
    if (endPos < content.size() && content[endPos] == '\n')
        ++endPos;

    std::string result = content.substr(0, b) + content.substr(endPos);

    // Trim trailing whitespace/newlines left behind
    size_t last = result.find_last_not_of(" \t\r\n");
    if (last != std::string::npos)
        result = result.substr(0, last + 1) + "\n";
    else
        result.clear();

    return result;
}

static bool writeEtcIssue(const std::string& title, const std::string& body)
{
    std::string existing = stripBlock(readFile(kIssuePath));

    std::string block = existing;
    if (!block.empty() && block.back() != '\n')
        block += '\n';
    block += '\n';
    block += kIssueBegin;
    block += '\n';
    if (!title.empty()) block += title + '\n';
    if (!body.empty())  block += body  + '\n';
    block += kIssueEnd;
    block += '\n';

    return writeFile(kIssuePath, block);
}

static bool clearEtcIssue()
{
    std::string content = readFile(kIssuePath);
    return writeFile(kIssuePath, stripBlock(content));
}

static void readEtcIssue(std::string& title, std::string& body)
{
    title.clear();
    body.clear();

    std::string content = readFile(kIssuePath);
    size_t b = content.find(kIssueBegin);
    if (b == std::string::npos) return;

    size_t lineEnd = content.find('\n', b);
    if (lineEnd == std::string::npos) return;
    size_t blockStart = lineEnd + 1;

    size_t e = content.find(kIssueEnd, blockStart);
    if (e == std::string::npos) return;

    std::string block = content.substr(blockStart, e - blockStart);

    // First line = title, rest = body
    size_t nl = block.find('\n');
    if (nl != std::string::npos) {
        title = block.substr(0, nl);
        body  = block.substr(nl + 1);
        // Trim trailing newline from body
        if (!body.empty() && body.back() == '\n')
            body.pop_back();
    } else {
        title = block;
    }
}

// -- DM-specific config writes -----------------------------------------------

static bool writeSDDM(const std::string& title, const std::string& body)
{
    std::string msg = title.empty() ? body
                    : (body.empty() ? title : title + " \xe2\x80\x94 " + body);
    std::string content =
        "# Written by Shutdown Timer - do not edit manually\n"
        "[General]\n"
        "WelcomeMessage=" + msg + "\n";
    return writeFile(kSddmConf, content);
}

static bool writeLightDM(const std::string& title, const std::string& body)
{
    std::string msg = title.empty() ? body
                    : (body.empty() ? title : title + "\n" + body);
    std::string content =
        "# Written by Shutdown Timer - do not edit manually\n"
        "[greeter]\n"
        "banner-message-enable=true\n"
        "banner-message-text=" + msg + "\n";
    return writeFile(kLightDMConf, content);
}

static bool isSddmRunning()
{
    // SDDM creates a PID file in one of these locations
    return (access("/run/sddm.pid",     F_OK) == 0) ||
           (access("/var/run/sddm.pid", F_OK) == 0) ||
           (access("/run/sddm",         F_OK) == 0);
}

static bool isLightDMRunning()
{
    return (access("/run/lightdm.pid",     F_OK) == 0) ||
           (access("/var/run/lightdm.pid", F_OK) == 0) ||
           (access("/run/lightdm",         F_OK) == 0);
}

// -- D-Bus method handlers ----------------------------------------------------

static void handleWriteMessage(GDBusMethodInvocation* invocation,
                                const char* title, const char* body)
{
    if (!checkAuthorization(kActionWrite, invocation)) {
        g_dbus_method_invocation_return_error_literal(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
            "PolicyKit authorization denied");
        return;
    }

    std::string t(title ? title : "");
    std::string b(body  ? body  : "");

    if (!writeEtcIssue(t, b)) {
        g_dbus_method_invocation_return_error_literal(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
            "Failed to write /etc/issue");
        return;
    }

    // Write to graphical DM if active
    if (isSddmRunning())
        writeSDDM(t, b);
    else if (isLightDMRunning())
        writeLightDM(t, b);

    // Return empty tuple matching the method signature (no out args)
    g_dbus_method_invocation_return_value(invocation, nullptr);
}

static void handleClearMessage(GDBusMethodInvocation* invocation)
{
    if (!checkAuthorization(kActionClear, invocation)) {
        g_dbus_method_invocation_return_error_literal(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
            "PolicyKit authorization denied");
        return;
    }

    clearEtcIssue();
    removeFile(kSddmConf);
    removeFile(kLightDMConf);

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

static void handleReadMessage(GDBusMethodInvocation* invocation)
{
    // Read is unprivileged — no PolicyKit check needed.
    std::string title, body;
    readEtcIssue(title, body);

    // Return (ss) — must match introspection XML exactly.
    g_dbus_method_invocation_return_value(
        invocation,
        g_variant_new("(ss)", title.c_str(), body.c_str()));
}

// -- D-Bus dispatch -----------------------------------------------------------

static void onMethodCall(GDBusConnection*,
                         const char*, const char*, const char*,
                         const char* methodName,
                         GVariant* params,
                         GDBusMethodInvocation* invocation,
                         void*)
{
    if (strcmp(methodName, "WriteMessage") == 0) {
        const char* title = nullptr;
        const char* body  = nullptr;
        g_variant_get(params, "(&s&s)", &title, &body);
        handleWriteMessage(invocation, title, body);

    } else if (strcmp(methodName, "ClearMessage") == 0) {
        handleClearMessage(invocation);

    } else if (strcmp(methodName, "ReadMessage") == 0) {
        handleReadMessage(invocation);

    } else {
        g_dbus_method_invocation_return_error(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD,
            "Unknown method: %s", methodName);
    }
}

static const GDBusInterfaceVTable kVTable = {
    onMethodCall, nullptr, nullptr, { nullptr }
};

// -- Introspection XML --------------------------------------------------------

static const char kIntrospectionXML[] =
    "<node>"
    "  <interface name='org.fakylab.ShutdownTimerHelper'>"
    "    <method name='WriteMessage'>"
    "      <arg type='s' name='title' direction='in'/>"
    "      <arg type='s' name='body'  direction='in'/>"
    "    </method>"
    "    <method name='ClearMessage'/>"
    "    <method name='ReadMessage'>"
    "      <arg type='s' name='title' direction='out'/>"
    "      <arg type='s' name='body'  direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

// -- Bus callbacks ------------------------------------------------------------

static void onBusAcquired(GDBusConnection* conn, const char*, void*)
{
    GError* err = nullptr;
    GDBusNodeInfo* info = g_dbus_node_info_new_for_xml(kIntrospectionXML, &err);
    if (!info) {
        fprintf(stderr, "helper: failed to parse introspection XML: %s\n",
                err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }

    g_dbus_connection_register_object(
        conn, kObjectPath,
        info->interfaces[0],
        &kVTable, nullptr, nullptr, &err);

    g_dbus_node_info_unref(info);

    if (err) {
        fprintf(stderr, "helper: register_object failed: %s\n", err->message);
        g_error_free(err);
    }
}

static void onNameLost(GDBusConnection*, const char* name, void* loop)
{
    fprintf(stderr, "helper: lost bus name %s — exiting\n", name);
    g_main_loop_quit(static_cast<GMainLoop*>(loop));
}

// -- Main ---------------------------------------------------------------------

int main()
{
    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);

    g_bus_own_name(
        G_BUS_TYPE_SYSTEM,
        kBusName,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        onBusAcquired,
        nullptr,          // name_acquired callback (not needed)
        onNameLost,
        loop,
        nullptr);         // user_data destroy notify

    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return 0;
}
