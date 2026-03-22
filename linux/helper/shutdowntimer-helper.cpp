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
// Methods:
//   WriteMessage(title: s, body: s, dm: s) -> ()
//   ClearMessage()                          -> ()
//   ReadMessage()                           -> (title: s, body: s)
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
static const char kGdmProfile[]   = "/etc/dconf/profile/gdm";
static const char kGdmDbDir[]     = "/etc/dconf/db/gdm.d";
static const char kGdmBannerDb[]  = "/etc/dconf/db/gdm.d/01-banner-message";

// Sentinel markers that bracket our block inside /etc/issue.
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

static bool runCommand(const std::string& cmd)
{
    return system(cmd.c_str()) == 0;
}

// -- /etc/issue block management ---------------------------------------------

static std::string stripBlock(const std::string& content)
{
    size_t b = content.find(kIssueBegin);
    if (b == std::string::npos) return content;

    size_t e = content.find(kIssueEnd, b);
    if (e == std::string::npos) return content;

    size_t endPos = e + strlen(kIssueEnd);
    if (endPos < content.size() && content[endPos] == '\n')
        ++endPos;

    std::string result = content.substr(0, b) + content.substr(endPos);

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

    size_t nl = block.find('\n');
    if (nl != std::string::npos) {
        title = block.substr(0, nl);
        body  = block.substr(nl + 1);
        if (!body.empty() && body.back() == '\n')
            body.pop_back();
    } else {
        title = block;
    }
}

// -- DM detection (systemctl is-active — reliable, no fragile PID files) -----

static bool isServiceActive(const std::string& name)
{
    std::string cmd = "systemctl is-active --quiet " + name + " 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

static std::string detectDM()
{
    if (isServiceActive("gdm") || isServiceActive("gdm3")) return "gdm";
    if (isServiceActive("plasma-login-manager"))             return "sddm"; // PLM = SDDM fork
    if (isServiceActive("sddm"))                             return "sddm";
    if (isServiceActive("lightdm"))                          return "lightdm";
    return "none";
}

// -- DM-specific config writes -----------------------------------------------

static bool writeSDDM(const std::string& title, const std::string& body)
{
    // SDDM WelcomeMessage shows in the greeter header.
    // Combine title and body with em dash (UTF-8: \xe2\x80\x94).
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

static bool writeGDM(const std::string& title, const std::string& body)
{
    // Step 1: ensure /etc/dconf/profile/gdm has system-db:gdm
    makeDirs(kGdmProfile);
    std::string profileContent = readFile(kGdmProfile);
    if (profileContent.find("system-db:gdm") == std::string::npos) {
        std::string newContent = profileContent;
        if (!newContent.empty() && newContent.back() != '\n')
            newContent += '\n';
        newContent += "system-db:gdm\n";
        writeFile(kGdmProfile, newContent);
    }

    // Step 2: write the banner key file
    makeDirs(kGdmBannerDb);
    std::string combined = title.empty() ? body
                         : (body.empty() ? title : title + "\n" + body);

    // Escape single quotes for the dconf value format
    std::string escaped;
    for (char c : combined) {
        if (c == '\'') escaped += "\\'";
        else           escaped += c;
    }

    std::string content =
        "[org/gnome/login-screen]\n"
        "banner-message-enable=true\n"
        "banner-message-text='" + escaped + "'\n";

    if (!writeFile(kGdmBannerDb, content))
        return false;

    // Step 3: apply changes
    runCommand("dconf update");
    return true;
}

static bool clearSDDM()   { return removeFile(kSddmConf); }
static bool clearLightDM(){ return removeFile(kLightDMConf); }
static bool clearGDM()
{
    removeFile(kGdmBannerDb);
    runCommand("dconf update");
    return true;
}

// -- D-Bus method handlers ----------------------------------------------------

static void handleWriteMessage(GDBusMethodInvocation* invocation,
                                const char* title, const char* body,
                                const char* dm)
{
    if (!checkAuthorization(kActionWrite, invocation)) {
        g_dbus_method_invocation_return_error_literal(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED,
            "PolicyKit authorization denied");
        return;
    }

    std::string t(title ? title : "");
    std::string b(body  ? body  : "");

    // Determine DM: use caller-provided hint if given, otherwise auto-detect.
    // Auto-detection runs systemctl calls — caller hint avoids that overhead
    // since the GUI already detected the DM at startup.
    std::string dmStr(dm ? dm : "");
    if (dmStr.empty() || dmStr == "auto")
        dmStr = detectDM();

    if (!writeEtcIssue(t, b)) {
        g_dbus_method_invocation_return_error_literal(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
            "Failed to write /etc/issue");
        return;
    }

    if (dmStr == "sddm")    writeSDDM(t, b);
    else if (dmStr == "lightdm") writeLightDM(t, b);
    else if (dmStr == "gdm")     writeGDM(t, b);
    // "none" or unknown: /etc/issue is the only output, which is already written

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
    clearSDDM();
    clearLightDM();
    clearGDM();

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

static void handleReadMessage(GDBusMethodInvocation* invocation)
{
    // Read is unprivileged — no PolicyKit check needed.
    std::string title, body;
    readEtcIssue(title, body);

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
        const char* dm    = nullptr;
        g_variant_get(params, "(&s&s&s)", &title, &body, &dm);
        handleWriteMessage(invocation, title, body, dm);

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
    "      <arg type='s' name='dm'    direction='in'/>"
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
        nullptr,
        onNameLost,
        loop,
        nullptr);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    return 0;
}
