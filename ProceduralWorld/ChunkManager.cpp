//
// Created by Monika on 05.03.2026.
//

#include "ChunkManager.h"
#include "GreedyBoxes.h"

#include <Graphics/Types/Geometry/ProceduralMesh.h>

#include <Physics/CollisionShape.h>

#include <Utils/DebugDraw.h>
#include <Utils/Common/Vertices.h>

#include <Codegen/ChunkManager.generated.hpp>

namespace ProceduralWorld {
    struct Vertex {
        SR_MATH_NS::FVector3 pos;
        float pad0;
        uint32_t materialID;
        uint32_t materialID2;
        float blend;
        float pad1;
    };

    void computeSmoothNormals(SR_UTILS_NS::VertexDataBuffer& vertices, const SR_HTYPES_NS::FastMemoryArray<uint32_t>& indices) {
        SR_TRACY_ZONE;

        static SR_HTYPES_NS::FastMemoryArray<SR_MATH_NS::FVector3> localSums;
        localSums.resize(vertices.GetVertexCount());
        std::memset(localSums.data(), 0, localSums.size() * sizeof(SR_MATH_NS::FVector3));

        // вычисляем нормали по треугольникам
        auto range = std::views::iota(size_t(0), indices.size() / 3);
        SR_UTILS_NS::ForEach<SR_UTILS_NS::ExecutionPolicy::Seq>(range.begin(), range.end(), [&](size_t t){
            uint32_t ia = indices[t * 3 + 0];
            uint32_t ib = indices[t * 3 + 1];
            uint32_t ic = indices[t * 3 + 2];

            SR_MATH_NS::FVector3 a = *(SR_MATH_NS::FVector3*)vertices.GetVertex(ia, SR_UTILS_NS::VertexAttribute::Position);
            SR_MATH_NS::FVector3 b = *(SR_MATH_NS::FVector3*)vertices.GetVertex(ib, SR_UTILS_NS::VertexAttribute::Position);
            SR_MATH_NS::FVector3 c = *(SR_MATH_NS::FVector3*)vertices.GetVertex(ic, SR_UTILS_NS::VertexAttribute::Position);

            SR_MATH_NS::FVector3 n = (SR_MATH_NS::FVector3::Cross(b - a, c - a)).Normalized();

            localSums[ia] += n;
            localSums[ib] += n;
            localSums[ic] += n;
        });

        // объединяем локальные суммы
        range = std::views::iota(size_t(0), vertices.GetVertexCount());
        SR_UTILS_NS::ForEach<SR_UTILS_NS::ExecutionPolicy::ParUnSeq>(range.begin(), range.end(), [&](uint32_t index){
            SR_MATH_NS::FVector3 normal = localSums[index].Normalized();
            SR_MATH_NS::FVector3 tangent = SR_MATH_NS::FVector3::Cross(normal, SR_MATH_NS::FVector3(0, 1, 0)).Normalized();
            vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::Normal, &normal);
            vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::Tangent, &tangent);
        });
    }

    void ChunkManager::Awake() {
        SR_TRACY_ZONE;

        m_observerPosition = ChunkPosition(SR_INF);

        m_pMarchingComputeShader = SR_GTYPES_NS::ComputeShader::Load(m_marchingCubesShader);
        m_pDensityComputeShader = SR_GTYPES_NS::ComputeShader::Load(m_densityShader);

        if (!m_pMarchingComputeShader || !m_pDensityComputeShader) {
            SRHalt("Failed to load compute shaders for Marching Cubes!");
            return;
        }

        ReInit();

        Super::Awake();
    }

    void ChunkManager::RequestChunk(const ChunkPosition& position) {
        SR_TRACY_ZONE;

        if (m_chunks.contains(position)) {
            return;
        }

        if (std::find(m_chunksToLoad.begin(), m_chunksToLoad.end(), position) != m_chunksToLoad.end()) {
            return;
        }
        m_chunksToLoad.emplace_back(position);
    }

    void ChunkManager::ReloadChunks() {
        SR_TRACY_ZONE;

        m_observerPosition = ChunkPosition(SR_INF);
        UnloadChunks(true);
    }

    void ChunkManager::ReInit() {
        const int numVoxelsPerAxis = static_cast<int>(m_numPointsPerAxis) - 1;
        if (numVoxelsPerAxis <= 0) {
            SRHalt("ChunkManager::ReInit() : numPointsPerAxis must be greater than 1!");
            return;
        }

        const int numVoxels = numVoxelsPerAxis * numVoxelsPerAxis * numVoxelsPerAxis;
        const int maxTriangleCount = numVoxels * 5;
        const int maxVertexCount = maxTriangleCount * 3;

        m_pHashTableSSBO.reset();
        m_pVerticesSSBO.reset();
        m_pIndicesSSBO.reset();

        m_pHashTableSSBO = SR_GRAPH_NS::SSBOInstance::Create<uint32_t>(m_vertexHashTableSize, SR_GRAPH_NS::SSBOUsage::CPUToGPU, "hashTable");
        m_pVerticesSSBO = SR_GRAPH_NS::SSBOInstance::Create<Vertex>(maxVertexCount, SR_GRAPH_NS::SSBOUsage::GPUToCPU, "vertices", SR_GRAPH_NS::SSBOFlags::StructuredCounter);
        m_pIndicesSSBO = SR_GRAPH_NS::SSBOInstance::Create<uint32_t>(maxVertexCount, SR_GRAPH_NS::SSBOUsage::GPUToCPU, "indices", SR_GRAPH_NS::SSBOFlags::Counter);

        ReloadChunks();
    }

    void ChunkManager::UpdateChunks() {
        SR_TRACY_ZONE;

        auto&& pMainCamera = gameObject->GetScene()->GetMainCamera();
        if (!pMainCamera) {
            return;
        }

        SpaRcle::Utils::Math::FVector3 cameraPosition = pMainCamera->GetTransform()->GetTranslation();

        SpaRcle::Utils::Math::IVector3 currentChunkCoords = {
            static_cast<int32_t>(std::floor(cameraPosition.x / m_chunkSize)),
            static_cast<int32_t>(std::floor(cameraPosition.y / m_chunkSize)),
            static_cast<int32_t>(std::floor(cameraPosition.z / m_chunkSize))
        };

        if (currentChunkCoords == m_observerPosition) {
            return;
        }
        m_observerPosition = currentChunkCoords;
        m_chunksToLoad.clear();

        const int r = m_loadRadius;
        const int r2 = r * r;

        for (int y = -r; y <= r; ++y) {
            int x = 0, z = 0;
            int dx = 1, dz = 0;

            int segmentLength = 1;
            int segmentPassed = 0;
            int segmentCount = 0;

            const int maxSteps = (r * 2 + 1) * (r * 2 + 1);

            for (int i = 0; i < maxSteps; ++i) {
                if (x * x + y * y + z * z <= r2) {
                    ChunkPosition coord{ m_observerPosition.x + x, m_observerPosition.y + y, m_observerPosition.z + z };
                    if (!m_chunks.contains(coord)) {
                        RequestChunk(coord);
                    }
                }

                x += dx;
                z += dz;

                segmentPassed++;

                if (segmentPassed == segmentLength) {
                    segmentPassed = 0;

                    int tmp = dx;
                    dx = -dz;
                    dz = tmp;

                    segmentCount++;
                    if (segmentCount % 2 == 0) {
                        segmentLength++;
                    }
                }
            }
        }

        UnloadChunks(false);
    }

    void ChunkManager::GenerateChunkDensity(const ChunkPosition& position) {
        SR_TRACY_ZONE;

        const auto densitiesCount = static_cast<uint64_t>(std::pow(m_densityCountAxis, 3));
        if (densitiesCount == 0) {
            SR_WARN("ChunkManager::GenerateChunkDensity() : densityCountAxis must be greater than 0!");
            return;
        }

        m_pDensitySSBO = SR_GRAPH_NS::SSBOInstance::Create<Voxel>(densitiesCount, SR_GRAPH_NS::SSBOUsage::AutoPreferDevice, "voxels");
        m_pDensitySSBO->Memset(0);

        if (m_pDensityComputeShader->BeginCompute()) {
            m_pDensitySSBO->Bind();
            m_pDensityComputeShader->GetShader()->SetConstInt("densityCountAxis"_atom, static_cast<int>(m_densityCountAxis));
            m_pDensityComputeShader->GetShader()->SetConstInt("seed"_atom, static_cast<int>(m_seed));
            m_pDensityComputeShader->GetShader()->SetConstFloat("isoLevel"_atom, m_isoLevel);
            m_pDensityComputeShader->GetShader()->SetConstFloat("noiseScale"_atom, m_noiseScale);
            m_pDensityComputeShader->GetShader()->SetConstIVec3("chunkCoord"_atom, position);
            m_pDensityComputeShader->Dispatch(m_densityComputeGroups, m_densityComputeGroups, m_densityComputeGroups);
            m_pDensityComputeShader->EndCompute();
        }

        m_chunks[position].voxels.resize(densitiesCount);
        if (m_pDensitySSBO->Map()) {
            std::memcpy(m_chunks[position].voxels.data(), m_pDensitySSBO->GetMappedData(), densitiesCount * sizeof(Voxel));
            m_pDensitySSBO->UnMap();
        }
    }

    void ChunkManager::GenerateGeometry() {
        int numVoxelsPerAxis = m_numPointsPerAxis - 1;
        if (numVoxelsPerAxis <= 0) {
            return;
        }

        m_pHashTableSSBO->Memset(-1);

        for (int stage = 0; stage <= 1; ++stage) {
            if (m_pMarchingComputeShader->BeginCompute()) {
                m_pDensitySSBO->Bind();
                m_pHashTableSSBO->Bind();
                m_pVerticesSSBO->Bind();
                m_pIndicesSSBO->Bind();
                m_pMarchingComputeShader->GetShader()->SetConstInt("vertexHashTableSize"_atom, static_cast<int>(m_vertexHashTableSize));
                m_pMarchingComputeShader->GetShader()->SetConstInt("densityCountAxis"_atom, static_cast<int>(m_densityCountAxis));
                m_pMarchingComputeShader->GetShader()->SetConstFloat("isoLevel"_atom, m_isoLevel);
                m_pMarchingComputeShader->GetShader()->SetConstInt(SR_GRAPH_NS::SHADER_COMPUTE_STAGE, stage);
                m_pMarchingComputeShader->Dispatch(numVoxelsPerAxis, numVoxelsPerAxis, numVoxelsPerAxis);
                m_pMarchingComputeShader->EndCompute();
            }
        }
    }

    void ChunkManager::ReadIndices() {
        SR_TRACY_ZONE;

        if (void* pData = m_pIndicesSSBO->MapData()) {
            const uint32_t indicesCount = m_pIndicesSSBO->GetCounter();
            m_indices.resize(indicesCount);
            std::memcpy(m_indices.data(), pData, sizeof(uint32_t) * indicesCount);
            m_pIndicesSSBO->ResetCounter();
            m_pIndicesSSBO->FlushCounter();
            m_pIndicesSSBO->UnMap();
        }
    }

    void ChunkManager::ReadVertices() {
        SR_TRACY_ZONE;

        if (auto&& pVertices = reinterpret_cast<Vertex*>(m_pVerticesSSBO->MapData())) {
            const uint32_t verticesCount = m_pVerticesSSBO->GetCounter();
            m_vertices.Allocate(verticesCount);
            m_vertices.SetLayout(SR_UTILS_NS::VertexLayoutDescription()
                .AddAttribute(SR_UTILS_NS::VertexAttribute::Position, SR_UTILS_NS::VertexAttributeFormat::Float32, 3)
                .AddAttribute(SR_UTILS_NS::VertexAttribute::Normal, SR_UTILS_NS::VertexAttributeFormat::Float32, 3)
                .AddAttribute(SR_UTILS_NS::VertexAttribute::Tangent, SR_UTILS_NS::VertexAttributeFormat::Float32, 4)
                .AddAttribute(SR_UTILS_NS::VertexAttribute::MaterialID0, SR_UTILS_NS::VertexAttributeFormat::UInt32, 1)
                .AddAttribute(SR_UTILS_NS::VertexAttribute::MaterialID1, SR_UTILS_NS::VertexAttributeFormat::UInt32, 1)
                .AddAttribute(SR_UTILS_NS::VertexAttribute::BlendFactor, SR_UTILS_NS::VertexAttributeFormat::Float32, 1)
            );

            auto&& range = std::views::iota(0, static_cast<int>(verticesCount));

            SR_UTILS_NS::ForEach<SR_UTILS_NS::ExecutionPolicy::ParUnSeq>(range.begin(), range.end(), [&](int index) {
                const Vertex& vertex = pVertices[index];
                m_vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::Position, &vertex.pos);
                m_vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::MaterialID0, &vertex.materialID);
                m_vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::MaterialID1, &vertex.materialID2);
                m_vertices.SetVertex(index, SR_UTILS_NS::VertexAttribute::BlendFactor, &vertex.blend);
            });

            m_pVerticesSSBO->ResetCounter();
            m_pVerticesSSBO->FlushCounter();
            m_pVerticesSSBO->UnMap();
        }
    }

    void ChunkManager::GenerateChunk(const ChunkPosition& position) {
        SR_TRACY_ZONE;

        m_chunksToReload.Remove(position);

        auto&& chunkInfo = m_chunks[position];
        if (!chunkInfo.voxelsDirty) {
            return;
        }
        chunkInfo.voxelsDirty = false;

        auto&& pTransform = chunkInfo.pChunkObject->GetTransform().DynamicCast<SpaRcle::Utils::Transform3D>();
        if (!pTransform) {
            SR_ERROR("ChunkManager::GenerateChunk() : chunk object transform is not Transform3D!");
            return;
        }

        pTransform->SetTranslation({
            static_cast<float_t>(position.x * m_chunkSize),
            static_cast<float_t>(position.y * m_chunkSize),
            static_cast<float_t>(position.z * m_chunkSize)
        });

        chunkInfo.worldPosition = pTransform->GetGlobalTranslation();

        const float_t scale = GetChunkScale();

        const auto aabb = SpaRcle::Utils::Math::AABB(SpaRcle::Utils::Math::FVector3(), {
            static_cast<float_t>(m_chunkSize),
            static_cast<float_t>(m_chunkSize),
            static_cast<float_t>(m_chunkSize)
        });

        pTransform->SetAABB(aabb);
        pTransform->SetScale({ scale, scale, scale });

        if (chunkInfo.voxels.empty()) {
            GenerateChunkDensity(position);
        }
        else {
            if (m_pDensitySSBO->Map()) {
                std::memcpy(m_pDensitySSBO->GetMappedData(), chunkInfo.voxels.data(), chunkInfo.voxels.size() * sizeof(Voxel));
                m_pDensitySSBO->Flush();
                m_pDensitySSBO->UnMap();
            }
        }

        GenerateGeometry();
        ReadIndices();
        ReadVertices();

        computeSmoothNormals(m_vertices, m_indices);

        auto&& pCollisionShape = chunkInfo.pChunkObject->GetComponent<SR_PTYPES_NS::CollisionShape>();
        auto&& pProceduralMesh = chunkInfo.pChunkObject->GetComponent<SR_GTYPES_NS::ProceduralMesh>();
        auto&& pRigidBody = chunkInfo.pChunkObject->GetComponent<SR_PTYPES_NS::Rigidbody>();

        if (!pCollisionShape || !pProceduralMesh || !pRigidBody) {
            SR_ERROR("ChunkManager::GenerateChunk() : chunk object must have CollisionShape and ProceduralMesh components!");
            return;
        }

        m_verticesPositions.resize(m_vertices.GetVertexCount());
        auto&& positionsRange = std::views::iota(0, static_cast<int>(m_vertices.GetVertexCount()));
        std::ranges::transform(positionsRange, m_verticesPositions.begin(), [&](uint32_t index) {
            SR_MATH_NS::FVector3 pos = *(SR_MATH_NS::FVector3*)m_vertices.GetVertex(index, SR_UTILS_NS::VertexAttribute::Position);
            return pos * scale;
        });

        SR_UTILS_NS::OptimizeVertices(m_verticesPositions, m_indices, m_indices.size() / 4, 1e-2f, m_optimizedIndices);

        if (m_verticesPositions.empty()) {
            pProceduralMesh->SetEnabled(false);
            pCollisionShape->SetEnabled(false);
            pRigidBody->SetEnabled(false);
            return;
        }

        pProceduralMesh->SetEnabled(true);
        pCollisionShape->SetEnabled(true);
        pRigidBody->SetEnabled(true);

        pCollisionShape->SwapCustomTriangleMeshVertices(m_verticesPositions);
        pCollisionShape->SwapCustomTriangleMeshIndices(m_optimizedIndices);

        pProceduralMesh->SwapIndices(m_indices);
        pProceduralMesh->SetIndexedVertices(m_vertices);
    }

    void ChunkManager::GenerateChunks() {
        SR_TRACY_ZONE;

        if (m_chunksToLoad.empty()) {
            return;
        }

        const ChunkPosition position = m_chunksToLoad.front();
        m_chunksToLoad.erase(m_chunksToLoad.begin());

        if (m_chunks.contains(position)) {
            return;
        }

        SpaRcle::Utils::GameObject::Ptr pChunkObject;
        if (m_chunksPools.empty()) {
            pChunkObject = gameObject->GetScene()->InstanceFromFile(m_chunkPrefabPath).DynamicCast<SpaRcle::Utils::GameObject>();
            gameObject->AddChild(pChunkObject.StaticCast<SpaRcle::Utils::SceneObject>());
        }
        else {
            pChunkObject = m_chunksPools.back();
            m_chunksPools.pop_back();
        }

        if (!pChunkObject) {
            SR_ERROR("ChunkManager::GenerateChunks() : failed to create chunk object from prefab!");
            return;
        }

        pChunkObject->SetName("Chunk [{}, {}, {}]"_format(position.x, position.y, position.z));
        pChunkObject->SetEnabled(true);

        auto&& chunkInfo = m_chunks[position];
        chunkInfo.position = position;
        chunkInfo.pChunkObject = pChunkObject;

        GenerateChunk(position);
    }

    void ChunkManager::UnloadChunks(bool all) {
        SR_TRACY_ZONE;

        const int unloadRadius = m_unloadRadius;
        m_chunksToUnload.clear();

        for (auto&& [position, chunkInfo] : m_chunks) {
            if (all
                || std::abs(position.x - m_observerPosition.x) > unloadRadius
                || std::abs(position.y - m_observerPosition.y) > unloadRadius
                || std::abs(position.z - m_observerPosition.z) > unloadRadius
            ) {
                m_chunksToUnload.emplace_back(position);
                m_chunksToReload.Remove(position);
            }
        }

        for (const auto& position : m_chunksToUnload) {
            auto it = m_chunks.find(position);
            if (it != m_chunks.end()) {
                auto&& chunkInfo = it->second;
                if (chunkInfo.pChunkObject) {
                    chunkInfo.pChunkObject->SetEnabled(false);
                    m_chunksPools.emplace_back(chunkInfo.pChunkObject);
                }
                m_chunks.erase(it);
            }
        }
    }

    void ChunkManager::ReloadChunkAtPosition(const ChunkPosition& position) {
        SR_TRACY_ZONE;
        m_chunksToReload.Add(position);
    }

    void ChunkManager::Update(float_t dt) {
        SR_TRACY_ZONE;

        Super::Update(dt);

        if (!gameObject || !m_pMarchingComputeShader || !m_pDensityComputeShader) {
            return;
        }

        UpdateChunks();
        GenerateChunks();

        while (!m_chunksToReload.empty()) {
            GenerateChunk(m_chunksToReload.front());
        }
    }

    float_t ChunkManager::GetChunkScale() const {
        float_t scale = static_cast<float_t>(m_chunkSize) / static_cast<float_t>(m_densityCountAxis);
        scale /= static_cast<float_t>(m_densityCountAxis - 2) / static_cast<float_t>(m_densityCountAxis);
        return scale;
    }

    ChunkInfo* ChunkManager::GetChunk(const SR_MATH_NS::IVector3& position) {
        auto&& pIt = m_chunks.find(position);
        return pIt != m_chunks.end() ? &pIt->second : nullptr;
    }

    SR_MATH_NS::IVector3 ChunkManager::WorldToChunkPosition(const SR_MATH_NS::FVector3& worldPosition) const {
        const float_t yOffset = transform->GetGlobalTranslation().y;
        return SR_MATH_NS::IVector3(
            static_cast<int32_t>(std::floor(worldPosition.x / m_chunkSize)),
            static_cast<int32_t>(std::floor((worldPosition.y - yOffset) / m_chunkSize)),
            static_cast<int32_t>(std::floor(worldPosition.z / m_chunkSize))
        );
    }

    ChunkInfo* ChunkManager::GetChunkAtPosition(const SR_MATH_NS::FVector3& position) {
        return GetChunk(WorldToChunkPosition(position));
    }
}