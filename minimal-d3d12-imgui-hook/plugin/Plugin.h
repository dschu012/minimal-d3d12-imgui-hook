#pragma once

class Plugin {
protected:
public:
	virtual void OnLoad() {};
	virtual void OnUnload() {};
	virtual void OnDraw() {};
};