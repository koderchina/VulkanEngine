#pragma once

#define GLM_FROCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Engine
{
    /**
     * @brief Pure math object: builds the projection and view matrices uploaded into
     * GlobalUniformBufferObject each frame. Holds no Vulkan handles itself.
     */
    class Camera
    {
    public:
        /**
         * @brief Builds an orthographic projection matrix mapping [left,right]x[bottom,top]x[near,far] to NDC.
         * @param left Left clip plane.
         * @param right Right clip plane.
         * @param top Top clip plane.
         * @param bottom Bottom clip plane.
         * @param near Near clip plane.
         * @param far Far clip plane.
         */
        void setOrthographicProjection(
            float left,
            float right,
            float top,
            float bottom,
            float near,
            float far);

            /**
             * @brief Builds a perspective projection matrix for Vulkan's [0,1] depth range.
             * @param fovy Vertical field of view, in radians.
             * @param aspect Viewport width/height ratio.
             * @param near Near clip plane.
             * @param far Far clip plane.
             */
            void setPerspectiveProjection(float fovy, float aspect, float near, float far);

        /**
         * @brief Builds the view matrix (and its inverse) from a position and look direction.
         * @param position Camera position in world space.
         * @param direction Direction the camera is looking.
         * @param up Up vector used to orthonormalize the basis.
         */
        void setViewDirection(
            glm::vec3 position,
            glm::vec3 direction,
            glm::vec3 up = glm::vec3{0.0f, -1.0f, 0.0f}
        );

        /**
         * @brief setViewDirection() computed as (target - position).
         * @param position Camera position in world space.
         * @param target World-space point the camera looks at.
         * @param up Up vector used to orthonormalize the basis.
         */
        void setViewTarget(
            glm::vec3 position,
            glm::vec3 target,
            glm::vec3 up = glm::vec3{0.0f, -1.0f, 0.0f}
        );

        /**
         * @brief Builds the view matrix (and its inverse) from a position and Y-X-Z Euler rotation.
         * @param position Camera position in world space.
         * @param rotation Euler angles applied in Y, then X, then Z order.
         */
        void setViewYXZ(
            glm::vec3 position,
            glm::vec3 rotation
        );

        /// @return The current projection matrix.
        const glm::mat4& getProjection() const {return projectionMatrix;}
        /// @return The current view matrix.
        const glm::mat4& getView() const {return viewMatrix;}
        /// @return The inverse of the current view matrix (world transform of the camera).
        const glm::mat4& getInverseView() const {return inverseViewMatrix;}
        /// @return Camera world-space position, read off the inverse view matrix's translation column.
        const glm::vec3 getPosition() const {return glm::vec3(inverseViewMatrix[3]);}
    private:
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 inverseViewMatrix{1.0f};
    };
}
