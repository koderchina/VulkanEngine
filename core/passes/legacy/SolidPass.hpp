#pragma once

#include "FrameInfo.hpp"
#include "SolidRenderSystem.hpp"
#include "RenderPass.hpp"
namespace Engine
{
    class SolidPass : public RenderPass
    {
    public:
        SolidPass(FrameInfo& frameInfo, SolidRenderSystem& solid);

        void record(RenderPassContext& ctx) override;
    private:
        FrameInfo& frameInfo;
        SolidRenderSystem& solid;
    };
}