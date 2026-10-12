#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cinttypes>
#include <string>
#include <iostream>

namespace Engine
{
    class Window
    {
    public:
        Window(int16_t w, int16_t h, std::string name);
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        bool shouldClose() 
        {
            return glfwWindowShouldClose(this->window);
        }
        void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
        bool windowResized() {return frameBufferResized;}
        void resetWindowResizeFlag() {frameBufferResized = false;}
        GLFWwindow* getWindowPtr() const {return window;}
        VkExtent2D getExtent() {return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};}
    private:
        static void frameBufferResizeCallback(GLFWwindow *window, int width, int height);

        void initWindow();
        int16_t width;
        int16_t height;
        bool frameBufferResized = false;

        const std::string windowName;
        GLFWwindow *window;
    };
}