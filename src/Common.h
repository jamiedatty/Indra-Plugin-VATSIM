#pragma once
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#undef RGB

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "EuroScopePlugIn.h"
#include "TopSkyFunctions.h"

inline COLORREF MakeRGB(int r, int g, int b) {
    return (COLORREF)((b << 16) | (g << 8) | r);
}
#define rgb(r,g,b) MakeRGB(r,g,b)

#undef max
#undef min

namespace Indra
{

constexpr const char *kPluginName    = "Indra APC";
constexpr const char *kDisplayName   = "Indra APC Overlay";
constexpr int         kObjButton     = 0x494E4401;

const COLORREF kColBarBg      = rgb( 55,  58,  55);
const COLORREF kColBtnFace    = rgb( 88,  92,  84);
const COLORREF kColBtnPress   = rgb( 52,  55,  50);
const COLORREF kColBtnHiEdge  = rgb(210, 213, 194);
const COLORREF kColBtnShEdge  = rgb( 38,  40,  36);
const COLORREF kColBtnText    = rgb(210, 213, 194);
const COLORREF kColBarFrame   = rgb(180, 183, 168);
const COLORREF kColPopupBg    = rgb( 55,  58,  55);
const COLORREF kColPopupTitle = rgb( 52,  55,  50);

constexpr int kBarH      = 80;
constexpr int kBtnH      = 56;
constexpr int kBtnMargin = 8;
constexpr int kBtnGap    = 4;

inline int rectW(const RECT &r) { return r.right - r.left; }
inline int rectH(const RECT &r) { return r.bottom - r.top; }

inline bool startsWith(const char *s, const char *prefix)
{
    return s && _strnicmp(s, prefix, strlen(prefix)) == 0;
}

inline const char *safe(const char *s) { return s && *s ? s : "-"; }

enum FunctionId
{
    FN_ARR_FILTER = 3001,
    FN_DEP_FILTER,
    FN_FPL_WINDOW,
    FN_VIEW1,
    FN_VIEW2,
    FN_AREAS,
    FN_RTE_TOGGLE,
    FN_DATBLK,
    FN_DISPLAY_TOGGLE,
    FN_QDM_MODE,
    FN_METEO,
    FN_MTCD_TOGGLE,
    FN_ALARM_TOGGLE,
    FN_SECTORS,
    FN_FINDER,
    FN_SSRF,
    FN_ZOOM_IN,
    FN_ZOOM_OUT,
    FN_PAN_UP,
    FN_PAN_DOWN,
    FN_PAN_LEFT,
    FN_PAN_RIGHT,
    FN_SAVE_VIEW_0,
    FN_SAVE_VIEW_S,
    FN_SAVE_VIEW_12,
    FN_SAVE_VIEW_1,
    FN_SAVE_VIEW_3,
    FN_SAVE_VIEW_5,
    FN_SAVE_VIEW_8,
    FN_LOAD_VIEW_0,
    FN_LOAD_VIEW_S,
    FN_LOAD_VIEW_12,
    FN_LOAD_VIEW_1,
    FN_LOAD_VIEW_3,
    FN_LOAD_VIEW_5,
    FN_LOAD_VIEW_8,
    FN_MESSAGES_CENTER,
    FN_MESSAGES_SEND,
    FN_MESSAGES_NEW_DM,
    FN_ZOOM_NM_EDIT,
    FN_SEP_TOOL
};

struct ViewDef
{
    std::string button;
    std::string name;
    double      centerLat = 0.0;
    double      centerLon = 0.0;
    int         zoomNm    = 120;
    bool        hasCenter = false;
};

struct ChatMessage
{
    std::string sender;
    std::string receiver;
    std::string message;
    std::string time;
    bool        fromMe;
};

extern std::map<std::string, std::string>              g_metars;
extern std::vector<std::string>                        g_messages;
extern std::map<std::string, std::vector<ChatMessage>> g_chatHistory;
extern bool g_newMessageArrived;

void rememberMessage(const std::string &value);
void addChatMessage(const std::string &sender,
                    const std::string &receiver,
                    const std::string &msg,
                    bool fromMe = false);

} // namespace Indra
