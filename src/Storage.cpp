#include "Storage.h"

namespace Indra
{

std::string moduleDirectory()
{
    HMODULE module = nullptr;
    char path[MAX_PATH] = {};
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&moduleDirectory), &module);
    GetModuleFileNameA(module, path, MAX_PATH);
    std::string result(path);
    std::size_t slash = result.find_last_of("\\/");
    return slash == std::string::npos ? "." : result.substr(0, slash);
}

std::string dataDirectory()
{
    std::string dir = moduleDirectory();
    std::size_t slash = dir.find_last_of("\\/");
    std::string leaf = slash == std::string::npos ? dir : dir.substr(slash + 1);
    if (_stricmp(leaf.c_str(), "build") == 0 && slash != std::string::npos)
        return dir.substr(0, slash);
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

std::map<std::string, std::string>              g_metars;
std::vector<std::string>                        g_messages;
std::map<std::string, std::vector<ChatMessage>> g_chatHistory;

bool g_newMessageArrived = false;

void rememberMessage(const std::string &value)
{
    g_messages.push_back(value);
    if (g_messages.size() > 30)
        g_messages.erase(g_messages.begin());
}

void addChatMessage(const std::string &sender,
                    const std::string &receiver,
                    const std::string &msg,
                    bool fromMe)
{
    ChatMessage cm;
    cm.sender   = sender;
    cm.receiver = receiver;
    cm.message  = msg;
    cm.fromMe   = fromMe;
    char timeBuf[20];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", st.wHour, st.wMinute);
    cm.time = timeBuf;
    std::string contactKey = fromMe ? receiver : sender;
    g_chatHistory[contactKey].push_back(cm);
    if (!fromMe) g_newMessageArrived = true;
}

std::vector<ViewDef> loadViewsJson(const std::string &filename)
{
    std::vector<ViewDef> views;
    std::string json = readFile(dataDirectory() + "\\" + filename);
    std::size_t p = 0;
    while ((p = json.find('{', p)) != std::string::npos)
    {
        std::size_t e = json.find('}', p + 1);
        if (e == std::string::npos) break;
        std::string obj = json.substr(p, e - p + 1);
        ViewDef v;
        double  n = 0.0;
        jsonString(obj, "button", v.button);
        jsonString(obj, "name",   v.name);
        if (jsonNumber(obj, "centerLat", n)) { v.centerLat = n; v.hasCenter = true; }
        if (jsonNumber(obj, "centerLon", n))   v.centerLon = n;
        if (jsonNumber(obj, "zoomNm",    n))   v.zoomNm    = static_cast<int>(n);
        if (!v.button.empty()) views.push_back(v);
        p = e + 1;
    }
    return views;
}

void saveViewToJson(const std::string &button, const ViewDef &view)
{
    std::string          path  = dataDirectory() + "\\indra_saved_views.json";
    std::vector<ViewDef> views = loadViewsJson();
    bool found = false;
    for (auto &v : views)
        if (v.button == button) { v = view; found = true; break; }
    if (!found) views.push_back(view);
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

} // namespace Indra
