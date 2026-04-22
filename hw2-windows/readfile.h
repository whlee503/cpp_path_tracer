#pragma once 

#include <sstream>
#include <stack>
#include "Scene.h" // 우리가 만든 Scene 클래스
#include "Transform.h" // 행렬 변환을 위해 필요

// readfile 함수가 Scene 객체의 참조(&)를 인자로 받도록 수정합니다.
void readfile(const char* filename, Scene& scene);

// 이 함수들은 readfile.cpp 내부에서만 사용되거나, Transform 클래스로 대체될 수 있으므로
// 헤더에 노출할 필요가 없습니다. 필요하다면 readfile.cpp에 남겨둡니다.
// void matransform (stack<mat4> &transfstack, GLfloat * values) ;
// void rightmultiply (const mat4 & M, stack<mat4> &transfstack) ;
// bool readvals (stringstream &s, const int numvals, GLfloat * values) ;