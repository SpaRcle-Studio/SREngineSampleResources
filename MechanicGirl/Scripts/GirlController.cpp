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

        auto&& pRigidbody = rigidbody.Get();
        if (!pRigidbody) {
            return;
        }

        auto&& pAnimator = animator.Get();
        if (!pAnimator) {
            return;
        }

        auto&& pTransform = pRigidbody->GetGameObject()->GetTransform();
        pTransform->Rotate(SR_MATH_NS::FVector3(0.f, drag.x * (bodyRotateSpeed / 10.f), 0.f));

        /// Walk and Run logic

        const bool isWPressed = SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::W);
        const bool isSPressed = SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::S);
        const bool isAPressed = SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::A);
        const bool isDPressed = SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::D);

        SR_MATH_NS::FVector3 wishDir;
        if (isWPressed) wishDir += SR_UTILS_NS::Transform3D::FORWARD;
        if (isSPressed) wishDir -= SR_UTILS_NS::Transform3D::FORWARD;
        if (isAPressed) wishDir -= SR_UTILS_NS::Transform3D::RIGHT;
        if (isDPressed) wishDir += SR_UTILS_NS::Transform3D::RIGHT;

        m_bodyYawTarget = 0.f;
        if (!(isAPressed && isDPressed)) {
            if ((isWPressed || isSPressed) && isAPressed) {
                m_bodyYawTarget = isSPressed ? 45.f : -45.f;
            }
            else if ((isWPressed || isSPressed) && isDPressed) {
                m_bodyYawTarget = isSPressed ? -45.f : 45.f;
            }
            else if ((isAPressed || isDPressed)) {
                m_bodyYawTarget = isDPressed ? 90.f : -90.f;
            }
        }

        auto&& targetBodyRootQuat = SR_MATH_NS::Quaternion::FromEulerAngles(SR_MATH_NS::FVector3(0.f, m_bodyYawTarget, 0.f));

        if (auto&& pCamera = cameraObject.Get()) {
            if (auto&& pHead = headObject.Get()) {
                auto&& headTranslation = pHead->GetTransform()->GetGlobalTranslation();
                headTranslation += targetBodyRootQuat.Inverse() * pHead->GetTransform()->TransformDirection(cameraOffset);
                auto&& cameraTranslation = pCamera->GetTransform()->GetGlobalTranslation();
                pCamera->GetTransform()->SetGlobalTranslation(cameraTranslation.Lerp(headTranslation, cameraShakeSpeed));
            }

            m_targetCameraPitch += drag.y * (cameraDragSpeed / 10.0);
            m_targetCameraPitch = SR_MATH_NS::Clamp(m_targetCameraPitch, -cameraPitchLimit, cameraPitchLimit);

            SR_MATH_NS::FVector3 cameraRotation = pCamera->GetTransform()->GetRotation();
            cameraRotation.x = m_targetCameraPitch;

            pCamera->GetTransform()->SetRotation(cameraRotation);
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

        if (auto&& pBody = bodyRoot.Get()) {
            pBody->GetTransform()->SetRotation(pBody->GetTransform()->GetQuaternion().Slerp(targetBodyRootQuat, bodyWalkRotateSpeed * dt * 5.f));
        }

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
