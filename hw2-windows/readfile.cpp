#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <deque>
#include <stack>
#include <glm/gtc/matrix_inverse.hpp>
#include "Transform.h" 

#include "readfile.h"
#include "Scene.h" 

using namespace std;
using namespace glm;


std::vector<glm::vec3> vertices;


void rightmultiply(const mat4& M, stack<mat4>& transfstack) {
    mat4& T = transfstack.top();
    T = T * M;
}

bool readvals(stringstream& s, const int numvals, float* values) {
    for (int i = 0; i < numvals; i++) {
        s >> values[i];
        if (s.fail()) {
            cout << "Failed reading value " << i << " will skip\n";
            return false;
        }
    }
    return true;
}


void readfile(const char* filename, Scene& scene) {
    string str, cmd;
    ifstream in;
    stack <mat4> transfstack;
    transfstack.push(mat4(1.0));
    std::vector<glm::vec3> vertices;
    BRDFType current_brdf = PHONG;

    glm::vec3 current_ambient(0.0f, 0.0f, 0.0f); // 기본값
    glm::vec3 current_diffuse(0.0f, 0.0f, 0.0f);
    glm::vec3 current_specular(0.0f, 0.0f, 0.0f);
    glm::vec3 current_emission(0.0f, 0.0f, 0.0f);
    float current_shininess = 0.0f;
    float current_roughness = 0.0f;

    in.open(filename);
    if (in.is_open()) {
        stack <mat4> transfstack;
        transfstack.push(mat4(1.0));

        // 재질(material) 상태를 저장하기 위한 지역 변수
        glm::vec3 current_diffuse(0.0f);
        // (ambient, specular 등 다른 재질 속성도 필요에 따라 추가)

        getline(in, str);
        while (in) {
            if ((str.find_first_not_of(" \t\r\n") != string::npos) && (str[0] != '#')) {
                stringstream s(str);
                s >> cmd;
                float values[24];
                bool validinput;

                // --- 각 명령어 처리 부분을 Scene 객체를 사용하도록 수정 ---

                if (cmd == "size") {
                    validinput = readvals(s, 2, values);
                    if (validinput) {
                        // 전역 변수 w, h 대신 scene 객체에 이미지 크기 저장
                        scene.setDimensions((int)values[0], (int)values[1]);
                    }
                }
                else if (cmd == "output") {
                    string filename;
                    s >> filename;
                    scene.setOutputFilename(filename);
                }
                else if (cmd == "camera") {
                    validinput = readvals(s, 10, values);
                    if (validinput) {
                        // 전역 변수 대신 scene.camera 객체에 값 저장
                        scene.camera.eye = glm::vec3(values[0], values[1], values[2]);
                        scene.camera.lookat = glm::vec3(values[3], values[4], values[5]);
                        scene.camera.up = glm::vec3(values[6], values[7], values[8]);
                        scene.camera.fovy = values[9];
                    }
                }

                else if (cmd == "ambient") {
                    validinput = readvals(s, 3, values);
                    if (validinput) { current_ambient = glm::vec3(values[0], values[1], values[2]); }
                }
                else if (cmd == "diffuse") {
                    validinput = readvals(s, 3, values);
                    if (validinput) { current_diffuse = glm::vec3(values[0], values[1], values[2]); }
                }
                else if (cmd == "specular") {
                    validinput = readvals(s, 3, values);
                    if (validinput) { current_specular = glm::vec3(values[0], values[1], values[2]); }
                }
                else if (cmd == "shininess") {
                    validinput = readvals(s, 1, values);
                    if (validinput) { current_shininess = values[0]; }
                }
                else if (cmd == "emission") {
                    validinput = readvals(s, 3, values);
                    if (validinput) { current_emission = glm::vec3(values[0], values[1], values[2]); }
                }

                else if (cmd == "point") {
                    validinput = readvals(s, 6, values); // pos(3), color(3)
                    if (validinput) {
                        Light light;
                        light.type = POINT;
                        light.position_or_direction = glm::vec3(values[0], values[1], values[2]);
                        light.color = glm::vec3(values[3], values[4], values[5]);
                        scene.addLight(light);
                    }
                }
                else if (cmd == "directional") {
                    validinput = readvals(s, 6, values); // dir(3), color(3)
                    if (validinput) {
                        Light light;
                        light.type = DIRECTIONAL;
                        light.position_or_direction = glm::normalize(glm::vec3(values[0], values[1], values[2]));
                        light.color = glm::vec3(values[3], values[4], values[5]);
                        scene.addLight(light);
                    }
                }

                else if (cmd == "attenuation") {
                    validinput = readvals(s, 3, values); // const, linear, quadratic
                    if (validinput) {
                        scene.setAttenuation(glm::vec3(values[0], values[1], values[2]));
                    }
                }

                else if (cmd == "maxverts") {
                    // std::vector를 사용하므로 이 명령어는 무시해도 괜찮습니다.
                    // 하지만 파서가 인식하도록 빈 블록을 만들어 줍니다.
                    validinput = readvals(s, 1, values);
                }
                else if (cmd == "vertex") {
                    validinput = readvals(s, 3, values);
                    if (validinput) {
                        // 임시 정점 목록에 추가
                        vertices.push_back(glm::vec3(values[0], values[1], values[2]));
                    }
                }
                else if (cmd == "tri") {
                    validinput = readvals(s, 3, values);
                    if (validinput) {
                        // 정수형 인덱스로 변환
                        int v1_idx = static_cast<int>(values[0]);
                        int v2_idx = static_cast<int>(values[1]);
                        int v3_idx = static_cast<int>(values[2]);

                        // 인덱스를 이용해 실제 정점 좌표를 가져옴
                        glm::vec3 v1 = vertices[v1_idx];
                        glm::vec3 v2 = vertices[v2_idx];
                        glm::vec3 v3 = vertices[v3_idx];

                        // 새로운 Triangle 객체 생성 (Triangle 클래스 필요)
                        Triangle* tri = new Triangle(v1, v2, v3);

                        //materials
                        tri->ambient = current_ambient;
                        tri->diffuse = current_diffuse;
                        tri->specular = current_specular;
                        tri->emission = current_emission;
                        tri->shininess = current_shininess;
                        tri->brdfType = current_brdf;      // [추가]
                        tri->roughness = current_roughness; // [추가]
                        // transform
                        tri->transform = transfstack.top();
                        tri->invTransform = glm::inverse(tri->transform);
                        tri->invTranspose = glm::inverseTranspose(tri->transform); // 법선 벡터 변환용

                        scene.addObject(tri);
                    }
                }

                // --- 도형 명령어 처리 ---
                else if (cmd == "sphere") {
                    validinput = readvals(s, 4, values); // x, y, z, radius
                    if (validinput) {
                        Sphere* sp = new Sphere(glm::vec3(values[0], values[1], values[2]), values[3]);

                        // 재질 정보 복사
                        sp->ambient = current_ambient;
                        sp->diffuse = current_diffuse;
                        sp->specular = current_specular;
                        sp->emission = current_emission;
                        sp->shininess = current_shininess;
                        sp->brdfType = current_brdf;      // [추가]
                        sp->roughness = current_roughness; // [추가]

                        // 변환 행렬 및 역행렬 저장
                        sp->transform = transfstack.top();
                        sp->invTransform = glm::inverse(sp->transform);
                        sp->invTranspose = glm::inverseTranspose(sp->transform);

                        scene.addObject(sp);
                    }
                }

                else if (cmd == "maxdepth") {
                    validinput = readvals(s, 1, values);
                    if (validinput) { scene.setMaxDepth((int)values[0]); }
                }

                else if (cmd == "integrator") {
                    std::string name;
                    s >> name;
                    scene.setIntegrator(name);
                }
                else if (cmd == "lightsamples") {
                    validinput = readvals(s, 1, values);
                    if (validinput) { scene.setLightSamples((int)values[0]); }
                }
                else if (cmd == "lightstratify") {
                    std::string status;
                    s >> status;
                    scene.setStratify(status == "on");
                }

                else if (cmd == "quadLight") {
                    validinput = readvals(s, 12, values); // 4개 vec3 = 12 floats
                    if (validinput) {

                        // --- 1. 조명 데이터(QuadLight) 생성 ---
                        QuadLight ql;
                        ql.a = glm::vec3(values[0], values[1], values[2]);
                        ql.ab = glm::vec3(values[3], values[4], values[5]);
                        ql.ac = glm::vec3(values[6], values[7], values[8]);
                        ql.intensity = glm::vec3(values[9], values[10], values[11]);

                        glm::vec3 cross_prod = glm::cross(ql.ab, ql.ac);
                        ql.normal = glm::normalize(cross_prod);
                        ql.area = glm::length(cross_prod);

                        scene.addQuadLight(ql);

                        // --- ⬇️ 2. [핵심] 빛나는 "물체(Triangle)" 생성 ⬇️ ---

                        glm::vec3 v0 = ql.a;
                        glm::vec3 v1 = ql.a + ql.ab;
                        glm::vec3 v2 = ql.a + ql.ab + ql.ac;
                        glm::vec3 v3 = ql.a + ql.ac;

                        Triangle* tri1 = new Triangle(v0, v1, v3);
                        Triangle* tri2 = new Triangle(v1, v2, v3);

                        //  재질 설정: emission은 광원 강도로, 나머지는 0으로!
                        tri1->emission = ql.intensity; // ⬅️ "current_emission"이 아님
                        tri1->ambient = glm::vec3(0.0f);
                        tri1->diffuse = glm::vec3(0.0f);
                        tri1->specular = glm::vec3(0.0f);
                        tri1->shininess = 0.0f;

                        tri2->emission = ql.intensity; // ⬅️ "current_emission"이 아님
                        tri2->ambient = glm::vec3(0.0f);
                        tri2->diffuse = glm::vec3(0.0f);
                        tri2->specular = glm::vec3(0.0f);
                        tri2->shininess = 0.0f;

                        // 변환은 항등 행렬 (이미 월드 좌표에 정의됨)
                        tri1->transform = mat4(1.0f);
                        tri1->invTransform = mat4(1.0f);
                        tri1->invTranspose = mat4(1.0f);
                        tri2->transform = mat4(1.0f);
                        tri2->invTransform = mat4(1.0f);
                        tri2->invTranspose = mat4(1.0f);

                        scene.addObject(tri1);
                        scene.addObject(tri2);
                    }
                }

                // HW3 명령어 파싱
                else if (cmd == "spp") {
                    validinput = readvals(s, 1, values);
                    if (validinput) { scene.setSPP((int)values[0]); }
                }
                else if (cmd == "nexteventestimation") {
                    std::string status;
                    s >> status;
                    if (status == "on") {
                        scene.nextEventEstimation = ON;
                    }
                    // 2. "mis" 인 경우: NEE 켜기 + MIS 모드 활성화 (핵심!)
                    else if (status == "mis") {
                        scene.nextEventEstimation = ON;
                        scene.nextEventEstimation = MIS; // 여기서 MIS 모드로 강제 설정
                        cout << "Next Event Estimation: MIS Mode Enabled" << endl;
                    }
                    // 3. 그 외 ("off"): 끄기
                    else {
                        scene.nextEventEstimation = NONE;
                    }
                }
                else if (cmd == "russianroulette") {
                    std::string status;
                    s >> status;
                    scene.setRR(status == "on");
                }

                else if (cmd == "importancesampling") {
                    std::string type;
                    s >> type;
                    if (type == "cosine") {
                        scene.importanceSampling = COSINE;
                        cout << "Importance Sampling: Cosine" << endl;
                    }
                    else if (type == "brdf") {
                        scene.importanceSampling = BRDF; // 다음 단계 준비
                        cout << "Importance Sampling: BRDF" << endl;
                    }
                    else if (type == "hemisphere") {
                        scene.importanceSampling = HEMISPHERE;
                        cout << "Importance Sampling: Uniform Hemisphere" << endl;
                    }
                    else {
                        cout << "Unknown Importance Sampling type: " << type << endl;
                    }
                }

                // [명령어 추가] brdf <phong/ggx>
                else if (cmd == "brdf") {
                    string type;
                    s >> type;
                    if (type == "ggx") current_brdf = GGX;
                    else current_brdf = PHONG;
                }
                // [명령어 추가] roughness <float>
                else if (cmd == "roughness") {
                    readvals(s, 1, values);
                    current_roughness = values[0];
                }                

                // --- 변환 명령어 처리 ---
                else if (cmd == "translate") {
                    validinput = readvals(s, 3, values);
                    if (validinput) {
                        mat4 T = Transform::translate(values[0], values[1], values[2]);
                        rightmultiply(T, transfstack);
                    }
                }
                else if (cmd == "scale") {
                    validinput = readvals(s, 3, values);
                    if (validinput) {
                        mat4 S = Transform::scale(values[0], values[1], values[2]);
                        rightmultiply(S, transfstack);
                    }
                }
                else if (cmd == "rotate") {
                    validinput = readvals(s, 4, values);
                    if (validinput) {
                        vec3 axis(values[0], values[1], values[2]);
                        mat4 R = Transform::rotate(values[3], axis);
                        rightmultiply(R, transfstack);
                    }
                }

                // --- 스택 명령어 (이 부분은 그대로 사용) ---
                else if (cmd == "pushTransform") {
                    transfstack.push(transfstack.top());
                }
                else if (cmd == "popTransform") {
                    if (transfstack.size() <= 1) {
                        cerr << "Stack has no elements. Cannot Pop\n";
                    }
                    else {
                        transfstack.pop();
                    }
                }
                else {
                    cerr << "Unknown Command: " << cmd << " Skipping \n";
                }
            }
            getline(in, str);
        }
    }
    else {
        cerr << "Unable to Open Input Data File " << filename << "\n";
        throw 2;
    }
}