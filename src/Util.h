#pragma once

#include <vector>

#ifdef DSIP_GUI
class Window;
#endif

namespace Util
{

#ifdef DSIP_GUI
void SetupPlatformMenu(Window* window);
#endif

std::vector<char> BytesFromBundle(const char* file_path);

std::vector<char> BytesFromFile(const char* file_path);

}
