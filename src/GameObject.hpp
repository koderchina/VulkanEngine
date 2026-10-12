#pragma once

#include "Model.hpp"

#include <memory>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
    struct TransformComponent
    {   
        glm::vec3 translation{}; // position offset
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 rotation{};

        // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
        // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
        // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
        glm::mat4 mat4();
        glm::mat3 normalMatrix();
    };

    struct PointLightComponent
    {
        float lightIntensity = 1.0f;
    };
    
    // Anything with a collection of properties and methods
    class GameObject
    {
    public:
        using id_t = unsigned int;
        using Map = std::unordered_map<id_t, GameObject>;

        static GameObject createGameObject()
        {
            static id_t currId = 0;
            return GameObject{currId++};
        }

        GameObject(const GameObject&) = delete;
        GameObject &operator=(const GameObject&) = delete;
        GameObject(GameObject &&) = default;
        GameObject &operator=(GameObject&&) = default;

        static GameObject makePointLight(float intensity = 10.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.0f));

        std::shared_ptr<Model> model{};
        glm::vec3 color{};
        TransformComponent transform{};
        std::unique_ptr<PointLightComponent> pointLight = nullptr;

        id_t getId() {return id;}
    private:
        GameObject(id_t id) : id{id} {}

        id_t id;
    };  
}