#ifndef HYDR_SHAD_HPP
#define HYDR_SHAD_HPP

#include <GL/glew.h>
#include <string>

class Shader_core {
protected:
    GLuint program;
    GLuint load_shader(GLenum shader_type, std::string filename);
public:
    void use();
    GLuint get_attrib_location(const char* attribute);
    GLuint get_uniform_location(const char* uniform);

    template<typename T>
    void set_uniform(const char* id, T const& v);
};

class Shader_program : public Shader_core {
private:
    GLuint vertex;
    GLuint fragment;
public:
    Shader_program(std::string vert_file, std::string frag_file);
    ~Shader_program();
};

class Compute_program : public Shader_core {
private:
    GLuint compute;
public:
    void ub_bind(const char* variable, GLuint bind);
    Compute_program(std::string comput_files);
    ~Compute_program();
};

#endif // HYDR_SHAD_HPP 
