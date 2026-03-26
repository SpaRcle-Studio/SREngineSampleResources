//
// Created by Monika on 23.03.2026.
//

namespace ProceduralWorld {
    SR_HTYPES_NS::FastMemoryArray<uint8_t> BuildSurface(
            const SR_HTYPES_NS::FastMemoryArray<uint8_t>& solid,
            int sx, int sy, int sz,
            int minX, int minY, int minZ,
            int maxX, int maxY, int maxZ)
    {
        auto index = [&](int x, int y, int z) {
            return x + y * sx + z * sx * sy;
        };

        SR_HTYPES_NS::FastMemoryArray<uint8_t> surface;
        surface.resize(solid.size());
        surface.fill(0);

        for (int z = minZ; z < maxZ; ++z) {
            for (int y = minY; y < maxY; ++y) {
                for (int x = minX; x < maxX; ++x) {
                    int i = index(x, y, z);
                    if (!solid[i]) continue;

                    const int dx[6] = {1, -1, 0, 0, 0, 0};
                    const int dy[6] = {0, 0, 1, -1, 0, 0};
                    const int dz[6] = {0, 0, 0, 0, 1, -1};

                    for (int k = 0; k < 6; ++k) {
                        int nx = x + dx[k];
                        int ny = y + dy[k];
                        int nz = z + dz[k];

                        // ВАЖНО: проверяем ТОЛЬКО внутри bounds
                        if (nx < minX || ny < minY || nz < minZ ||
                            nx >= maxX || ny >= maxY || nz >= maxZ) {
                            surface[i] = 1;
                            break;
                        }

                        if (!solid[index(nx, ny, nz)]) {
                            surface[i] = 1;
                            break;
                        }
                    }
                }
            }
        }

        return surface;
    }

    SR_HTYPES_NS::FastMemoryArray<SR_MATH_NS::AABB> BuildGreedyBoxes(
            const SR_HTYPES_NS::FastMemoryArray<uint8_t>& solid,
            const SR_HTYPES_NS::FastMemoryArray<uint8_t>& surface,
            int sizeX, int sizeY, int sizeZ,
            int minX, int minY, int minZ,
            int maxX, int maxY, int maxZ)
    {
        SR_TRACY_ZONE;

        auto index = [&](int x, int y, int z) {
            return x + y * sizeX + z * sizeX * sizeY;
        };

        std::vector<uint8_t> visited(sizeX * sizeY * sizeZ, 0);

        SR_HTYPES_NS::FastMemoryArray<SR_MATH_NS::AABB> result;
        result.reserve(solid.size() / 8);

        for (int z = minZ; z < maxZ; ++z) {
            for (int y = minY; y < maxY; ++y) {
                for (int x = minX; x < maxX; ++x) {

                    int i = index(x, y, z);

                    if (visited[i] || !solid[i] || surface[i])
                        continue;

                    // === 1. X ===
                    int w = 1;
                    while (x + w < maxX) {
                        int ni = index(x + w, y, z);
                        if (visited[ni] || !solid[ni] || surface[ni])
                            break;
                        ++w;
                    }

                    // === 2. Y ===
                    int h = 1;
                    bool expandY = true;

                    while (y + h < maxY && expandY) {
                        for (int dx = 0; dx < w; ++dx) {
                            int ni = index(x + dx, y + h, z);
                            if (visited[ni] || !solid[ni] || surface[ni]) {
                                expandY = false;
                                break;
                            }
                        }
                        if (expandY) ++h;
                    }

                    // === 3. Z ===
                    int d = 1;
                    bool expandZ = true;

                    while (z + d < maxZ && expandZ) {
                        for (int dy = 0; dy < h; ++dy) {
                            for (int dx = 0; dx < w; ++dx) {
                                int ni = index(x + dx, y + dy, z + d);
                                if (visited[ni] || !solid[ni] || surface[ni]) {
                                    expandZ = false;
                                    break;
                                }
                            }
                            if (!expandZ) break;
                        }
                        if (expandZ) ++d;
                    }

                    // === visited ===
                    for (int dz = 0; dz < d; ++dz)
                        for (int dy = 0; dy < h; ++dy)
                            for (int dx = 0; dx < w; ++dx)
                                visited[index(x + dx, y + dy, z + dz)] = 1;

                    SR_MATH_NS::AABB box;
                    box.min = SR_MATH_NS::FVector3(x, y, z);
                    box.max = SR_MATH_NS::FVector3(x + w, y + h, z + d);

                    result.emplace_back(box);
                }
            }
        }

        return result;
    }
}
