#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include "Storage.h"

namespace Indra
{

namespace
{
constexpr const char *kVacsHost = "127.0.0.1";
constexpr int         kVacsPort = 9600;
constexpr const char *kVacsPath = "/ws";

class WinsockSession
{
public:
    WinsockSession() : ok_(false)
    {
        WSADATA data;
        ok_ = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }

    ~WinsockSession()
    {
        if (ok_) WSACleanup();
    }

    bool ok() const { return ok_; }

private:
    bool ok_;
};

std::string escapeJsonString(const std::string &value)
{
    return nlohmann::json(value).dump();
}

std::string makeSourceJson(const nlohmann::json &session)
{
    auto clientIt = session.find("clientId");
    if (clientIt == session.end() || !clientIt->is_string() || clientIt->get<std::string>().empty())
        return "{}";

    nlohmann::json source;
    source["clientId"] = clientIt->get<std::string>();
    return source.dump();
}

std::string callIdText(const nlohmann::json &call)
{
    auto callId = call.find("callId");
    if (callId == call.end() || !callId->is_string())
        callId = call.find("id");
    return (callId != call.end() && callId->is_string()) ? callId->get<std::string>() : "";
}

std::string callPartyText(const nlohmann::json &call, const char *partyName)
{
    auto party = call.find(partyName);
    if (party == call.end() || !party->is_object()) return "";

    for (const char *key : {"position", "station", "client", "callsign"})
    {
        auto value = party->find(key);
        if (value != party->end() && value->is_string())
            return value->get<std::string>();
    }
    return "";
}

std::string callTargetText(const nlohmann::json &call)
{
    return callPartyText(call, "target");
}

std::string callSourceText(const nlohmann::json &call)
{
    return callPartyText(call, "source");
}

bool jsonObjectHasCallId(const nlohmann::json &object, const std::string &callId)
{
    if (!object.is_object() || callId.empty()) return false;

    for (const char *key : {"callId", "id", "activeCallId", "currentCallId"})
    {
        auto it = object.find(key);
        if (it != object.end() && it->is_string() && it->get<std::string>() == callId)
            return true;
    }
    return false;
}

bool sessionReferencesCallId(const nlohmann::json &value, const std::string &callId)
{
    if (callId.empty()) return false;

    if (value.is_object())
    {
        if (jsonObjectHasCallId(value, callId)) return true;
        for (auto it = value.begin(); it != value.end(); ++it)
            if (sessionReferencesCallId(it.value(), callId))
                return true;
    }
    else if (value.is_array())
    {
        for (const auto &item : value)
            if (sessionReferencesCallId(item, callId))
                return true;
    }

    return false;
}

bool firstIncomingCallDetails(const nlohmann::json &session,
                              std::string &callId,
                              std::string &source)
{
    auto it = session.find("incomingCalls");
    if (it == session.end() || !it->is_array() || it->empty()) return false;

    const nlohmann::json &call = it->front();
    callId = callIdText(call);
    if (callId.empty()) return false;
    source = callSourceText(call);
    return true;
}

bool outgoingCallDetails(const nlohmann::json &session,
                         std::string &callId,
                         std::string &target)
{
    auto it = session.find("outgoingCall");
    if (it == session.end() || !it->is_object()) return false;

    callId = callIdText(*it);
    if (callId.empty()) return false;
    target = callTargetText(*it);
    return true;
}

bool activeCallDetails(const nlohmann::json &session,
                       std::string &callId,
                       std::string &target,
                       std::string &source)
{
    auto it = session.find("activeCall");
    if (it == session.end() || !it->is_object())
        it = session.find("currentCall");
    if (it == session.end() || !it->is_object())
        it = session.find("connectedCall");
    if (it == session.end() || !it->is_object()) return false;

    callId = callIdText(*it);
    if (callId.empty()) return false;
    target = callTargetText(*it);
    source = callSourceText(*it);
    return true;
}

std::string normalizeCallsign(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(),
        [](unsigned char c) { return std::isspace(c); }), value.end());
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string g_activeDataPosition;

nlohmann::json findPositionNode(const nlohmann::json &root, const std::string &position)
{
    if (!root.is_object() || position.empty()) return nullptr;

    auto exact = root.find(position);
    if (exact != root.end()) return *exact;

    std::string wanted = normalizeCallsign(position);
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        if (normalizeCallsign(it.key()) == wanted)
            return *it;
    }
    return nullptr;
}

std::vector<ViewDef> parseViewsArray(const nlohmann::json &items)
{
    std::vector<ViewDef> views;
    if (!items.is_array()) return views;

    for (const auto &item : items)
    {
        if (!item.is_object()) continue;
        ViewDef v;
        v.button = item.value("button", "");
        v.name = item.value("name", "");
        v.zoomNm = item.value("zoomNm", 120);
        if (item.contains("centerLat") && item.contains("centerLon"))
        {
            v.centerLat = item.value("centerLat", 0.0);
            v.centerLon = item.value("centerLon", 0.0);
            v.hasCenter = true;
        }
        if (!v.button.empty()) views.push_back(v);
    }
    return views;
}

enum class ScopedParseKind
{
    NoData,
    ScopedNoMatch,
    ScopedMatch,
    LegacyRoot
};

struct ViewsParseResult
{
    ScopedParseKind kind = ScopedParseKind::NoData;
    std::vector<ViewDef> views;
};

ViewsParseResult parseViewsJsonText(const std::string &text)
{
    ViewsParseResult result;
    if (text.empty()) return result;

    try
    {
        nlohmann::json root = nlohmann::json::parse(text);
        if (root.is_array())
        {
            result.kind = ScopedParseKind::LegacyRoot;
            result.views = parseViewsArray(root);
            return result;
        }

        nlohmann::json selected = findPositionNode(root, g_activeDataPosition);
        if (selected.is_array())
        {
            result.kind = ScopedParseKind::ScopedMatch;
            result.views = parseViewsArray(selected);
            return result;
        }
        if (selected.is_object())
        {
            auto views = selected.find("views");
            if (views != selected.end())
            {
                result.kind = ScopedParseKind::ScopedMatch;
                result.views = parseViewsArray(*views);
                return result;
            }
        }

        auto legacy = root.find("views");
        if (legacy != root.end())
        {
            result.kind = ScopedParseKind::LegacyRoot;
            result.views = parseViewsArray(*legacy);
            return result;
        }

        if (root.is_object() && !g_activeDataPosition.empty())
            result.kind = ScopedParseKind::ScopedNoMatch;
    }
    catch (const nlohmann::json::exception &)
    {
    }

    return result;
}

std::vector<Contact> parseContactsArray(const nlohmann::json &items)
{
    std::vector<Contact> result;
    if (!items.is_array()) return result;

    for (const auto &item : items)
    {
        if (!item.is_object()) continue;
        auto name = item.find("name");
        auto station = item.find("station");
        if (name == item.end() || station == item.end() ||
            !name->is_string() || !station->is_string())
            continue;

        Contact c;
        c.name = name->get<std::string>();
        c.station = station->get<std::string>();
        if (!c.name.empty() && !c.station.empty())
            result.push_back(c);
    }
    return result;
}

nlohmann::json::const_iterator findJsonKeyCaseInsensitive(const nlohmann::json &object,
                                                          const char *key)
{
    if (!object.is_object() || !key) return object.end();
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        if (_stricmp(it.key().c_str(), key) == 0)
            return it;
    }
    return object.end();
}

struct ContactsParseResult
{
    ScopedParseKind kind = ScopedParseKind::NoData;
    std::vector<Contact> contacts;
};

ContactsParseResult parseContactsJsonText(const std::string &text)
{
    ContactsParseResult result;
    if (text.empty()) return result;

    try
    {
        nlohmann::json root = nlohmann::json::parse(text);
        nlohmann::json selected = findPositionNode(root, g_activeDataPosition);

        // Position value is a bare array of contacts (e.g. "DAAG_APP": [{...}, ...])
        if (selected.is_array())
        {
            result.kind = ScopedParseKind::ScopedMatch;
            result.contacts = parseContactsArray(selected);
            return result;
        }

        if (selected.is_object())
        {
            auto contacts = findJsonKeyCaseInsensitive(selected, "contacts");
            if (contacts != selected.end())
            {
                result.kind = ScopedParseKind::ScopedMatch;
                result.contacts = parseContactsArray(*contacts);
                return result;
            }
        }

        auto contacts = findJsonKeyCaseInsensitive(root, "contacts");
        if (contacts != root.end())
        {
            result.kind = ScopedParseKind::LegacyRoot;
            result.contacts = parseContactsArray(*contacts);
            return result;
        }

        if (root.is_object() && !g_activeDataPosition.empty())
            result.kind = ScopedParseKind::ScopedNoMatch;
    }
    catch (const nlohmann::json::exception &)
    {
    }

    return result;
}

} // namespace

std::string moduleDirectory()
{
    static const std::string dir = []() {
        HMODULE module = nullptr;
        char path[MAX_PATH] = {};
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&moduleDirectory), &module);
        GetModuleFileNameA(module, path, MAX_PATH);
        std::string result(path);
        std::size_t slash = result.find_last_of("\\/");
        return slash == std::string::npos ? std::string(".") : result.substr(0, slash);
    }();
    return dir;
}

std::string dataDirectory()
{
    static const std::string dir = []() {
        std::string moduleDir = moduleDirectory();
        std::size_t slash = moduleDir.find_last_of("\\/");
        std::string leaf = slash == std::string::npos ? moduleDir : moduleDir.substr(slash + 1);
        if (_stricmp(leaf.c_str(), "build") == 0 && slash != std::string::npos)
            return moduleDir.substr(0, slash);
        return moduleDir;
    }();
    return dir;
}

std::string readFile(const std::string &path)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if (!file) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string readDataFile(const std::string &filename)
{
    std::string modulePath = moduleDirectory() + "\\" + filename;
    std::string text = readFile(modulePath);
    if (!text.empty()) return text;

    std::string dataPath = dataDirectory() + "\\" + filename;
    if (_stricmp(modulePath.c_str(), dataPath.c_str()) == 0) return "";
    return readFile(dataPath);
}

bool jsonString(const std::string &obj, const char *key, std::string &out)
{
    std::string token = std::string("\"") + key + "\"";
    std::size_t p = obj.find(token);
    if (p == std::string::npos) return false;
    p = obj.find(':', p);
    if (p == std::string::npos) return false;
    p = obj.find('"', p);
    if (p == std::string::npos) return false;
    std::size_t e = obj.find('"', p + 1);
    if (e == std::string::npos) return false;
    out = obj.substr(p + 1, e - p - 1);
    return true;
}

bool jsonNumber(const std::string &obj, const char *key, double &out)
{
    std::string token = std::string("\"") + key + "\"";
    std::size_t p = obj.find(token);
    if (p == std::string::npos) return false;
    p = obj.find(':', p);
    if (p == std::string::npos) return false;
    ++p;
    while (p < obj.size() && isspace(static_cast<unsigned char>(obj[p]))) ++p;
    char *end = nullptr;
    out = strtod(obj.c_str() + p, &end);
    return end && end != obj.c_str() + p;
}

void setActiveDataPosition(const std::string &position)
{
    g_activeDataPosition = normalizeCallsign(position);
}

std::vector<ViewDef> loadViewsJson(const std::string &filename)
{
    if (g_activeDataPosition.empty())
        return {};

    std::string modulePath = moduleDirectory() + "\\" + filename;
    std::string dataPath = dataDirectory() + "\\" + filename;
    std::string moduleText = readFile(modulePath);
    std::string dataText = (_stricmp(modulePath.c_str(), dataPath.c_str()) == 0)
        ? std::string()
        : readFile(dataPath);

    ViewsParseResult moduleResult = parseViewsJsonText(moduleText);
    ViewsParseResult dataResult = parseViewsJsonText(dataText);

    if (moduleResult.kind == ScopedParseKind::ScopedMatch)
        return moduleResult.views;
    if (dataResult.kind == ScopedParseKind::ScopedMatch)
        return dataResult.views;
    if (moduleResult.kind == ScopedParseKind::ScopedNoMatch ||
        dataResult.kind == ScopedParseKind::ScopedNoMatch)
        return {};
    if (moduleResult.kind == ScopedParseKind::LegacyRoot)
        return moduleResult.views;
    if (dataResult.kind == ScopedParseKind::LegacyRoot)
        return dataResult.views;

    std::string fallbackText = !moduleText.empty() ? moduleText : dataText;
    std::vector<ViewDef> fallbackViews;
    std::size_t p = 0;
    while ((p = fallbackText.find('{', p)) != std::string::npos)
    {
        std::size_t e = fallbackText.find('}', p + 1);
        if (e == std::string::npos) break;
        std::string obj = fallbackText.substr(p, e - p + 1);
        ViewDef v;
        double  n = 0.0;
        jsonString(obj, "button", v.button);
        jsonString(obj, "name",   v.name);
        if (jsonNumber(obj, "centerLat", n)) { v.centerLat = n; v.hasCenter = true; }
        if (jsonNumber(obj, "centerLon", n))   v.centerLon = n;
        if (jsonNumber(obj, "zoomNm",    n))   v.zoomNm    = static_cast<int>(n);
        if (!v.button.empty()) fallbackViews.push_back(v);
        p = e + 1;
    }
    return fallbackViews;
}

void saveViewToJson(const std::string &button, const ViewDef &view)
{
    if (g_activeDataPosition.empty())
        return;

    std::string path  = dataDirectory() + "\\indra_saved_views.json";
    std::string existing = readDataFile("indra_saved_views.json");

    nlohmann::json root;
    try
    {
        root = existing.empty() ? nlohmann::json::object() : nlohmann::json::parse(existing);
    }
    catch (const nlohmann::json::exception &)
    {
        root = nlohmann::json::object();
    }

    bool scoped = !g_activeDataPosition.empty() && !root.is_array();
    nlohmann::json *arrayNode = nullptr;
    if (scoped)
    {
        nlohmann::json &positionNode = root[g_activeDataPosition];
        if (!positionNode.is_object()) positionNode = nlohmann::json::object();
        if (!positionNode["views"].is_array()) positionNode["views"] = nlohmann::json::array();
        arrayNode = &positionNode["views"];
    }

    std::vector<ViewDef> views = scoped ? parseViewsArray(*arrayNode) : loadViewsJson();
    bool found = false;
    for (auto &v : views)
        if (v.button == button) { v = view; found = true; break; }
    if (!found) views.push_back(view);

    if (scoped)
    {
        *arrayNode = nlohmann::json::array();
        for (const auto &v : views)
        {
            nlohmann::json item;
            item["button"] = v.button;
            item["name"] = v.name;
            item["zoomNm"] = v.zoomNm;
            if (v.hasCenter)
            {
                item["centerLat"] = v.centerLat;
                item["centerLon"] = v.centerLon;
            }
            arrayNode->push_back(item);
        }

        std::ofstream file(path.c_str());
        if (file) file << root.dump(2) << "\n";
        return;
    }

    std::ofstream file(path.c_str());
    if (!file) return;
    file << "[\n";
    for (std::size_t i = 0; i < views.size(); ++i)
    {
        const auto &v = views[i];
        file << "{\"button\":\"" << v.button << "\",\"name\":\"" << v.name
             << "\",\"zoomNm\":" << v.zoomNm;
        if (v.hasCenter)
            file << ",\"centerLat\":" << v.centerLat
                 << ",\"centerLon\":" << v.centerLon;
        file << "}";
        if (i + 1 < views.size()) file << ",";
        file << "\n";
    }
    file << "]\n";
}


std::vector<Contact> loadContactsJson(const std::string &filename)
{
    if (g_activeDataPosition.empty())
        return {};

    std::string modulePath = moduleDirectory() + "\\" + filename;
    std::string dataPath = dataDirectory() + "\\" + filename;
    std::string moduleText = readFile(modulePath);
    std::string dataText = (_stricmp(modulePath.c_str(), dataPath.c_str()) == 0)
        ? std::string()
        : readFile(dataPath);

    static std::string cachedFilename;
    static std::string cachedPosition;
    static std::string cachedModuleText;
    static std::string cachedDataText;
    static std::vector<Contact> cachedContacts;
    if (cachedFilename == filename &&
        cachedPosition == g_activeDataPosition &&
        cachedModuleText == moduleText &&
        cachedDataText == dataText)
        return cachedContacts;

    cachedFilename = filename;
    cachedPosition = g_activeDataPosition;
    cachedModuleText = moduleText;
    cachedDataText = dataText;

    ContactsParseResult moduleResult = parseContactsJsonText(moduleText);
    ContactsParseResult dataResult = parseContactsJsonText(dataText);

    if (moduleResult.kind == ScopedParseKind::ScopedMatch)
        cachedContacts = moduleResult.contacts;
    else if (dataResult.kind == ScopedParseKind::ScopedMatch)
        cachedContacts = dataResult.contacts;
    else if (moduleResult.kind == ScopedParseKind::ScopedNoMatch ||
             dataResult.kind == ScopedParseKind::ScopedNoMatch)
        cachedContacts.clear();
    else if (moduleResult.kind == ScopedParseKind::LegacyRoot)
        cachedContacts = moduleResult.contacts;
    else
        cachedContacts = dataResult.contacts;
    return cachedContacts;
}

VacsManager::VacsManager()
    : activeIncomingCall_(false),
      activeCall_(false),
      ringingCall_(false),
      ringingMissingPolls_(0),
      nextRequestId_(1)
{
}

void VacsManager::StartVacsCall(const std::string &station)
{
    std::string target = normalizeCallsign(station);
    if (target.empty()) return;

    nlohmann::json session;
    if (!readSession(session)) return;

    std::string source = makeSourceJson(session);
    if (source == "{}") return;

    std::string args =
        "{\"target\":{\"position\":" + escapeJsonString(target) +
        "},\"source\":" + source +
        ",\"prio\":false}";

    nlohmann::json data;
    if (invoke("signaling_start_call", args, data) && data.is_string())
    {
        activeCallId_ = data.get<std::string>();
        activeTarget_ = target;
        activeIncomingCall_ = false;
        activeCall_ = true;
        ringingCall_ = true;
        ringingMissingPolls_ = 0;
    }
}

bool VacsManager::RefreshCallStatus(VacsCallStatus &status)
{
    nlohmann::json session;
    if (!readSession(session))
    {
        bool hadState = activeCall_ || !activeCallId_.empty() || !incomingCallId_.empty();
        activeCallId_.clear();
        incomingCallId_.clear();
        activeTarget_.clear();
        activeIncomingCall_ = false;
        activeCall_ = false;
        ringingCall_ = false;
        ringingMissingPolls_ = 0;
        status.state = VacsCallUiState::Idle;
        status.callId.clear();
        status.target.clear();
        status.incoming = false;
        return hadState;
    }

    std::string previousIncomingCallId = incomingCallId_;
    std::string incomingSource;
    if (firstIncomingCallDetails(session, incomingCallId_, incomingSource))
    {
        if (!incomingSource.empty())
            activeTarget_ = normalizeCallsign(incomingSource);
        status.state = VacsCallUiState::IncomingRinging;
        status.callId = incomingCallId_;
        status.target = activeTarget_;
        status.incoming = true;
        activeCall_ = true;
        ringingCall_ = true;
        ringingMissingPolls_ = 0;
        return true;
    }
    incomingCallId_.clear();

    std::string outgoingId;
    std::string outgoingTarget;
    if (outgoingCallDetails(session, outgoingId, outgoingTarget))
    {
        activeCallId_ = outgoingId;
        if (!outgoingTarget.empty())
            activeTarget_ = normalizeCallsign(outgoingTarget);
        activeIncomingCall_ = false;
        activeCall_ = true;
        ringingCall_ = true;
        ringingMissingPolls_ = 0;
        status.state = VacsCallUiState::OutgoingRinging;
        status.callId = activeCallId_;
        status.target = activeTarget_;
        status.incoming = false;
        return true;
    }

    std::string activeId;
    std::string activeTarget;
    std::string activeSource;
    if (activeCallDetails(session, activeId, activeTarget, activeSource))
    {
        bool knewActiveCall = activeCall_;
        activeCallId_ = activeId;
        if (!previousIncomingCallId.empty() && previousIncomingCallId == activeId)
            activeIncomingCall_ = true;
        std::string remote = activeIncomingCall_ ? activeSource : activeTarget;
        if (!remote.empty() && (!knewActiveCall || activeTarget_.empty()))
            activeTarget_ = normalizeCallsign(remote);
        activeCall_ = true;
        ringingCall_ = false;
        ringingMissingPolls_ = 0;
        status.state = VacsCallUiState::Active;
        status.callId = activeCallId_;
        status.target = activeTarget_;
        status.incoming = activeIncomingCall_;
        return true;
    }

    if (activeCall_ && sessionReferencesCallId(session, activeCallId_))
    {
        ringingCall_ = false;
        ringingMissingPolls_ = 0;
        status.state = VacsCallUiState::Active;
        status.callId = activeCallId_;
        status.target = activeTarget_;
        status.incoming = activeIncomingCall_;
        return true;
    }

    if (activeCall_ && ringingCall_ && !activeCallId_.empty())
    {
        ++ringingMissingPolls_;
        if (ringingMissingPolls_ < 2)
        {
            status.state = activeIncomingCall_
                ? VacsCallUiState::IncomingRinging
                : VacsCallUiState::OutgoingRinging;
        }
        else
        {
            ringingCall_ = false;
            status.state = VacsCallUiState::Active;
        }
        status.callId = activeCallId_;
        status.target = activeTarget_;
        status.incoming = activeIncomingCall_;
        return true;
    }

    if (activeCall_ && !activeCallId_.empty())
    {
        status.state = VacsCallUiState::Active;
        status.callId = activeCallId_;
        status.target = activeTarget_;
        status.incoming = activeIncomingCall_;
        return true;
    }

    activeCallId_.clear();
    incomingCallId_.clear();
    activeTarget_.clear();
    activeIncomingCall_ = false;
    activeCall_ = false;
    ringingCall_ = false;
    ringingMissingPolls_ = 0;
    status.state = VacsCallUiState::Idle;
    status.callId.clear();
    status.target.clear();
    status.incoming = false;
    return true;
}

bool VacsManager::AcceptIncomingCall()
{
    VacsCallStatus status;
    RefreshCallStatus(status);
    if (incomingCallId_.empty()) return false;

    std::string args = "{\"callId\":" + escapeJsonString(incomingCallId_) + "}";
    nlohmann::json data;
    if (!invoke("signaling_accept_call", args, data)) return false;

    activeCallId_ = incomingCallId_;
    incomingCallId_.clear();
    activeIncomingCall_ = true;
    activeCall_ = true;
    ringingCall_ = false;
    ringingMissingPolls_ = 0;
    return true;
}

bool VacsManager::EndCurrentCall()
{
    if (activeCallId_.empty()) return false;

    std::string args = "{\"callId\":" + escapeJsonString(activeCallId_) + "}";
    nlohmann::json data;
    bool ok = invoke("signaling_end_call", args, data);
    activeCallId_.clear();
    incomingCallId_.clear();
    activeTarget_.clear();
    activeIncomingCall_ = false;
    activeCall_ = false;
    ringingCall_ = false;
    ringingMissingPolls_ = 0;
    return ok;
}

bool VacsManager::readSession(nlohmann::json &session)
{
    return invoke("remote_get_session_state", "{}", session) && session.is_object();
}

bool VacsManager::invoke(const std::string &command,
                         const std::string &args,
                         nlohmann::json &data)
{
    static WinsockSession winsock;
    if (!winsock.ok()) return false;

    SOCKET sock = connectRemote();
    if (sock == INVALID_SOCKET) return false;

    bool ok = sendRemoteCommand(sock, command, args, data);
    closesocket(sock);
    return ok;
}

SOCKET VacsManager::connectRemote()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;

    // SO_SNDTIMEO/SO_RCVTIMEO do NOT apply to connect() on Windows.
    // Set non-blocking, use select() with a 50ms timeout for connect,
    // then restore blocking so send/recv timeouts work as before.
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(kVacsPort));
    inet_pton(AF_INET, kVacsHost, &addr.sin_addr);

    connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    // Wait up to 50ms for the connection to complete
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    timeval tv = { 0, 50 * 1000 }; // 50ms
    if (select(0, nullptr, &wfds, nullptr, &tv) != 1)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // Check whether connect actually succeeded
    int err = 0;
    int errLen = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &errLen);
    if (err != 0)
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    // Restore blocking mode and apply send/recv timeouts
    u_long blocking = 0;
    ioctlsocket(sock, FIONBIO, &blocking);

    DWORD timeout = 50;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    char req[512];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s:%d\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n",
        kVacsPath, kVacsHost, kVacsPort);
    send(sock, req, static_cast<int>(strlen(req)), 0);

    char resp[512] = {};
    recv(sock, resp, sizeof(resp) - 1, 0);
    if (!strstr(resp, "101"))
    {
        closesocket(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

bool VacsManager::sendRemoteCommand(SOCKET sock,
                                    const std::string &command,
                                    const std::string &args,
                                    nlohmann::json &data)
{
    std::string id = nextId();
    std::string payload =
        "{\"type\":\"invoke\",\"id\":" + escapeJsonString(id) +
        ",\"cmd\":" + escapeJsonString(command) +
        ",\"args\":" + args + "}";

    if (!sendTextFrame(sock, payload)) return false;

    std::string responseText;
    if (!readTextFrame(sock, responseText)) return false;

    try
    {
        nlohmann::json response = nlohmann::json::parse(responseText);
        if (response.value("type", "") != "response" ||
            response.value("id", "") != id ||
            !response.value("ok", false))
            return false;

        auto dataIt = response.find("data");
        data = (dataIt == response.end()) ? nlohmann::json() : *dataIt;
        return true;
    }
    catch (const nlohmann::json::exception &)
    {
        return false;
    }
}

bool VacsManager::sendTextFrame(SOCKET sock, const std::string &payload)
{
    size_t len = payload.size();

    std::vector<unsigned char> frame;
    frame.push_back(0x81);
    if (len <= 125)
    {
        frame.push_back(static_cast<unsigned char>(0x80 | len));
    }
    else if (len <= 65535)
    {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(len & 0xFF));
    }
    else
    {
        return false;
    }

    unsigned char mask[4] = { 0xAB, 0xCD, 0xEF, 0x01 };
    for (auto m : mask) frame.push_back(m);
    for (size_t i = 0; i < len; ++i)
        frame.push_back(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]);

    int sent = send(sock, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()), 0);
    return sent == static_cast<int>(frame.size());
}

bool VacsManager::readTextFrame(SOCKET sock, std::string &payload)
{
    unsigned char hdr[2] = {};
    if (recv(sock, reinterpret_cast<char*>(hdr), 2, MSG_WAITALL) != 2) return false;

    int opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    unsigned long long len = hdr[1] & 0x7F;

    if (len == 126)
    {
        unsigned char ext[2] = {};
        if (recv(sock, reinterpret_cast<char*>(ext), 2, MSG_WAITALL) != 2) return false;
        len = (static_cast<unsigned long long>(ext[0]) << 8) | ext[1];
    }
    else if (len == 127)
    {
        return false;
    }

    unsigned char mask[4] = {};
    if (masked && recv(sock, reinterpret_cast<char*>(mask), 4, MSG_WAITALL) != 4)
        return false;

    if (len > 65535) return false;
    std::vector<char> buf(static_cast<size_t>(len));
    if (len > 0 &&
        recv(sock, buf.data(), static_cast<int>(buf.size()), MSG_WAITALL) != static_cast<int>(buf.size()))
        return false;

    if (masked)
    {
        for (size_t i = 0; i < buf.size(); ++i)
            buf[i] = static_cast<char>(static_cast<unsigned char>(buf[i]) ^ mask[i % 4]);
    }

    if (opcode != 0x1) return false;
    payload.assign(buf.begin(), buf.end());
    return true;
}

std::string VacsManager::nextId()
{
    return "indra-" + std::to_string(nextRequestId_++);
}

void vacsCall(const std::string &station)
{
    static VacsManager manager;
    manager.StartVacsCall(station);
}

} // namespace Indra
