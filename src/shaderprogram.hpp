#ifndef HYDR_SHAD_HPP
#define HYDR_SHAD_HPP

#include <GL/glew.h>

class Shader_program {
private:
    GLuint program;
    GLuint vertex;
    GLuint fragment;
    char* read_file(const char* filename);
    GLuint load_shader(GLenum shader_type, const char* filename);
public:
    Shader_program(const char* vert_file, const char* frag_file);
    ~Shader_program();
    void use();
    GLuint u(const char* variable);
    GLuint a(const char* attribute);
};

void destroy_shader(Shader_program* sp);

Shader_program* create_shader(const char* vert, const char* frag);

#endif // HYDR_SHAD_HPP 
