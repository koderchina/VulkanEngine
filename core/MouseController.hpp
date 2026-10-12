#pragma once

#include "GameObject.hpp"
#include "Window.hpp"

namespace Engine
{
    /**
     * @brief Tracks GLFW cursor delta to rotate a GameObject (mouse-look for the free-fly camera).
     *
     * Holds no Vulkan handles; uses glfwSetInputMode()/glfwGetCursorPos() only.
     */
    class MouseController
    {
    public:
        /**
         * @brief Disables and recenters the cursor (glfwSetInputMode(GLFW_CURSOR_DISABLED)) and enables raw motion if supported.
         * @param window GLFWwindow to capture the cursor on.
         */
        MouseController(GLFWwindow *window);
        /**
         * @brief Reads the cursor delta since the last call (glfwGetCursorPos()) and applies it to gameObject's rotation.
         * @param dt Frame time in seconds (currently unused in the rotation math, kept for API symmetry with KeyboardController).
         * @param gameObject Object (typically the camera's "viewer" object) whose TransformComponent is updated.
         */
        void move(float dt, GameObject &gameObject);
    private:

        GLFWwindow *window;
        float lookSpeed{0.005f};
        double lastX{0.0};
        double lastY{0.0};
        bool firstMouse{true};
    };
}
