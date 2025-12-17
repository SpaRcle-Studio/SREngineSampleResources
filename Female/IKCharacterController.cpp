//
// Created by Monika on 11.12.2025.
//

#include <Female/IKCharacterController.h>

#include <Codegen/IKCharacterController.generated.hpp>

namespace Samples {
    void IKCharacterController::Update(float_t dt) {
        SR_TRACY_ZONE;

        Super::Update(dt);

        auto&& pCameraObject = cameraObject.Get();
        auto&& pRigidbody = rigidbody.Get();

        if (!pCameraObject || !pRigidbody) {
            return;
        }


    }
} // Samples