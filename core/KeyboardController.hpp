#pragma once

#include <GameObject.hpp>
#include <Window.hpp>

namespace Engine
{
    /**
     * @brief Polls GLFW keyboard state each frame to move/rotate a GameObject (used for free-fly camera movement).
     *
     * Holds no Vulkan handles; moveInPlaneXZ() only calls glfwGetKey() and
     * writes into the target GameObject's TransformComponent.
     */
    class KeyboardController
    {
    public:
        /// GLFW key codes (GLFW_KEY_*) mapped to each movement/look action.
        struct KeyMappings
        {
            int moveLeft = GLFW_KEY_A;
            int moveRight = GLFW_KEY_D;
            int moveForward = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp = GLFW_KEY_SPACE;
            int moveDown = GLFW_KEY_LEFT_SHIFT;
            int lookLeft = GLFW_KEY_LEFT;
            int lookRight = GLFW_KEY_RIGHT;
            int lookUp = GLFW_KEY_UP;
            int lookDown = GLFW_KEY_DOWN;
        };

        /**
         * @brief Calls glfwGetKey() for each mapped key and integrates look/move into gameObject's transform.
         * @param window GLFWwindow to poll key state from.
         * @param dt Frame time in seconds, used to scale movement/look speed.
         * @param gameObject Object (typically the camera's "viewer" object) whose TransformComponent is updated.
         */
        void moveInPlaneXZ(GLFWwindow *window, float dt, GameObject &gameObject);

        KeyMappings keys{};
        float moveSpeed{3.0f};
        float lookSpeed{1.5f};
    private:

    };
}
