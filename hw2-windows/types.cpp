// types.cpp
#include <iostream>
#include "types.h" // Sphere 클래스 선언이 포함된 헤더

glm::vec3 Sphere::getNormal(const glm::vec3& hit_point_local) const {
    // 구의 법선 벡터는 (히트 지점 - 중심점) 을 정규화한 것
    return glm::normalize(hit_point_local - this->center);
}

glm::vec3 Triangle::getNormal(const glm::vec3& hit_point_local) const {
    // 삼각형은 모든 지점의 법선 벡터가 동일 (생성자에서 이미 계산함)
    return this->normal;
}

// Sphere 클래스의 intersect 함수를 실제로 구현하는 부분
bool Sphere::intersect(const Ray& ray, float& t) const {
    // 광선의 시작점을 oc로 표현 (origin - center)
    glm::vec3 oc = ray.origin - this->center;

    // 이차 방정식의 계수 a, b, c 계산
    // a = dot(ray.direction, ray.direction) = 1 (direction은 정규화되었다고 가정)
    float a = glm::dot(ray.direction, ray.direction);
    // b = 2 * dot(oc, ray.direction)
    float b = 2.0f * glm::dot(oc, ray.direction);
    // c = dot(oc, oc) - radius^2
    float c = glm::dot(oc, oc) - this->radius * this->radius;

    // 판별식(discriminant) 계산: b^2 - 4ac
    float discriminant = b * b - 4 * a * c;

    // 판별식이 0보다 작으면 교차점이 없음 (허근)
    if (discriminant < 0) {
        return false;
    }
    else {
        // 두 개의 근(교차점)을 계산
        float t0 = (-b - sqrt(discriminant)) / (2.0f * a);
        float t1 = (-b + sqrt(discriminant)) / (2.0f * a);

        // 더 가까운 양수 근을 선택
        if (t0 > 0.00001f) { // 0.001f 같은 작은 epsilon 값을 주어 자기 자신과 교차하는 것을 방지
            t = t0;
            return true;
        }
        if (t1 > 0.00001f) {
            t = t1;
            return true;
        }
        // 두 근 모두 음수이면 구가 광선 뒤에 있다는 의미이므로 교차하지 않음
        return false;
    }
}

// Triangle 클래스의 생성자 구현
Triangle::Triangle(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3)
    : v1(p1), v2(p2), v3(p3) {
    // 생성될 때 삼각형의 법선 벡터를 미리 계산해 둠
    glm::vec3 edge1 = v2 - v1;
    glm::vec3 edge2 = v3 - v1;
    normal = glm::normalize(glm::cross(edge1, edge2));
}

// Triangle 클래스의 intersect 함수 구현 (묄러-트럼보어 알고리즘)
bool Triangle::intersect(const Ray& ray, float& t) const {
    const float EPSILON = 0.00001f;
    glm::vec3 edge1, edge2, h, s, q;
    float a, f, u, v;

    edge1 = v2 - v1;
    edge2 = v3 - v1;
    h = glm::cross(ray.direction, edge2);
    a = glm::dot(edge1, h);

    // 광선이 삼각형의 평면과 평행한 경우
    if (a > -EPSILON && a < EPSILON)
        return false;

    f = 1.0f / a;
    s = ray.origin - v1;
    u = f * glm::dot(s, h);

    // u 좌표가 삼각형 내부에 있는지 확인
    if (u < 0.0f || u > 1.0f)
        return false;

    q = glm::cross(s, edge1);
    v = f * glm::dot(ray.direction, q);

    // v 좌표가 삼각형 내부에 있는지 확인
    if (v < 0.0f || u + v > 1.0f)
        return false;

    // 교차점까지의 거리 t 계산
    float temp_t = f * glm::dot(edge2, q);

    if (temp_t > EPSILON) { // 광선이 삼각형을 통과함
        t = temp_t;
        return true;
    }

    // 교차선만 있고 실제 교차는 없는 경우
    return false;
}

AABB Sphere::getAABB() const {
    // 1. 구의 로컬(오브젝트) 공간 AABB를 계산합니다.
    glm::vec3 min_local = this->center - glm::vec3(this->radius);
    glm::vec3 max_local = this->center + glm::vec3(this->radius);

    // 2. 이 로컬 AABB의 8개 꼭짓점을 정의합니다.
    std::vector<glm::vec4> corners_local = {
        {min_local.x, min_local.y, min_local.z, 1.0f},
        {max_local.x, min_local.y, min_local.z, 1.0f},
        {min_local.x, max_local.y, min_local.z, 1.0f},
        {min_local.x, min_local.y, max_local.z, 1.0f},
        {max_local.x, max_local.y, min_local.z, 1.0f},
        {min_local.x, max_local.y, max_local.z, 1.0f},
        {max_local.x, min_local.y, max_local.z, 1.0f},
        {max_local.x, max_local.y, max_local.z, 1.0f}
    };

    AABB world_aabb; // 반환할 월드 공간 AABB (초기값은 max/lowest)

    // 3. 8개의 꼭짓점을 모두 월드 공간으로 변환(transform)하고,
    //    새로운 min/max를 찾아 월드 AABB를 확장합니다.
    for (const auto& corner : corners_local) {
        glm::vec3 corner_world = glm::vec3(this->transform * corner);

        // AABB 구조체를 임시로 사용해 min/max를 찾습니다.
        AABB temp_aabb;
        temp_aabb.min = corner_world;
        temp_aabb.max = corner_world;
        world_aabb.expand(temp_aabb);
    }

    return world_aabb;
}

AABB Triangle::getAABB() const {
    float epsilon = 0.000001f;
    // 1. 삼각형의 3개 정점은 이미 로컬 공간에 있습니다 (v1, v2, v3).
    // 2. 이 정점들을 월드 공간으로 변환합니다.
    glm::vec3 v1_world = glm::vec3(this->transform * glm::vec4(v1, 1.0f + epsilon));
    glm::vec3 v2_world = glm::vec3(this->transform * glm::vec4(v2, 1.0f + epsilon));
    glm::vec3 v3_world = glm::vec3(this->transform * glm::vec4(v3, 1.0f + epsilon));

    // 3. 3개의 월드 좌표를 기반으로 AABB를 생성합니다.
    AABB world_aabb;
    world_aabb.min = glm::min(v1_world, glm::min(v2_world, v3_world));
    world_aabb.max = glm::max(v1_world, glm::max(v2_world, v3_world));

    // 물체가 매우 얇을 경우 AABB가 납작해지는 것을 방지하기 위해
    // 아주 작은 두께(epsilon)를 더해줄 수 있습니다.
     
     if (world_aabb.min.x == world_aabb.max.x) {
         world_aabb.max.x += epsilon;
     }
     if (world_aabb.min.y == world_aabb.max.y) {
         world_aabb.max.y += epsilon;
     }
     if (world_aabb.min.z == world_aabb.max.z) {
         world_aabb.max.z += epsilon;
     }

    return world_aabb;
}