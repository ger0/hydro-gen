#ifndef HYDR_SHAD_HPP
#define HYDR_SHAD_HPP

#include <GL/glew.h>
#include <string>
#include <unordered_map>
#include "utils.hpp"

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

struct Buffer {
    GLuint  bo;
    GLuint  binding;
    GLint   type = GL_UNIFORM_BUFFER;
    GLint   mode = GL_STATIC_DRAW;

    template <typename T>
    void push_data(T& data, size_t size = 0) {
        glBindBuffer(type, bo);
        glBindBufferBase(type, binding, bo);
        if (size == 0) {
            size = sizeof(data);
        }
        glBufferData(
            type, 
            size, &data, 
            mode
        );
        glBindBuffer(type, 0);
    }
};
void gen_buffer(Buffer& buff);
void gen_buffer(Buffer& buff, size_t size);
void del_buffer(Buffer& buff);

// TODO: Refactor
// texture pairs for swapping
struct Tex_pair {
    gl::Texture tex[2]; 
    u32 idx_write = 1;
    u32 idx_read = 0;

    u32 cntr = 0;
    void swap(bool read_write = false);
    Tex_pair(GLenum access, GLuint width, GLuint height);
    const gl::Texture& get_write_tex() const;
    const gl::Texture& get_read_tex() const;

    void delete_textures();
};

};


class Shader_core {
protected:
    GLuint program;
    GLuint load_shader(GLenum shader_type, std::string filename);
    std::unordered_map<std::string, GLuint> cached_bindings;

    // GLuint get_attrib_location(const char* attribute) const;
    GLuint get_uniform_location(std::string uniform);
public:
    void use() const;

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
    void bind_uniform_block(const char* var, gl::Buffer &buff) const;

    void bind_texture(const char* var_name, const gl::Texture &tex);
    void unbind_texture(const char* var_name);
    void listActiveUniforms();

    void bind_image(const char* var_name, const gl::Texture &tex);
    void unbind_image(const char* var_name);

    void bind_storage_buffer(const char* variable, gl::Buffer &buff) const;

    Compute_program(std::string comput_files);
    ~Compute_program();
};

#endif // HYDR_SHAD_HPP 
