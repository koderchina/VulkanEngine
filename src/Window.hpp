#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cinttypes>
#include <string>
#include <iostream>

namespace Engine
{
    /**
     * @brief Thin wrapper around a GLFWwindow.
     *
     * Owns the glfwCreateWindow() handle and is the only place in the engine
     * that calls glfwCreateWindowSurface() to get a VkSurfaceKHR, since GLFW
     * is the one that knows which platform-specific WSI extension to use.
     */
    class Window
    {
    public:
        /**
         * @brief Calls glfwInit()/glfwCreateWindow() to open an OS window.
         * @param w Initial window width in pixels.
         * @param h Initial window height in pixels.
         * @param name Window title.
         */
        Window(int16_t w, int16_t h, std::string name);

        /// Calls glfwDestroyWindow() and glfwTerminate().
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        /// @return true once glfwWindowShouldClose() reports the user asked to close the window.
        bool shouldClose()
        {
            return glfwWindowShouldClose(this->window);
        }

        /**
         * @brief Calls glfwCreateWindowSurface() to create the VkSurfaceKHR this window will present to.
         * @param instance VkInstance the surface is created against.
         * @param surface Out parameter receiving the new VkSurfaceKHR.
         */
        void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

        /// @return true if frameBufferResizeCallback() fired since the flag was last reset.
        bool windowResized() {return frameBufferResized;}

        /// Clears the resize flag set by frameBufferResizeCallback().
        void resetWindowResizeFlag() {frameBufferResized = false;}

        /// @return The underlying GLFWwindow* handle.
        GLFWwindow* getWindowPtr() const {return window;}

        /// @return Current framebuffer size as a VkExtent2D, for swap chain creation.
        VkExtent2D getExtent() {return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};}
    private:
        /// GLFW framebuffer-size callback (registered via glfwSetFramebufferSizeCallback); flags a resize.
        static void frameBufferResizeCallback(GLFWwindow *window, int width, int height);

        /// Calls glfwInit(), sets window hints, and calls glfwCreateWindow().
        void initWindow();
        int16_t width;
        int16_t height;
        bool frameBufferResized = false;

        const std::string windowName;
        GLFWwindow *window;
    };
}
