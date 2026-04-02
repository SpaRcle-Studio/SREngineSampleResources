//
// Created by Monika on 28.03.2026.
//

#pragma once

#include <Scripting/Cpp/BehaviourRef.h>

#include <Utils/Types/FlatHashMap.h>

namespace ProceduralWorld {
    struct ChunkInfo;

    class SphereCasting : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = SpaRcle::Scripting::CppBehaviour;
    public:
        void Update(float_t dt) override;

    private:
        bool EraseSphere(ChunkInfo& chunk, const SR_MATH_NS::FVector3& erasePos, float_t chunkScale, uint32_t densityPerAxis);

    private:
        /// @property
        SR_SCRIPTING_NS::BehaviourRef<ChunkManager> chunkManager;
        /// @property
        float_t eraseRadius = 5.f;
        /// @property
        float_t hardness = 1.f;
        /// @property
        bool singleErase = false;

    private:
        uint64_t m_debugRayId = SR_ID_INVALID;

    };
}