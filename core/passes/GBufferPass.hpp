#pragma once

#include "RenderPass.hpp"
#include "SolidRenderSystem.hpp"
#include "FrameInfo.hpp"

namespace Engine
{
    class GBufferPass : public RenderPass
    {
    public:
        GBufferPass(FrameInfo& frameInfo, SolidRenderSystem& solid);

        void record(RenderPassContext& ctx) override;
    private:
        FrameInfo& frameInfo;
        SolidRenderSystem &solid;
    };
}