#include "Screen.h"
#include "Storage.h"

namespace Indra
{

static constexpr bool kLowResourceMode = false;
static constexpr DWORD kVacsFlashMs = 250;

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
          absAirEnabled_(false),
          lm0Enabled_(false),
          ringsEnabled_(false),
          elwEnabled_(false),
          obiEnabled_(false),
          brightEnabled_(false),
          f3dEnabled_(false),
          rblAlarmEnabled_(false),
          overlapEnabled_(false),
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
          messagesToggled_(false),
          vacsShowMenu_(false),
          vacsRole_(0),
          vacsCallState_(VacsCallUiState::Idle),
          vacsLastPollTick_(0),
          vacsFailedPollTick_(0),
          vacsDeferredPoll_(false),
          vacsDeferredPollTick_(0),
          vacsLastFlashRefreshTick_(0),
          zoomLastCalcTick_(0)
    {
        vacsContacts_ = loadContactsJson();

        EuroScopePlugIn::CController me = GetPlugIn()->ControllerMyself();
        if (me.IsValid())
        {
            std::string cs = me.GetCallsign();
            std::size_t u  = cs.find('_');
            controllerAirport_ = (u != std::string::npos) ? cs.substr(0, u) : cs;
            setActiveDataPosition(cs);
        }
        views_ = loadViewsJson();
        vacsContacts_ = loadContactsJson();
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
            loadBool("INDRA_APC_ABS_AIR",       absAirEnabled_);
            loadBool("INDRA_APC_LM0",           lm0Enabled_);
            loadBool("INDRA_APC_RINGS",         ringsEnabled_);
            loadBool("INDRA_APC_ELW",           elwEnabled_);
            loadBool("INDRA_APC_OBI",           obiEnabled_);
            loadBool("INDRA_APC_BRIGHT",        brightEnabled_);
            loadBool("INDRA_APC_F3D",           f3dEnabled_);
            loadBool("INDRA_APC_RBL_ALM",       rblAlarmEnabled_);
            loadBool("INDRA_APC_OVERLAP",       overlapEnabled_);
            loadBool("INDRA_APC_RECORDING",     recording_);
            loadBool("INDRA_APC_RTE",           rteEnabled_);
            loadBool("INDRA_APC_MTCD",          mtcdEnabled_);
            loadBool("INDRA_APC_ALARM",         alarmEnabled_);
            loadInt ("INDRA_APC_TAGFAMILY",     currentTagFamily_);
            const char *f = GetDataFromAsr("INDRA_APC_FILTER");
            if (f && *f) filter_ = f;
        }
        updateDataPosition();
        views_ = loadViewsJson();
        vacsContacts_ = loadContactsJson();
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

        screenObjectStrings_.clear();
        screenObjectStrings_.reserve(128);

        SetBkMode(hdc, TRANSPARENT);
        processHeldViewButtons();
        expireSearchHighlight();

        pollVacsState(false);
        if (kLowResourceMode)
        {
            drawLowResourceBottomBar(hdc);
        }
        else
        {
            drawBottomBar(hdc);
        }
        scheduleVacsFlashRefresh();
        if (searchCircleValid_) drawSearchHighlight(hdc);
        if (qdmLineValid_)      drawQDMLine(hdc);
    }

    bool OnCompileCommand(const char *cmd) override
    {
        if (!startsWith(cmd, ".indra")) return false;
        const char *arg = cmd + 6;
        while (*arg == ' ') ++arg;
        if      (_stricmp(arg, "off") == 0) overlayEnabled_ = false;
        else if (_stricmp(arg, "on")  == 0 || *arg == 0) overlayEnabled_ = true;
        saveSettings();
        return true;
    }

    void OnButtonDownScreenObject(int objType, const char *objId,
                                  POINT pt, RECT area, int button) override
    {
        if (objType != kObjButton || !objId || button != EuroScopePlugIn::BUTTON_LEFT) return;
        std::string s = objId;
        if (!isViewButton(s)) return;

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

        if (button == EuroScopePlugIn::BUTTON_LEFT)
        {
            int role = (s == "EXECUTIVE") ? 0 : (s == "PLANNER") ? 1 : -1;
            if (role >= 0)
            {
                vacsShowMenu_ = !(vacsShowMenu_ && vacsRole_ == role);
                vacsRole_     = role;
                if (vacsShowMenu_)
                {
                    updateDataPosition();
                    vacsContacts_ = loadContactsJson();
                    vacsDeferredPoll_ = true;
                    vacsDeferredPollTick_ = GetTickCount();
                }
                else
                {
                    vacsDeferredPoll_ = false;
                }
                RequestRefresh();
                return;
            }
            if (s == "INCOMING")
            {
                if (vacsCallState_ == VacsCallUiState::IncomingRinging)
                {
                    if (vacsManager_.AcceptIncomingCall())
                    {
                        vacsCallState_ = VacsCallUiState::Active;
                        vacsStatus_.state = VacsCallUiState::Active;
                        vacsStatus_.incoming = true;
                        vacsLastPollTick_ = GetTickCount();
                        RequestRefresh();
                        return;
                    }
                }
                else if (isActiveVacsCall())
                    vacsManager_.EndCurrentCall();
                vacsLastPollTick_ = 0;
                pollVacsState(true);
                RequestRefresh();
                return;
            }
            if (s == "VACS_CUSTOM")
            {
                if (isActiveVacsCall())
                {
                    vacsManager_.EndCurrentCall();
                    vacsLastPollTick_ = 0;
                    pollVacsState(true);
                    RequestRefresh();
                }
                else
                {
                    GetPlugIn()->OpenPopupEdit(area, FN_VACS_CUSTOM, "");
                }
                return;
            }
            if (s.size() > 5 && s.substr(0, 5) == "VACS_" &&
                std::isdigit(static_cast<unsigned char>(s[5])))
            {
                int idx = atoi(s.c_str() + 5);
                if (idx >= 0 && idx < static_cast<int>(vacsContacts_.size()))
                {
                    if (isActiveVacsCall())
                        vacsManager_.EndCurrentCall();
                    else if (isCurrentVacsTarget(vacsContacts_[idx].station))
                        vacsManager_.EndCurrentCall();
                    else
                        vacsManager_.StartVacsCall(vacsContacts_[idx].station);
                }
                vacsLastPollTick_ = 0;
                pollVacsState(true);
                RequestRefresh();
                return;
            }
        }

        if (isViewButton(s))
        {
            if (button == EuroScopePlugIn::BUTTON_LEFT &&
                pressStartTime_.find(s) == pressStartTime_.end())
                applyView(s);
            return;
        }

        if (button == EuroScopePlugIn::BUTTON_LEFT)
        {
            if (s == "QDM")    toggleQDMMode();
            else if (s == "FINDER") showFinderPopup(area);
            else if (s == "SSR F")  showSSRFPopup(area);
            else if (s == "CEN")    applyView("1");
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
                messagesToggled_ = !messagesToggled_;
                RequestRefresh();
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
        (void)objType; (void)objId; (void)pt;
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
            toggleTopSkyRte(pt, area);
            break;
        case FN_DATBLK:
            if (itemString && *itemString) onTagFamilySelected(parseTrailingNumber(itemString));
            break;
        case FN_DISPLAY_TOGGLE:
            if (itemString && *itemString) onDisplayToggle(itemString);
            break;
        case FN_MTCD_TOGGLE:
            mtcdEnabled_ = !mtcdEnabled_;
            GetPlugIn()->OnCompileCommand(mtcdEnabled_ ? ".mtcd on" : ".mtcd off");
            break;
        case FN_ALARM_TOGGLE:
            alarmEnabled_ = !alarmEnabled_;
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
        case FN_VACS_CUSTOM:
            if (itemString && *itemString)
            {
                std::string station = itemString;
                station.erase(std::remove(station.begin(), station.end(), '\r'), station.end());
                station.erase(std::remove(station.begin(), station.end(), '\n'), station.end());
                station.erase(std::remove_if(station.begin(), station.end(),
                    [](unsigned char c) { return std::isspace(c); }), station.end());
                if (!station.empty())
                {
                    vacsManager_.StartVacsCall(station);
                    vacsLastPollTick_ = 0;
                    pollVacsState(true);
                    RequestRefresh();
                }
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
    bool        absAirEnabled_;
    bool        lm0Enabled_;
    bool        ringsEnabled_;
    bool        elwEnabled_;
    bool        obiEnabled_;
    bool        brightEnabled_;
    bool        f3dEnabled_;
    bool        rblAlarmEnabled_;
    bool        overlapEnabled_;
    int         rangeNm_;
    double      centerLat_;
    double      centerLon_;
    bool        hasJsonCenter_;
    std::string filter_;
    std::vector<ViewDef> views_;
    bool        messagesToggled_;
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

    std::map<std::string, DWORD> pressStartTime_;
    std::map<std::string, bool>   longPressTriggered_;

    // VACS quick-call menu state
    std::vector<Contact> vacsContacts_;
    bool                 vacsShowMenu_ = false;
    int                  vacsRole_     = 0;
    VacsManager          vacsManager_;
    VacsCallStatus       vacsStatus_;
    VacsCallUiState      vacsCallState_ = VacsCallUiState::Idle;
    DWORD                vacsLastPollTick_ = 0;
    DWORD                vacsFailedPollTick_ = 0;
    bool                 vacsDeferredPoll_ = false;
    DWORD                vacsDeferredPollTick_ = 0;
    DWORD                vacsLastFlashRefreshTick_ = 0;
    DWORD                zoomLastCalcTick_ = 0;
    std::vector<std::string> screenObjectStrings_;

    void addScreenObjectStable(int objectType, const std::string &objectId,
                               RECT area, bool moveable, const std::string &message)
    {
        screenObjectStrings_.push_back(objectId);
        const char *id = screenObjectStrings_.back().c_str();
        screenObjectStrings_.push_back(message);
        const char *msg = screenObjectStrings_.back().c_str();
        AddScreenObject(objectType, id, area, moveable, msg);
    }

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
        SaveDataToAsr("INDRA_APC_ABS_AIR",        "absair",     absAirEnabled_     ? "1" : "0");
        SaveDataToAsr("INDRA_APC_LM0",            "lm0",        lm0Enabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RINGS",          "rings",      ringsEnabled_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_ELW",            "elw",        elwEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_OBI",            "obi",        obiEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_BRIGHT",         "bright",     brightEnabled_     ? "1" : "0");
        SaveDataToAsr("INDRA_APC_F3D",            "f3d",        f3dEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RBL_ALM",        "rblalm",     rblAlarmEnabled_   ? "1" : "0");
        SaveDataToAsr("INDRA_APC_OVERLAP",        "overlap",    overlapEnabled_    ? "1" : "0");
        SaveDataToAsr("INDRA_APC_RECORDING",      "recording",  recording_         ? "1" : "0");
        SaveDataToAsr("INDRA_APC_FILTER",         "filter",     filter_.c_str());
        SaveDataToAsr("INDRA_APC_RTE",            "rte",        rteEnabled_        ? "1" : "0");
        SaveDataToAsr("INDRA_APC_MTCD",           "mtcd",       mtcdEnabled_       ? "1" : "0");
        SaveDataToAsr("INDRA_APC_ALARM",          "alarm",      alarmEnabled_      ? "1" : "0");
        SaveDataToAsr("INDRA_APC_TAGFAMILY",      "tagfamily",  std::to_string(currentTagFamily_).c_str());
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

    void scheduleVacsFlashRefresh()
    {
        if (vacsCallState_ != VacsCallUiState::IncomingRinging &&
            vacsCallState_ != VacsCallUiState::OutgoingRinging)
            return;

        DWORD now = GetTickCount();
        if (vacsLastFlashRefreshTick_ != 0 &&
            now - vacsLastFlashRefreshTick_ < kVacsFlashMs)
            return;

        vacsLastFlashRefreshTick_ = now;
        RequestRefresh();
    }

    void pollVacsState(bool force)
    {
        DWORD now = GetTickCount();
        if (!force && vacsDeferredPoll_)
        {
            if (now - vacsDeferredPollTick_ < 750)
                return;
            vacsDeferredPoll_ = false;
        }

        DWORD interval = 3000;
        if (vacsCallState_ == VacsCallUiState::IncomingRinging ||
            vacsCallState_ == VacsCallUiState::OutgoingRinging)
            interval = 1000;
        else if (vacsCallState_ == VacsCallUiState::Active)
            interval = 1000;

        if (!force && vacsFailedPollTick_ != 0 && now - vacsFailedPollTick_ < 5000)
            return;

        if (!force && vacsLastPollTick_ != 0 && now - vacsLastPollTick_ < interval)
            return;

        vacsLastPollTick_ = now;
        VacsCallUiState previousState = vacsCallState_;
        std::string previousCallId = vacsStatus_.callId;
        if (vacsManager_.RefreshCallStatus(vacsStatus_))
        {
            vacsCallState_ = vacsStatus_.state;
            vacsFailedPollTick_ = 0;
            if (!force &&
                (previousState != vacsCallState_ || previousCallId != vacsStatus_.callId))
                RequestRefresh();
        }
        else
        {
            vacsFailedPollTick_ = now;
        }
    }

    std::string normalizedCallsign(std::string value) const
    {
        value.erase(std::remove_if(value.begin(), value.end(),
            [](unsigned char c) { return std::isspace(c); }), value.end());
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return value;
    }

    bool isOutgoingOrActiveVacsCall() const
    {
        return vacsCallState_ == VacsCallUiState::OutgoingRinging ||
               (vacsCallState_ == VacsCallUiState::Active && !vacsStatus_.incoming);
    }

    bool isActiveVacsCall() const
    {
        return vacsCallState_ == VacsCallUiState::Active;
    }

    bool isIncomingActiveVacsCall() const
    {
        return vacsCallState_ == VacsCallUiState::Active && vacsStatus_.incoming;
    }

    bool isIncomingVacsCall() const
    {
        return vacsCallState_ == VacsCallUiState::IncomingRinging ||
               isIncomingActiveVacsCall();
    }

    bool shouldFlashVacsIncoming() const
    {
        return vacsCallState_ == VacsCallUiState::IncomingRinging &&
               (((GetTickCount() / kVacsFlashMs) % 2) == 0);
    }

    COLORREF incomingVacsFaceColor() const
    {
        if (isIncomingActiveVacsCall()) return rgb(0, 160, 0);
        if (shouldFlashVacsIncoming()) return rgb(0, 180, 0);
        return kColBtnFace;
    }

    COLORREF incomingVacsTextColor() const
    {
        return isIncomingVacsCall() && (isIncomingActiveVacsCall() || shouldFlashVacsIncoming())
            ? rgb(255, 255, 255)
            : kColBtnText;
    }

    bool isCurrentVacsTarget(const std::string &station) const
    {
        return (vacsCallState_ == VacsCallUiState::IncomingRinging ||
                vacsCallState_ == VacsCallUiState::OutgoingRinging ||
                vacsCallState_ == VacsCallUiState::Active) &&
               !vacsStatus_.target.empty() &&
               normalizedCallsign(station) == normalizedCallsign(vacsStatus_.target);
    }

    bool targetIsConfiguredContact() const
    {
        if (vacsStatus_.target.empty()) return false;
        std::string target = normalizedCallsign(vacsStatus_.target);
        for (const auto &contact : vacsContacts_)
            if (normalizedCallsign(contact.station) == target)
                return true;
        return false;
    }

    bool isCurrentCustomVacsTarget() const
    {
        return (vacsCallState_ == VacsCallUiState::IncomingRinging ||
                vacsCallState_ == VacsCallUiState::OutgoingRinging ||
                vacsCallState_ == VacsCallUiState::Active) &&
               !vacsStatus_.target.empty() &&
               !targetIsConfiguredContact();
    }

    bool shouldFlashVacsTarget() const
    {
        return (vacsCallState_ == VacsCallUiState::IncomingRinging ||
                vacsCallState_ == VacsCallUiState::OutgoingRinging) &&
               (((GetTickCount() / kVacsFlashMs) % 2) == 0);
    }

    COLORREF currentVacsTargetFaceColor(bool flash, bool solid) const
    {
        if (!flash && !solid) return kColBtnFace;
        return vacsStatus_.incoming ? rgb(0, 160, 0) : rgb(0, 96, 210);
    }

    bool isViewButton(const std::string &value) const
    {
        static const char *buttons[] = {"S", "0", "1/2", "1", "3", "5", "8", "VIEW1", "VIEW2"};
        for (const char *button : buttons)
            if (value == button)
                return true;
        return false;
    }

    void drawLowResourceButton(HDC hdc, const std::string &id, const char *label,
                               RECT r, bool active)
    {
        drawLowResourceButtonEx(hdc, id, label, r,
                                active ? rgb(0, 86, 150) : kColBtnFace,
                                active ? rgb(255, 255, 255) : kColBtnText);
    }

    void drawLowResourceButtonEx(HDC hdc, const std::string &id, const char *label,
                                 RECT r, COLORREF face, COLORREF text)
    {
        fillSolid(hdc, r, face);
        FrameRect(hdc, &r, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        SetTextColor(hdc, text);
        DrawTextA(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        addScreenObjectStable(kObjButton, id, r, false, label);
    }

    void drawLowResourceBottomBar(HDC hdc)
    {
        RECT bar = bottomBarArea();
        const int colW = 58;
        const int h = 20;
        const int gap = 4;
        int reservedX = bar.left + 6;
        int reservedW = colW + gap;

        // When messages toggled, grey background starts at ARR/DEP (after reserved space)
        int arrDepX = reservedX + reservedW;
        RECT bgRect = messagesToggled_
            ? RECT{ arrDepX, bar.top, bar.right, bar.bottom }
            : bar;
        fillSolid(hdc, bgRect, kColBarBg);
        SetBkMode(hdc, TRANSPARENT);

        int x = bar.left + 6;
        int y = bar.top + 7;

        auto buttonAt = [&](int bx, int by, int bw, const std::string &id,
                            const char *label, bool active = false) {
            RECT r = { bx, by, bx + bw, by + h };
            drawLowResourceButton(hdc, id, label, r, active);
        };

        auto stack = [&](std::initializer_list<std::pair<const char*, bool>> items) {
            int row = 0;
            for (const auto &item : items)
            {
                if (item.first && *item.first)
                    buttonAt(x, y + row * h, colW, item.first, item.first, item.second);
                ++row;
            }
            x += colW + gap;
        };

        auto single = [&](const std::string &id, const char *label, int bw, bool active = false) {
            buttonAt(x, y + h, bw, id, label, active);
            x += bw + gap;
        };

        if (!messagesToggled_)
        {
            COLORREF vacsFace = incomingVacsFaceColor();
            COLORREF vacsText = incomingVacsTextColor();
            RECT exec = { x, y, x + colW, y + h };
            RECT plan = { x, y + h, x + colW, y + h + h };
            if (isIncomingVacsCall())
            {
                drawLowResourceButtonEx(hdc, "EXECUTIVE", "EXEC", exec, vacsFace, vacsText);
                drawLowResourceButtonEx(hdc, "PLANNER", "PLAN", plan, vacsFace, vacsText);
            }
            else
            {
                buttonAt(x, y, colW, "EXECUTIVE", "EXEC", vacsShowMenu_ && vacsRole_ == 0);
                buttonAt(x, y + h, colW, "PLANNER", "PLAN", vacsShowMenu_ && vacsRole_ == 1);
            }
        }

        x += colW + gap;  // Always advance, so nothing shifts when buttons are hidden

        if (vacsShowMenu_ && !messagesToggled_)
        {
            int maxX = bar.right - 130;
            for (int i = 0; i < static_cast<int>(vacsContacts_.size()) && x + 84 < maxX; ++i)
            {
                RECT r = { x, y, x + 84, y + h };
                bool active = isCurrentVacsTarget(vacsContacts_[i].station);
                bool flash = active && shouldFlashVacsTarget();
                bool solid = active && vacsCallState_ == VacsCallUiState::Active;
                drawLowResourceButtonEx(hdc, "VACS_" + std::to_string(i),
                                      vacsContacts_[i].name.c_str(), r,
                                      currentVacsTargetFaceColor(flash, solid),
                                      (flash || solid) ? rgb(255, 255, 255) : kColBtnText);
                x += 84 + gap;
            }
            RECT incoming = { x, y, x + 66, y + h };
            drawLowResourceButtonEx(hdc, "INCOMING", "IN", incoming,
                                    incomingVacsFaceColor(), incomingVacsTextColor());
            x += 66 + gap;
            RECT custom = { x, y, x + 74, y + h };
            bool customCurrent = isCurrentCustomVacsTarget();
            bool customFlash = customCurrent && shouldFlashVacsTarget();
            bool customSolid = customCurrent && vacsCallState_ == VacsCallUiState::Active;
            std::string customLabel = customCurrent ? vacsStatus_.target : "CUSTOM";
            drawLowResourceButtonEx(hdc, "VACS_CUSTOM", customLabel.c_str(), custom,
                                    currentVacsTargetFaceColor(customFlash, customSolid),
                                    (customFlash || customSolid) ? rgb(255, 255, 255) : kColBtnText);
        }
        else
        {
            stack({{"ARR", filter_ == "ARRIVALS"}, {"DEP", filter_ == "DEPARTURES"}, {"FPL", false}});
            stack({{"CPDLC", false}, {"ABS AIR", absAirEnabled_}});
            stack({{"VIEW1", false}, {"VIEW2", false}});
            stack({{"LM 0", lm0Enabled_}, {"RINGS", ringsEnabled_}});
            stack({{"AREAS", false}, {"ELW", elwEnabled_}});
            stack({{"RTE", rteEnabled_}, {"RBL", rblEnabled_}, {"OBI", obiEnabled_}});
            stack({{"DATBLK", false}, {"BRIGHT", brightEnabled_}, {"F 3D", f3dEnabled_}});
            stack({{"QDM", qdmMode_}, {"SEP", false}, {"METEO", false}});
            stack({{"RBL ALM", rblAlarmEnabled_}, {"MTCD", mtcdEnabled_}, {"ALM OFF", !alarmEnabled_}});
            stack({{"OVERLAP", overlapEnabled_}, {"FREETEXT", showFreeText_}, {"SECTORS", false}});
            stack({{"LAST POS", false}, {"FINDER", false}, {"SSR F", false}});
            stack({{"USERS", false}});

            single("+", "+", 24);
            single("-", "-", 24);
            single("^", "^", 24);
            single("<", "<", 24);
            single(">", ">", 24);
            single("v", "v", 24);
            single("EXP +", "EXP+", 42);
            single("EXP -", "EXP-", 42);
            single("CEN", "CEN", 38);

            const char *views[] = {"S", "0", "1/2", "1", "3", "5", "8"};
            for (const char *view : views)
                single(view, view, 28);

            single("MESSAGES", "MSG", 42, messagesToggled_);
        }

        RECT status = { bar.right - 116, bar.top + 8, bar.right - 8, bar.top + 32 };
        SetTextColor(hdc, kColBtnText);
        DrawTextA(hdc, "vINDRA", -1, &status, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    void drawBottomBar(HDC hdc)
    {
        RECT bar = bottomBarArea();
        int colW = 72;
        int gap = 4;
        int reservedX = bar.left + 6;
        int reservedW = colW + gap;

        // When messages toggled, grey background starts at ARR/DEP (after reserved space)
        int arrDepX = reservedX + reservedW;
        RECT bgRect = messagesToggled_
            ? RECT{ arrDepX, bar.top, bar.right, bar.bottom }
            : bar;
        fillSolid(hdc, bgRect, kColBarBg);

        HPEN topPen = cachedPen(kColBarFrame);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, topPen));
        MoveToEx(hdc, arrDepX, bar.top, nullptr);
        LineTo  (hdc, bar.right, bar.top);
        SelectObject(hdc, oldPen);

        int y0 = bar.top + kBtnMargin;
        int bh = kBtnH;
        int x = bar.left + 6;

        auto drawSeparator = [&](int xPos) {
            HPEN sepPen = cachedPen(kColBarFrame);
            HPEN old = static_cast<HPEN>(SelectObject(hdc, sepPen));
            MoveToEx(hdc, xPos, bar.top + 4, nullptr);
            LineTo(hdc, xPos, bar.bottom - 4);
            SelectObject(hdc, old);
        };

        int vacsWidth = drawVacsGroup(hdc, x, y0, colW, bh);
        x += vacsWidth + gap;

        if (vacsShowMenu_ && !messagesToggled_)
        {
            drawBottomRightBranding(hdc, bar);
            return;
        }

        drawVerticalGroup(hdc, x, y0, colW, bh, "ARR", "DEP", "FPL",
                          filter_ == "ARRIVALS", filter_ == "DEPARTURES", false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "CPDLC", "ABS AIR", "",
                          false, absAirEnabled_, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "VIEW1", "VIEW2", "",
                          false, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "LM 0", "RINGS", "",
                          lm0Enabled_, ringsEnabled_, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "AREAS", "ELW", "",
                          false, elwEnabled_, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "RTE", "RBL", "OBI",
                          rteEnabled_, rblEnabled_, obiEnabled_);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "DATBLK", "BRIGHT", "F 3D",
                          false, brightEnabled_, f3dEnabled_);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "QDM", "SEP", "METEO",
                          qdmMode_, false, false);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "RBL ALM", "MTCD", "ALM OFF",
                          rblAlarmEnabled_, mtcdEnabled_, !alarmEnabled_);
        x += colW + gap;
        drawVerticalGroup(hdc, x, y0, colW, bh, "OVERLAP", "FREETEXT", "SECTORS",
                          overlapEnabled_, showFreeText_, false);
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

        DWORD now = GetTickCount();
        if (zoomLastCalcTick_ == 0 || now - zoomLastCalcTick_ > 1000)
        {
            EuroScopePlugIn::CPosition zoomLd, zoomRu;
            GetDisplayArea(&zoomLd, &zoomRu);
            currentZoom_ = currentRangeFromDisplay(zoomLd, zoomRu);
            rangeNm_ = currentZoom_;
            zoomLastCalcTick_ = now;
        }
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
            bool toggled = messagesToggled_;
            RECT msgRect = { x, y0 + bh / 4, x + 76, y0 + bh / 4 + bh / 2 };
            COLORREF faceCol = toggled ? kColBtnPress : kColBtnFace;
            fillSolid(hdc, msgRect, faceCol);

            HPEN hiPen2 = cachedPen(toggled ? kColBtnShEdge : kColBtnHiEdge);
            HPEN shPen2 = cachedPen(toggled ? kColBtnHiEdge : kColBtnShEdge);
            HPEN oldPen2 = static_cast<HPEN>(SelectObject(hdc, hiPen2));
            MoveToEx(hdc, msgRect.left,      msgRect.bottom - 1, nullptr);
            LineTo  (hdc, msgRect.left,      msgRect.top);
            LineTo  (hdc, msgRect.right - 1, msgRect.top);
            SelectObject(hdc, shPen2);
            MoveToEx(hdc, msgRect.right - 1, msgRect.top,        nullptr);
            LineTo  (hdc, msgRect.right - 1, msgRect.bottom - 1);
            LineTo  (hdc, msgRect.left,      msgRect.bottom - 1);
            SelectObject(hdc, oldPen2);

            SetBkMode   (hdc, TRANSPARENT);
            SetTextColor(hdc, kColBtnText);
            HFONT fnt2 = cachedFont(-10, FW_NORMAL, "Courier New", FIXED_PITCH | FF_MODERN);
            HFONT oldFnt2 = static_cast<HFONT>(SelectObject(hdc, fnt2));
            DrawTextA(hdc, "MESSAGES", -1, &msgRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, oldFnt2);
            AddScreenObject(kObjButton, "MESSAGES", msgRect, false, "Messages");
        }

        drawBottomRightBranding(hdc, bar);
    }

    int drawVacsGroup(HDC hdc, int x, int y, int colW, int totalH)
    {
        if (messagesToggled_)
            return colW;  // Reserve space without drawing, so nothing else shifts

        int h2 = totalH / 2;
        bool incomingCall = isIncomingVacsCall();
        drawBtnEx(hdc, x, y,      colW, h2, "EXECUTIVE", "EXECUTIVE",
                  incomingCall ? false : (vacsShowMenu_ && vacsRole_ == 0),
                  incomingCall ? incomingVacsFaceColor() : kColBtnFace,
                  incomingCall ? incomingVacsTextColor() : kColBtnText);
        drawBtnEx(hdc, x, y + h2, colW, h2, "PLANNER", "PLANNER",
                  incomingCall ? false : (vacsShowMenu_ && vacsRole_ == 1),
                  incomingCall ? incomingVacsFaceColor() : kColBtnFace,
                  incomingCall ? incomingVacsTextColor() : kColBtnText);
        if (vacsShowMenu_)
        {
            int menuX = x + colW + kBtnGap;
            int contactsW = drawCallMenu(hdc, menuX, y, totalH);
            int controlsX = menuX + contactsW + (contactsW > 0 ? kBtnGap : 0);
            drawVacsPanelControls(hdc, controlsX, y, colW, totalH);
            return colW + kBtnGap + contactsW + (contactsW > 0 ? kBtnGap : 0) + colW;
        }
        return colW;
    }

    int drawCallMenu(HDC hdc, int menuX, int menuY, int totalH)
    {
        const int btnW = 96;
        const int btnH = (totalH - kBtnGap) / 2;
        int cols = static_cast<int>((vacsContacts_.size() + 1) / 2);
        for (int i = 0; i < static_cast<int>(vacsContacts_.size()); ++i)
        {
            int bx = menuX + (i / 2) * (btnW + kBtnGap);
            int by = menuY + (i % 2) * (btnH + kBtnGap);
            std::string id = "VACS_" + std::to_string(i);
            bool currentTarget = isCurrentVacsTarget(vacsContacts_[i].station);
            bool flash = currentTarget && shouldFlashVacsTarget();
            bool solid = currentTarget && vacsCallState_ == VacsCallUiState::Active;
            COLORREF face = currentVacsTargetFaceColor(flash, solid);
            COLORREF text = (flash || solid) ? rgb(255, 255, 255) : kColBtnText;
            drawBtnEx(hdc, bx, by, btnW, btnH, id.c_str(), vacsContacts_[i].name.c_str(),
                      false, face, text);
        }
        return cols > 0 ? cols * btnW + (cols - 1) * kBtnGap : 0;
    }

    void drawVacsPanelControls(HDC hdc, int x, int y, int colW, int totalH)
    {
        int h2 = totalH / 2;
        bool customCurrent = isCurrentCustomVacsTarget();
        bool customFlash = customCurrent && shouldFlashVacsTarget();
        bool customSolid = customCurrent && vacsCallState_ == VacsCallUiState::Active;
        COLORREF customFace = currentVacsTargetFaceColor(customFlash, customSolid);
        COLORREF customText = (customFlash || customSolid) ? rgb(255, 255, 255) : kColBtnText;
        std::string customLabel = customCurrent ? vacsStatus_.target : "CUSTOM";

        drawBtnEx(hdc, x, y, colW, h2, "INCOMING", "INCOMING",
                  false, incomingVacsFaceColor(), incomingVacsTextColor());
        drawBtnEx(hdc, x, y + h2, colW, h2, "VACS_CUSTOM", customLabel.c_str(),
                  false, customFace, customText);

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
        return drawBtnEx(hdc, x, y, w, h, label, label, pressed, kColBtnFace, kColBtnText);
    }

    int drawBtnEx(HDC hdc, int x, int y, int w, int h,
                  const char *objId, const char *label, bool pressed,
                  COLORREF faceColor, COLORREF textColor)
    {
        RECT r = { x, y, x + w, y + h };
        COLORREF faceCol = pressed ? kColBtnPress : faceColor;
        fillSolid(hdc, r, faceCol);

        HPEN hiPen = cachedPen(pressed ? kColBtnShEdge : kColBtnHiEdge);
        HPEN shPen = cachedPen(pressed ? kColBtnHiEdge : kColBtnShEdge);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, hiPen));
        MoveToEx(hdc, r.left,      r.bottom - 1, nullptr);
        LineTo  (hdc, r.left,      r.top);
        LineTo  (hdc, r.right - 1, r.top);
        SelectObject(hdc, shPen);
        MoveToEx(hdc, r.right - 1, r.top,        nullptr);
        LineTo  (hdc, r.right - 1, r.bottom - 1);
        LineTo  (hdc, r.left,      r.bottom - 1);
        SelectObject(hdc, oldPen);

        SetBkMode   (hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        HFONT font = cachedButtonFont();
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
        DrawTextA(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        addScreenObjectStable(kObjButton, objId ? objId : "", r, false, label ? label : "");
        return x + w + kBtnGap;
    }

    void fillSolid(HDC hdc, const RECT &r, COLORREF color)
    {
        HBRUSH brush = cachedBrush(color);
        FillRect(hdc, &r, brush);
    }

    HBRUSH cachedBrush(COLORREF color)
    {
        static std::map<COLORREF, HBRUSH> brushes;
        auto it = brushes.find(color);
        if (it != brushes.end()) return it->second;
        HBRUSH brush = CreateSolidBrush(color);
        brushes[color] = brush;
        return brush;
    }

    HPEN cachedPen(COLORREF color)
    {
        return cachedPen(PS_SOLID, 1, color);
    }

    HPEN cachedPen(int style, int width, COLORREF color)
    {
        struct PenKey
        {
            int style;
            int width;
            COLORREF color;

            bool operator<(const PenKey &other) const
            {
                if (style != other.style) return style < other.style;
                if (width != other.width) return width < other.width;
                return color < other.color;
            }
        };

        static std::map<PenKey, HPEN> pens;
        PenKey key = { style, width, color };
        auto it = pens.find(key);
        if (it != pens.end()) return it->second;
        HPEN pen = CreatePen(style, width, color);
        pens[key] = pen;
        return pen;
    }

    HFONT cachedFont(int height, int weight, const char *face, DWORD pitchAndFamily)
    {
        struct FontKey
        {
            int height;
            int weight;
            DWORD pitchAndFamily;
            std::string face;

            bool operator<(const FontKey &other) const
            {
                if (height != other.height) return height < other.height;
                if (weight != other.weight) return weight < other.weight;
                if (pitchAndFamily != other.pitchAndFamily) return pitchAndFamily < other.pitchAndFamily;
                return face < other.face;
            }
        };

        static std::map<FontKey, HFONT> fonts;
        FontKey key = { height, weight, pitchAndFamily, face ? face : "" };
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        HFONT font = CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, pitchAndFamily, face);
        fonts[key] = font;
        return font;
    }

    HFONT cachedButtonFont()
    {
        return cachedFont(-10, FW_NORMAL, "Courier New", FIXED_PITCH | FF_MODERN);
    }

    void drawBottomRightBranding(HDC hdc, const RECT &bar)
    {
        const int w = 170;
        const int h = 54;
        RECT box = { bar.right - w - 28, bar.top + (rectH(bar) - h) / 2,
                     bar.right - 28, bar.top + (rectH(bar) + h) / 2 };

        SetBkMode(hdc, TRANSPARENT);
        HPEN pen = cachedPen(rgb(168, 170, 154));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH dotBrush = cachedBrush(rgb(168, 170, 154));
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

        HFONT logoFont = cachedLogoFont();
        HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, logoFont));
        SetTextColor(hdc, rgb(218, 216, 196));
        RECT textRect = { box.left + 62, box.top + 2, box.right, box.bottom - 2 };
        DrawTextA(hdc, "vINDRA", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
    }

    HFONT cachedLogoFont()
    {
        return cachedFont(-24, FW_BOLD, "Arial Rounded MT Bold", DEFAULT_PITCH | FF_SWISS);
    }

    void drawSearchHighlight(HDC hdc)
    {
        HPEN dashPen = cachedPen(PS_DASH, 1, rgb(180, 180, 180));
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
    }

    void drawSelectedRoute(HDC hdc)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) return;
        EuroScopePlugIn::CFlightPlanExtractedRoute route = fp.GetExtractedRoute();
        int n = route.GetPointsNumber();
        if (n < 2) return;
        HPEN pen = cachedPen(PS_SOLID, 2, rgb(0, 220, 80));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        for (int i = 0; i < n - 1; ++i)
        {
            POINT p1 = ConvertCoordFromPositionToPixel(route.GetPointPosition(i));
            POINT p2 = ConvertCoordFromPositionToPixel(route.GetPointPosition(i + 1));
            MoveToEx(hdc, p1.x, p1.y, nullptr);
            LineTo  (hdc, p2.x, p2.y);
        }
        SelectObject(hdc, oldPen);
    }

    void drawQDMLine(HDC hdc)
    {
        if (!qdmLineValid_) return;
        HPEN pen = cachedPen(PS_SOLID, 2, rgb(255, 255, 0));
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
        HFONT fnt = cachedFont(-11, FW_BOLD, "Courier New", FIXED_PITCH | FF_MODERN);
        HFONT oldFnt = static_cast<HFONT>(SelectObject(hdc, fnt));
        TextOutA(hdc, mx + 4, my - 14, buf, (int)strlen(buf));
        SelectObject(hdc, oldFnt);
        SelectObject(hdc, oldPen);
    }

    void handleButton(const std::string &s, POINT pt, RECT area)
    {
        if (s == "ARR")                  setFilter("ARRIVALS");
        else if (s == "DEP")             setFilter("DEPARTURES");
        else if (s == "FPL")             openSelectedFlightPlan(pt, area);
        else if (s == "RTE")
        {
            rteEnabled_ = !rteEnabled_;
            toggleTopSkyRte(pt, area);
        }
        else if (s == "ABS AIR")
        {
            absAirEnabled_ = !absAirEnabled_;
            showAirspace_ = absAirEnabled_;
            applySectorType(EuroScopePlugIn::SECTOR_ELEMENT_AIRSPACE, showAirspace_);
            RefreshMapContent();
        }
        else if (s == "CPDLC")           openCpdlcWindows(pt, area);
        else if (s == "LM 0")            toggleLatchedButton(lm0Enabled_, "LM 0");
        else if (s == "RINGS")           toggleLatchedButton(ringsEnabled_, "Range rings");
        else if (s == "SEP")             openSepTool(pt, area);
        else if (s == "METEO")           { /* inert */ }
        else if (s == "MTCD")            toggleMTCD();
        else if (s == "ALM OFF")         { alarmEnabled_ = !alarmEnabled_; }
        else if (s == "AREAS" || s == "SECTORS") showDisplaySettingsPicker(area);
        else if (s == "ELW")             toggleLatchedButton(elwEnabled_, "ELW");
        else if (s == "RBL")             toggleLatchedButton(rblEnabled_, "RBL");
        else if (s == "OBI")             toggleLatchedButton(obiEnabled_, "OBI");
        else if (s == "DATBLK")          openEuroScopeDisplaySettings();
        else if (s == "BRIGHT")          toggleLatchedButton(brightEnabled_, "Bright mode");
        else if (s == "F 3D")            toggleLatchedButton(f3dEnabled_, "F 3D");
        else if (s == "RBL ALM")         toggleLatchedButton(rblAlarmEnabled_, "RBL alarm");
        else if (s == "OVERLAP")         toggleLatchedButton(overlapEnabled_, "Overlap");
        else if (s == "FREETEXT")
        {
            openTopSkyOpText2(pt, area);
        }
        else if (s == "FINDER")          showFinderPopup(area);
        else if (s == "SSR F")           showSSRFPopup(area);
        else if (s == "LAST POS")        centerScreen();
        else if (s == "USERS")           openConnectionDialog(pt, area);
        else if (s == "VIEW1")           applyView("VIEW1");
        else if (s == "VIEW2")           applyView("VIEW2");
        else if (s == "CEN")             applyView("1");
        else if (s == "EXP +")           zoomIn();
        else if (s == "EXP -")           zoomOut();
        else if (s == "+")               zoomIn();
        else if (s == "-")               zoomOut();
        else if (s == "^")               pan(0, -160);
        else if (s == "v")               pan(0,  160);
        else if (s == "<")               pan(-160, 0);
        else if (s == ">")               pan( 160, 0);
        else if (s == "MESSAGES")
        {
            messagesToggled_ = !messagesToggled_;
            RequestRefresh();
            return;
        }
        saveSettings();
        RequestRefresh();
    }

    void toggleLatchedButton(bool &state, const char *label)
    {
        (void)label;
        state = !state;
    }

    void applyView(const std::string &button)
    {
        updateDataPosition();
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
            return;
        }
    }

    void saveCurrentView(const std::string &button)
    {
        updateDataPosition();
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
        }
        else if (filter_ == "DEPARTURES")
        {
            std::string cmd = ".filter departures";
            if (!controllerAirport_.empty()) cmd += " " + controllerAirport_;
            GetPlugIn()->OnCompileCommand(cmd.c_str());
        }
        else
        {
            GetPlugIn()->OnCompileCommand(".filter clear");
        }
        RequestRefresh();
    }

    void toggleMTCD()
    {
        mtcdEnabled_ = !mtcdEnabled_;
        GetPlugIn()->OnCompileCommand(mtcdEnabled_ ? ".mtcd on" : ".mtcd off");
    }

    void toggleQDMMode()
    {
        qdmMode_ = false;
        qdmFirstClick_ = false;
        qdmLineValid_ = false;
        sendEuroScopeShortcut(VK_MENU, 'Q');
        RequestRefresh();
    }

    void sendEuroScopeShortcut(WORD modifier, WORD keyCode)
    {
        INPUT inputs[4] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = modifier;
        inputs[1].type = INPUT_KEYBOARD;
        inputs[1].ki.wVk = keyCode;
        inputs[2].type = INPUT_KEYBOARD;
        inputs[2].ki.wVk = keyCode;
        inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
        inputs[3].type = INPUT_KEYBOARD;
        inputs[3].ki.wVk = modifier;
        inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, inputs, sizeof(INPUT));
    }

    void handleQDMClick(POINT clickPt)
    {
        if (!qdmFirstClick_)
        {
            qdmPoint1_ = ConvertCoordFromPixelToPosition(clickPt);
            qdmFirstClick_ = true;
        }
        else
        {
            qdmPoint2_ = ConvertCoordFromPixelToPosition(clickPt);
            qdmFirstClick_ = false;
            qdmMode_ = false;
            qdmLineValid_ = true;
            RequestRefresh();
        }
    }

    void onTagFamilySelected(int family)
    {
        currentTagFamily_ = family;
        char cmd[32];
        snprintf(cmd, sizeof(cmd), ".tagfamily %d", family);
        GetPlugIn()->OnCompileCommand(cmd);
    }

    void openEuroScopeDisplaySettings()
    {
        if (invokeEuroScopeMenu({"display settings"}))
            return;
        if (invokeEuroScopeMenu({"other settings", "display"}))
            return;
    }

    void openConnectionDialog(POINT pt, RECT area)
    {
        (void)pt;
        (void)area;
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
        if (!me.IsValid())
        {
            controllerAirport_.clear();
            return;
        }
        std::string cs = me.GetCallsign();
        if (cs.empty())
        {
            controllerAirport_.clear();
            return;
        }
        std::size_t u = cs.find('_');
        controllerAirport_ = (u != std::string::npos) ? cs.substr(0, u) : cs;
    }

    void updateDataPosition()
    {
        EuroScopePlugIn::CController me = GetPlugIn()->ControllerMyself();
        if (!me.IsValid())
        {
            controllerAirport_.clear();
            setActiveDataPosition("");
            return;
        }
        std::string cs = me.GetCallsign();
        if (cs.empty())
        {
            controllerAirport_.clear();
            setActiveDataPosition("");
            return;
        }
        std::size_t u = cs.find('_');
        controllerAirport_ = (u != std::string::npos) ? cs.substr(0, u) : cs;
        setActiveDataPosition(cs);
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
            return false;

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
        if (!fp.IsValid()) return;
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
        if (!fp.IsValid()) return;
        invokeTopSkyFn(fp.GetCallsign(), TopSky::TOGGLE_ROUTE_DRAW_MTCD_SAP, pt, area);
    }

    void openCpdlcWindows(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) return;
        const char *cs = fp.GetCallsign();
        invokeTopSkyFn(cs, TopSky::OPEN_CPDLC_CURRENT_MESSAGE_WINDOW_CPDLC_W, pt, area);
    }

    void openSepTool(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) return;
        invokeTopSkyFn(fp.GetCallsign(), TopSky::INVOKE_SEP_TOOL_WITH_VSEP, pt, area);
    }

    void openTopSkyOpText2(POINT pt, RECT area)
    {
        EuroScopePlugIn::CFlightPlan fp = aselFp();
        if (!fp.IsValid()) return;
        const char *cs = fp.GetCallsign();
        GetPlugIn()->SetASELAircraft(fp);
        StartTagFunction(
            cs,
            nullptr,
            EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN,
            cs,
            kTopSkyPluginName,
            TopSky::EDIT_OP_TEXT2,
            pt,
            area
        );
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
                    RequestRefresh();
                    return;
                }
            }
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
                        RequestRefresh();
                        return;
                    }
                }
            }
            fp = GetPlugIn()->FlightPlanSelectNext(fp);
        }
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

    EuroScopePlugIn::CFlightPlan   aselFp() { return GetPlugIn()->FlightPlanSelectASEL();  }
    EuroScopePlugIn::CRadarTarget  aselRt() { return GetPlugIn()->RadarTargetSelectASEL(); }
};

EuroScopePlugIn::CRadarScreen *CreateIndraApcScreen()
{
    return new IndraApcScreen();
}

} // namespace Indra
