#pragma once // 헤더 파일 중복 포함 방지

#include <vector>
#include <glm/glm.hpp> // GLM 라이브러리 사용
#include "Transform.h"

enum LightType { DIRECTIONAL, POINT };
enum BRDFType { PHONG, GGX }; // BRDF 종류 정의

// 광선(Ray) 구조체
struct Ray {
    glm::vec3 origin;    // 시작점
    glm::vec3 direction; // 방향
};

struct Light {
    LightType type;
    glm::vec3 color;
    glm::vec3 position_or_direction; // 타입에 따라 위치 또는 방향
};

// A2: 면 광원 (QuadLight) 구조체
struct QuadLight {
    glm::vec3 a;    // 코너 a
    glm::vec3 ab;   // 벡터 (b - a)
    glm::vec3 ac;   // 벡터 (c - a)
    glm::vec3 intensity; // 광원의 밝기 (L_p)

    glm::vec3 normal;    // 광원의 법선 벡터 (계산 필요)
    float area;          // 광원의 면적 (계산 필요)
};

struct AABB {
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

    // AABB를 확장하는 함수
    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
};

// 모든 도형(Object)의 부모가 될 추상 클래스
class Object {
public:
    //변환 행렬
    mat4 transform;      // 모델 -> 월드 변환 행렬
    mat4 invTransform;   // 월드 -> 모델 변환 행렬 (광선 변환용)
    mat4 invTranspose;   // 법선 벡터 변환용 (나중에 셰이딩에 필요)
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 emission;
    float shininess;
    BRDFType brdfType = PHONG; // 기본값은 Phong
    float roughness = 0.0f;    // GGX에서 사용 (Phong에서는 shininess 사용)

    // 광선과 도형의 교차를 판정하는 순수 가상 함수
    // t: 교차점까지의 거리, ray: 광선

    virtual AABB getAABB() const = 0;
    virtual bool intersect(const Ray& ray, float& t) const = 0;
    virtual glm::vec3 getNormal(const glm::vec3& hit_point_local) const = 0;
    virtual ~Object() {} // 가상 소멸자

};

class Sphere : public Object {
public:
    glm::vec3 center;
    float radius;

    // 생성자
    Sphere(const glm::vec3& c, float r) : center(c), radius(r) {}

    // 교차 판정 함수 (구현은 나중에)
    virtual bool intersect(const Ray& ray, float& t) const override;
    virtual glm::vec3 getNormal(const glm::vec3& hit_point_local) const override;
    virtual AABB getAABB() const override;
};

class Triangle : public Object {
public:
    glm::vec3 v1, v2, v3; // 3개의 정점
    glm::vec3 normal;     // 삼각형의 법선 벡터

    // 생성자
    Triangle(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);

    // 교차 판정 함수 (구현은 .cpp 파일에서)
    virtual bool intersect(const Ray& ray, float& t) const override;
    virtual glm::vec3 getNormal(const glm::vec3& hit_point_local) const override;
    virtual AABB getAABB() const override;
};
