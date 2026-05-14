#include "Screen.h"
#include "Storage.h"

namespace Indra
{

class IndraApcScreen : public EuroScopePlugIn::CRadarScreen
{
public:
    IndraApcScreen()
        : EuroScopePlugIn::CRadarScreen(),
          overlayEnabled_(true),
          decentered_(false),
          simulationMode_(false),
          recording_(false),
          mapAirways_(false),
          mapAero_(false),
          mapRestricted_(false),
          mapWeather_(false),
          showAirports_(false),
          showFixes_(false),
          showAirways_(false),
          showSidStar_(false),
          showGeo_(false),
          showAirspace_(false),
          showRegions_(false),
          showFreeText_(false),
          rblEnabled_(false),
          rangeNm_(250),
          centerLat_(0.0),
          centerLon_(0.0),
          hasJsonCenter_(false),
          filter_("OFF"),
          rteEnabled_(false),
          mtcdEnabled_(false),
          alarmEnabled_(true),
          qdmMode_(false),
          qdmFirstClick_(false),
          qdmLineValid_(false),
          currentTagFamily_(0),
          currentZoom_(250),
          searchCircleValid_(false),
          searchCircleRadiusPx_(40),
          searchExpireTick_(0),
          searchLineValid_(false),
          popupVisible_(false),
          messagesPopupVisible_(false),
          m_currentPopupId(-1),
          selectedContact_(""),
          messageInputBuffer_(""),
          lastSentMessage_(""),
          lastSentRecipient_(""),
          lastSentTick_(0),
          lastUsersClickTick_(0),
          pendingConnectionDialog_(false),
          pendingConnectionDialogTick_(0),
          pendingCommandClear_(false),
          pendingCommandClearTick_(0),
          messagesNeedRefresh_(false),
          hasUnreadMessages_(false),
          unreadFlashTick_(0)
    {
        views_     = loadViewsJson();
        popupRect_ = {120, 120, 550, 430};
        messagesPopupRect_ = {120, 120, 550, 430};

        EuroScopePlugIn::CController me = GetPlugIn()->ControllerMyself();
        if (me.IsValid())
        {
            std::string cs = me.GetCallsign();
            std::size_t u  = cs.find('_');
            controllerAirport_ = (u != std::string::npos) ? cs.substr(0, u) : cs;
        }
    }

    void OnAsrContentLoaded(bool loaded) override
    {
        if (loaded)
        {
            loadBool("INDRA_APC_OVERLAY",       overlayEnabled_);
            loadInt ("INDRA_APC_RANGE",         rangeNm_);
            loadBool("INDRA_APC_DECEN",         decentered_);
            loadBool("INDRA_APC_SIM",           simulationMode_);
            loadBool("INDRA_APC_MAP_AIRWAYS",   mapAirways_);
            loadBool("INDRA_APC_MAP_AERO",      mapAero_);
            loadBool("INDRA_APC_MAP_RESTRICTED",mapRestricted_);
            loadBool("INDRA_APC_MAP_WEATHER",   mapWeather_);
            loadBool("INDRA_APC_SHOW_AIRPORTS", showAirports_);
            loadBool("INDRA_APC_SHOW_FIXES",    showFixes_);
            loadBool("INDRA_APC_SHOW_AIRWAYS",  showAirways_);
            loadBool("INDRA_APC_SHOW_SIDSTAR",  showSidStar_);
            loadBool("INDRA_APC_SHOW_GEO",      showGeo_);
            loadBool("INDRA_APC_SHOW_AIRSPACE", showAirspace_);
            loadBool("INDRA_APC_SHOW_REGIONS",  showRegions_);
            loadBool("INDRA_APC_SHOW_FREETEXT", showFreeText_);
            loadBool("INDRA_APC_RBL",           rblEnabled_);
            loadBool("INDRA_APC_RECORDING",     recording_);
            loadBool("INDRA_APC_RTE",           rteEnabled_);
            loadBool("INDRA_APC_MTCD",          mtcdEnabled_);
            loadBool("INDRA_APC_ALARM",         alarmEnabled_);
            loadInt ("INDRA_APC_TAGFAMILY",     currentTagFamily_);
            const char *f = GetDataFromAsr("INDRA_APC_FILTER");
            if (f && *f) filter_ = f;
        }
        currentZoom_ = rangeNm_;
        applySavedDisplaySettings();
    }

    void OnAsrContentToBeSaved()  override { saveSettings(); }
    void OnAsrContentToBeClosed() override
    {
        saveSettings();
        delete this;
    }

    void OnRefresh(HDC hdc, int phase) override
    {
        if (!overlayEnabled_ ||
            phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS) return;

        SetBkMode(hdc, TRANSPARENT);
        processHeldViewButtons();
        expireSearchHighlight();
        processPendingConnectionDialog();
        processPendingCommandClear();

        if (g_newMessageArrived && !messagesPopupVisible_)
        {
            hasUnreadMessages_ = true;
            g_newMessageArrived = false;
            if (unreadFlashTick_ == 0)
                unreadFlashTick_ = GetTickCount();
        }

        drawBottomBar(hdc);
        drawInlinePopup(hdc);
        if (messagesPopupVisible_) drawMessagesPopup(hdc);
        if (searchCircleValid_) drawSearchHighlight(hdc);
        if (qdmLineValid_)      drawQDMLine(hdc);
        if (hasUnreadMessages_) RequestRefresh();
    }

    bool OnCompileCommand(const char *cmd) override
    {
        if (!startsWith(cmd, ".indra")) return false;
        const char *arg = cmd + 6;
        while (*arg == ' ') ++arg;
        if      (_stricmp(arg, "off") == 0) overlayEnabled_ = false;
        else if (_stricmp(arg, "on")  == 0 || *arg == 0) overlayEnabled_ = true;
        else    message(".indra on | off");
        saveSettings();
        return true;
    }

    void OnButtonDownScreenObject(int objType, const char *objId,
                                  POINT pt, RECT area, int button) override
    {
        if (objType != kObjButton || !objId || button != EuroScopePlugIn::BUTTON_LEFT) return;
        std::string s = objId;
        static const std::vector<std::string> viewButtons = {"S","0","1/2","1","3","5","8","VIEW1","VIEW2"};
        bool isView = false;
        for (const auto &vb : viewButtons)
            if (s == vb) { isView = true; break; }
        if (!isView) return;

        pressStartTime_[s] = GetTickCount();
        longPressTriggered_[s] = false;
        CRadarScreen::OnButtonDownScreenObject(objType, objId, pt, area, button);
    }

    void OnButtonUpScreenObject(int objType, const char *objId,
                                POINT pt, RECT area, int button) override
    {
        if (objType != kObjButton || !objId || button != EuroScopePlugIn::BUTTON_LEFT) return;
        std::string s = objId;
        auto it = pressStartTime_.find(s);
        if (it != pressStartTime_.end())
        {
            DWORD duration = GetTickCount() - it->second;
            if (duration >= 5000 && !longPressTriggered_[s])
            {
                longPressTriggered_[s] = true;
                saveCurrentView(s);
            }
            else if (!longPressTriggered_[s])
            {
                applyView(s);
            }
            pressStartTime_.erase(it);
            longPressTriggered_.erase(s);
        }
        CRadarScreen::OnButtonUpScreenObject(objType, objId, pt, area, button);
    }

    void OnClickScreenObject(int objType, const char *objId,
                             POINT pt, RECT area, int button) override
    {
        if (button == EuroScopePlugIn::BUTTON_LEFT && qdmMode_ &&
            objType != kObjButton)
        {
            handleQDMClick(pt);
            return;
        }

        if (objType != kObjButton || !objId) return;
        std::string s = objId;

        static const std::vector<std::string> viewButtons = {"S","0","1/2","1","3","5","8","VIEW1","VIEW2"};
        bool isView = false;
        for (const auto &vb : viewButtons)
            if (s == vb) { isView = true; break; }
        if (isView)
        {
            if (button == EuroScopePlugIn::BUTTON_LEFT &&
                pressStartTime_.find(s) == pressStartTime_.end())
                applyView(s);
            return;
        }

        if (button == EuroScopePlugIn::BUTTON_LEFT)
        {
            if (qdmMode_ && s != "QDM")
            {
                handleQDMClick(pt);
                return;
            }

            if (messagesPopupVisible_ && handleMessagesPopupClick(s, pt, area))
                return;

            if (s == "QDM")    toggleQDMMode();
            else if (s == "FINDER") showFinderPopup(area);
            else if (s == "SSR F")  showSSRFPopup(area);
            else if (s == "CEN")    centerScreen();
            else if (s == "EXP +")  zoomIn();
            else if (s == "EXP -")  zoomOut();
            else if (s == "+")      zoomIn();
            else if (s == "-")      zoomOut();
            else if (s == "^")      pan(0, -160);
            else if (s == "v")      pan(0,  160);
            else if (s == "<")      pan(-160, 0);
            else if (s == ">")      pan( 160, 0);
            else if (s == "MESSAGES")
            {
                toggleMessagesPopup(area);
            }
            else if (s == "POPUP_CLOSE")
            {
                popupVisible_ = false;
                messagesPopupVisible_ = false;
                RequestRefresh();
            }
            else if (s == "MSG_SEND")
            {
                openMessageSendPopup();
            }
            else if (s == "MSG_NEW_DM")
            {
                openNewDmPopup();
            }
            else
            {
                handleButton(s, pt, area);
            }
        }
    }

    void OnMoveScreenObject(int objType, const char *objId,
                            POINT pt, RECT area, bool released) override
    {
        (void)area; (void)released;
        if (objType != kObjButton || !objId) return;
        if (_stricmp(objId, "POPUP_MOVE") == 0)
        {
            int w = rectW(popupRect_), h = rectH(popupRect_);
            popupRect_.left   = pt.x - w / 2;
            popupRect_.top    = pt.y - 10;
            popupRect_.right  = popupRect_.left + w;
            popupRect_.bottom = popupRect_.top  + h;
            clampPopupToRadar(popupRect_);
        }
        else if (_stricmp(objId, "MSG_POPUP_MOVE") == 0)
        {
            int w = rectW(messagesPopupRect_), h = rectH(messagesPopupRect_);
            messagesPopupRect_.left   = pt.x - w / 2;
            messagesPopupRect_.top    = pt.y - 10;
            messagesPopupRect_.right  = messagesPopupRect_.left + w;
            messagesPopupRect_.bottom = messagesPopupRect_.top  + h;
            clampPopupToRadar(messagesPopupRect_);
        }
    }

    void OnFunctionCall(int functionId, const char *itemString,
                        POINT pt, RECT area) override
    {
        (void)pt;
        switch (functionId)
        {
        case FN_ARR_FILTER:   setFilter("ARRIVALS");    break;
        case FN_DEP_FILTER:   setFilter("DEPARTURES");  break;
        case FN_FPL_WINDOW:   openSelectedFlightPlan(pt, area); break;
        case FN_VIEW1:        applyView("VIEW1");        break;
        case FN_VIEW2:        applyView("VIEW2");        break;
        case FN_AREAS:        showDisplaySettingsPicker(area); break;
        case FN_RTE_TOGGLE:
            rteEnabled_ = !rteEnabled_;
            message(rteEnabled_ ? "Route display ON" : "Route display OFF");
            toggleTopSkyRte(pt, area);
            break;
        case FN_DATBLK:
            if (itemString && *itemString) onTagFamilySelected(parseTrailingNumber(itemString));
            break;
        case FN_DISPLAY_TOGGLE:
            if (itemString && *itemString) onDisplayToggle(itemString);
            break;
        case FN_METEO:        showMetPopup(area);       break;
        case FN_MTCD_TOGGLE:
            mtcdEnabled_ = !mtcdEnabled_;
            GetPlugIn()->OnCompileCommand(mtcdEnabled_ ? ".mtcd on" : ".mtcd off");
            message(mtcdEnabled_ ? "MTCD ON" : "MTCD OFF");
            break;
        case FN_ALARM_TOGGLE:
            alarmEnabled_ = !alarmEnabled_;
            message(alarmEnabled_ ? "Alarm sounds ON" : "Alarm sounds OFF");
            break;
        case FN_SECTORS:      showDisplaySettingsPicker(area); break;
        case FN_FINDER:
            if (itemString && *itemString) onFinder(itemString);
            break;
        case FN_SSRF:
            if (itemString && *itemString) onSSRF(itemString);
            break;
        case FN_ZOOM_IN:      zoomIn();                break;
        case FN_ZOOM_OUT:     zoomOut();               break;
        case FN_PAN_UP:       pan(0, -50);             break;
        case FN_PAN_DOWN:     pan(0,  50);             break;
        case FN_PAN_LEFT:     pan(-50, 0);             break;
        case FN_PAN_RIGHT:    pan( 50, 0);             break;
        case FN_SAVE_VIEW_0:  saveCurrentView("0");    break;
        case FN_SAVE_VIEW_S:  saveCurrentView("S");    break;
        case FN_SAVE_VIEW_12: saveCurrentView("1/2");  break;
        case FN_SAVE_VIEW_1:  saveCurrentView("1");    break;
        case FN_SAVE_VIEW_3:  saveCurrentView("3");    break;
        case FN_SAVE_VIEW_5:  saveCurrentView("5");    break;
        case FN_SAVE_VIEW_8:  saveCurrentView("8");    break;
        case FN_LOAD_VIEW_0:  applyView("0");          break;
        case FN_LOAD_VIEW_S:  applyView("S");          break;
        case FN_LOAD_VIEW_12: applyView("1/2");        break;
        case FN_LOAD_VIEW_1:  applyView("1");          break;
        case FN_LOAD_VIEW_3:  applyView("3");          break;
        case FN_LOAD_VIEW_5:  applyView("5");          break;
        case FN_LOAD_VIEW_8:  applyView("8");          break;
        case FN_MESSAGES_SEND:
            if (itemString && *itemString)
            {
                if (selectedContact_.empty())
                {
                    message("No message recipient selected.");
                    break;
                }
                std::string myCallsign = GetPlugIn()->ControllerMyself().GetCallsign();
                std::string text = itemString;
                text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
                text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                if (isDuplicateSend(selectedContact_, text))
                    break;
                lastSentRecipient_ = selectedContact_;
                lastSentMessage_ = text;
                lastSentTick_ = GetTickCount();
                char cmd[512];
                snprintf(cmd, sizeof(cmd), ".msg %s %s", selectedContact_.c_str(), text.c_str());
                sendEuroScopeCommand(cmd);
                messagesNeedRefresh_ = true;
                RequestRefresh();
            }
            break;
        case FN_MESSAGES_NEW_DM:
            if (itemString && *itemString)
            {
                selectedContact_ = itemString;
                g_chatHistory[selectedContact_];
                messagesNeedRefresh_ = true;
                RequestRefresh();
            }
            break;
        default: break;
        }
        saveSettings();
    }

protected:
    bool        overlayEnabled_;
    bool        decentered_;
    bool        simulationMode_;
    bool        recording_;
    bool        mapAirways_;
    bool        mapAero_;
    bool        mapRestricted_;
    bool        mapWeather_;
    bool        showAirports_;
    bool        showFixes_;
    bool        showAirways_;
    bool        showSidStar_;
    bool        showGeo_;
    bool        showAirspace_;
    bool        showRegions_;
    bool        showFreeText_;
    bool        rblEnabled_;
    int         rangeNm_;
    double      centerLat_;
    double      centerLon_;
    bool        hasJsonCenter_;
    std::string filter_;
    std::vector<ViewDef> views_;
    int         m_currentPopupId;
    bool        popupVisible_;
    RECT        popupRect_;
    std::string popupTitle_;
    std::string popupContent_;
    bool        rteEnabled_;
    bool        mtcdEnabled_;
    bool        alarmEnabled_;
    bool        qdmMode_;
    bool        qdmFirstClick_;
    bool        qdmLineValid_;
    EuroScopePlugIn::CPosition qdmPoint1_, qdmPoint2_;
    bool        searchCircleValid_;
    EuroScopePlugIn::CPosition searchCirclePos_;
    int         searchCircleRadiusPx_;
    DWORD       searchExpireTick_;
    bool        searchLineValid_;
    EuroScopePlugIn::CPosition searchLineFromPos_;
    int         currentTagFamily_;
    int         currentZoom_;
    std::string controllerAirport_;

    bool        messagesPopupVisible_;
    RECT        messagesPopupRect_;
    std::string selectedContact_;
    std::string messageInputBuffer_;
    std::string lastSentMessage_;
    std::string lastSentRecipient_;
    DWORD       lastSentTick_;
    DWORD       lastUsersClickTick_;
    bool        pendingConnectionDialog_;
    DWORD       pendingConnectionDialogTick_;
    bool        pendingCommandClear_;
    DWORD       pendingCommandClearTick_;
    bool        messagesNeedRefresh_;

    std::map<std::string, DWORD> pressStartTime_;
    std::map<std::string, bool>   longPressTriggered_;

    bool   hasUnreadMessages_ = false;
    DWORD  unreadFlashTick_   = 0;

    void loadBool(const char *key, bool &value)
    {
        const char *s = GetDataFromAsr(key);
        if (s && *s) value = (strcmp(s, "0") != 0);
    }
    void loadInt(const char *key, int &value)
    {
        const char *s = GetDataFromAsr(key);
        if (s && *s) value = atoi(s);
    }
    void saveSettings()
    {
        SaveDataToAsr("INDRA_APC_OVERLAY",        "overlay",    overlayEnabled_    ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RANGE",          "range",      std::to_string(rangeNm_).c_str());
        SaveDataToAsr("INDRA_APC_DECEN",          "decentered", decentered_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SIM",            "sim",        simulationMode_    ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MAP_AIRWAYS",    "airways",    mapAirways_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MAP_AERO",       "aero",       mapAero_           ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MAP_RESTRICTED", "restricted", mapRestricted_     ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MAP_WEATHER",    "weather",    mapWeather_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_AIRPORTS",  "airports",   showAirports_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_FIXES",     "fixes",      showFixes_         ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_AIRWAYS",   "airways2",   showAirways_       ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_SIDSTAR",   "sidstar",    showSidStar_       ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_GEO",       "geo",        showGeo_           ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_AIRSPACE",  "airspace",   showAirspace_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_REGIONS",   "regions",    showRegions_       ? "1" : "0");
        SaveDataToAsr("INDRA_APC_SHOW_FREETEXT",  "freetext",   showFreeText_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RBL",            "rbl",        rblEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RECORDING",      "recording",  recording_         ? "1" : "0");
        SaveDataToAsr("INDRA_APC_FILTER",         "filter",     filter_.c_str());
        SaveDataToAsr("INDRA_APC_RTE",            "rte",        rteEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MTCD",           "mtcd",       mtcdEnabled_       ? "1" : "0");
        SaveDataToAsr("INDRA_APC_ALARM",          "alarm",      alarmEnabled_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_TAGFAMILY",      "tagfamily",  std::to_string(currentTagFamily_).c_str());
    }

    void message(const std::string &value)
    {
        GetPlugIn()->DisplayUserMessage(kPluginName, "INDRA", value.c_str(),
                                        true, true, true, false, false);
        rememberMessage(value);
    }

    void sendEuroScopeCommand(const std::string &command)
    {
        std::vector<INPUT> inputs;
        auto key = [&inputs](WORD vk, DWORD flags = 0) {
            INPUT in = {};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = vk;
            in.ki.dwFlags = flags;
            inputs.push_back(in);
        };
        auto unicodeChar = [&inputs](char ch, DWORD flags = 0) {
            INPUT in = {};
            in.type = INPUT_KEYBOARD;
            in.ki.wScan = static_cast<WORD>(static_cast<unsigned char>(ch));
            in.ki.dwFlags = KEYEVENTF_UNICODE | flags;
            inputs.push_back(in);
        };

        key(VK_ESCAPE);
        key(VK_ESCAPE, KEYEVENTF_KEYUP);
        for (char ch : command)
        {
            unicodeChar(ch);
            unicodeChar(ch, KEYEVENTF_KEYUP);
        }
        key(VK_RETURN);
        key(VK_RETURN, KEYEVENTF_KEYUP);

        if (!inputs.empty())
            SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }

    RECT bottomBarArea()
    {
        RECT radar = GetRadarArea();
        RECT result = { radar.left,
                        radar.bottom - kBarH,
                        radar.right,
                        radar.bottom };
        return result;
    }

    void drawBottomBar(HDC hdc)
    {
        RECT bar = bottomBarArea();
        fillSolid(hdc, bar, kColBarBg);

        HPEN topPen = CreatePen(PS_SOLID, 1, kColBarFrame);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, topPen));
        MoveToEx(hdc, bar.left,  bar.top, nullptr);
        LineTo  (hdc, bar.right, bar.top);
        SelectObject(hdc, oldPen);
        DeleteObject(topPen);

        int y0 = bar.top + kBtnMargin;
        int bh = kBtnH;
        int x = bar.left + 6;
        int colW = 72;
        int gap = 4;

        auto drawSeparator = [&](int xPos) {
            HPEN sepPen = CreatePen(PS_SOLID, 1, kColBarFrame);
            HPEN old = static_cast<HPEN>(SelectObject(hdc, sepPen));
            MoveToEx(hdc, xPos, bar.top + 4, nullptr);
            LineTo(hdc, xPos, bar.bottom - 4);
            SelectObject(hdc, old);
            DeleteObject(sepPen);
        };

        drawVerticalGroup(hdc, x, y0, colW, bh, "EXECUTIVE", "PLANNER", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "ARR", "DEP", "FPL",
                          filter_ == "ARRIVALS", filter_ == "DEPARTURES", false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "CPDLC", "ABS AIR", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "VIEW1", "VIEW2", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "LM 0", "RINGS", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "AREAS", "ELW", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "RTE", "RBL", "OBI",
                          rteEnabled_, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "DATBLK", "BRIGHT", "F 3D",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "QDM", "SEP", "METEO",
                          qdmMode_, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "RBL ALM", "MTCD", "ALM OFF",
                          false, mtcdEnabled_, !alarmEnabled_);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "OVERLAP", "FREETEXT", "SECTORS",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "LAST POS", "FINDER", "SSR F",
                          false, false, false);
        x += colW + gap;

        drawSeparator(x - gap/2);
        x += 4;

        drawVerticalGroup(hdc, x, y0, colW, bh, "USERS", "", "",
                          false, false, false);
        x += colW + gap;

        drawSeparator(x - gap/2);
        x += 4;

        EuroScopePlugIn::CPosition zoomLd, zoomRu;
        GetDisplayArea(&zoomLd, &zoomRu);
        currentZoom_ = currentRangeFromDisplay(zoomLd, zoomRu);
        rangeNm_ = currentZoom_;
        char zoomLabel[20];
        snprintf(zoomLabel, sizeof(zoomLabel), "%dNM", currentZoom_);
        drawVerticalGroup(hdc, x, y0, colW, bh, zoomLabel, "+", "-",
                          false, false, false);
        x += colW + gap;
        drawSeparator(x - gap/2);
        x += 4;

        int arrowW = colW;
        int arrowH = bh / 3;
        drawBtn(hdc, x, y0, arrowW, arrowH, "^", false);
        drawBtn(hdc, x, y0 + arrowH, arrowW/2, arrowH, "<", false);
        drawBtn(hdc, x + arrowW/2, y0 + arrowH, arrowW/2, arrowH, ">", false);
        drawBtn(hdc, x, y0 + arrowH*2, arrowW, arrowH, "v", false);
        x += arrowW + gap;
        drawSeparator(x - gap/2);
        x += 4;

        drawVerticalGroup(hdc, x, y0, colW, bh, "EXP +", "EXP -", "CEN",
                          false, false, false);
        x += colW + gap;
        drawSeparator(x - gap/2);
        x += 4;

        const char *viewBtns[] = {"S", "0", "1/2", "1", "3", "5", "8"};
        int viewW = 36;
        for (const char *btn : viewBtns)
        {
            drawBtn(hdc, x, y0 + bh/4, viewW, bh/2, btn, false);
            x += viewW + 2;
        }
        x += 4;
        drawSeparator(x - gap/2);
        x += 4;

        {
            bool msgFlash = false;
            if (hasUnreadMessages_ && !messagesPopupVisible_)
            {
                DWORD elapsed = GetTickCount() - unreadFlashTick_;
                msgFlash = ((elapsed / 500) % 2) == 0;
            }

            RECT msgRect = { x, y0 + bh / 4, x + 76, y0 + bh / 4 + bh / 2 };
            COLORREF faceCol = msgFlash ? rgb(0, 100, 200) : kColBtnFace;
            fillSolid(hdc, msgRect, faceCol);

            HPEN hiPen2 = CreatePen(PS_SOLID, 1, msgFlash ? rgb(80, 160, 255) : kColBtnHiEdge);
            HPEN shPen2 = CreatePen(PS_SOLID, 1, kColBtnShEdge);
            HPEN oldPen2 = static_cast<HPEN>(SelectObject(hdc, hiPen2));
            MoveToEx(hdc, msgRect.left,      msgRect.bottom - 1, nullptr);
            LineTo  (hdc, msgRect.left,      msgRect.top);
            LineTo  (hdc, msgRect.right - 1, msgRect.top);
            SelectObject(hdc, shPen2);
            MoveToEx(hdc, msgRect.right - 1, msgRect.top,        nullptr);
            LineTo  (hdc, msgRect.right - 1, msgRect.bottom - 1);
            LineTo  (hdc, msgRect.left,      msgRect.bottom - 1);
            SelectObject(hdc, oldPen2);
            DeleteObject(hiPen2);
            DeleteObject(shPen2);

            SetBkMode   (hdc, TRANSPARENT);
            SetTextColor(hdc, msgFlash ? rgb(255, 255, 255) : kColBtnText);
            HFONT fnt2 = CreateFontA(-10, 0, 0, 0, msgFlash ? FW_BOLD : FW_NORMAL,
                                     FALSE, FALSE, FALSE, ANSI_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
            HFONT oldFnt2 = static_cast<HFONT>(SelectObject(hdc, fnt2));
            DrawTextA(hdc, "MESSAGES", -1, &msgRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFnt2);
            DeleteObject(fnt2);
            AddScreenObject(kObjButton, "MESSAGES", msgRect, false, "Messages");
        }

        drawBottomRightBranding(hdc, bar);
    }

    void drawVerticalGroup(HDC hdc, int x, int y, int w, int totalH,
                           const char *top, const char *mid, const char *bot,
                           bool pressedTop, bool pressedMid, bool pressedBot)
    {
        int h3 = totalH / 3;
        int yPos = y;
        if (top && *top)
        {
            drawBtn(hdc, x, yPos, w, h3, top, pressedTop);
            yPos += h3;
        }
        if (mid && *mid)
        {
            drawBtn(hdc, x, yPos, w, h3, mid, pressedMid);
            yPos += h3;
        }
        if (bot && *bot)
        {
            drawBtn(hdc, x, yPos, w, h3, bot, pressedBot);
        }
    }

    int drawBtn(HDC hdc, int x, int y, int w, int h, const char *label, bool pressed)
    {
        RECT r = { x, y, x + w, y + h };
        COLORREF faceCol = pressed ? kColBtnPress : kColBtnFace;
        fillSolid(hdc, r, faceCol);

        HPEN hiPen = CreatePen(PS_SOLID, 1, pressed ? kColBtnShEdge : kColBtnHiEdge);
        HPEN shPen = CreatePen(PS_SOLID, 1, pressed ? kColBtnHiEdge : kColBtnShEdge);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, hiPen));
        MoveToEx(hdc, r.left,      r.bottom - 1, nullptr);
        LineTo  (hdc, r.left,      r.top);
        LineTo  (hdc, r.right - 1, r.top);
        SelectObject(hdc, shPen);
        MoveToEx(hdc, r.right - 1, r.top,        nullptr);
        LineTo  (hdc, r.right - 1, r.bottom - 1);
        LineTo  (hdc, r.left,      r.bottom - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(hiPen);
        DeleteObject(shPen);

        SetBkMode   (hdc, TRANSPARENT);
        SetTextColor(hdc, kColBtnText);
        HFONT font = CreateFontA(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        DrawTextA(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        DeleteObject(font);
        AddScreenObject(kObjButton, label, r, false, label);
        return x + w + kBtnGap;
    }

    void fillSolid(HDC hdc, const RECT &r, COLORREF color)
    {
        HBRUSH brush = CreateSolidBrush(color);
        FillRect(hdc, &r, brush);
        DeleteObject(brush);
    }

    void drawBottomRightBranding(HDC hdc, const RECT &bar)
    {
        const int w = 150;
        const int h = 54;
        RECT box = { bar.right - w - 10, bar.top + (rectH(bar) - h) / 2,
                     bar.right - 10, bar.top + (rectH(bar) + h) / 2 };

        SetBkMode(hdc, TRANSPARENT);
        HPEN pen = CreatePen(PS_SOLID, 1, rgb(168, 170, 154));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH dotBrush = CreateSolidBrush(rgb(168, 170, 154));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, dotBrush));

        int cx = box.left + 28;
        int cy = box.top + h / 2;
        const int spacing = 6;
        const int dot = 3;
        for (int row = -4; row <= 4; ++row)
        {
            for (int col = -4; col <= 4; ++col)
            {
                int px = cx + col * spacing;
                int py = cy + row * spacing;
                double dist = sqrt(static_cast<double>(col * col + row * row));
                if (dist > 4.4) continue;
                Ellipse(hdc, px - dot / 2, py - dot / 2, px + dot / 2 + 1, py + dot / 2 + 1);
            }
        }

        HFONT logoFont = CreateFontA(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial Rounded MT Bold");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, logoFont));
        SetTextColor(hdc, rgb(218, 216, 196));
        RECT textRect = { box.left + 66, box.top + 2, box.right, box.bottom - 2 };
        DrawTextA(hdc, "Indra", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(logoFont);
        DeleteObject(pen);
        DeleteObject(dotBrush);
    }

    void drawInlinePopup(HDC hdc)
    {
        if (!popupVisible_) return;
        clampPopupToRadar(popupRect_);
        RECT r = popupRect_;
        HBRUSH bg = CreateSolidBrush(kColPopupBg);
        FillRect(hdc, &r, bg);
        DeleteObject(bg);
        RECT title = { r.left, r.top, r.right, r.top + 24 };
        HBRUSH titleBg = CreateSolidBrush(kColPopupTitle);
        FillRect(hdc, &title, titleBg);
        DeleteObject(titleBg);
        HPEN frame  = CreatePen(PS_SOLID, 1, kColBarFrame);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, frame));
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        MoveToEx(hdc, r.left, title.bottom, nullptr);
        LineTo  (hdc, r.right, title.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(frame);
        HFONT titleFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
        SetBkMode   (hdc, TRANSPARENT);
        SetTextColor(hdc, kColBtnText);
        RECT titleText = { title.left + 6, title.top, title.right - 32, title.bottom };
        DrawTextA(hdc, popupTitle_.c_str(), -1, &titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        AddScreenObject(kObjButton, "POPUP_MOVE", title, true, "Move popout");
        RECT close = { r.right - 26, r.top + 3, r.right - 5, r.top + 21 };
        drawBtn(hdc, close.left, close.top, rectW(close), rectH(close), "X", false);
        AddScreenObject(kObjButton, "POPUP_CLOSE", close, false, "Close popout");
        RECT content = { r.left + 8, title.bottom + 8, r.right - 8, r.bottom - 8 };
        HFONT bodyFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        SelectObject(hdc, bodyFont);
        SetTextColor(hdc, kColBtnText);
        DrawTextA(hdc, popupContent_.empty() ? "No data available." : popupContent_.c_str(),
                  -1, &content, DT_LEFT | DT_TOP | DT_WORDBREAK);
        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);
        DeleteObject(bodyFont);
    }

    void drawMessagesPopup(HDC hdc)
    {
        if (!messagesPopupVisible_) return;
        clampPopupToRadar(messagesPopupRect_);
        RECT r = messagesPopupRect_;
        HBRUSH bg = CreateSolidBrush(kColPopupBg);
        FillRect(hdc, &r, bg);
        DeleteObject(bg);
        RECT title = { r.left, r.top, r.right, r.top + 24 };
        HBRUSH titleBg = CreateSolidBrush(kColPopupTitle);
        FillRect(hdc, &title, titleBg);
        DeleteObject(titleBg);
        HPEN frame  = CreatePen(PS_SOLID, 1, kColBarFrame);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, frame));
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        MoveToEx(hdc, r.left, title.bottom, nullptr);
        LineTo  (hdc, r.right, title.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(frame);
        HFONT titleFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, titleFont));
        SetBkMode   (hdc, TRANSPARENT);
        SetTextColor(hdc, kColBtnText);
        RECT titleText = { title.left + 6, title.top, title.right - 32, title.bottom };
        DrawTextA(hdc, "Indra APC - Messages", -1, &titleText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        AddScreenObject(kObjButton, "MSG_POPUP_MOVE", title, true, "Move messages");
        RECT close = { r.right - 26, r.top + 3, r.right - 5, r.top + 21 };
        drawBtn(hdc, close.left, close.top, rectW(close), rectH(close), "X", false);
        AddScreenObject(kObjButton, "POPUP_CLOSE", close, false, "Close popup");

        int rightW = 150;
        int padding = 8;
        RECT contactsRect = { r.right - padding - rightW, title.bottom + padding,
                              r.right - padding, r.bottom - padding - 40 };
        RECT convRect = { r.left + padding, title.bottom + padding,
                          contactsRect.left - padding, r.bottom - padding - 40 };
        RECT newDmRect = { contactsRect.left, contactsRect.bottom + 4,
                           contactsRect.right, contactsRect.bottom + 28 };
        RECT sendRect = { convRect.left, convRect.bottom + 4,
                          convRect.left + 60, convRect.bottom + 28 };
        RECT inputHintRect = { sendRect.right + 6, sendRect.top,
                               convRect.right, sendRect.bottom };

        HPEN whitePen = CreatePen(PS_SOLID, 1, kColBarFrame);
        SelectObject(hdc, whitePen);
        Rectangle(hdc, contactsRect.left, contactsRect.top, contactsRect.right, contactsRect.bottom);
        Rectangle(hdc, convRect.left, convRect.top, convRect.right, convRect.bottom);
        DeleteObject(whitePen);

        SetTextColor(hdc, kColBtnText);
        HFONT smallFont = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
        SelectObject(hdc, smallFont);

        int yOff = contactsRect.top + 4;
        for (const auto &pair : g_chatHistory)
        {
            RECT contactItem = { contactsRect.left + 2, yOff, contactsRect.right - 2, yOff + 16 };
            bool selected = (pair.first == selectedContact_);
            if (selected)
            {
                HBRUSH selBrush = CreateSolidBrush(rgb(0, 80, 120));
                FillRect(hdc, &contactItem, selBrush);
                DeleteObject(selBrush);
            }
            DrawTextA(hdc, pair.first.c_str(), -1, &contactItem, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            AddScreenObject(kObjButton, ("CONTACT_" + pair.first).c_str(), contactItem, false, pair.first.c_str());
            yOff += 18;
        }

        std::string convText;
        auto it = g_chatHistory.find(selectedContact_);
        if (it != g_chatHistory.end())
        {
            for (const auto &msg : it->second)
            {
                convText += msg.time + (msg.fromMe ? " >> " : " << ") + msg.sender + ": " + msg.message + "\r\n";
            }
        }
        if (convText.empty()) convText = selectedContact_.empty()
            ? "Select or create a contact to view conversation."
            : "No messages with " + selectedContact_ + " yet.";
        RECT convTextRect = { convRect.left + 4, convRect.top + 4, convRect.right - 4, convRect.bottom - 4 };
        DrawTextA(hdc, convText.c_str(), -1, &convTextRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

        drawBtn(hdc, sendRect.left, sendRect.top, rectW(sendRect), rectH(sendRect), "SEND", false);
        AddScreenObject(kObjButton, "MSG_SEND", sendRect, false, "Send message");
        HBRUSH inputBrush = CreateSolidBrush(rgb(224, 224, 214));
        FillRect(hdc, &inputHintRect, inputBrush);
        DeleteObject(inputBrush);
        SetTextColor(hdc, rgb(0, 0, 0));
        RECT inputTextRect = { inputHintRect.left + 5, inputHintRect.top,
                               inputHintRect.right - 5, inputHintRect.bottom };
        DrawTextA(hdc, selectedContact_.empty() ? "Select contact" : "Type message...",
                  -1, &inputTextRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(hdc, kColBtnText);
        drawBtn(hdc, newDmRect.left, newDmRect.top, rectW(newDmRect), rectH(newDmRect), "NEW DM", false);
        AddScreenObject(kObjButton, "MSG_NEW_DM", newDmRect, false, "New direct message");

        SelectObject(hdc, oldFont);
        DeleteObject(titleFont);
        DeleteObject(smallFont);
    }

    bool handleMessagesPopupClick(const std::string &objId, POINT pt, RECT area)
    {
        (void)pt; (void)area;
        if (objId.find("CONTACT_") == 0)
        {
            selectedContact_ = objId.substr(8);
            messagesNeedRefresh_ = true;
            RequestRefresh();
            return true;
        }
        return false;
    }

    void toggleMessagesPopup(RECT area)
    {
        if (messagesPopupVisible_)
        {
            messagesPopupVisible_ = false;
        }
        else
        {
            messagesPopupVisible_ = true;
            positionMessagesPopup(area);
            selectedContact_ = g_chatHistory.empty() ? "" : g_chatHistory.begin()->first;
            messagesNeedRefresh_ = false;
            hasUnreadMessages_ = false;
            unreadFlashTick_   = 0;
            g_newMessageArrived = false;
        }
        RequestRefresh();
    }

    void positionMessagesPopup(const RECT &area)
    {
        RECT radar = GetRadarArea();
        int w = 550, h = 430;
        int x = area.left;
        int y = area.top - h - 8;
        if (y < radar.top + 50) y = area.bottom + 8;
        messagesPopupRect_ = { x, y, x + w, y + h };
        clampPopupToRadar(messagesPopupRect_);
    }

    void openMessageSendPopup()
    {
        if (selectedContact_.empty())
        {
            message("No contact selected.");
            return;
        }
        RECT edit = messageInputRect();
        GetPlugIn()->OpenPopupEdit(edit, FN_MESSAGES_SEND, "");
    }

    void openNewDmPopup()
    {
        RECT edit = newDmInputRect();
        GetPlugIn()->OpenPopupEdit(edit, FN_MESSAGES_NEW_DM, "");
    }

    RECT messageInputRect() const
    {
        int padding = 8;
        int rightW = 150;
        int bottom = messagesPopupRect_.bottom - padding - 12;
        int top = bottom - 24;
        int left = messagesPopupRect_.left + padding + 66;
        int right = messagesPopupRect_.right - padding - rightW - padding;
        return { left, top, right, bottom };
    }

    RECT newDmInputRect() const
    {
        int padding = 8;
        int rightW = 150;
        int bottom = messagesPopupRect_.bottom - padding - 12;
        int top = bottom - 24;
        int left = messagesPopupRect_.right - padding - rightW;
        int right = messagesPopupRect_.right - padding;
        return { left, top, right, bottom };
    }

    bool isDuplicateSend(const std::string &recipient, const std::string &text) const
    {
        DWORD now = GetTickCount();
        return recipient == lastSentRecipient_ &&
               text == lastSentMessage_ &&
               now - lastSentTick_ < 2000;
    }

    void clampPopupToRadar(RECT &rect)
    {
        RECT radar = GetRadarArea();
        RECT console = bottomBarArea();
        int w = rectW(rect), h = rectH(rect);
        if (rect.left < radar.left + 8) { rect.left = radar.left + 8; rect.right = rect.left + w; }
        if (rect.right > radar.right - 8) { rect.right = radar.right - 8; rect.left = rect.right - w; }
        if (rect.top < radar.top + 52) { rect.top = radar.top + 52; rect.bottom = rect.top + h; }
        if (rect.bottom > console.top - 8) { rect.bottom = console.top - 8; rect.top = rect.bottom - h; }
    }

    void drawSearchHighlight(HDC hdc)
    {
        HPEN dashPen = CreatePen(PS_DASH, 1, rgb(180, 180, 180));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, dashPen));
        HBRUSH brush = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, brush));
        POINT c = ConvertCoordFromPositionToPixel(searchCirclePos_);
        if (searchLineValid_)
        {
            POINT from = ConvertCoordFromPositionToPixel(searchLineFromPos_);
            MoveToEx(hdc, from.x, from.y, nullptr);
            LineTo(hdc, c.x, c.y);
        }
        int r = searchCircleRadiusPx_;
        Ellipse(hdc, c.x - r, c.y - r, c.x + r, c.y + r);
        MoveToEx(hdc, c.x, c.y - r - 2, nullptr);
        LineTo  (hdc, c.x, c.y - r - 14);
        MoveToEx(hdc, c.x + 2, c.y - r - 8, nullptr);
        LineTo  (hdc, c.x - 2, c.y - r - 8);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(dashPen);
    }

    void drawSelectedRoute(HDC hdc)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) return;
        EuroScopePlugIn::CFlightPlanExtractedRoute route = fp.GetExtractedRoute();
        int n = route.GetPointsNumber();
        if (n < 2) return;
        HPEN pen = CreatePen(PS_SOLID, 2, rgb(0, 220, 80));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        for (int i = 0; i < n - 1; ++i)
        {
            POINT p1 = ConvertCoordFromPositionToPixel(route.GetPointPosition(i));
            POINT p2 = ConvertCoordFromPositionToPixel(route.GetPointPosition(i + 1));
            MoveToEx(hdc, p1.x, p1.y, nullptr);
            LineTo  (hdc, p2.x, p2.y);
        }
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    void drawQDMLine(HDC hdc)
    {
        if (!qdmLineValid_) return;
        HPEN pen = CreatePen(PS_SOLID, 2, rgb(255, 255, 0));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        POINT p1 = ConvertCoordFromPositionToPixel(qdmPoint1_);
        POINT p2 = ConvertCoordFromPositionToPixel(qdmPoint2_);
        MoveToEx(hdc, p1.x, p1.y, nullptr);
        LineTo  (hdc, p2.x, p2.y);
        int mx = (p1.x + p2.x) / 2;
        int my = (p1.y + p2.y) / 2;
        double bearing = qdmPoint1_.DirectionTo(qdmPoint2_);
        char buf[32];
        snprintf(buf, sizeof(buf), "QDM %.1f\xB0", bearing);
        SetBkMode   (hdc, TRANSPARENT);
        SetTextColor(hdc, rgb(255, 255, 0));
        HFONT fnt = CreateFontA(-11, 0,0,0, FW_BOLD, FALSE,FALSE,FALSE,
                                ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                FIXED_PITCH|FF_MODERN, "Courier New");
        HFONT oldFnt = static_cast<HFONT>(SelectObject(hdc, fnt));
        TextOutA(hdc, mx + 4, my - 14, buf, (int)strlen(buf));
        SelectObject(hdc, oldFnt);
        DeleteObject(fnt);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    void handleButton(const std::string &s, POINT pt, RECT area)
    {
        if (s == "ARR")                  setFilter("ARRIVALS");
        else if (s == "DEP")             setFilter("DEPARTURES");
        else if (s == "FPL")             openSelectedFlightPlan(pt, area);
        else if (s == "RTE")
        {
            rteEnabled_ = !rteEnabled_;
            message(rteEnabled_ ? "TopSky route draw ON" : "TopSky route draw OFF");
            toggleTopSkyRte(pt, area);
        }
        else if (s == "CPDLC")           openCpdlcWindows(pt, area);
        else if (s == "SEP")             openSepTool(pt, area);
        else if (s == "METEO")           showMetPopup(area);
        else if (s == "MTCD")            toggleMTCD();
        else if (s == "ALM OFF")         { alarmEnabled_ = !alarmEnabled_; message(alarmEnabled_ ? "Alarm sounds ON" : "Alarm sounds OFF"); }
        else if (s == "AREAS" || s == "SECTORS") showDisplaySettingsPicker(area);
        else if (s == "DATBLK")          showTagFamilyPicker(area);
        else if (s == "FINDER")          showFinderPopup(area);
        else if (s == "SSR F")           showSSRFPopup(area);
        else if (s == "USERS")           { /* intentionally no-op */ }
        else if (s == "VIEW1")           applyView("VIEW1");
        else if (s == "VIEW2")           applyView("VIEW2");
        else if (s == "CEN")             centerScreen();
        else if (s == "EXP +")           zoomIn();
        else if (s == "EXP -")           zoomOut();
        else if (s == "+")               zoomIn();
        else if (s == "-")               zoomOut();
        else if (s == "^")               pan(0, -160);
        else if (s == "v")               pan(0,  160);
        else if (s == "<")               pan(-160, 0);
        else if (s == ">")               pan( 160, 0);
        else if (s == "MESSAGES")        toggleMessagesPopup(area);
        saveSettings();
        RequestRefresh();
    }

    void applyView(const std::string &button)
    {
        views_ = loadViewsJson();
        for (const auto &v : views_)
        {
            if (v.button != button) continue;
            if (v.hasCenter)
            {
                EuroScopePlugIn::CPosition pos;
                pos.m_Latitude  = v.centerLat;
                pos.m_Longitude = v.centerLon;
                double delta = v.zoomNm * 0.00833333;
                EuroScopePlugIn::CPosition ld, ru;
                ld.m_Latitude  = pos.m_Latitude  - delta;
                ld.m_Longitude = pos.m_Longitude - delta;
                ru.m_Latitude  = pos.m_Latitude  + delta;
                ru.m_Longitude = pos.m_Longitude + delta;
                SetDisplayArea(ld, ru);
                rangeNm_     = v.zoomNm;
                currentZoom_ = v.zoomNm;
            }
            else
            {
                setRange(v.zoomNm);
            }
            message("View loaded: " + v.name);
            return;
        }
        message("No view saved for button " + button);
    }

    void saveCurrentView(const std::string &button)
    {
        ViewDef v;
        v.button = button;
        v.name   = "User " + button;
        EuroScopePlugIn::CPosition ld, ru;
        GetDisplayArea(&ld, &ru);
        v.zoomNm = currentRangeFromDisplay(ld, ru);
        v.centerLat = (ld.m_Latitude  + ru.m_Latitude)  / 2.0;
        v.centerLon = (ld.m_Longitude + ru.m_Longitude) / 2.0;
        v.hasCenter = true;
        rangeNm_ = currentZoom_ = v.zoomNm;
        saveViewToJson(button, v);
        views_ = loadViewsJson();
        message("Current view saved to " + button);
    }

    void processHeldViewButtons()
    {
        DWORD now = GetTickCount();
        for (auto &entry : pressStartTime_)
        {
            const std::string &button = entry.first;
            if (longPressTriggered_[button]) continue;
            if (now - entry.second >= 5000)
            {
                longPressTriggered_[button] = true;
                saveCurrentView(button);
            }
        }
    }

    void expireSearchHighlight()
    {
        if (!searchCircleValid_ || searchExpireTick_ == 0) return;
        if (static_cast<DWORD>(GetTickCount() - searchExpireTick_) < 0x80000000UL)
        {
            searchCircleValid_ = false;
            searchLineValid_ = false;
            searchExpireTick_ = 0;
            RequestRefresh();
        }
    }

    int currentRangeFromDisplay(const EuroScopePlugIn::CPosition &ld,
                                const EuroScopePlugIn::CPosition &ru) const
    {
        double latNm = fabs(ru.m_Latitude - ld.m_Latitude) * 60.0;
        int nm = static_cast<int>(latNm + 0.5);
        return (std::max)(1, (std::min)(nm, 2000));
    }

    void setRange(int nm)
    {
        rangeNm_     = (std::max)(1, (std::min)(nm, 2000));
        currentZoom_ = rangeNm_;
        EuroScopePlugIn::CPosition ld, ru;
        GetDisplayArea(&ld, &ru);
        double cLat  = (ld.m_Latitude  + ru.m_Latitude)  / 2.0;
        double cLon  = (ld.m_Longitude + ru.m_Longitude) / 2.0;
        double delta = rangeNm_ * 0.00833333;
        ld.m_Latitude  = cLat - delta;  ld.m_Longitude = cLon - delta;
        ru.m_Latitude  = cLat + delta;  ru.m_Longitude = cLon + delta;
        SetDisplayArea(ld, ru);
        saveSettings();
    }

    void zoomIn()  { setRange(rangeNm_ / 2); }
    void zoomOut() { setRange(rangeNm_ * 2); }

    void pan(int dx, int dy)
    {
        EuroScopePlugIn::CPosition ld, ru;
        GetDisplayArea(&ld, &ru);
        RECT radarArea = GetRadarArea();
        double dLat = (ru.m_Latitude  - ld.m_Latitude)  * -dy / rectH(radarArea);
        double dLon = (ru.m_Longitude - ld.m_Longitude) * dx / rectW(radarArea);
        ld.m_Latitude  += dLat;  ld.m_Longitude += dLon;
        ru.m_Latitude  += dLat;  ru.m_Longitude += dLon;
        SetDisplayArea(ld, ru);
        RequestRefresh();
    }

    void centerScreen()
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (fp.IsValid())
        {
            EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
            if (rt.IsValid())
            {
                EuroScopePlugIn::CRadarTargetPositionData p = rt.GetPosition();
                if (p.IsValid())
                {
                    centerOnPosition(p.GetPosition());
                    return;
                }
            }
        }
        EuroScopePlugIn::CController me = GetPlugIn()->ControllerMyself();
        if (me.IsValid())
        {
            centerOnPosition(me.GetPosition());
        }
    }

    void centerOnPosition(const EuroScopePlugIn::CPosition &pos)
    {
        EuroScopePlugIn::CPosition ld, ru;
        GetDisplayArea(&ld, &ru);
        double wDeg = ru.m_Longitude - ld.m_Longitude;
        double hDeg = ru.m_Latitude  - ld.m_Latitude;
        ld.m_Latitude  = pos.m_Latitude  - hDeg / 2;
        ld.m_Longitude = pos.m_Longitude - wDeg / 2;
        ru.m_Latitude  = pos.m_Latitude  + hDeg / 2;
        ru.m_Longitude = pos.m_Longitude + wDeg / 2;
        SetDisplayArea(ld, ru);
        RequestRefresh();
    }

    void setFilter(const std::string &filter)
    {
        updateControllerAirport();
        if (filter_ == filter) filter_ = "OFF";
        else                   filter_ = filter;

        if (filter_ == "ARRIVALS")
        {
            std::string cmd = ".filter arrivals";
            if (!controllerAirport_.empty()) cmd += " " + controllerAirport_;
            GetPlugIn()->OnCompileCommand(cmd.c_str());
            message("Showing arrivals to " + controllerAirport_);
        }
        else if (filter_ == "DEPARTURES")
        {
            std::string cmd = ".filter departures";
            if (!controllerAirport_.empty()) cmd += " " + controllerAirport_;
            GetPlugIn()->OnCompileCommand(cmd.c_str());
            message("Showing departures from " + controllerAirport_);
        }
        else
        {
            GetPlugIn()->OnCompileCommand(".filter clear");
            message("Filter cleared");
        }
        RequestRefresh();
    }

    void toggleMTCD()
    {
        mtcdEnabled_ = !mtcdEnabled_;
        GetPlugIn()->OnCompileCommand(mtcdEnabled_ ? ".mtcd on" : ".mtcd off");
        message(mtcdEnabled_ ? "MTCD ON" : "MTCD OFF");
    }

    void toggleQDMMode()
    {
        if (qdmMode_)
        {
            qdmMode_ = false;
            qdmFirstClick_ = false;
            qdmLineValid_ = false;
            message("QDM mode OFF");
        }
        else
        {
            qdmMode_ = true;
            qdmFirstClick_ = false;
            qdmLineValid_ = false;
            message("QDM mode ON \x96 click first point on radar");
        }
        RequestRefresh();
    }

    void handleQDMClick(POINT clickPt)
    {
        if (!qdmFirstClick_)
        {
            qdmPoint1_ = ConvertCoordFromPixelToPosition(clickPt);
            qdmFirstClick_ = true;
            message("First QDM point set \x96 click second point");
        }
        else
        {
            qdmPoint2_ = ConvertCoordFromPixelToPosition(clickPt);
            qdmFirstClick_ = false;
            qdmMode_ = false;
            qdmLineValid_ = true;
            double bearing = qdmPoint1_.DirectionTo(qdmPoint2_);
            char buf[64];
            snprintf(buf, sizeof(buf), "QDM: %.1f\xB0", bearing);
            message(buf);
            RequestRefresh();
        }
    }

    void showTagFamilyPicker(RECT area)
    {
        GetPlugIn()->OpenPopupList(area, "Tag Family", 1);
        for (int i = 0; i < 8; ++i)
        {
            char label[32];
            snprintf(label, sizeof(label), "Tag Family %d", i);
            GetPlugIn()->AddPopupListElement(label, "", FN_DATBLK, false,
                (i == currentTagFamily_) ? EuroScopePlugIn::POPUP_ELEMENT_CHECKED
                                         : EuroScopePlugIn::POPUP_ELEMENT_UNCHECKED);
        }
    }

    void onTagFamilySelected(int family)
    {
        currentTagFamily_ = family;
        char cmd[32];
        snprintf(cmd, sizeof(cmd), ".tagfamily %d", family);
        GetPlugIn()->OnCompileCommand(cmd);
        message("Tag family set to " + std::to_string(family));
    }

    void openDisplaySettings()
    {
        showDisplaySettingsPicker(bottomBarArea());
    }

    void openConnectionDialog(POINT pt, RECT area)
    {
        if (pt.x < area.left || pt.x > area.right || pt.y < area.top || pt.y > area.bottom)
            return;
        DWORD now = GetTickCount();
        if (now - lastUsersClickTick_ < 1500)
            return;
        lastUsersClickTick_ = now;
        message("USERS connection dialog disabled: EuroScope keeps .connectdialog armed after plugin command injection.");
    }

    void processPendingConnectionDialog()
    {
        if (!pendingConnectionDialog_)
            return;
        DWORD now = GetTickCount();
        if (static_cast<DWORD>(now - pendingConnectionDialogTick_) >= 0x80000000UL)
            return;
        pendingConnectionDialog_ = false;
        pendingCommandClear_ = false;
    }

    void processPendingCommandClear()
    {
        if (!pendingCommandClear_)
            return;
        DWORD now = GetTickCount();
        if (static_cast<DWORD>(now - pendingCommandClearTick_) >= 0x80000000UL)
            return;
        pendingCommandClear_ = false;
        sendEuroScopeCommand(".");
    }

    bool invokeEuroScopeMenu(const std::vector<std::string> &needles)
    {
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        while (hwnd && GetParent(hwnd)) hwnd = GetParent(hwnd);
        HMENU menu = hwnd ? GetMenu(hwnd) : NULL;
        UINT commandId = 0;
        if (!menu || !findMenuCommand(hwnd, menu, needles, commandId))
            return false;
        PostMessage(hwnd, WM_COMMAND, commandId, 0);
        return true;
    }

    bool findMenuCommand(HWND hwnd, HMENU menu, const std::vector<std::string> &needles, UINT &commandId)
    {
        int count = GetMenuItemCount(menu);
        for (int i = 0; i < count; ++i)
        {
            char label[256] = {};
            MENUITEMINFOA info = {};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
            info.dwTypeData = label;
            info.cch = sizeof(label) - 1;
            if (!GetMenuItemInfoA(menu, i, TRUE, &info))
                continue;

            std::string lower = label;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(tolower(c)); });
            for (const auto &needle : needles)
            {
                if (lower.find(needle) != std::string::npos)
                {
                    if (info.wID != 0)
                    {
                        commandId = info.wID;
                        return true;
                    }
                    if (info.hSubMenu)
                    {
                        SendMessage(hwnd, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(info.hSubMenu),
                                    MAKELPARAM(i, FALSE));
                        UINT subCommand = 0;
                        if (findFirstEnabledMenuCommand(hwnd, info.hSubMenu, subCommand))
                        {
                            commandId = subCommand;
                            return true;
                        }
                    }
                }
            }
            if (info.hSubMenu && findMenuCommand(hwnd, info.hSubMenu, needles, commandId))
                return true;
        }
        return false;
    }

    bool findFirstEnabledMenuCommand(HWND hwnd, HMENU menu, UINT &commandId)
    {
        int count = GetMenuItemCount(menu);
        for (int i = 0; i < count; ++i)
        {
            MENUITEMINFOA info = {};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
            if (!GetMenuItemInfoA(menu, i, TRUE, &info))
                continue;
            if (info.hSubMenu)
            {
                SendMessage(hwnd, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(info.hSubMenu),
                            MAKELPARAM(i, FALSE));
                if (findFirstEnabledMenuCommand(hwnd, info.hSubMenu, commandId))
                    return true;
            }
            if (info.wID != 0 && !(info.fState & (MFS_DISABLED | MFS_GRAYED)))
            {
                commandId = info.wID;
                return true;
            }
        }
        return false;
    }

    int parseTrailingNumber(const char *text)
    {
        if (!text) return 0;
        const char *p = text + strlen(text);
        while (p > text && !isdigit(static_cast<unsigned char>(*(p - 1)))) --p;
        const char *end = p;
        while (p > text && isdigit(static_cast<unsigned char>(*(p - 1)))) --p;
        return (p < end) ? atoi(p) : atoi(text);
    }

    void showDisplaySettingsPicker(RECT area)
    {
        GetPlugIn()->OpenPopupList(area, "Map Layers", 2);
        addTopSkyMapItem("Airports",      showAirports_);
        addTopSkyMapItem("Fixes",         showFixes_);
        addTopSkyMapItem("Airways",       showAirways_);
        addTopSkyMapItem("SID/STAR",      showSidStar_);
        addTopSkyMapItem("Geo",           showGeo_);
        addTopSkyMapItem("Airspace",      showAirspace_);
        addTopSkyMapItem("Regions",       showRegions_);
        addTopSkyMapItem("Free Text",     showFreeText_);
    }

    void addTopSkyMapItem(const char *name, bool enabled)
    {
        GetPlugIn()->AddPopupListElement(name, enabled ? "ON" : "OFF",
            FN_DISPLAY_TOGGLE, false,
            enabled ? EuroScopePlugIn::POPUP_ELEMENT_CHECKED
                    : EuroScopePlugIn::POPUP_ELEMENT_UNCHECKED);
    }

    void onDisplayToggle(const char *item)
    {
        std::string name = item ? item : "";
        if (name == "Airports")      { showAirports_ = !showAirports_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT, showAirports_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY, showAirports_); }
        else if (name == "Fixes")    { showFixes_ = !showFixes_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_FIX, showFixes_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_VOR, showFixes_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_NDB, showFixes_); }
        else if (name == "Airways")  { showAirways_ = !showAirways_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_LOW_AIRWAY, showAirways_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_HIGH_AIRWAY, showAirways_); }
        else if (name == "SID/STAR") { showSidStar_ = !showSidStar_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_SID, showSidStar_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_STAR, showSidStar_); applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS, showSidStar_); }
        else if (name == "Geo")      { showGeo_ = !showGeo_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_GEO, showGeo_); }
        else if (name == "Airspace") { showAirspace_ = !showAirspace_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_AIRSPACE, showAirspace_); }
        else if (name == "Regions")  { showRegions_ = !showRegions_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_REGIONS, showRegions_); }
        else if (name == "Free Text"){ showFreeText_ = !showFreeText_; applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_FREE_TEXT, showFreeText_); }
        RefreshMapContent();
        saveSettings();
        RequestRefresh();
    }

    void updateControllerAirport()
    {
        EuroScopePlugIn::CController me = GetPlugIn()->ControllerMyself();
        if (!me.IsValid()) return;
        std::string cs = me.GetCallsign();
        std::size_t u = cs.find('_');
        controllerAirport_ = (u != std::string::npos) ? cs.substr(0, u) : cs;
    }

    void applySectorType(int elementType, bool visible)
    {
        GetPlugIn()->SelectScreenSectorfile(this);
        EuroScopePlugIn::CSectorElement elem = GetPlugIn()->SectorFileElementSelectFirst(elementType);
        while (elem.IsValid())
        {
            bool hadComponent = false;
            for (int i = 0; i < 32; ++i)
            {
                const char *component = elem.GetComponentName(i);
                if (!component || !*component) break;
                ShowSectorFileElement(elem, component, visible);
                hadComponent = true;
            }
            if (!hadComponent)
                ShowSectorFileElement(elem, "", visible);
            elem = GetPlugIn()->SectorFileElementSelectNext(elem, elementType);
        }
    }

    void applySavedDisplaySettings()
    {
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT, showAirports_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY, showAirports_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_FIX, showFixes_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_VOR, showFixes_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_NDB, showFixes_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_LOW_AIRWAY, showAirways_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_HIGH_AIRWAY, showAirways_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_SID, showSidStar_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_STAR, showSidStar_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_SIDS_STARS, showSidStar_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_GEO, showGeo_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_AIRSPACE, showAirspace_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_REGIONS, showRegions_);
        applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_FREE_TEXT, showFreeText_);
        RefreshMapContent();
    }

    static constexpr const char *kTopSkyPluginName = "TopSky plugin";

    bool invokeTopSkyFn(const char *callsign, int topSkyFunctionId,
                        POINT pt, RECT area)
    {
        if (!callsign || !*callsign) return false;

        EuroScopePlugIn::CFlightPlan fp = GetPlugIn()->FlightPlanSelect(callsign);
        if (!fp.IsValid())
        {
            message(std::string("TopSky call failed: no FP for ") + callsign);
            return false;
        }
        GetPlugIn()->SetASELAircraft(fp);

        RECT radar = GetRadarArea();
        RECT bar   = bottomBarArea();
        if (pt.y >= bar.top)
        {
            int cx = (radar.left + radar.right)  / 2;
            int cy = (radar.top  + radar.bottom) / 2;
            pt   = { cx, cy };
            area = { cx - 60, cy - 12, cx + 60, cy + 12 };
        }

        StartTagFunction(
            callsign,
            nullptr,
            9,
            callsign,
            kTopSkyPluginName,
            topSkyFunctionId,
            pt,
            area
        );
        return true;
    }

    void openSelectedFlightPlan(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) { message("FPL: No ASEL aircraft selected."); return; }
        const char *cs = fp.GetCallsign();

        RECT radar = GetRadarArea();
        RECT bar   = bottomBarArea();
        if (pt.y >= bar.top)
        {
            int cx = (radar.left + radar.right)  / 2;
            int cy = (radar.top  + radar.bottom) / 2;
            pt   = { cx, cy };
            area = { cx - 60, cy - 12, cx + 60, cy + 12 };
        }

        StartTagFunction(
            cs,
            nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, cs,
            nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_OPEN_FP_DIALOG,
            pt, area
        );
    }

    void toggleTopSkyRte(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) { message("RTE: No ASEL aircraft selected."); return; }
        message(std::string("RTE: toggling for ") + fp.GetCallsign());
        invokeTopSkyFn(fp.GetCallsign(), TopSky::TOGGLE_ROUTE_DRAW_MTCD_SAP, pt, area);
    }

    void openCpdlcWindows(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) { message("CPDLC: No ASEL aircraft selected."); return; }
        const char *cs = fp.GetCallsign();
        message(std::string("CPDLC: opening for ") + cs);
        invokeTopSkyFn(cs, TopSky::OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_CPDLC_W, pt, area);
    }

    void openSepTool(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) { message("No selected aircraft for SEP."); return; }
        invokeTopSkyFn(fp.GetCallsign(), TopSky::INVOKE_SEP_TOOL_WITH_VSEP, pt, area);
    }

    void showFinderPopup(RECT area) { GetPlugIn()->OpenPopupEdit(area, FN_FINDER, ""); }
    void showSSRFPopup(RECT area)   { GetPlugIn()->OpenPopupEdit(area, FN_SSRF,   ""); }

    void onFinder(const char *callsign)
    {
        EuroScopePlugIn::CFlightPlan fp = GetPlugIn()->FlightPlanSelect(callsign);
        if (fp.IsValid())
        {
            EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
            if (rt.IsValid())
            {
                EuroScopePlugIn::CRadarTargetPositionData p = rt.GetPosition();
                if (p.IsValid())
                {
                    setSearchHighlight(p.GetPosition());
                    message("Found: " + std::string(callsign));
                    RequestRefresh();
                    return;
                }
            }
            message("No radar position for " + std::string(callsign));
        }
        else
        {
            message("No flight plan: " + std::string(callsign));
        }
        searchCircleValid_ = false;
        searchLineValid_ = false;
    }

    void onSSRF(const char *squawk)
    {
        EuroScopePlugIn::CFlightPlan fp = GetPlugIn()->FlightPlanSelectFirst();
        while (fp.IsValid())
        {
            EuroScopePlugIn::CFlightPlanControllerAssignedData d = fp.GetControllerAssignedData();
            if (_stricmp(d.GetSquawk(), squawk) == 0)
            {
                EuroScopePlugIn::CRadarTarget rt = fp.GetCorrelatedRadarTarget();
                if (rt.IsValid())
                {
                    EuroScopePlugIn::CRadarTargetPositionData p = rt.GetPosition();
                    if (p.IsValid())
                    {
                        setSearchHighlight(p.GetPosition());
                        message("Squawk " + std::string(squawk) + " on " + std::string(fp.GetCallsign()));
                        RequestRefresh();
                        return;
                    }
                }
            }
            fp = GetPlugIn()->FlightPlanSelectNext(fp);
        }
        message("No aircraft with squawk " + std::string(squawk));
        searchCircleValid_ = false;
        searchLineValid_ = false;
    }

    void setSearchHighlight(const EuroScopePlugIn::CPosition &target)
    {
        EuroScopePlugIn::CPosition ld, ru;
        GetDisplayArea(&ld, &ru);
        searchLineFromPos_.m_Latitude = (ld.m_Latitude + ru.m_Latitude) / 2.0;
        searchLineFromPos_.m_Longitude = (ld.m_Longitude + ru.m_Longitude) / 2.0;
        searchCirclePos_ = target;
        searchCircleValid_ = true;
        searchLineValid_ = true;
        searchExpireTick_ = GetTickCount() + 5000;
    }

    std::string generateMetContent()
    {
        std::ostringstream ss;
        if (g_metars.empty())
            ss << "No METAR data received yet.\n";
        else
            for (const auto &m : g_metars)
                ss << m.first << ":\n  " << m.second << "\n\n";
        return ss.str();
    }

    void showMetPopup(RECT area)
    {
        showPopup("Indra APC Weather (METAR)", generateMetContent().c_str(), area);
    }

    int showPopup(const char *title, const char *content, const RECT &area)
    {
        popupTitle_   = title   ? title   : "APC";
        popupContent_ = content ? content : "";
        positionInlinePopup(area);
        popupVisible_ = true;
        RequestRefresh();
        return 1;
    }

    void positionInlinePopup(const RECT &area)
    {
        RECT radar = GetRadarArea();
        int w = 430, h = 310;
        int x = area.left;
        int y = area.top - h - 8;
        if (y < radar.top + 50) y = area.bottom + 8;
        popupRect_ = { x, y, x + w, y + h };
        clampPopupToRadar(popupRect_);
    }

    EuroScopePlugIn::CFlightPlan   aselFp() { return GetPlugIn()->FlightPlanSelectASEL();  }
    EuroScopePlugIn::CRadarTarget  aselRt() { return GetPlugIn()->RadarTargetSelectASEL(); }
};

EuroScopePlugIn::CRadarScreen *CreateIndraApcScreen()
{
    return new IndraApcScreen();
}

} // namespace Indra
