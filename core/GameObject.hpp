#pragma once

#include "Model.hpp"

#include <memory>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
    /**
     * @brief Position/rotation/scale of a GameObject, and the matrices built from them.
     *
     * No Vulkan calls here; mat4()/normalMatrix() are pure CPU math run once per
     * object per frame and the result is uploaded as a push constant
     * (see SimplePushConstantData in systems/SolidRenderSystem.cpp).
     */
    struct TransformComponent
    {
        glm::vec3 translation{}; // position offset
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 rotation{};

        // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
        // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
        // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
        /// @return The model matrix Translate * Ry * Rx * Rz * Scale built from translation/rotation/scale.
        glm::mat4 mat4();
        /// @return The matrix used to transform normals (inverse-transpose of the model matrix's rotation/scale part, computed directly).
        glm::mat3 normalMatrix();
    };

    /// Marks a GameObject as a point light and stores its brightness; presence of this (non-null) is what PointLightRenderSystem checks for.
    struct PointLightComponent
    {
        float lightIntensity = 1.0f;
    };

    // Anything with a collection of properties and methods
    /**
     * @brief A scene entity: an optional Model to draw, a TransformComponent, and optionally a PointLightComponent.
     *
     * Holds no Vulkan handles directly - rendering is driven by the render
     * systems (SolidRenderSystem, PointLightRenderSystem) reading a GameObject::Map
     * and dispatching based on which components are present.
     */
    class GameObject
    {
    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, GameObject>;

        /// @return A new GameObject with a fresh, process-unique id_t.
        static GameObject createGameObject()
        {
            static id_t currId = 0;
            return GameObject{currId++};
        }

        GameObject(const GameObject&) = delete;
        GameObject &operator=(const GameObject&) = delete;
        GameObject(GameObject &&) = default;
        GameObject &operator=(GameObject&&) = default;

        /**
         * @brief Creates a model-less GameObject with a PointLightComponent attached.
         * @param intensity Light intensity, stored in PointLightComponent::lightIntensity.
         * @param radius Point-light radius, stored in TransformComponent::scale.x.
         * @param color Light color.
         * @return The new point-light GameObject.
         */
        static GameObject makePointLight(float intensity = 10.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.0f));

        std::shared_ptr<Model> model{};
        glm::vec3 color{};
        TransformComponent transform{};
        std::unique_ptr<PointLightComponent> pointLight = nullptr;

        /// @return This object's unique id, used as the key in GameObject::Map.
        id_t getId() {return id;}
    private:
        GameObject(id_t id) : id{id} {}

        id_t id;
    };
}
