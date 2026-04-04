//
// Created by Monika on 28.03.2026.
//

#include "SphereCasting.h"

#include <Physics/3D/Raycast3D.h>

#include <Utils/DebugDraw.h>

#include <Codegen/SphereCasting.generated.hpp>

namespace ProceduralWorld {
    bool SphereCasting::EraseSphere(ChunkInfo& chunk, const SR_MATH_NS::FVector3& erasePos, float_t chunkScale, uint32_t densityPerAxis) {
        SR_TRACY_ZONE;

        bool affected = false;

        const float_t eraseRadiusSquared = eraseRadius * eraseRadius;
        auto&& chunkPos = chunk.worldPosition;

        // вычисляем диапазон вокселей, которые реально могут быть затронуты
        int32_t minX = std::max(0, static_cast<int32_t>((erasePos.x - eraseRadius - chunkPos.x) / chunkScale));
        int32_t maxX = std::min(static_cast<int32_t>(densityPerAxis - 1), static_cast<int32_t>((erasePos.x + eraseRadius - chunkPos.x) / chunkScale));
        int32_t minY = std::max(0, static_cast<int32_t>((erasePos.y - eraseRadius - chunkPos.y) / chunkScale));
        int32_t maxY = std::min(static_cast<int32_t>(densityPerAxis - 1), static_cast<int32_t>((erasePos.y + eraseRadius - chunkPos.y) / chunkScale));
        int32_t minZ = std::max(0, static_cast<int32_t>((erasePos.z - eraseRadius - chunkPos.z) / chunkScale));
        int32_t maxZ = std::min(static_cast<int32_t>(densityPerAxis - 1), static_cast<int32_t>((erasePos.z + eraseRadius - chunkPos.z) / chunkScale));

        for (int32_t z = minZ; z <= maxZ; ++z) {
            const float_t zPos = chunkPos.z + z * chunkScale;
            for (int32_t y = minY; y <= maxY; ++y) {
                const float_t yPos = chunkPos.y + y * chunkScale;
                for (int32_t x = minX; x <= maxX; ++x) {
                    const float_t xPos = chunkPos.x + x * chunkScale;
                    const SR_MATH_NS::FVector3 voxelPosition(xPos, yPos, zPos);

                    const float_t distSquared = (voxelPosition - erasePos).LengthSq();
                    if (distSquared < eraseRadiusSquared) {
                        const uint32_t index = z * densityPerAxis * densityPerAxis + y * densityPerAxis + x;
                        const float_t dist = sqrt(distSquared); // sqrt только для формулы снижения плотности

                        float_t& density = chunk.voxels[index].density;
                        const float_t previousDensity = density;

                        density -= (eraseRadius - dist) * hardness;
                        density = std::max(density, -1.0f);

                        if (!SR_MATH_NS::IsEquals(previousDensity, chunk.voxels[index].density)) {
                            chunk.voxelsDirty = true;
                            affected = true;
                        }
                    }
                }
            }
        }

        return affected;
    }

    void SphereCasting::Update(float dt) {
        SR_TRACY_ZONE;

        auto&& pChunkManager = chunkManager.GetBehaviour();
        if (!pChunkManager) {
            return;
        }

        auto&& pMainCamera = gameObject->GetScene()->GetMainCamera();
        if (!pMainCamera) {
            return;
        }

        const SR_MATH_NS::FVector3 start = pMainCamera->GetTransform()->GetGlobalTranslation();
        const SR_MATH_NS::FVector3 direction = pMainCamera->GetTransform()->Forward();

        const float_t maxDistance = 1000.f;
        std::optional<SR_UTILS_NS::RaycastHit> hit = SR_PHYSICS_NS::Raycast3D::Instance().CastSingle(start, direction, maxDistance);
        if (!hit) {
            return;
        }

        m_debugRayId = SR_UTILS_NS::DebugDraw::Instance().DrawSphere(m_debugRayId, hit->position, SR_MATH_NS::Quaternion::Identity(), SR_MATH_NS::FVector3(eraseRadius), SR_MATH_NS::FColor::Red(), 0.1f);

        const bool apply = singleErase ? SR_UTILS_NS::Input::Instance().GetKeyDown(SR_UTILS_NS::KeyCode::F) : SR_UTILS_NS::Input::Instance().GetKey(SR_UTILS_NS::KeyCode::F);
        if (!apply) {
            return;
        }

        const uint32_t densityPerAxis = pChunkManager->GetDensitiesCountPerAxis();
        const float_t chunkScale = pChunkManager->GetChunkScale();

        const SR_MATH_NS::FVector3 sphereCenter = hit->position;
        const float_t radius = eraseRadius;

        const SR_MATH_NS::FVector3 min = sphereCenter - SR_MATH_NS::FVector3(radius);
        const SR_MATH_NS::FVector3 max = sphereCenter + SR_MATH_NS::FVector3(radius);

        SR_MATH_NS::IVector3 minChunk = pChunkManager->WorldToChunkPosition(min);
        SR_MATH_NS::IVector3 maxChunk = pChunkManager->WorldToChunkPosition(max);

        // захватываем границы (marching cubes иначе даст швы)
        minChunk -= SR_MATH_NS::IVector3(1);
        maxChunk += SR_MATH_NS::IVector3(1);

        for (int cx = minChunk.x; cx <= maxChunk.x; ++cx) {
            for (int cy = minChunk.y; cy <= maxChunk.y; ++cy) {
                for (int cz = minChunk.z; cz <= maxChunk.z; ++cz) {
                    SR_MATH_NS::IVector3 coord { cx, cy, cz };
                    auto&& pChunk = pChunkManager->GetChunk(coord);
                    if (!pChunk) {
                        continue;
                    }

                    const bool affected = EraseSphere(
                        *pChunk,
                        hit->position,
                        chunkScale,
                        densityPerAxis
                    );

                    //if (affected) {
                        pChunkManager->ReloadChunkAtPosition(coord);
                    //}
                }
            }
        }
    }
}