#pragma once
#include <winsock2.h>
#include "Common.h"
#include <nlohmann/json.hpp>

namespace Indra
{

std::string moduleDirectory();
std::string dataDirectory();
std::string readFile(const std::string &path);
std::string readDataFile(const std::string &filename);

bool jsonString(const std::string &obj, const char *key, std::string &out);
bool jsonNumber(const std::string &obj, const char *key, double &out);

void setActiveDataPosition(const std::string &position);
std::vector<ViewDef> loadViewsJson(const std::string &filename = "indra_saved_views.json");
void saveViewToJson(const std::string &button, const ViewDef &view);

struct Contact { std::string name, station; };

std::vector<Contact> loadContactsJson(const std::string &filename = "settings.json");

enum class VacsCallUiState
{
    Idle,
    IncomingRinging,
    OutgoingRinging,
    Active
};

struct VacsCallStatus
{
    VacsCallUiState state = VacsCallUiState::Idle;
    std::string     callId;
    std::string     target;
};

class VacsManager
{
public:
    VacsManager();

    void StartVacsCall(const std::string &station);
    bool RefreshCallStatus(VacsCallStatus &status);
    bool AcceptIncomingCall();
    bool EndCurrentCall();

private:
    std::string activeCallId_;
    std::string incomingCallId_;
    std::string activeTarget_;
    bool        activeCall_;
    unsigned    nextRequestId_;

    bool invoke(const std::string &command,
                const std::string &args,
                nlohmann::json &data);
    bool readSession(nlohmann::json &session);
    bool sendRemoteCommand(SOCKET sock,
                           const std::string &command,
                           const std::string &args,
                           nlohmann::json &data);
    SOCKET connectRemote();
    bool sendTextFrame(SOCKET sock, const std::string &payload);
    bool readTextFrame(SOCKET sock, std::string &payload);
    std::string nextId();
};

void vacsCall(const std::string &station);

} // namespace Indra
