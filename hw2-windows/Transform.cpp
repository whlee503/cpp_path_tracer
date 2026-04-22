// Transform.cpp: implementation of the Transform class.

#include "Transform.h"
#include <glm/gtc/matrix_transform.hpp> // GLM의 변환 함수들을 사용하기 위해 추가

// Helper rotation function. Please implement this. 
// *** 반환 타입을 mat4로 변경했습니다 ***
mat4 Transform::rotate(const float degrees, const vec3& axis)
{
    // GLM의 rotate 함수를 사용합니다.
    // 1. mat4(1.0f)는 항등 행렬(identity matrix)입니다.
    // 2. glm::radians()는 각도(degree)를 라디안(radian)으로 변환합니다.
    // 3. axis는 회전의 기준 축입니다.
    return glm::rotate(mat4(1.0f), glm::radians(degrees), axis);
}

// 아래 두 함수(left, up)는 실시간 카메라 조작용이므로 레이트레이서에서는 필요 없습니다.
// 컴파일 오류를 막기 위해 함수 내용은 비워둡니다.
void Transform::left(float degrees, vec3& eye, vec3& up)
{
    // For interactive viewing only, not used in raytracer
}

void Transform::up(float degrees, vec3& eye, vec3& up)
{
    // For interactive viewing only, not used in raytracer
}

// lookAt 함수는 카메라의 위치와 방향을 정의하는 View Matrix를 생성합니다.
// 레이트레이서에서 월드 좌표계의 물체를 카메라 좌표계로 가져올 때 (또는 그 반대) 필요합니다.
mat4 Transform::lookAt(const vec3& eye, const vec3& center, const vec3& up)
{
    // GLM의 lookAt 함수가 모든 복잡한 계산을 처리해 줍니다.
    return glm::lookAt(eye, center, up);
}

// perspective 함수는 3D를 2D로 투영하는 '투영 행렬'을 만듭니다.
// OpenGL에서는 필수적이지만, 레이트레이서는 광선을 직접 계산하므로 이 행렬을 사용하지 않습니다.
// 컴파일 오류를 막기 위해 항등 행렬을 반환하도록 둡니다.
mat4 Transform::perspective(float fovy, float aspect, float zNear, float zFar)
{
    // Not used in raytracer, return identity matrix
    return mat4(1.0f);
}

// 크기 변환 행렬을 생성합니다.
mat4 Transform::scale(const float& sx, const float& sy, const float& sz)
{
    return glm::scale(mat4(1.0f), vec3(sx, sy, sz));
}

// 이동 변환 행렬을 생성합니다.
mat4 Transform::translate(const float& tx, const float& ty, const float& tz)
{
    return glm::translate(mat4(1.0f), vec3(tx, ty, tz));
}

// To normalize the up direction and construct a coordinate frame. 
// As discussed in the lecture. May be relevant to create a properly 
// orthogonal and normalized up. 
// This function is provided as a helper, in case you want to use it. 
// Using this function (in readfile.cpp or display.cpp) is optional. 

vec3 Transform::upvector(const vec3& up, const vec3& zvec)
{
    vec3 x = glm::cross(up, zvec);
    vec3 y = glm::cross(zvec, x);
    vec3 ret = glm::normalize(y);
    return ret;
}

Transform::Transform()
{

}

Transform::~Transform()
{

}