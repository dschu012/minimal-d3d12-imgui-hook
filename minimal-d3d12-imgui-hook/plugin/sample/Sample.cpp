#include "Sample.h"
#include <imgui.h>
#include <array>

void Sample::OnLoad() {
}

void Sample::OnUnload() {
	
}

void Sample::OnDraw() {
	auto drawList = ImGui::GetBackgroundDrawList();

	//simple shape
	drawList->AddCircleFilled({ 100.f, 100.f }, 30.f, IM_COL32(255, 0, 0, 128));
}
