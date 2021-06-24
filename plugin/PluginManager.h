#pragma once
#include <vector>
#include <Windows.h>
#include "Plugin.h"

class PluginManager {
private:
	std::vector<Plugin*> m_Plugins;

	//show settings by default
	bool m_ShowSettings = true;
public:
	PluginManager();
	void Present();
	void WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

