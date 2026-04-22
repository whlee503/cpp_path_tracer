#include <iostream>
#include "Scene.h"

void Scene::printDebugInfo() {
    std::cout << "--- Scene Debug Info ---" << std::endl;
    std::cout << "Image Size: " << width << " * " << height << std::endl;
    std::cout << "Output File: " << outputFilename << std::endl;
    std::cout << "Camera Eye: (" << camera.eye.x << ", " << camera.eye.y << ", " << camera.eye.z << ")" << std::endl;
    std::cout << "Camera LookAt: (" << camera.lookat.x << ", " << camera.lookat.y << ", " << camera.lookat.z << ")" << std::endl;
    std::cout << "Camera Up: (" << camera.up.x << ", " << camera.up.y << ", " << camera.up.z << ")" << std::endl;
    std::cout << "Camera FOVy: " << camera.fovy << std::endl << std::endl;
    std::cout << "Total Objects Loaded: " << objects.size() << std::endl;
    std::cout << "Total PointLights Loaded: " << lights.size() << std::endl;
    std::cout << "Total QuadLights Loaded: " << quadLights.size() << std::endl;
    std::cout << "Integrator Type: " << integratorType << std::endl;
    std::cout << "Light Samples: " << lightSamples << std::endl << std::endl;
    std::cout << "Samples per Pixel: " << spp << std::endl;
    std::cout << "Max Depth: " << maxdepth << std::endl;
    std::cout << "Next Event Estimation: " << nextEventEstimation << std::endl;
    std::cout << "Russian Roulette: " << russianRoulette << std::endl;
    std::cout << "Importance Sampling: " << importanceSampling << std::endl;

    std::cout << "------------------------" << std::endl;
}

void Scene::buildAccelerationStructure() {
    // 1. 씬 전체의 AABB 계산
    for (Object* obj : objects) {
        sceneBounds.expand(obj->getAABB());
    }

    float epsilon = 0.00001f;
    if (sceneBounds.min.x == sceneBounds.max.x) {
        sceneBounds.min.x -= epsilon;
        sceneBounds.max.x += epsilon;
    }
    if (sceneBounds.min.y == sceneBounds.max.y) {
        sceneBounds.min.y -= epsilon;
        sceneBounds.max.y += epsilon;
    }
    if (sceneBounds.min.z == sceneBounds.max.z) {
        sceneBounds.min.z -= epsilon;
        sceneBounds.max.z += epsilon;
    }

    // [추가] 물체 개수에 비례하여 Grid 해상도 자동 설정 (Heuristic)
    // 공식: 해상도 ~= 세제곱근(물체개수) * 상주 (보통 2~5배)
    float multiplier = 2.0f; // 씬에 따라 조절 가능 (밀도)
    int resolution = (int)(pow((float)objects.size(), 1.0f / 3.0f) * multiplier);

    // 너무 작거나 크지 않게 클램핑
    if (resolution < 10) resolution = 10;
    if (resolution > 128) resolution = 128; // 메모리 제한 고려 (너무 크면 빌드 느려짐)

    // 계산된 해상도 적용
    gridResolution[0] = resolution;
    gridResolution[1] = resolution;
    gridResolution[2] = resolution;

    std::cout << "Dynamic Grid Resolution: "
        << resolution << " * " << resolution << " * " << resolution << std::endl;

    // 2. 각 셀의 크기 계산
    cellSize = (sceneBounds.max - sceneBounds.min) / glm::vec3(gridResolution[0], gridResolution[1], gridResolution[2]);

    // 3. 그리드 초기화 (10*10*10 = 1000개 셀)
    grid.resize(gridResolution[0] * gridResolution[1] * gridResolution[2]);

    // 4. 모든 물체를 순회하며, 각 물체가 속한 셀에 추가
    for (Object* obj : objects) {
        AABB objBounds = obj->getAABB();

        // 물체의 AABB가 시작하는 셀 인덱스
        glm::vec3 minIdx = (objBounds.min - sceneBounds.min) / cellSize;
        // 물체의 AABB가 끝나는 셀 인덱스
        glm::vec3 maxIdx = (objBounds.max - sceneBounds.min) / cellSize;

        // 물체가 걸쳐있는 모든 셀(i, j, k)을 순회
        for (int z = (int)minIdx.z; z <= (int)maxIdx.z; ++z) {
            for (int y = (int)minIdx.y; y <= (int)maxIdx.y; ++y) {
                for (int x = (int)minIdx.x; x <= (int)maxIdx.x; ++x) {

                    // 3D 인덱스 (x, y, z)를 1D 인덱스로 변환
                    int index = z * (gridResolution[0] * gridResolution[1]) + y * gridResolution[0] + x;

                    // 그리드 범위 체크 (필수)
                    if (index >= 0 && index < grid.size()) {
                        grid[index].objects.push_back(obj);
                    }
                }
            }
        }
    }
}