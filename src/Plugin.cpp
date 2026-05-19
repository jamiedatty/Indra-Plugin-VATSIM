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
                  kPluginName, "1.0.1",
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
        RegisterTagItemFunction("VACS Custom Call", FN_VACS_CUSTOM);
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
        return true;
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
        }
        else if (functionId == FN_SSRF && itemString && *itemString)
        {
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
