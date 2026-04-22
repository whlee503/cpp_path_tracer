
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <stack>
#include <FreeImage.h>
#include <random>
#include <omp.h>
#include "UCSD/grader.h"
#include "readfile.h" 
#include "Scene.h"
// #include "Geometry.h" // 이전 헤더 대신 새로운 헤더 포함
//#include "variables.h" 
 

using namespace std;

// Main variables in the program.  
#define MAINPROGRAM

Grader grader;
bool allowGrader = false;

std::mt19937 generator;
std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
const float PI = 3.14159265359f;


glm::vec3 trace(const Ray& ray, const Scene& scene, int depth, bool is_shadow_ray = false, glm::vec3 throughput = glm::vec3(1.0f), float last_pdf = 0.0f);

glm::vec3 computeShading(const Ray& ray, const Scene& scene, Object* hit_object, float min_t, const Ray& rayInObjectSpace, int depth, glm::vec3 throughput, float last_pdf);


// Ray-AABB 'Slab Test' 교차 판정 함수
// t_min_out과 t_max_out에 교차점의 t값을 저장합니다.
bool intersectAABB(const Ray& ray, const AABB& bounds, float& t_min_out, float& t_max_out) {
    // X축
    float invDirX = 1.0f / ray.direction.x;
    float t0x = (bounds.min.x - ray.origin.x) * invDirX;
    float t1x = (bounds.max.x - ray.origin.x) * invDirX;
    if (invDirX < 0.0f) std::swap(t0x, t1x);

    // Y축
    float invDirY = 1.0f / ray.direction.y;
    float t0y = (bounds.min.y - ray.origin.y) * invDirY;
    float t1y = (bounds.max.y - ray.origin.y) * invDirY;
    if (invDirY < 0.0f) std::swap(t0y, t1y);

    // Z축
    float invDirZ = 1.0f / ray.direction.z;
    float t0z = (bounds.min.z - ray.origin.z) * invDirZ;
    float t1z = (bounds.max.z - ray.origin.z) * invDirZ;
    if (invDirZ < 0.0f) std::swap(t0z, t1z);

    // 각 축의 '진입 시간(t)' 중 가장 늦은 시간
    t_min_out = std::max(t0x, std::max(t0y, t0z));
    // 각 축의 '탈출 시간(t)' 중 가장 이른 시간
    t_max_out = std::min(t1x, std::min(t1y, t1z));

    // t_min이 t_max보다 크면 교차하지 않음
    // t_max가 0보다 작으면 AABB가 광선 뒤에 있음
    return (t_min_out < t_max_out) && (t_max_out > 0.0f);
}

// [헬퍼] 좌표계 생성 (Orthonormal Basis)
// N: 표면의 법선 (W축 역할)
// Nt, Nb: 계산될 접선 벡터들 (U, V축)
void createCoordinateSystem(const glm::vec3& N, glm::vec3& Nt, glm::vec3& Nb) {
    if (std::abs(N.x) > std::abs(N.y))
        Nt = glm::vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
    else
        Nt = glm::vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
    Nb = glm::cross(N, Nt);
}

// [헬퍼] 균일 반구 샘플링 (Uniform Sample Hemisphere)
// r1, r2: 0~1 사이의 난수
glm::vec3 uniformSampleHemisphere(float r1, float r2) {
    // 구면 좌표계 공식 (지시사항에 나온 식)
    float z = r1;
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2.0f * PI * r2;
    float x = r * cos(phi);
    float y = r * sin(phi);
    return glm::vec3(x, y, z);
}

// [헬퍼] cosin 반구 샘플링 (cosine Sample Hemisphere)
glm::vec3 cosineSampleHemisphere(float u1, float u2) {
    float r = sqrt(u1);             // 수평 거리 (sin theta에 해당)
    float theta = 2.0f * PI * u2;   // 방위각 (phi)

    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(std::max(0.0f, 1.0f - u1)); // 높이 (cos theta에 해당)

    return glm::vec3(x, y, z);
}

// [헬퍼] Modified Phong의 Specular Lobe를 샘플링하는 함수
glm::vec3 samplePhongSpecular(const glm::vec3& view_dir, const glm::vec3& normal, float shininess, float r1, float r2) {
    // 1. 완벽한 반사 벡터 R 계산
    glm::vec3 R = glm::reflect(-view_dir, normal); // view_dir는 표면->눈 방향이라 가정하므로 - 붙임

    // 2. R을 기준으로 하는 좌표계 생성 (u, v, w)
    glm::vec3 w = R;
    glm::vec3 u, v;
    createCoordinateSystem(w, u, v);

    // 3. 구면 좌표계 샘플링 (Phong Lobe에 맞게)
    // cos(alpha) = u1 ^ (1 / (s + 1))
    float cos_alpha = pow(r1, 1.0f / (shininess + 1.0f));
    float sin_alpha = sqrt(std::max(0.0f, 1.0f - cos_alpha * cos_alpha));
    float phi = 2.0f * PI * r2;

    // 4. 좌표 변환 (Local -> World)
    float x = sin_alpha * cos(phi);
    float y = sin_alpha * sin(phi);
    float z = cos_alpha;

    glm::vec3 sampled_dir = glm::normalize(u * x + v * y + w * z);
    return sampled_dir;
}

// [헬퍼] Phong Specular의 PDF 계산 함수
float pdfPhongSpecular(const glm::vec3& view_dir, const glm::vec3& normal, const glm::vec3& sampled_dir, float shininess) {
    glm::vec3 R = glm::reflect(-view_dir, normal);
    float cos_alpha = std::max(0.0f, glm::dot(R, sampled_dir));

    // PDF = (s + 1) / (2 * PI) * cos_alpha ^ s
    return ((shininess + 1.0f) / (2.0f * PI)) * pow(cos_alpha, shininess);
}

// [헬퍼] GGX
// 1. GGX Normal Distribution Function (Trowbridge-Reitz)
float D_GGX(float NdotH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH) * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / (PI * denom * denom);
}
// 2. Geometry Function (Smith's Schlick-GGX)
float G1_GGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0f - k) + k);
}
float G_Smith(float NdotV, float NdotL, float roughness) {
    // Direct Lighting에서는 k = (r+1)^2 / 8, IBL/PathTracing은 k = r^2 / 2
    // 여기서는 Path Tracing이므로 k = alpha^2 / 2 가 정석이나, 
    // 과제마다 Direct Light용 k를 쓰기도 합니다. 보통 안전하게 Direct용 k를 많이 씁니다.
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float ggx1 = G1_GGX(NdotV, k);
    float ggx2 = G1_GGX(NdotL, k);
    return ggx1 * ggx2;
}
// 3. Fresnel Function (Schlick approximation)
glm::vec3 F_Schlick(float HdotV, glm::vec3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - HdotV, 5.0f);
}

// [헬퍼] Modified Phong BRDF 평가 함수
glm::vec3 evalBRDF(Object* hit_object, const glm::vec3& V, const glm::vec3& N, const glm::vec3& L) {
    if (hit_object->brdfType == PHONG) {
        // 1. Diffuse Term: kd / PI
        glm::vec3 brdf_diffuse = hit_object->diffuse / PI;

        // 2. Specular Term: ks * (n+2)/(2*PI) * (R dot L)^n
        glm::vec3 brdf_specular(0.0f);

        // specular나 shininess가 있을 때만 계산
        if (glm::length(hit_object->specular) > 0.0f && hit_object->shininess > 0.0f) {
            // R: 시선(V)을 법선(N)에 대해 반사시킨 벡터 (또는 L을 반사시켜 V와 내적해도 됨)
            // 여기서는 지시사항의 수식에 맞춰 R = reflect(-L, N) dot V 로 계산하거나
            // 대칭성을 이용해 R = reflect(-V, N) dot L 을 사용합니다.

            // 반사 벡터 계산 (V를 N에 대해 반사)
            glm::vec3 R = glm::reflect(-V, N);
            float RdotL = std::max(0.0f, glm::dot(R, L));

            float normalization = (hit_object->shininess + 2.0f) / (2.0f * PI);
            brdf_specular = hit_object->specular * normalization * std::pow(RdotL, hit_object->shininess);
        }

        return brdf_diffuse + brdf_specular;
    }

    // 2. GGX BRDF (신규 추가)
    else if (hit_object->brdfType == GGX) {
        glm::vec3 H = glm::normalize(V + L); // Half Vector
        float NdotV = std::max(0.0f, glm::dot(N, V));
        float NdotL = std::max(0.0f, glm::dot(N, L));
        float NdotH = std::max(0.0f, glm::dot(N, H));
        float HdotV = std::max(0.0f, glm::dot(H, V));

        if (NdotL <= 0.0f || NdotV <= 0.0f) return glm::vec3(0.0f);

        // Cook-Torrance BRDF 수식
        // f_r = (D * F * G) / (4 * (N.V) * (N.L))

        float D = D_GGX(NdotH, hit_object->roughness);
        float G = G_Smith(NdotV, NdotL, hit_object->roughness);
        glm::vec3 F = F_Schlick(HdotV, hit_object->specular); // Specular 색상을 F0로 사용

        glm::vec3 numerator = D * G * F;
        float denominator = 4.0f * NdotV * NdotL + 0.00001f; // 0 나누기 방지

        glm::vec3 specularTerm = numerator / denominator;

        // Energy Conservation: Specular가 반사한 만큼 Diffuse는 줄어듦 (선택 사항이나 퀄리티 위해 추천)
        glm::vec3 kS = F;
        glm::vec3 kD = glm::vec3(1.0f) - kS;
        glm::vec3 diffuseTerm = kD * (hit_object->diffuse / PI);

        return diffuseTerm + specularTerm;
    }

    return glm::vec3(0.0f);        
}

// [헬퍼] GGX Importance Sampling
glm::vec3 sampleGGX(float r1, float r2, float roughness, const glm::vec3& N, const glm::vec3& V) {
    float alpha = roughness * roughness;

    // 1. 구면 좌표계 샘플링 (NDF에 비례)
    float phi = 2.0f * PI * r1;
    float cos_theta = sqrt((1.0f - r2) / (r2 * (alpha * alpha - 1.0f) + 1.0f));
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

    // 2. Half Vector (Local)
    float x = sin_theta * cos(phi);
    float y = sin_theta * sin(phi);
    float z = cos_theta;
    glm::vec3 H_local = glm::vec3(x, y, z);

    // 3. World Space 변환
    glm::vec3 Up, Right;
    createCoordinateSystem(N, Up, Right);
    glm::vec3 H = glm::normalize(Up * x + Right * y + N * z); // 순서 주의 (Up=Tangent, Right=Bitangent, N=Normal)

    // 4. Light Vector 계산 (Reflection)
    // L = reflect(-V, H) -> V를 H에 대해 반사
    // 공식: L = 2 * dot(V, H) * H - V
    glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

    return L;
}

// [헬퍼] GGX PDF Calculation
float pdfGGX(const glm::vec3& N, const glm::vec3& V, const glm::vec3& L, float roughness) {
    glm::vec3 H = glm::normalize(V + L);
    float NdotH = std::max(0.0f, glm::dot(N, H));
    float HdotV = std::max(0.0f, glm::dot(H, V));

    if (NdotH <= 0.0f || HdotV <= 0.0f) return 0.0f;

    // PDF(H) = D(H) * cos(theta_h)
    // PDF(L) = PDF(H) / (4 * (V.H))  <-- Jacobian 변환
    float D = D_GGX(NdotH, roughness);

    // 원래 식: D * NdotH / (4 * HdotV)
    return (D * NdotH) / (4.0f * HdotV);
}

// 헬퍼 함수: 그림자 광선이 막혔는지 확인 (가속 구조 사용)
// ray: 월드 좌표계 그림자 광선
// max_distance: 광원까지의 거리
bool isOccluded(const Ray& ray, const Scene& scene, float max_distance, Object* self_object) {

    int loop_safety_count = 0;

    // 1. 광선이 씬 바운드와 교차하는지 확인
    float t_min_grid, t_max_grid;
    if (!intersectAABB(ray, scene.sceneBounds, t_min_grid, t_max_grid)) {
        return false; // 씬 바운드와 안 부딪히면 안 막힘
    }

    // AABB 교차가 광원보다 멀리서 시작하면 안 막힘
    if (t_min_grid > max_distance) {
        return false;
    }

    // 광선이 그리드 안에서 시작하면(t_min < 0), 진입점을 0으로 (혹은 epsilon)
    if (t_min_grid < 0.00001f) {
        t_min_grid = 0.00001f;
    }

    // 2. Voxel Traversal 초기화 (trace 함수와 동일)
    glm::vec3 rayStart = ray.origin + ray.direction * t_min_grid;
    glm::vec3 currentCellIdx = (rayStart - scene.sceneBounds.min) / scene.cellSize;

    int x = (int)currentCellIdx.x;
    int y = (int)currentCellIdx.y;
    int z = (int)currentCellIdx.z;

    x = glm::clamp(x, 0, scene.gridResolution[0] - 1);
    y = glm::clamp(y, 0, scene.gridResolution[1] - 1);
    z = glm::clamp(z, 0, scene.gridResolution[2] - 1);

    glm::vec3 tMax, tDelta, step;

    // (X, Y, Z축 tMax, tDelta, step 초기화 - trace 함수와 동일)
    // X축
    if (ray.direction.x > 0) {
        step.x = 1;
        tMax.x = (scene.sceneBounds.min.x + (x + 1) * scene.cellSize.x - ray.origin.x) / ray.direction.x;
        tDelta.x = scene.cellSize.x / ray.direction.x;
    }
    else {
        step.x = -1;
        tMax.x = (scene.sceneBounds.min.x + x * scene.cellSize.x - ray.origin.x) / ray.direction.x;
        tDelta.x = -scene.cellSize.x / ray.direction.x;
    }
    // Y축
    if (ray.direction.y > 0) {
        step.y = 1;
        tMax.y = (scene.sceneBounds.min.y + (y + 1) * scene.cellSize.y - ray.origin.y) / ray.direction.y;
        tDelta.y = scene.cellSize.y / ray.direction.y;
    }
    else {
        step.y = -1;
        tMax.y = (scene.sceneBounds.min.y + y * scene.cellSize.y - ray.origin.y) / ray.direction.y;
        tDelta.y = -scene.cellSize.y / ray.direction.y;
    }
    // Z축
    if (ray.direction.z > 0) {
        step.z = 1;
        tMax.z = (scene.sceneBounds.min.z + (z + 1) * scene.cellSize.z - ray.origin.z) / ray.direction.z;
        tDelta.z = scene.cellSize.z / ray.direction.z;
    }
    else {
        step.z = -1;
        tMax.z = (scene.sceneBounds.min.z + z * scene.cellSize.z - ray.origin.z) / ray.direction.z;
        tDelta.z = -scene.cellSize.z / ray.direction.z;
    }

    float t_next = t_min_grid;

    // 3. Voxel Traversal 루프
    while (true) {
        // [안전 차단기] 루프가 비정상적으로 많이 돌면 강제 탈출
        if (++loop_safety_count > 9999) {
            break;
        }

        // [수정] 광선이 광원보다 멀어졌거나(1) 그리드를 이탈하면(2) 종료
        if (t_next > max_distance || t_next > t_max_grid) {
            break;
        }

        int index = z * (scene.gridResolution[0] * scene.gridResolution[1]) + y * scene.gridResolution[0] + x;
        if (index >= 0 && index < scene.grid.size()) {
            const GridCell& cell = scene.grid[index];

            // 4. 셀 내부 물체와 교차 판정
            for (Object* obj : cell.objects) {

                // 셰이딩을 시작한 "자기 자신"은 교차 테스트에서 제외
                if (obj == self_object) {
                    continue;
                }
                // [수정] 광원 자신이 스스로 그림자를 만들지 않도록 함
                if (glm::length(obj->emission) > 0.0f) {
                    continue;
                }

                Ray rayInObjectSpace;
                rayInObjectSpace.origin = glm::vec3(obj->invTransform * vec4(ray.origin, 1.0f));
                rayInObjectSpace.direction = glm::vec3(obj->invTransform * vec4(ray.direction, 0.0f));

                float t;
                // [수정] t < max_distance 조건만 확인
                if (obj->intersect(rayInObjectSpace, t) && t < max_distance) {
                    // 이 교차점(t)이 현재 셀 범위(t_next)보다 뒤에 있는지 확인
                    if (t > t_next) {
                        return true; // ★ 막혔음! (Occluded)
                    }
                }
            }
        }

        // 5. 다음 셀로 이동 (trace 함수와 동일)
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                t_next = tMax.x;
                x += step.x;
                tMax.x += tDelta.x;
            }
            else {
                t_next = tMax.z;
                z += step.z;
                tMax.z += tDelta.z;
            }
        }
        else {
            if (tMax.y < tMax.z) {
                t_next = tMax.y;
                y += step.y;
                tMax.y += tDelta.y;
            }
            else {
                t_next = tMax.z;
                z += step.z;
                tMax.z += tDelta.z;
            }
        }
    }

    return false; // ★ 안 막혔음! (Visible)
}


void printHelp() {
    std::cout << "\nRaytracer for CSE167\n";
}


glm::vec3 computeShading(const Ray& ray, const Scene& scene, Object* hit_object, float min_t, const Ray& rayInObjectSpace, int depth, glm::vec3 throughput, float last_pdf)
{
    // --- 1. 셰이딩에 필요한 공통 정보 계산 ---
    glm::vec3 hit_pos_local = rayInObjectSpace.origin + min_t * rayInObjectSpace.direction;
    glm::vec3 hit_pos_world = glm::vec3(hit_object->transform * vec4(hit_pos_local, 1.0f));
    glm::vec3 normal_local = hit_object->getNormal(hit_pos_local);
    glm::vec3 normal_world = glm::normalize(glm::vec3(hit_object->invTranspose * vec4(normal_local, 0.0f)));
    glm::vec3 view_dir = glm::normalize(ray.origin - hit_pos_world);

    // 1. 초기 Emission 값 가져오기
    glm::vec3 L_emission = hit_object->emission;


    // [MIS Step 2] Emission에 MIS 가중치 적용 (Double Counting 방지)

    // 깊이가 0보다 크다 = "간접광(Bounce)으로 빛을 쳐다봤다"
    // NEE가 켜져 있다면, 이 빛은 이미 NEE에서 계산했어야 할 빛입니다.
    if (depth > 0 && scene.nextEventEstimation != NONE && glm::length(L_emission) > 0.0f) {

        // (Case A) MIS 모드가 아닐 때 (순수 NEE)
        // -> 간접광으로 맞은 빛은 무조건 0으로 끕니다. (NEE가 다 하니까)
        if (scene.nextEventEstimation != MIS) {
            L_emission = glm::vec3(0.0f);
        }

        // (Case B) MIS 모드일 때 (Balance Heuristic)
        // -> "내가 BRDF로 쐈는데(last_pdf), 만약 NEE로 쐈으면 확률(pdf_nee)이 얼마였을까?"
        else {
            // 1) 광원 기하 정보 계산
            float dist_sq = min_t * min_t;
            float cos_theta_light = std::max(0.0f, glm::dot(normal_world, -ray.direction));

            // 2) 광원 면적(Area) 가져오기

            // [수정] 광원 면적(light_area) 계산
            float light_area = 0.0f;

            // 1. 구(Sphere) 인지 확인
            Sphere* sphere = dynamic_cast<Sphere*>(hit_object);
            if (sphere != nullptr) {
                // Sphere Area = 4 * PI * r^2
                light_area = 4.0f * PI * sphere->radius * sphere->radius;
            }
            // 2. 삼각형(Triangle) 인지 확인
            else {
                Triangle* tri = dynamic_cast<Triangle*>(hit_object);
                if (tri != nullptr) {
                    // [기본] 삼각형 자체의 면적 계산
                    // Area = 0.5 * |(v2 - v1) x (v3 - v1)|
                    glm::vec3 edge1 = tri->v2 - tri->v1;
                    glm::vec3 edge2 = tri->v3 - tri->v1;
                    light_area = 0.5f * glm::length(glm::cross(edge1, edge2));

                    // [중요] QuadLight 처리 (MIS 핵심)
                    // 만약 이 삼각형이 '사각형 광원(QuadLight)'의 일부라면, 
                    // NEE는 '전체 사각형 면적'을 기준으로 확률을 계산했습니다.
                    // 따라서 여기서도 '전체 사각형 면적'을 써줘야 가중치 균형이 맞습니다.

                    // Scene에 있는 QuadLight 목록을 뒤져서, 밝기(Emission)가 같은 광원을 찾습니다.
                    for (const auto& light : scene.quadLights) {
                        // 밝기가 거의 같다면 같은 광원으로 간주
                        if (glm::length(light.intensity - hit_object->emission) < 1e-4f) {
                            light_area = light.area; // QuadLight 전체 면적 사용!
                            break;
                        }
                    }
                }
            }

            // 3) NEE PDF 역산 (Solid Angle 기준)
            // PDF_NEE = (dist^2) / (Area * cos_theta * LightCount)
            float pdf_nee = 0.0f;
            float num_lights = (float)scene.lights.size(); // 전체 광원 개수

            if (light_area > 0.0f && cos_theta_light > 1e-6f) {
                pdf_nee = dist_sq / (light_area * cos_theta_light * num_lights);
            }

            // 4) 가중치 계산: w = p_brdf / (p_brdf + p_nee)
            float mis_weight = 1.0f;
            float sum_pdf = last_pdf + pdf_nee;
            if (sum_pdf > 1e-10f) {
                mis_weight = last_pdf / sum_pdf;
            }

            // 가중치 적용
            L_emission *= mis_weight;
        }
    }

    glm::vec3 final_color = L_emission; // 최종 결정된 Emission으로 초기화


    // --- [버그 1 수정] 조기 리턴 (광원 표면의 검은 점 제거) ⬇️ ---
    // 물체 자체가 빛을 낸다면(광원이라면), 추가 조명 계산(analytic, direct)을
    // 시도하지 말고(NaN 발생함) 즉시 방출 색상을 반환합니다.
    if (glm::length(hit_object->emission) > 0.00001f) {
        // [NEE 수정] NEE가 켜져있고(ON), 이것이 간접 광선(depth > 0)이라면,
        // 광원의 빛을 0으로 반환해야 합니다. (직접 조명 단계에서 이미 계산함)
        if (scene.integratorType == PATHTRACER && scene.nextEventEstimation != NONE && depth > 0) {
            return glm::vec3(0.0f);
        }
        return final_color;
    }
    // --- ⬆️ 버그 1 수정 끝 ⬆️ ---

    // --- 3. 적분기(Integrator)에 따라 분기 ---
    
    if (scene.integratorType == ANALYTIC) {
        // [ analyticdirect ] 구현
        glm::vec3 brdf = hit_object->diffuse / glm::pi<float>();

        for (const QuadLight& light : scene.quadLights) {
            glm::vec3 L_p = light.intensity;
            glm::vec3 E_vec(0.0f);

            glm::vec3 v0 = light.a;
            glm::vec3 v1 = light.a + light.ab;
            glm::vec3 v2 = light.a + light.ab + light.ac;
            glm::vec3 v3 = light.a + light.ac;


            // --- ⬇️ [버그 1 수정] L0,L1,L2,L3 계산 시 normalize(0) 방지 ⬇️ ---
            glm::vec3 L_vec[4];
            L_vec[0] = v0 - hit_pos_world;
            L_vec[1] = v1 - hit_pos_world;
            L_vec[2] = v2 - hit_pos_world;
            L_vec[3] = v3 - hit_pos_world;

            for (int i = 0; i < 4; ++i) {
                if (glm::length(L_vec[i]) > 1e-6f) { // 0벡터인지 확인
                    L_vec[i] = glm::normalize(L_vec[i]);
                }
                else {
                    L_vec[i] = glm::vec3(0.0f); // 0벡터면 그냥 0으로
                }
            }

            glm::vec3 L0 = L_vec[0];
            glm::vec3 L1 = L_vec[1];
            glm::vec3 L2 = L_vec[2];
            glm::vec3 L3 = L_vec[3];
            // --- ⬆️ 버그 1 수정 끝 ⬆️ ---


            // --- ⬇️ [버그 2 수정] Gamma 계산 시 normalize(0) 방지 ⬇️ ---

            // Edge 0->1
            float dot01 = glm::clamp(glm::dot(L0, L1), -1.0f, 1.0f);
            float theta0 = acos(dot01);
            glm::vec3 Gamma0 = glm::cross(L0, L1); // 1. normalize 없이 cross만
            if (glm::length(Gamma0) > 1e-6f) {    // 2. 길이를 먼저 체크
                Gamma0 = glm::normalize(Gamma0); // 3. 안전할 때만 normalize
                E_vec += theta0 * Gamma0;
            }

            // Edge 1->2
            float dot12 = glm::clamp(glm::dot(L1, L2), -1.0f, 1.0f);
            float theta1 = acos(dot12);
            glm::vec3 Gamma1 = glm::cross(L1, L2); // 1.
            if (glm::length(Gamma1) > 1e-6f) {    // 2.
                Gamma1 = glm::normalize(Gamma1); // 3.
                E_vec += theta1 * Gamma1;
            }

            // Edge 2->3
            float dot23 = glm::clamp(glm::dot(L2, L3), -1.0f, 1.0f);
            float theta2 = acos(dot23);
            glm::vec3 Gamma2 = glm::cross(L2, L3); // 1.
            if (glm::length(Gamma2) > 1e-6f) {    // 2.
                Gamma2 = glm::normalize(Gamma2); // 3.
                E_vec += theta2 * Gamma2;
            }

            // Edge 3->0
            float dot30 = glm::clamp(glm::dot(L3, L0), -1.0f, 1.0f);
            float theta3 = acos(dot30);
            glm::vec3 Gamma3 = glm::cross(L3, L0); // 1.
            if (glm::length(Gamma3) > 1e-6f) {    // 2.
                Gamma3 = glm::normalize(Gamma3); // 3.
                E_vec += theta3 * Gamma3;
            }
            // --- ⬆️ 버그 2 수정 끝 ⬆️ ---

            E_vec *= 0.5f;

            float E = glm::dot(E_vec, normal_world);
            final_color += L_p * brdf * std::max(0.0f, E);
        }

    }

    else if (scene.integratorType == DIRECT) {
        // [ direct ] (몬테카를로) 구현
        // (이 적분기는 A1의 점/방향 광원을 무시하고, 오직 quadLights만 사용합니다)

        // 모든 면 광원(QuadLight)을 순회합니다.
        for (const QuadLight& light : scene.quadLights) {
            glm::vec3 L_p = light.intensity; // 광원 강도
            float A = light.area;          // 광원 면적
            int N = scene.lightSamples;    // 샘플 개수

            glm::vec3 accumulated_light(0.0f); // N개의 샘플 결과를 누적할 변수

            // --- N번 샘플링 루프 ---
            for (int i = 0; i < N; ++i) {

                // --- 1. 샘플링 (Sampling) ---
                float xi1, xi2; // 샘플링에 사용할 두 난수

                if (scene.stratifyLights) {
                    // [ 계층화 샘플링 (Stratified Sampling) ]
                    int n = (int)sqrtf((float)N); 

                    // i를 (row, col) 좌표로 변환
                    int row = i / n; // 0, 0, 0, 1, 1, 1, 2, 2, 2
                    int col = i % n; // 0, 1, 2, 0, 1, 2, 0, 1, 2

                    // 각 격자 칸 안에서의 임의의 위치 (0.0 ~ 1.0)
                    float rand_u = distribution(generator);
                    float rand_v = distribution(generator);

                    // 최종 샘플 위치를 (0.0 ~ 1.0) 범위로 매핑
                    xi1 = (col + rand_u) / (float)n;
                    xi2 = (row + rand_v) / (float)n;

                }
                else {
                    // [ 기존의 무작위 샘플링 ]
                    xi1 = distribution(generator);
                    xi2 = distribution(generator);
                }


                // 평행사변형 위의 임의의 점 (x_prime)
                glm::vec3 x_prime = light.a + xi1 * light.ab + xi2 * light.ac;

                // --- 2. 가시성 V (Shadow Ray) ---
                glm::vec3 light_vec = x_prime - hit_pos_world;
                float distance_to_light = glm::length(light_vec);
                glm::vec3 light_dir = glm::normalize(light_vec);

                float epsilon = 0.00001f;
                Ray shadowRay;
                shadowRay.origin = hit_pos_world + epsilon * normal_world;
                shadowRay.direction = light_dir;

                float V = 1.0f; // 가시성 (1.0 = 보임, 0.0 = 그림자)

                // ✅ [수정] isOccluded 함수로 그림자 광선 테스트
                if (isOccluded(shadowRay, scene, distance_to_light, hit_object)) {
                    continue; // 이 샘플은 막혔으므로 (V=0) 다음 샘플로
                }

                // --- 3. 기하 항 G (Geometry Term) ---
                // (광원의 법선은 셰이딩 지점을 "바라보는" 방향이어야 함)
                glm::vec3 light_normal = -light.normal; // 지시사항 주의!

                float cos_theta = std::max(0.0f, glm::dot(normal_world, light_dir));
                float cos_theta_prime = std::max(0.0f, glm::dot(light_normal, -light_dir));
                float G = (cos_theta * cos_theta_prime) / (distance_to_light * distance_to_light);

                // --- 4. 수정된 Phong BRDF (f_r) ---
                glm::vec3 R = glm::reflect(-view_dir, normal_world);
                float RdotL = std::max(0.0f, glm::dot(R, light_dir));

                glm::vec3 brdf_diffuse = hit_object->diffuse / glm::pi<float>();
                glm::vec3 brdf_specular = hit_object->specular * (hit_object->shininess + 2.0f) / (2.0f * glm::pi<float>()) * std::pow(RdotL, hit_object->shininess);
                glm::vec3 f_r = brdf_diffuse + brdf_specular;

                // --- 5. 합산 ---
                if (V > 0.0f) {
                    accumulated_light += V * f_r * G * L_p;
                }
            }

            // N으로 나누어 평균을 낸 후, 면적 A를 곱함 (Equation 10)
            final_color += (accumulated_light * A) / (float)N;
        }
    }

    else if (scene.integratorType == PATHTRACER) {
        // [ 1단계: Simple Path Tracer ]
        // 1. 방출광 (Emission) 더하기(재귀의 끝이거나, 광원을 직접 본 경우)
        glm::vec3 L_emit = hit_object->emission;

        // 2. 직접 조명 (L_direct) - NEE가 켜져있을 때만 계산
        glm::vec3 L_direct(0.0f);
        if (scene.nextEventEstimation != NONE) {
            // [ DIRECT 적분기 로직 재사용 ] (quadLight 루프, 샘플링, 그림자 광선, Phong BRDF 등)
            // Phong BRDF 대신 여기서는 Lambertian BRDF를 써도 되고, 과제 요구사항에 따라 Phong을 써도 됩니다.
            // 보통 Path Tracer에서는 물체의 실제 재질(여기선 Lambertian 가정)을 씁니다.

            if (scene.maxdepth > 0 && depth >= scene.maxdepth) {
                return final_color + L_direct;
            }

            for (const QuadLight& light : scene.quadLights) {
                glm::vec3 L_p = light.intensity;
                float A = light.area;
                int N_light = 1; // NEE에서는 보통 교차점당 광원 샘플 1개만 씁니다 (지시사항 확인)

                glm::vec3 accumulated_light(0.0f);

                for (int i = 0; i < N_light; ++i) {
                    // 1. 광원 샘플링
                    float xi1 = distribution(generator);
                    float xi2 = distribution(generator);
                    glm::vec3 x_prime = light.a + xi1 * light.ab + xi2 * light.ac;

                    // 2. 그림자 광선
                    glm::vec3 light_vec = x_prime - hit_pos_world;
                    float dist = glm::length(light_vec);
                    glm::vec3 L_dir = glm::normalize(light_vec);

                    Ray shadowRay;
                    shadowRay.origin = hit_pos_world + 0.00001f * normal_world;
                    shadowRay.direction = L_dir;

                    if (isOccluded(shadowRay, scene, dist, hit_object)) continue;

                    // 3. 기하 항 (G)
                    float cos_theta = std::max(0.0f, glm::dot(normal_world, L_dir));
                    float cos_theta_prime = std::max(0.0f, glm::dot(-light.normal, -L_dir)); // 광원 법선 주의
                    if (cos_theta == 0 || cos_theta_prime == 0) continue;

                    // [수정] 거리 제곱이 너무 작아지는 것을 방지 (Singularity Clamp)
                    float distSq = dist * dist;
                    if (distSq < 0.00001f) distSq = 0.00001f; // 최소값 제한

                    float G = (cos_theta * cos_theta_prime) / distSq; // 수정된 distSq 사용

                    // 4. BRDF (Lambertian: diffuse / PI)
                    // L_dir: 셰이딩 포인트에서 광원을 향하는 방향
                    glm::vec3 f_r = evalBRDF(hit_object, view_dir, normal_world, L_dir);

                    // [MIS 핵심 추가] 가중치(Weight) 계산
                    float mis_weight = 1.0f;

                    if (scene.nextEventEstimation == MIS) {
                        // 1) NEE PDF (Solid Angle 변환)
                        // PDF_area = 1 / Area
                        // PDF_solid = PDF_area * (dist^2 / cos_theta_prime)
                        // Light Selection Prob = 1 / NumLights
                        float num_lights = (float)scene.quadLights.size();
                        float pdf_nee = (distSq) / (A * cos_theta_prime * num_lights);

                        // 2) BRDF PDF (Mixture PDF)
                        // (A) 선택 확률 계산 (Diffuse vs Specular)
                        float lum_diff = std::max({ hit_object->diffuse.r, hit_object->diffuse.g, hit_object->diffuse.b });
                        float lum_spec = std::max({ hit_object->specular.r, hit_object->specular.g, hit_object->specular.b });
                        float prob_spec = 0.0f;
                        if (lum_diff + lum_spec > 0.0f) prob_spec = lum_spec / (lum_diff + lum_spec);
                        prob_spec = std::max(0.0f, std::min(prob_spec, 1.0f));
                        float prob_diff = 1.0f - prob_spec;

                        // (B) PDF 계산
                        float pdf_spec = 0.0f;
                        if (hit_object->brdfType == PHONG) {
                            pdf_spec = pdfPhongSpecular(view_dir, normal_world, L_dir, hit_object->shininess);
                        }
                        else { // GGX
                            pdf_spec = pdfGGX(normal_world, view_dir, L_dir, hit_object->roughness);
                        }

                        float pdf_diff = cos_theta / PI; // Diffuse PDF (Cosine)

                        float pdf_brdf = (prob_spec * pdf_spec) + (prob_diff * pdf_diff);

                        // 3) Balance Heuristic
                        if (pdf_nee + pdf_brdf > 1e-10f) {
                            mis_weight = pdf_nee / (pdf_nee + pdf_brdf);
                        }
                    }

                    accumulated_light += (f_r * G * L_p) * mis_weight;
                }
                L_direct += (accumulated_light * A) / (float)N_light;
            }


        }

        // 3. 간접 조명 (L_indirect) - (기존 Simple Path Tracer 로직)

        // 반구 샘플링
        float r1 = distribution(generator);
        float r2 = distribution(generator);

        glm::vec3 u, v;
        float pdf = 0.0f;
        glm::vec3 bounce_weight = glm::vec3(0.0f);
        
        createCoordinateSystem(normal_world, u, v);
        glm::vec3 local_dir;
        glm::vec3 world_dir = glm::normalize(
            u * local_dir.x + v * local_dir.y + normal_world * local_dir.z
        );

        if (scene.importanceSampling == COSINE) {
            // --- A. Cosine Importance Sampling ---
            local_dir = cosineSampleHemisphere(r1, r2);

            // PDF = cos(theta) / PI
            pdf = local_dir.z / PI;
            if (pdf < 0.00001f) pdf = 0.00001f; // 0 나누기 방지
        }
        if (scene.importanceSampling == BRDF) {
            // 1. Diffuse vs Specular 선택 확률 계산
            // (간단하게 Diffuse 밝기와 Specular 밝기의 비율로 결정)
            float lum_diffuse = glm::length(hit_object->diffuse);
            float lum_specular = glm::length(hit_object->specular);
            float prob_specular = 0.0f;

            // 0~1 사이로 안전하게 클램핑 (부동소수점 오차 방지)
            prob_specular = std::max(0.0f, std::min(prob_specular, 1.0f));
            float prob_diffuse = 1.0f - prob_specular;

            if (lum_diffuse + lum_specular > 0.0f) {
                prob_specular = lum_specular / (lum_diffuse + lum_specular);
            }

            // 2. 샘플링 (World Dir 생성)
            float rand_choice = distribution(generator);

            if (rand_choice < prob_specular) {
                // [A] Specular Sampling (Phong Lobe)
                // (Phong 재질이 아니라면 그냥 Cosine으로 가야 함)
                if (hit_object->brdfType == PHONG) {
                    local_dir = samplePhongSpecular(view_dir, normal_world, hit_object->shininess, r1, r2);
                    // World 좌표로 바로 나옴 (함수 내부에서 처리함)
                    world_dir = local_dir; // 변수명 통일
                }
                else if (hit_object->brdfType == GGX) {
                    // 1. GGX 샘플링 (World 좌표 반환됨)
                    local_dir = sampleGGX(r1, r2, hit_object->roughness, normal_world, view_dir);
                    world_dir = local_dir; // 변수명 통일
                }
            }
            else {
                // [B] Diffuse Sampling (Cosine Hemisphere)
                local_dir = cosineSampleHemisphere(r1, r2);
                // World 변환
                glm::vec3 u, v; createCoordinateSystem(normal_world, u, v);
                world_dir = glm::normalize(u * local_dir.x + v * local_dir.y + normal_world * local_dir.z);
            }
        
            float pdf_spec = 0.0f;
            float pdf_diff = 0.0f;

            // [중요 수정] Mixture PDF 계산
            // 1) Specular 쪽 PDF 계산
            if (prob_specular > 0.0f) {
                if (hit_object->brdfType == PHONG) {
                    pdf_spec = pdfPhongSpecular(view_dir, normal_world, world_dir, hit_object->shininess);
                }
                else { // GGX
                    pdf_spec = pdfGGX(normal_world, view_dir, world_dir, hit_object->roughness);
                }
            }

            // 2) Diffuse 쪽 PDF 계산 (Cosine PDF)
            if (prob_diffuse > 0.0f) {
                pdf_diff = std::max(0.0f, glm::dot(normal_world, world_dir)) / PI;
            }

            // 3) 최종 PDF = 확률 가중합 (Mixture PDF)
            pdf = (prob_specular * pdf_spec) + (prob_diffuse * pdf_diff);
        }
        else {
            // --- B. Uniform Hemisphere Sampling (기존) ---
            local_dir = uniformSampleHemisphere(r1, r2);

            pdf = 1.0f / (2.0f * PI);
        }

        // 현재 스텝의 물리적 가중치(Bounce Weight) 계산, 식: (BRDF * cos_theta) / pdf
        glm::vec3 f_r = evalBRDF(hit_object, view_dir, normal_world, world_dir);
        float cos_theta = std::max(0.0f, glm::dot(normal_world, world_dir));

        if (pdf > 0.0f) {
            bounce_weight = (f_r * cos_theta) / pdf;
        }

        // 다음 단계의 예상 Throughput 계산 (현재까지 누적 * 이번 반사)
        glm::vec3 next_throughput = throughput * bounce_weight;

        // 4. Russian Roulette (RR)
        if (scene.russianRoulette) {
            // 과제 지시: RGB 중 가장 큰 값을 생존 확률(P)로 사용
            float prob = glm::max(next_throughput.r, glm::max(next_throughput.g, next_throughput.b));

            // 확률 클램핑 (최대 1.0, Floor = 0.1) -> lower variance 
            prob = std::max(0.1f, std::min(prob, 1.0f));

            // [종료 판정] 주사위가 확률보다 높게 나오면 사망 -> Indirect 계산 안 함
            if (distribution(generator) > prob) {
                return final_color + L_direct;
            }

            // [생존 보상] 살아남았다면 가중치를 (1/P)로 증폭 (Boost)
            // 아주 작은 확률로 살아남은 경우 폭발적인 값이 튀는 것을 막기 위해 안전장치 추가 가능
            if (prob > 0.00001f) {
                float boost = 1.0f / prob;
                bounce_weight *= boost;   
                next_throughput *= boost;  // 다음 재귀로 넘길 Throughput도 부스트
            }
        }
        // 2. [수정] Max Depth 체크 (RR 여부와 상관없이 항상 체크!)
        // maxdepth가 0일 때만 무한 바운스 허용 (과제 Instruction 의도)
        if (scene.maxdepth > 0 && depth >= scene.maxdepth) {
            return final_color + L_direct;
        }

        // 재귀 추적
        Ray nextRay;
        nextRay.origin = hit_pos_world + 0.00001f * normal_world;
        nextRay.direction = world_dir;

        // [중요] 업데이트된 next_throughput을 전달, is_shadow_ray는 false로 전달 (간접광 추적 중이므로)
        glm::vec3 L_indirect_incoming = trace(nextRay, scene, depth + 1, false, next_throughput, pdf);

        // 최종 간접광 = bounce_weight * L_incoming
        glm::vec3 L_indirect = bounce_weight * L_indirect_incoming;

        // 4. 최종 합산 (final_color에는 이미 L_emit이 들어있음)
        return final_color + L_direct + L_indirect;
    }

    else { // [ RAYTRACER (A1 방식) ]

        // A1의 점/방향 광원(scene.lights)을 순회
        for (const Light& light : scene.lights) {
            glm::vec3 light_dir;
            glm::vec3 light_color = light.color;
            float distance_to_light = std::numeric_limits<float>::max();
            float attenuation_factor = 1.0f;

            if (light.type == POINT) {
                glm::vec3 light_vec = light.position_or_direction - hit_pos_world;
                distance_to_light = glm::length(light_vec);
                light_dir = glm::normalize(light_vec);

                float C = scene.attenuation.x;
                float L = scene.attenuation.y;
                float Q = scene.attenuation.z;
                attenuation_factor = 1.0f / (C + L * distance_to_light + Q * (distance_to_light * distance_to_light));
            }
            else {
                light_dir = glm::normalize(light.position_or_direction);
            }

            // 그림자 계산
            float epsilon = 0.00001f;
            Ray shadowRay;
            shadowRay.origin = hit_pos_world + epsilon * normal_world;
            shadowRay.direction = light_dir;
            float shadow_occlusion = 1.0f;

            for (Object* obj_shadow : scene.objects) {
                Ray shadowRayInObjectSpace;
                shadowRayInObjectSpace.origin = glm::vec3(obj_shadow->invTransform * vec4(shadowRay.origin, 1.0f));
                shadowRayInObjectSpace.direction = glm::vec3(obj_shadow->invTransform * vec4(shadowRay.direction, 0.0f));
                float t_shadow;
                if (obj_shadow->intersect(shadowRayInObjectSpace, t_shadow) && t_shadow < distance_to_light) {
                    shadow_occlusion = 0.0f;
                    break;
                }
            }

            // Diffuse
            float NdotL = std::max(0.0f, glm::dot(normal_world, light_dir));
            glm::vec3 diffuse_color = hit_object->diffuse * light_color * NdotL;

            // Specular
            glm::vec3 half_dir = glm::normalize(light_dir + view_dir);
            float NdotH = std::max(0.0f, glm::dot(normal_world, half_dir));
            glm::vec3 specular_color = hit_object->specular * light_color * std::pow(NdotH, hit_object->shininess);

            // 최종 색상에 더하기
            final_color += (diffuse_color + specular_color) * shadow_occlusion * attenuation_factor;
        }
    }

    return final_color;
}

glm::vec3 trace(const Ray& ray, const Scene& scene, int depth, bool is_shadow_ray, glm::vec3 throughput,  float last_pdf) {

    int loop_safety_count = 0;

    // --- ⬇️ 2. 가속 구조 로직 (수정) ⬇️ ---
    // 1. 광선이 씬 전체 바운드와 교차하는지 확인
    float t_min_grid, t_max_grid;
    if (!intersectAABB(ray, scene.sceneBounds, t_min_grid, t_max_grid)) {
        return glm::vec3(0, 0, 0); // 씬 바운드와 부딪히지 않으면 검은색
    }

    // 광선이 그리드 안에서 시작하면(t_min < 0), 진입점을 0으로 클램핑
    if (t_min_grid < 0.0f) {
        t_min_grid = 0.0f;
    }

    // 2. 광선이 "그리드에 진입하는 지점"에서 셀 인덱스 계산
    glm::vec3 rayStart = ray.origin + ray.direction * t_min_grid;
    glm::vec3 currentCellIdx = (rayStart - scene.sceneBounds.min) / scene.cellSize;

    int x = (int)currentCellIdx.x;
    int y = (int)currentCellIdx.y;
    int z = (int)currentCellIdx.z;

    // (수치 오류로 경계 살짝 벗어날 수 있으므로 클램핑)
    x = glm::clamp(x, 0, scene.gridResolution[0] - 1);
    y = glm::clamp(y, 0, scene.gridResolution[1] - 1);
    z = glm::clamp(z, 0, scene.gridResolution[2] - 1);

    // 3. tMax, tDelta 초기화
    // (★중요★: tMax 계산은 t_min_grid가 아닌 ray.origin 기준)
    glm::vec3 tMax;
    glm::vec3 tDelta;
    glm::vec3 step;

    // (X, Y, Z축 초기화 - 이전과 동일)
    // X축
    if (ray.direction.x > 0.0f) {
        step.x = 1;
        tMax.x = (scene.sceneBounds.min.x + (x + 1) * scene.cellSize.x - ray.origin.x) / ray.direction.x;
        tDelta.x = scene.cellSize.x / ray.direction.x;
    }
    else if (ray.direction.x < 0.0f) { // ⬅️ 0보다 작은지 명시적 확인
        step.x = -1;
        tMax.x = (scene.sceneBounds.min.x + x * scene.cellSize.x - ray.origin.x) / ray.direction.x;
        tDelta.x = -scene.cellSize.x / ray.direction.x;
    }
    else { // ⬅️ ray.direction.x == 0.0f 인 경우
        step.x = 0; // 이 축으로는 절대 움직이지 않음
        tMax.x = std::numeric_limits<float>::max(); // 무한대로 설정
        tDelta.x = std::numeric_limits<float>::max(); // 무한대로 설정
    }

    // Y축 (동일한 3-way 로직 적용)
    if (ray.direction.y > 0.0f) {
        step.y = 1;
        tMax.y = (scene.sceneBounds.min.y + (y + 1) * scene.cellSize.y - ray.origin.y) / ray.direction.y;
        tDelta.y = scene.cellSize.y / ray.direction.y;
    }
    else if (ray.direction.y < 0.0f) {
        step.y = -1;
        tMax.y = (scene.sceneBounds.min.y + y * scene.cellSize.y - ray.origin.y) / ray.direction.y;
        tDelta.y = -scene.cellSize.y / ray.direction.y;
    }
    else {
        step.y = 0;
        tMax.y = std::numeric_limits<float>::max();
        tDelta.y = std::numeric_limits<float>::max();
    }

    // Z축 (동일한 3-way 로직 적용)
    if (ray.direction.z > 0.0f) {
        step.z = 1;
        tMax.z = (scene.sceneBounds.min.z + (z + 1) * scene.cellSize.z - ray.origin.z) / ray.direction.z;
        tDelta.z = scene.cellSize.z / ray.direction.z;
    }
    else if (ray.direction.z < 0.0f) {
        step.z = -1;
        tMax.z = (scene.sceneBounds.min.z + z * scene.cellSize.z - ray.origin.z) / ray.direction.z;
        tDelta.z = -scene.cellSize.z / ray.direction.z;
    }
    else {
        step.z = 0;
        tMax.z = std::numeric_limits<float>::max();
        tDelta.z = std::numeric_limits<float>::max();
    }

    // 4. Voxel Traversal 루프
    float min_t = std::numeric_limits<float>::max();
    Object* hit_object = nullptr;
    Ray rayInObjectSpace_of_hit_object;

    while (true) {
        // [안전 차단기] 루프가 비정상적으로 많이 돌면 강제 탈출
        if (++loop_safety_count > 9999) {
            break;
        }

        int index = z * (scene.gridResolution[0] * scene.gridResolution[1]) + y * scene.gridResolution[0] + x;

        // (범위 체크 - 수치 오류 예외 처리)
        if (index < 0 || index >= scene.grid.size()) break;

        const GridCell& cell = scene.grid[index];

        // 4b. 현재 셀에 있는 물체들하고만 교차 판정
        for (Object* obj : cell.objects) {
            Ray rayInObjectSpace;
            rayInObjectSpace.origin = glm::vec3(obj->invTransform * vec4(ray.origin, 1.0f));
            rayInObjectSpace.direction = glm::vec3(obj->invTransform * vec4(ray.direction, 0.0f));

            float t;
            if (obj->intersect(rayInObjectSpace, t) && t < min_t) {
                min_t = t;
                hit_object = obj;
                rayInObjectSpace_of_hit_object = rayInObjectSpace;
            }
        }

        // 4d. 다음 셀로 이동
        float t_next;
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                t_next = tMax.x;
                x += step.x;
                tMax.x += tDelta.x;
            }
            else {
                t_next = tMax.z;
                z += step.z;
                tMax.z += tDelta.z;
            }
        }
        else {
            if (tMax.y < tMax.z) {
                t_next = tMax.y;
                y += step.y;
                tMax.y += tDelta.y;
            }
            else {
                t_next = tMax.z;
                z += step.z;
                tMax.z += tDelta.z;
            }
        }

        // 4e. 그리드 이탈 확인 (가장 중요)
        if (t_next > t_max_grid) {
            break; // 광선이 그리드 AABB를 벗어남
        }

        // 4c. (최적화) 히트 지점이 다음 셀 경계보다 가까우면 종료
        if (hit_object != nullptr && min_t < t_next) {
            break;
        }
    }
    // --- ⬆️ 가속 구조 로직 끝 ⬆️ ---


    // 3. 물체에 부딪혔는지 확인
    if (hit_object != nullptr) {
        return computeShading(ray, scene, hit_object, min_t, rayInObjectSpace_of_hit_object, depth, throughput, last_pdf);
    }
    else {
        // 4. 아무것에도 부딪히지 않으면 배경색 (검은색) 반환
        return glm::vec3(0, 0, 0);
    }
}


int main(int argc, char* argv[]) {

  if (argc < 2) {
    cerr << "Usage: transforms scenefile [grader input (optional)]\n"; 
    exit(-1); 
  }

  // 1. Scene 객체 생성
  Scene scene;

  FreeImage_Initialise();

  // 2. readfile 함수를 호출하여 scene 객체를 데이터로 채움
  readfile(argv[1], scene);
  scene.printDebugInfo();

  //acceleration
  std::cout << "Building acceleration structure..." << std::endl;
  scene.buildAccelerationStructure();

  // 디버깅 코드 추가
  long totalObjectsInGrid = 0;
  for (const auto& cell : scene.grid) {
      totalObjectsInGrid += cell.objects.size();
  }
  std::cout << "Grid Build Check: " << totalObjectsInGrid << " object entries added to grid." << std::endl;
  if (totalObjectsInGrid == 0) {
      std::cout << "ERROR: Grid build failed! No objects in grid." << std::endl;
  }
  // 디버깅 코드 끝

  std::cout << "Build complete." << std::endl;

    // 3. 렌더링 결과물을 저장할 픽셀 배열 생성
  int width = scene.width;
  int height = scene.height;
  unsigned char* pixels = new unsigned char[width * height * 3];

  long long total_pixels = (long long)width * height;
  long long done_pixels = 0;
  int barWidth = 30; // 프로그레스 바의 길이 (네모 칸 개수)

  // 4. 핵심 렌더링 루프
  std::cout << "Rendering started..." << std::endl;
  cout << endl;
  #pragma omp parallel for schedule(dynamic, 1)

  for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {

          glm::vec3 pixel_color(0.0f); // 색상 누적 변수

          // [SPP 루프]
          for (int s = 0; s < scene.spp; ++s) {

              // 픽셀 내 오프셋 계산 (Anti-Aliasing)
              float dx = 0.5f; // 기본값: 픽셀 중앙
              float dy = 0.5f;

              // 첫 번째 샘플은 항상 정중앙 (호환성 유지)
              // 그 이후 샘플부터는 무작위 위치
              if (s > 0) {
                  dx = distribution(generator);
                  dy = distribution(generator);
              }

              Ray ray = scene.camera.generateRay(x + dx, y + dy, width, height);
              pixel_color += trace(ray, scene, 0, false, glm::vec3(1.0f));
          }

          // 평균 내기
          pixel_color /= (float)scene.spp;

          // 픽셀 저장
          int pixel_index = (y * width + x) * 3;
          unsigned char r = static_cast<unsigned char>(glm::clamp(pixel_color.r, 0.0f, 1.0f) * 255.0f);
          unsigned char g = static_cast<unsigned char>(glm::clamp(pixel_color.g, 0.0f, 1.0f) * 255.0f);
          unsigned char b = static_cast<unsigned char>(glm::clamp(pixel_color.b, 0.0f, 1.0f) * 255.0f);

          pixels[pixel_index + 0] = b;
          pixels[pixel_index + 1] = g;
          pixels[pixel_index + 2] = r;
      }

      // 진행률 출력
      #pragma omp critical 
      {
          done_pixels += width; // 한 줄 완료

          // 진행률 계산 (0.0 ~ 1.0)
          float progress = (float)done_pixels / total_pixels;
          int pos = (int)(barWidth * progress);

          std::cout << "\r[";
          for (int i = 0; i < barWidth; ++i) {
              if (i < pos) std::cout << "■"; // 채워진 부분
              else std::cout << " ";         // 빈 부분
          }

          // 퍼센트 표시 및 즉시 출력(flush)
          std::cout << "] " << int(progress * 100.0) << "%" << std::flush;
      }
  }
  cout << endl << endl;
  std::cout << "Rendering finished." << std::endl;

  // 5. 결과 이미지를 파일로 저장
  FIBITMAP* img = FreeImage_ConvertFromRawBits(pixels, width, height, width * 3, 24, 0xFF0000, 0x00FF00, 0x0000FF, true); // 맨 위 픽셀부터 저장했으므로 마지막 인자를 true로 변경

  std::cout << "Saving image to: " << scene.outputFilename << std::endl;
  FreeImage_Save(FIF_PNG, img, scene.outputFilename.c_str(), 0);

  // 메모리 해제
  delete[] pixels;
  FreeImage_Unload(img); // FreeImage 객체 메모리 해제
  
  std::cout << "Parsing complete. " << scene.objects.size() << " objects loaded.\n";
  std::cout << "Image size: " << scene.width << "x" << scene.height << ".\n";
  std::cout << "Output file: " << scene.outputFilename << ".\n";


  if (argc > 2) {
    allowGrader = true;
    stringstream tcid;
    tcid << argv[1] << "." << argv[2];
    grader.init(tcid.str());
    grader.loadCommands(argv[2]);
    //grader.bindScreenshotFunc(saveScreenshot);
  }


  printHelp();

  
  std::cout << "\nPress Enter to exit...";
  std::cin.get(); // 사용자가 Enter 키를 누를 때까지 기다림

  FreeImage_DeInitialise();
  return 0;
}
