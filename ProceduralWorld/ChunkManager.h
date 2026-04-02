//
// Created by Monika on 05.03.2026.
//

#pragma once

#include <Scripting/Cpp/CppBehaviour.h>

#include <Utils/Types/FlatHashMap.h>

namespace ProceduralWorld {
    using ChunkPosition = SpaRcle::Utils::Math::IVector3;

    struct ChunkInfo {
        ChunkPosition position;
        SR_MATH_NS::FVector3 worldPosition;
        SpaRcle::Utils::GameObject::Ptr pChunkObject;
        SR_HTYPES_NS::FastMemoryArray<float_t> densities;
        bool densitiesDirty = true;
    };

    class ChunkManager : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = SpaRcle::Scripting::CppBehaviour;
    public:
        void Awake() override;
        void Update(float_t dt) override;
        void ReloadChunkAtPosition(const ChunkPosition& position);

        SR_NODISCARD ChunkInfo* GetChunk(const SR_MATH_NS::IVector3& position);
        SR_NODISCARD ChunkInfo* GetChunkAtPosition(const SR_MATH_NS::FVector3& position);
        SR_NODISCARD SR_MATH_NS::IVector3 WorldToChunkPosition(const SR_MATH_NS::FVector3& worldPosition) const;
        SR_NODISCARD uint32_t GetDensitiesCountPerAxis() const { return m_densityCountAxis; }
        SR_NODISCARD float_t GetChunkScale() const;

    private:
        void ReInit();

        void UpdateChunks();
        void RequestChunk(const ChunkPosition& position);
        void GenerateChunks();
        void UnloadChunks(bool all);

        void GenerateChunk(const ChunkPosition& position);
        void GenerateChunkDensity(const ChunkPosition& position);
        void GenerateGeometry();
        void ReadVertices();
        void ReadIndices();

        void ReloadChunks();

    private:
        /// @property
        /// @customArgs(pick: enabled, filter name: Shader, relative: resources)
        /// @customArg(filter value: srsl)
        SR_UTILS_NS::Path m_marchingCubesShader = "Samples/MarchingCubes/MarchingCubes.srsl";
        /// @property
        /// @customArgs(pick: enabled, filter name: Shader, relative: resources)
        /// @customArg(filter value: srsl)
        SR_UTILS_NS::Path m_densityShader = "Samples/MarchingCubes/Density.srsl";
        /// @property
        /// @customArgs(pick: enabled, filter name: Prefab, relative: resources)
        /// @customArg(filter value: prefab)
        SpaRcle::Utils::Path m_chunkPrefabPath;
        /// @property @onChanged(ReloadChunks)
        uint8_t m_chunkSize = 64;
        /// @property @onChanged(ReloadChunks)
        float_t m_chunkScale = 2.0f;
        /// @property @onChanged(ReloadChunks)
        uint8_t m_loadRadius = 5;
        /// @property @onChanged(ReloadChunks)
        uint8_t m_unloadRadius = 6;
        /// @property @onChanged(ReloadChunks)
        uint8_t m_worldHeight = 3;

        /// @property @onChanged(ReloadChunks)
        uint32_t m_densityCountAxis = 64;
        /// @property @onChanged(ReloadChunks)
        uint32_t m_densityComputeGroups = 8;
        /// @property @onChanged(ReloadChunks)
        float_t m_noiseScale = 10.0f;
        /// @property @onChanged(ReloadChunks)
        int64_t m_seed = 1;
        /// @property @onChanged(ReloadChunks)
        float_t m_isoLevel = 0.2f;

        /// @property @onChanged(ReInit) @group(SSBO)
        uint32_t m_numPointsPerAxis = 16;
        /// @property @onChanged(ReInit) @group(SSBO)
        uint32_t m_vertexHashTableSize = 65536;

    private:
        SR_HTYPES_NS::FastMemoryArray<SR_GRAPH_NS::Vertices::StaticMeshVertex> m_vertices;
        SR_HTYPES_NS::FastMemoryArray<SR_MATH_NS::FVector3> m_verticesPositions;
        SR_HTYPES_NS::FastMemoryArray<uint32_t> m_optimizedIndices;
        SR_HTYPES_NS::FastMemoryArray<uint32_t> m_indices;
        SR_HTYPES_NS::FastMemoryArray<uint8_t> m_solidDensities;

        SR_GTYPES_NS::ComputeShader::Ptr m_pMarchingComputeShader = nullptr;
        SR_GTYPES_NS::ComputeShader::Ptr m_pDensityComputeShader = nullptr;

        SR_GRAPH_NS::SSBOInstance::Ptr m_pDensitySSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pHashTableSSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pVerticesSSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pIndicesSSBO = nullptr;

        SR_HTYPES_NS::FlatHashMap<ChunkPosition, ChunkInfo> m_chunks;
        std::vector<SpaRcle::Utils::GameObject::Ptr> m_chunksPools;
        ChunkPosition m_observerPosition;
        std::vector<ChunkPosition> m_chunksToLoad;
        std::vector<ChunkPosition> m_chunksToUnload;

        SR_HTYPES_NS::SortedVector<ChunkPosition> m_chunksToReload;

    };
}
