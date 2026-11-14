#pragma once

#include "RenderPass.hpp"
#include "FrameInfo.hpp"
#include "LightingSystem.hpp"

namespace Engine
{
    class LightingPass : public RenderPass
    {
    public:
        LightingPass(FrameInfo& frameInfo, LightingSystem& lightingSystem);
        void onResourcesAllocated(const std::function<const RenderResource&(const std::string&)>& get) override;
        void record(RenderPassContext& ctx) override;
    private:
        FrameInfo& frameInfo;
        LightingSystem& lightingSystem;
    };
}