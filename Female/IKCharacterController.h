//
// Created by Monika on 11.12.2025.
//

#ifndef SR_ENGINE_SAMPLES_IK_CHARACTER_CONTROLLER_H
#define SR_ENGINE_SAMPLES_IK_CHARACTER_CONTROLLER_H

#include <Scripting/Cpp/CppBehaviour.h>

#include <Graphics/Types/Camera.h>

#include <Physics/3D/Rigidbody3D.h>

#include <Utils/ECS/GameObject.h>

namespace Samples {
    /// @category(Game)
    class IKCharacterController : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = CppBehaviour;
    public:
        void Update(float_t dt) override;

    public:
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Physics::Types::Rigidbody3D> rigidbody;
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Utils::GameObject> cameraObject;
        /// @property
        SpaRcle::Utils::EntityRef<SpaRcle::Utils::GameObject> headObject;

    };
} // Samples

#endif //SR_ENGINE_SAMPLES_IK_CHARACTER_CONTROLLER_H
