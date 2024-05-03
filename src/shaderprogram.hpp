#ifndef HYDR_SHAD_HPP
#define HYDR_SHAD_HPP

#include <GL/glew.h>
#include <string>
#include <unordered_map>

namespace gl {
struct Texture {
    GLuint      texture; 
    GLenum      target  = GL_TEXTURE_2D;
    GLint       level   = 0;
    GLboolean   layered = 0;
    GLint       layer   = 0;
    GLenum      access  = GL_READ_ONLY;
    GLenum      format  = GL_RGBA32F;
    GLuint      width;
    GLuint      height;
};
void gen_texture(Texture& tex,
    GLenum format = GL_RGBA,
    GLenum type = GL_FLOAT,
    const void* pixels = nullptr
);
void delete_texture(Texture& tex);
void bind_texture(Texture& tex, GLuint bind);

struct Uniform_buffer {
    GLuint ubo;
    GLuint binding;

    template <typename T>
    void push_data(T& data, GLuint binding, int type = GL_STATIC_DRAW) {
        this->binding = binding;
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubo);
        glBufferData(
            GL_UNIFORM_BUFFER, 
            sizeof(data), &data, 
            type
        );
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
};
void gen_uniform_buffer(Uniform_buffer& buff);
void delete_uniform_buffer(Uniform_buffer& buff);

};


class Shader_core {
protected:
    GLuint program;
    GLuint load_shader(GLenum shader_type, std::string filename);
    std::unordered_map<std::string, GLuint> cached_bindings;
public:
    void use() const;
    GLuint get_attrib_location(const char* attribute) const;
    GLuint get_uniform_location(const char* uniform) const;

    template<typename T>
    void set_uniform(const char* id, T const& v) const;
    void set_texture(gl::Texture texture, std::string str);
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
    void bind_uniform_block(const char* var, gl::Uniform_buffer &buff) const;
    Compute_program(std::string comput_files);
    ~Compute_program();
};

#endif // HYDR_SHAD_HPP 
