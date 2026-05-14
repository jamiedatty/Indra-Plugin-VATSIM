#include "Common.h"
#include "Storage.h"
#include "Screen.h"

namespace Indra
{

class IndraApcPlugin : public EuroScopePlugIn::CPlugIn
{
public:
    IndraApcPlugin()
        : CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
                  kPluginName, "0.1.4",
                  "Maghreb vACC",
                  "GPL v3")
    {
        RegisterDisplayType(kDisplayName, true, true, true, true);

        RegisterTagItemFunction("ARR Filter",      FN_ARR_FILTER);
        RegisterTagItemFunction("DEP Filter",      FN_DEP_FILTER);
        RegisterTagItemFunction("FPL Window",      FN_FPL_WINDOW);
        RegisterTagItemFunction("View 1",          FN_VIEW1);
        RegisterTagItemFunction("View 2",          FN_VIEW2);
        RegisterTagItemFunction("Areas",           FN_AREAS);
        RegisterTagItemFunction("RTE Toggle",      FN_RTE_TOGGLE);
        RegisterTagItemFunction("DATBLK",          FN_DATBLK);
        RegisterTagItemFunction("Display Toggle",  FN_DISPLAY_TOGGLE);
        RegisterTagItemFunction("QDM Mode",        FN_QDM_MODE);
        RegisterTagItemFunction("METEO",           FN_METEO);
        RegisterTagItemFunction("MTCD Toggle",     FN_MTCD_TOGGLE);
        RegisterTagItemFunction("Alarm Toggle",    FN_ALARM_TOGGLE);
        RegisterTagItemFunction("Sectors",         FN_SECTORS);
        RegisterTagItemFunction("Finder",          FN_FINDER);
        RegisterTagItemFunction("SSR F",           FN_SSRF);
        RegisterTagItemFunction("Zoom In",         FN_ZOOM_IN);
        RegisterTagItemFunction("Zoom Out",        FN_ZOOM_OUT);
        RegisterTagItemFunction("Pan Up",          FN_PAN_UP);
        RegisterTagItemFunction("Pan Down",        FN_PAN_DOWN);
        RegisterTagItemFunction("Pan Left",        FN_PAN_LEFT);
        RegisterTagItemFunction("Pan Right",       FN_PAN_RIGHT);
        RegisterTagItemFunction("Save View 0",     FN_SAVE_VIEW_0);
        RegisterTagItemFunction("Save View S",     FN_SAVE_VIEW_S);
        RegisterTagItemFunction("Save View 1/2",   FN_SAVE_VIEW_12);
        RegisterTagItemFunction("Save View 1",     FN_SAVE_VIEW_1);
        RegisterTagItemFunction("Save View 3",     FN_SAVE_VIEW_3);
        RegisterTagItemFunction("Save View 5",     FN_SAVE_VIEW_5);
        RegisterTagItemFunction("Save View 8",     FN_SAVE_VIEW_8);
        RegisterTagItemFunction("Load View 0",     FN_LOAD_VIEW_0);
        RegisterTagItemFunction("Load View S",     FN_LOAD_VIEW_S);
        RegisterTagItemFunction("Load View 1/2",   FN_LOAD_VIEW_12);
        RegisterTagItemFunction("Load View 1",     FN_LOAD_VIEW_1);
        RegisterTagItemFunction("Load View 3",     FN_LOAD_VIEW_3);
        RegisterTagItemFunction("Load View 5",     FN_LOAD_VIEW_5);
        RegisterTagItemFunction("Load View 8",     FN_LOAD_VIEW_8);
        RegisterTagItemFunction("Messages Center", FN_MESSAGES_CENTER);
        RegisterTagItemFunction("Messages Send",   FN_MESSAGES_SEND);
        RegisterTagItemFunction("Messages New DM", FN_MESSAGES_NEW_DM);
    }

    ~IndraApcPlugin() override {}

    EuroScopePlugIn::CRadarScreen *OnRadarScreenCreated(
        const char *displayName, bool needRadarContent,
        bool geoReferenced, bool canBeSaved, bool canBeCreated) override
    {
        (void)displayName; (void)canBeSaved; (void)canBeCreated;
        if (needRadarContent && geoReferenced)
            return CreateIndraApcScreen();
        return nullptr;
    }

    bool OnCompileCommand(const char *cmd) override
    {
        if (!startsWith(cmd, ".indra")) return false;
        DisplayUserMessage(kPluginName, "INDRA",
            "Indra APC Plugin v0.1.5 three-row bottom bar active.",
            true, true, true, false, false);
        return true;
    }

    void OnNewMetarReceived(const char *station, const char *fullMetar) override
    {
        if (station && *station && fullMetar)
            g_metars[station] = fullMetar;
    }

    void OnCompileFrequencyChat(const char *senderCallsign,
                                double frequency,
                                const char *chatMessage) override
    {
        char line[512];
        snprintf(line, sizeof(line), "[%.3f] %s: %s",
                 frequency, safe(senderCallsign), safe(chatMessage));
        rememberMessage(line);
        if (senderCallsign && chatMessage)
        {
            addChatMessage(senderCallsign, "Me", chatMessage, false);
        }
    }

    void OnCompilePrivateChat(const char *senderCallsign,
                              const char *receiverCallsign,
                              const char *chatMessage) override
    {
        char line[512];
        snprintf(line, sizeof(line), "[PRIV] %s->%s: %s",
                 safe(senderCallsign), safe(receiverCallsign), safe(chatMessage));
        rememberMessage(line);
        if (senderCallsign && receiverCallsign && chatMessage)
        {
            EuroScopePlugIn::CController me = ControllerMyself();
            std::string myCallsign = me.IsValid() ? me.GetCallsign() : "";
            bool fromMe = !myCallsign.empty() && _stricmp(senderCallsign, myCallsign.c_str()) == 0;
            addChatMessage(senderCallsign, receiverCallsign, chatMessage, fromMe);
        }
    }

    void OnFunctionCall(int functionId, const char *itemString,
                        POINT pt, RECT area) override
    {
        (void)pt; (void)area;
        if (functionId == FN_DATBLK && itemString && *itemString)
        {
            int n = 0;
            if (sscanf(itemString, "Tag Family %d", &n) == 1)
            {
                char cmd[32];
                snprintf(cmd, sizeof(cmd), ".tagfamily %d", n);
                OnCompileCommand(cmd);
            }
        }
        else if (functionId == FN_FINDER && itemString && *itemString)
        {
            DisplayUserMessage(kPluginName, "INDRA",
                ("Finder: " + std::string(itemString)).c_str(),
                true, true, true, false, false);
        }
        else if (functionId == FN_SSRF && itemString && *itemString)
        {
            DisplayUserMessage(kPluginName, "INDRA",
                ("SSR F: " + std::string(itemString)).c_str(),
                true, true, true, false, false);
        }
    }

private:
    static bool startsWith(const char *s, const char *prefix)
    {
        return s && _strnicmp(s, prefix, strlen(prefix)) == 0;
    }
};

} // namespace Indra

static EuroScopePlugIn::CPlugIn *g_plugin = nullptr;

void __declspec(dllexport) EuroScopePlugInInit(
    EuroScopePlugIn::CPlugIn **ppPlugInInstance)
{
    g_plugin = new Indra::IndraApcPlugin();
    *ppPlugInInstance = g_plugin;
}

void __declspec(dllexport) EuroScopePlugInExit(void)
{
    delete g_plugin;
    g_plugin = nullptr;
}
