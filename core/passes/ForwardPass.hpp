#pragma once

#include "RenderPass.hpp"
#include "FrameInfo.hpp"
#include "SolidRenderSystem.hpp"
#include "PointLightRenderSystem.hpp"

namespace Engine
{
    /**
     * @brief Clears "backbuffer" + "depth", then draws the solid objects and the point lights into them.
     *
     * Only uses the render systems and frameInfo (Application::run() owns them),
     * so they must outlive every RenderGraph::execute() call.
     */
    class ForwardPass : public RenderPass
    {
    public:
        ForwardPass(FrameInfo& frameInfo, PointLightRenderSystem& lights);

        /// vkCmdBeginRendering (backbuffer + depth, both cleared) -> solid + point light draws -> vkCmdEndRendering.
        void record(RenderPassContext& ctx) override;

    private:
        FrameInfo& frameInfo;
        PointLightRenderSystem& lights;
    };
}
