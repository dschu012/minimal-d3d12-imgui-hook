#pragma once
#include "../Plugin.h"

class Sample : public Plugin {
public:
	void OnLoad() override;
	void OnUnload() override;
	void OnDraw() override;
};

