//
// Created by Monika on 11.01.2026.
//

#include <MechanicGirl/Scripts/GirlController.h>

#include <Utils/ECS/Transform3D.h>

#include <Codegen/GirlController.generated.hpp>

namespace Samples {
    void GirlController::Update(float_t dt) {
        SR_TRACY_ZONE;

        /// Camera logic

        auto&& drag = SR_UTILS_NS::Input::Instance().GetMouseDrag();

        if (!m_lock) {
            m_lock = SR_UTILS_NS::CursorLock(SpaRcle::Utils::CursorLockMode::PlayMode);
        }

        if (auto&& pCamera = cameraObject.Get()) {
            if (auto&& pHead = headObject.Get()) {
                auto&& headTranslation = pHead->GetTransform()->GetGlobalTranslation();
                headTranslation += pHead->GetTransform()->TransformDirection(cameraOffset);
                auto&& cameraTranslation = pCamera->GetTransform()->GetGlobalTranslation();
                pCamera->GetTransform()->SetGlobalTranslation(cameraTranslation.Lerp(headTranslation, cameraShakeSpeed));
            }

            m_targetCameraPitch += drag.y * (cameraDragSpeed / 50.0);
            m_targetCameraPitch = SR_MATH_NS::Clamp(m_targetCameraPitch, -cameraPitchLimit, cameraPitchLimit);

            SR_MATH_NS::FVector3 cameraRotation = pCamera->GetTransform()->GetRotation();
            cameraRotation.x = m_targetCameraPitch;

            pCamera->GetTransform()->SetRotation(cameraRotation);
        }

        auto&& pRigidbody = rigidbody.Get();
        if (!pRigidbody) {
            return;
        }

        auto&& pAnimator = animator.Get();
        if (!pAnimator) {
            return;
        }

        auto&& pTransform = pRigidbody->GetGameObject()->GetTransform();
        pTransform->Rotate(SR_MATH_NS::FVector3(0.f, drag.x * (bodyRotateSpeed / 50.f), 0.f));

        /// Walk and Run logic

        SR_MATH_NS::FVector3 wishDir;
        if (SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::W)) {
            wishDir += SR_UTILS_NS::Transform3D::FORWARD;
        }

        if (SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::S)) {
            wishDir -= SR_UTILS_NS::Transform3D::FORWARD;
        }

        if (SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::A)) {
            wishDir -= SR_UTILS_NS::Transform3D::RIGHT;
        }

        if (SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::D)) {
            wishDir += SR_UTILS_NS::Transform3D::RIGHT;
        }

        wishDir = pTransform->TransformDirection(wishDir);

        if (wishDir.LengthSq() > 1.f) {
            wishDir = wishDir.Normalized();
        }

        const bool isShift = SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::LShift);

        const float_t walkSpeedCoefficient = walkSpeed * 4.0f;
        const float_t runSpeedCoefficient = runSpeed * 10.f;
        const float_t maxSpeed = (isShift ? runSpeedCoefficient : walkSpeedCoefficient);

        SR_MATH_NS::FVector3 wishVelocity = wishDir * maxSpeed;

        if (wishDir.Length() > 0.f) {
            if (isShift) {
                pAnimator->SetBool("Walk", false);
                pAnimator->SetBool("Run", true);
            }
            else {
                pAnimator->SetBool("Walk", true);
                pAnimator->SetBool("Run", false);
            }
        }
        else {
            pAnimator->SetBool("Walk", false);
            pAnimator->SetBool("Run", false);
        }

        SR_MATH_NS::FVector3 velocity = pRigidbody->GetLinearVelocity();
        SR_MATH_NS::FVector3 horizontalVel = SR_MATH_NS::FVector3(velocity.x, 0.f, velocity.z);
        SR_MATH_NS::FVector3 deltaV = wishVelocity - horizontalVel;
        deltaV.y = 0.f;

        if (deltaV.LengthSq() > 0.0001f) {
            const bool isGrounded = true;
            const float_t groundAccel = 20.f;
            const float_t airAccel = 1.f;
            const float_t maxAccel = isGrounded ? groundAccel : airAccel;
            const auto&& impulse = deltaV * maxAccel * dt;

            pRigidbody->AddImpulse(impulse);
        }
    }
}
