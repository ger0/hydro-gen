#include "shaderprogram.hpp"
#include "utils.hpp"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
std::string load_shader_file(std::string filename);
void resolve_includes(std::string& buff);

namespace gl {
    void gen_texture(Texture& tex,
            GLenum format,
            GLenum type,
            const void* pixels) {
        glGenTextures(1, &tex.texture);
        glBindTexture(tex.target, tex.texture);
        glTexImage2D(
            tex.target,
            tex.level,
            tex.format,
            tex.width, tex.height,
            0,  // border
            format, type,
            pixels
        );
    }
    void bind_texture(Texture& tex, GLuint bind) {
        glActiveTexture(GL_TEXTURE0 + bind); 
        glBindTexture(tex.target, tex.texture);
        glBindImageTexture(
            bind, 
            tex.texture, 
            tex.level, 
            tex.layered, 
            tex.layer, 
            tex.access, 
            tex.format 
        );
    }
}

enum Log_type {
    SHADER,
    PROGRAM
};

std::string load_shader_file(std::string filename) {
    char path[1 << 8];
    snprintf(path, sizeof(path), "../glsl/%s", filename.c_str());

    FILE* file = fopen(path, "r");
    assert(file != nullptr);
    defer { fclose(file); };

    assert(fseek(file, 0, SEEK_END) == 0);
    long int size = ftell(file);
    assert(size > 0);

    std::string buffer;
    buffer.resize(size);

    assert(fseek(file, 0, SEEK_SET) == 0);
    assert(fread(&buffer[0], size, 1, file) == 1);

    resolve_includes(buffer);

    return buffer;
}

void resolve_includes(std::string& buff) {
    for(uint i = 0; buff[i] != '\0'; i++) {
        if(buff[i] == '#' && (i == 0 || buff[i - 1] == '\n')) {
            uint include_start = i;
            const char* word = "include \"";
            uint word_i = 0;
            // skip hash
            i++;
            while(word[word_i] != '\0' && buff[i] != '\0' && buff[i] == word[word_i]) {
                word_i++;
                i++;
            }
            if(word[word_i] != '\0') {
                continue;
            }
            uint path_start = i;
            while(
                buff[i] != '\0' &&
                buff[i] != '\n' &&
                buff[i] != '\"'
            ) {
                i++;
            }
            if(buff[i] != '\"') {
                continue;
            }
            uint path_end = i;

            i++;
            if(buff[i] != '\n') {
                continue;
            }
            uint include_end = i;

            std::string include_path = buff.substr(path_start, path_end - path_start);
            buff.erase(include_start, include_end - include_start);

            std::string include = load_shader_file(include_path.c_str());
            constexpr char n_line = '\n';
            int n_lines = std::count(include.cbegin(), include.cend(), n_line);
            LOG_DBG("    Shader filename \t {:30} total lines: {}", include_path, n_lines);
            buff.insert(include_start, include);
        }
    }
}

GLuint Shader_core::load_shader(GLenum shader_type, std::string filename) {
    // handle
    GLuint shader = glCreateShader(shader_type);

    auto source_str = load_shader_file(filename);
    const GLchar* shader_source = source_str.c_str();
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    return shader;
}

Compute_program::Compute_program(std::string filename) {
    LOG_DBG("Loading compute shader...");
    compute = load_shader(GL_COMPUTE_SHADER, filename);

    program = glCreateProgram();

    glAttachShader(program, compute);
    glLinkProgram(program);

    LOG_DBG("Compute shader program created");
}

Shader_program::Shader_program(std::string vert_file, std::string frag_file) {
    LOG_DBG("Loading vertex shader...");
    vertex = load_shader(GL_VERTEX_SHADER, vert_file);

    LOG_DBG("Loading fragment shader...");
    fragment = load_shader(GL_FRAGMENT_SHADER, frag_file);

    program = glCreateProgram();

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    LOG_DBG("Shader pipeline created");
}

Shader_program::~Shader_program() {
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    glDeleteProgram(program);
    LOG_DBG("Shader program deleted");
}

Compute_program::~Compute_program() {
    glDetachShader(program, compute);

    glDeleteShader(compute);

    glDeleteProgram(program);
    LOG_DBG("Compute shader program deleted");
}

void Shader_core::use() const {
    glUseProgram(program);
}

void Compute_program::bind_uniform_block(const char* variable, GLuint bind) const  {
    GLuint idx = glGetUniformBlockIndex(program, variable);
    if (idx == GL_INVALID_INDEX) {
        LOG_ERR("ERROR: Invalid buffer block index");
    }
    // glUniformBlockBinding(program, idx, bind);
    glBindBufferBase(GL_UNIFORM_BUFFER, idx, bind);
}

GLuint Shader_core::get_attrib_location(const char* attribute) const {
    return glGetAttribLocation(program, attribute);
}

GLuint Shader_core::get_uniform_location(const char* name) const {
    return glGetUniformLocation(program, name);
}

/* void Shader_core::set_texture(gl::Texture tex, std::string str) {
    gl::bind_texture(tex);
    auto iter = cached_bindings.find(str);
    GLuint binding;
    if (iter == cached_bindings.end()) {
        binding = glGetUniformLocation(this->program, str.c_str());
        cached_bindings[str] = binding;
    } else {
        binding = iter->second;
    }
    glBindImageTexture(
        binding,
        tex.texture,
        tex.level, 
        tex.layered, 
        tex.layer, 
        tex.access, 
        tex.format 
    );
} */

template<>
void Shader_core::set_uniform(const char* id, bool const& v) const {
    glUniform1i(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uint const& v) const {
    glUniform1ui(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uvec2 const& v) const {
    glUniform2uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec3 const& v) const {
    glUniform3uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec4 const& v) const {
    glUniform4uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, int const& v) const {
    glUniform1i(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, ivec2 const& v) const {
    glUniform2iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec3 const& v) const {
    glUniform3iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec4 const& v) const {
    glUniform4iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, float const& v) const {
    glUniform1f(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, vec2 const& v) const {
    glUniform2fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec3 const& v) const {
    glUniform3fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec4 const& v) const {
    glUniform4fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat2 const& v) const {
    glUniformMatrix2fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat3 const& v) const {
    glUniformMatrix3fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat4 const& v) const {
    glUniformMatrix4fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}
