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
    struct alignas(16) Vertex {
        alignas(16) SR_MATH_NS::FVector3 position;
        alignas(16) SR_MATH_NS::FVector3 normal;
        alignas(16) SR_MATH_NS::FVector2 uv;
    };

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
        cameraPosition.y = 0.f; // ignore height for chunk loading

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

        int x = 0, z = 0;
        int dx = 1, dz = 0;

        int segmentLength = 1;
        int segmentPassed = 0;
        int segmentCount = 0;

        const int maxSteps = (r * 2 + 1) * (r * 2 + 1);

        for (int i = 0; i < maxSteps; ++i) {
            if (x * x + z * z <= r2) {
                ChunkPosition coord { m_observerPosition.x + x, 0, m_observerPosition.z + z };
                if (!m_chunks.contains(coord)) {
                    RequestChunk(coord);
                }
            }

            x += dx;
            z += dz;

            segmentPassed++;

            if (segmentPassed == segmentLength) {
                segmentPassed = 0;

                // rotate direction
                int tmp = dx;
                dx = -dz;
                dz = tmp;

                segmentCount++;
                if (segmentCount % 2 == 0) {
                    segmentLength++;
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

        m_pDensitySSBO = SR_GRAPH_NS::SSBOInstance::Create<float_t>(densitiesCount, SR_GRAPH_NS::SSBOUsage::AutoPreferDevice, "densities");
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
            m_vertices.resize(verticesCount);

            auto&& range = std::views::iota(0, static_cast<int>(verticesCount));

            SR_UTILS_NS::ForEach<SR_UTILS_NS::ExecutionPolicy::ParUnSeq>(range.begin(), range.end(), [&](int index) {
                const Vertex& vertex = pVertices[index];
                m_vertices[index] = SR_GRAPH_NS::Vertices::StaticMeshVertex{
                    .pos = vertex.position,
                    .uv = vertex.uv,
                    .norm = vertex.normal
                };
            });

            m_pVerticesSSBO->ResetCounter();
            m_pVerticesSSBO->FlushCounter();
            m_pVerticesSSBO->UnMap();
        }
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

        auto&& pTransform = pChunkObject->GetTransform().DynamicCast<SpaRcle::Utils::Transform3D>();
        if (!pTransform) {
            SR_ERROR("ChunkManager::GenerateChunks() : chunk object transform is not Transform3D!");
            return;
        }

        pTransform->SetTranslation({
            static_cast<float_t>(position.x * m_chunkSize),
            static_cast<float_t>(position.y * m_chunkSize),
            static_cast<float_t>(position.z * m_chunkSize)
        });

        float_t scale = static_cast<float_t>(m_chunkSize) / static_cast<float_t>(m_densityCountAxis);
        scale /= static_cast<float_t>(m_densityCountAxis - 2) / static_cast<float_t>(m_densityCountAxis);

        const auto aabb = SpaRcle::Utils::Math::AABB(SpaRcle::Utils::Math::FVector3(), {
            static_cast<float_t>(m_chunkSize),
            static_cast<float_t>(m_chunkSize),
            static_cast<float_t>(m_chunkSize)});

        pTransform->SetAABB(aabb);
        pTransform->SetScale({ scale, scale, scale });

        GenerateChunkDensity(position);

        GenerateGeometry();
        ReadIndices();
        ReadVertices();

        if (auto&& pCollisionShape = pChunkObject->GetComponent<SR_PTYPES_NS::CollisionShape>()) {
            m_verticesPositions.resize(m_vertices.size());
            std::ranges::transform(m_vertices, m_verticesPositions.begin(), [scale](const SR_GRAPH_NS::Vertices::StaticMeshVertex& vertex) {
                return vertex.pos * scale;
            });

            SR_UTILS_NS::OptimizeVertices(m_verticesPositions, m_indices, m_indices.size() / 4, 1e-2f, m_optimizedIndices);

            pCollisionShape->SwapCustomTriangleMeshVertices(m_verticesPositions);
            pCollisionShape->SwapCustomTriangleMeshIndices(m_optimizedIndices);
        }
        else {
            SR_ERROR("ChunkManager::GenerateChunks() : chunk object does not have CollisionShape component!");
        }

        if (auto&& pProceduralMesh = pChunkObject->GetComponent<SR_GTYPES_NS::ProceduralMesh>()) {
            pProceduralMesh->SwapIndices(m_indices);
            pProceduralMesh->SwapIndexedVertices(m_vertices);
        }
        else {
            SR_ERROR("ChunkManager::GenerateChunks() : chunk object does not have ProceduralMesh component!");
        }

        /*if (auto&& pCollisionShape = pChunkObject->GetComponent<SR_PTYPES_NS::CollisionShape>()) {
            m_densities.resize(std::pow(m_densityCountAxis, 3));
            m_solidDensities.resize(m_densities.size());

            if (void* pData = m_pDensitySSBO->MapData()) {
                std::memcpy(m_densities.data(), pData, sizeof(float_t) * m_densities.size());
                m_pDensitySSBO->UnMap();
            }

            std::ranges::transform(m_densities, m_solidDensities.begin(), [isoLevel = m_isoLevel](float_t density) {
                return static_cast<uint8_t>(density > isoLevel);
            });

            const int32_t padding = 0;
            const int32_t maxAxis = static_cast<int32_t>(m_densityCountAxis) - padding;
            const int32_t density = static_cast<int32_t>(m_densityCountAxis);

            auto&& surface = BuildSurface(m_solidDensities, density, density, density,
                padding, padding, padding,
                maxAxis, maxAxis, maxAxis
            );

            auto&& boxes = BuildGreedyBoxes(m_solidDensities, surface, density, density, density,
                padding, padding, padding,
                maxAxis, maxAxis, maxAxis
            );

            const SR_MATH_NS::FVector3 chunkWorldPos(
                static_cast<float_t>(position.x * static_cast<int32_t>(m_chunkSize)),
                static_cast<float_t>(position.y * static_cast<int32_t>(m_chunkSize)),
                static_cast<float_t>(position.z * static_cast<int32_t>(m_chunkSize))
            );

            for (auto&& box : boxes) {
                const SR_MATH_NS::FVector3 greedySize = box.max - box.min;
                const SR_MATH_NS::FVector3 greedyCenter = (box.min + box.max) * 0.5f;

                const SR_MATH_NS::FVector3 centerOffset = greedyCenter * scale;
                const SR_MATH_NS::FVector3 halfExtents = greedySize * 0.5f * scale;

                box.min = centerOffset;
                box.max = centerOffset + halfExtents;

                //SR_UTILS_NS::DebugDraw::Instance().DrawCube(
                //    SR_ID_INVALID,
                //    (chunkWorldPos + centerOffset) + transform->GetTranslation(),
                //    SR_MATH_NS::Quaternion::Identity(),
                //    halfExtents,
                //    SR_MATH_NS::FColor(255, 0, 0, 100),
                //    30.f
                //);
            }

            pCollisionShape->SwapBoxes(boxes);
        }
        else {
            SR_ERROR("ChunkManager::GenerateChunks() : chunk object does not have CollisionShape component!");
        }*/

        pChunkObject->SetEnabled(true);

        ChunkInfo& chunkInfo = m_chunks[position];
        chunkInfo.position = position;
        chunkInfo.pChunkObject = pChunkObject;
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

    void ChunkManager::Update(float_t dt) {
        SR_TRACY_ZONE;

        Super::Update(dt);

        if (!gameObject || !m_pMarchingComputeShader || !m_pDensityComputeShader) {
            return;
        }

        UpdateChunks();
        GenerateChunks();
    }
}