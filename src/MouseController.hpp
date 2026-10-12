#pragma once

#include "GameObject.hpp"
#include "Window.hpp"

namespace Engine
{
    class MouseController
    {
    public:
        MouseController(GLFWwindow *window);
        void move(float dt, GameObject &gameObject);
    private:

        GLFWwindow *window;
        float lookSpeed{0.005f};
        double lastX{0.0};
        double lastY{0.0};
        bool firstMouse{true};
    };
}