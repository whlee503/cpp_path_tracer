#pragma once
#include "types.h"

class Camera {
public:
    glm::vec3 eye;     // 카메라 위치
    glm::vec3 lookat;  // 바라보는 지점
    glm::vec3 up;      // 위쪽 방향
    float fovy;        // 수직 시야각


// Camera::generateRay 함수의 실제 구현
    Ray Camera::generateRay(int x, int y, int width, int height) {

        // 1. 카메라 좌표계 계산 (w, u, v 축)
        glm::vec3 w = glm::normalize(eye - lookat);
        glm::vec3 u = glm::normalize(glm::cross(up, w));
        glm::vec3 v = glm::cross(w, u);

        // 2. 이미지 평면의 크기 계산
        float fovy_rad = glm::radians(fovy);
        float half_h = tan(fovy_rad / 2.0f);
        float half_w = half_h * ((float)width / (float)height);

        // 3. 픽셀 좌표를 카메라 좌표계의 벡터로 변환
        float alpha = half_w * (2.0f * (x + 0.5f) / width - 1.0f);
        float beta = half_h * (1.0f - 2.0f * (y + 0.5f) / height);

        // 4. 광선 생성
        Ray ray;
        ray.origin = eye;
        ray.direction = glm::normalize(alpha * u + beta * v - w);

        return ray;
    }

};