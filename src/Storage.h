#pragma once
#include "Common.h"

namespace Indra
{

std::string moduleDirectory();
std::string dataDirectory();
std::string readFile(const std::string &path);

bool jsonString(const std::string &obj, const char *key, std::string &out);
bool jsonNumber(const std::string &obj, const char *key, double &out);

std::vector<ViewDef> loadViewsJson(const std::string &filename = "indra_saved_views.json");
void saveViewToJson(const std::string &button, const ViewDef &view);

} // namespace Indra
