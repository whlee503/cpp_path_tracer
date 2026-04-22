#pragma once
#include "types.h"
#include "Camera.h"
#include <string>
#include "types.h"
#include <vector>
#include <glm/gtx/string_cast.hpp>


struct GridCell {
    std::vector<Object*> objects;
};

enum IntegratorType { RAYTRACER, ANALYTIC, DIRECT, PATHTRACER };
enum ImportanceSamplingType { HEMISPHERE, COSINE, BRDF }; // BRDF와 MIS도 미리 추가
enum NextEventEstimationType { ON, MIS, NONE}; // BRDF와 MIS도 미리 추가

class Scene {
public:
    int width, height;
    int maxdepth = 5;
    std::string outputFilename;
    Camera camera;
    std::vector<Object*> objects; // 장면에 있는 모든 도형들
    std::vector<Light> lights;
    glm::vec3 attenuation = glm::vec3(1.0f, 0.0f, 0.0f);


    int spp = 1; // Samples Per Pixel (기본값 1)
    //bool nextEventEstimation = false; // NEE 사용 여부 (기본값 off)

    // 가속 구조 멤버 변수
    AABB sceneBounds;          // 씬 전체를 감싸는 AABB
    int gridResolution[3] = { 10, 10, 10 }; // 10x10x10 격자 (조절 가능)
    glm::vec3 cellSize;        // 각 셀의 크기
    std::vector<GridCell> grid; // 3D 격자를 1D 배열로 펼쳐서 사용

    // 파서가 이 함수들을 통해 Scene 객체를 채울 수 있도록 함
    void setDimensions(int w, int h) { width = w; height = h; }
    void setOutputFilename(const std::string& name) { outputFilename = name; }
    void addObject(Object* obj) { objects.push_back(obj); }
    void addLight(const Light& light) { lights.push_back(light); }
    void setMaxDepth(int d) { maxdepth = d; }
    void setAttenuation(const glm::vec3& att) { attenuation = att; } 
    IntegratorType integratorType = RAYTRACER; // 기본값
    int lightSamples = 1;
    bool stratifyLights = false;
    bool russianRoulette = false;
    std::vector<QuadLight> quadLights; // 면 광원 목록
    ImportanceSamplingType importanceSampling = HEMISPHERE;
    NextEventEstimationType nextEventEstimation = NONE;


    void setSPP(int s) { spp = s; }
    //void setNEE(bool on) { nextEventEstimation = on; }
    void setRR(bool on) { russianRoulette = on; }

    void setIntegrator(const std::string& name) {
        if (name == "analyticdirect") integratorType = ANALYTIC;
        else if (name == "direct") integratorType = DIRECT;
        else if (name == "pathtracer") integratorType = PATHTRACER;
        else integratorType = RAYTRACER;
    }
    void setLightSamples(int n) { lightSamples = n; }
    void setStratify(bool on) { stratifyLights = on; }
    void addQuadLight(const QuadLight& ql) { quadLights.push_back(ql); }

    void buildAccelerationStructure();

    int getGridIndex(const glm::vec3& p) const;

    void printDebugInfo();
};