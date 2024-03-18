#pragma once

#include <GL/glew.h>

class ShaderProgram {
private:
    GLuint shaderProgram;
    GLuint vertexShader;
    GLuint fragmentShader;
    char* readFile(const char* fileName);
    GLuint loadShader(GLenum shaderType, const char* fileName);
public:
    ShaderProgram(const char* vShaderFile, const char* fShaderFile);
    ~ShaderProgram();
    void use();
    GLuint u(const char* varName);
    GLuint a(const char* attName);
};
