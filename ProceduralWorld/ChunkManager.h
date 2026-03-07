//
// Created by Monika on 05.03.2026.
//

#pragma once

#include <Scripting/Cpp/CppBehaviour.h>

#include <Utils/Types/FlatHashMap.h>

namespace ProceduralWorld {
    class ChunkManager : public SpaRcle::Scripting::CppBehaviour {
        SR_CLASS()
        using Super = SpaRcle::Scripting::CppBehaviour;
        using ChunkPosition = SpaRcle::Utils::Math::IVector3;
        struct ChunkInfo {
            ChunkPosition position;
            SpaRcle::Utils::GameObject::Ptr pChunkObject;
        };
    public:
        void Awake() override;
        void Update(float_t dt) override;

    private:
        void UpdateChunks();
        void RequestChunk(const ChunkPosition& position);
        void GenerateChunks();
        void UnloadChunks();

        void GenerateChunkDensity(const ChunkPosition& position);
        void GenerateGeometry();
        void ReadVertices();
        void ReadIndices();

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
        /// @property
        uint8_t m_chunkSize = 64;
        /// @property
        uint8_t m_loadRadius = 5;
        /// @property
        uint8_t m_unloadRadius = 6;

        /// @property
        uint32_t m_numPointsPerAxis = 16;
        /// @property
        uint32_t m_vertexHashTableSize = 65536;
        /// @property
        uint32_t m_densityCountAxis = 64;
        /// @property
        uint32_t m_densityComputeGroups = 8;
        /// @property
        float_t m_noiseScale = 10.0f;
        /// @property
        int64_t m_seed = 1;
        /// @property
        float_t m_isoLevel = 0.2f;

    private:
        SR_HTYPES_NS::FastMemoryArray<SR_GRAPH_NS::Vertices::StaticMeshVertex> m_vertices;
        SR_HTYPES_NS::FastMemoryArray<uint32_t> m_indices;

        SR_GTYPES_NS::ComputeShader::Ptr m_pMarchingComputeShader = nullptr;
        SR_GTYPES_NS::ComputeShader::Ptr m_pDensityComputeShader = nullptr;

        SR_GRAPH_NS::SSBOInstance::Ptr m_pDensitySSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pHashTableSSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pVerticesSSBO = nullptr;
        SR_GRAPH_NS::SSBOInstance::Ptr m_pIndicesSSBO = nullptr;

        SpaRcle::Utils::Types::FlatHashMap<ChunkPosition, ChunkInfo> m_chunks;
        std::vector<SpaRcle::Utils::GameObject::Ptr> m_chunksPools;
        ChunkPosition m_observerPosition;
        std::vector<ChunkPosition> m_chunksToLoad;
        std::vector<ChunkPosition> m_chunksToUnload;

    };
}
