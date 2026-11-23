#pragma once

namespace CityExplorer {
	bool initVulkan();
	void shutdownVulkan();
	void recreateSwapchain();

	void drawFrame();
}
