#ifndef HYDR_SHAD_HPP
#define HYDR_SHAD_HPP

#include <GL/glew.h>

class Shader_core {
protected:
    GLuint program;
    GLuint load_shader(GLenum shader_type, const char* filename);
public:
    void use();
    GLuint u(const char* variable);
    GLuint a(const char* attribute);
};

class Shader_program : public Shader_core {
private:
    GLuint vertex;
    GLuint fragment;
public:
    Shader_program(const char* vert_file, const char* frag_file);
    ~Shader_program();
};

class Compute_program : public Shader_core {
private:
    GLuint compute;
public:
    Compute_program(const char* file);
    ~Compute_program();
};

Shader_program* create_shader(const char* vert, const char* frag);
Compute_program* create_shader(const char* comput);
void destroy_shader(Shader_program* sp);

#endif // HYDR_SHAD_HPP 
