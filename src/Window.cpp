#include "Window.hpp"

namespace Engine
{
    Window::Window(int16_t w, int16_t h, std::string name)
    : width{w}, height{h}, windowName{name}
    {
        initWindow();
        std::cout << "Window init\n";
    }
    Window::~Window()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
        std::cout << "Window destroyed\n";
    }

    void Window::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(this->width, this->height, this->windowName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
        std::cout << "Window create\n";
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface.");
        }
        std::cout << "Window surface created\n";
    }

    void Window::frameBufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto pWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
        pWindow->frameBufferResized = true;
        pWindow->width = static_cast<int16_t>(width);
        pWindow->height = static_cast<int16_t>(height);
    }
}