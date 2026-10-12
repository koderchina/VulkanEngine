#include "MouseController.hpp"

namespace Engine
{
    MouseController::MouseController(GLFWwindow *window) : window{window}
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
        {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }
    
    void MouseController::move(float dt, GameObject &gameObject)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = static_cast<float>(xpos - lastX);
        float yoffset = static_cast<float>(ypos - lastY);
        lastX = xpos;
        lastY = ypos;

        gameObject.transform.rotation.y += lookSpeed * xoffset;
        gameObject.transform.rotation.x -= lookSpeed * yoffset;

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());
    }
}