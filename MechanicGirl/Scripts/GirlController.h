//
// Created by Monika on 11.01.2026.
//

#ifndef SR_ENGINE_SAMPLES_GIRL_CONTROLLER_H
#define SR_ENGINE_SAMPLES_GIRL_CONTROLLER_H

#include <Scripting/Cpp/CppBehaviour.h>

#include <Graphics/Types/Camera.h>
#include <Graphics/Animations/Animator.h>

#include <Physics/3D/Rigidbody3D.h>

#include <Utils/ECS/GameObject.h>
#include <Utils/Input/InputSystem.h>

namespace Samples {
    /// @category(Game)
    class GirlController : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = CppBehaviour;
    public:
        void Update(float_t dt) override;

    public:
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Physics::Types::Rigidbody3D> rigidbody;
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Graphics::Animations::Animator> animator;
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Utils::GameObject> headObject;

        /// @property @group(Camera)
        SpaRcle::Utils::EntityRef<SpaRcle::Utils::GameObject> cameraObject;
        /// @property @group(Camera)
        SR_MATH_NS::FVector3 cameraOffset;
        /// @property @group(Camera)
        float_t cameraShakeSpeed = 0.1f;
        /// @property @group(Camera)
        float_t cameraDragSpeed = 1.0f;
        /// @property @group(Camera)
        float_t cameraPitchLimit = 80.f;

        /// @property @group(Movement)
        float_t bodyRotateSpeed = 1.0f;
        /// @property @group(Movement)
        float_t walkSpeed = 1.0f;
        /// @property @group(Movement)
        float_t runSpeed = 1.0f;

    private:
        float_t m_targetCameraPitch = 0.f;
        std::optional<SpaRcle::Utils::CursorLock> m_lock;

    };
} // Samples

#endif //SR_ENGINE_SAMPLES_GIRL_CONTROLLER_H