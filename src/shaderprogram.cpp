#include "shaderprogram.hpp"
#include "utils.hpp"
#include <csignal>
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
        glTexStorage2D(tex.target, 1, tex.format, (GLsizei)tex.width, (GLsizei)tex.height);
        glBindTexture(tex.target, 0);
    }
    void delete_texture(Texture& tex) {
        glDeleteTextures(1, &tex.texture);
    }
    void gen_buffer(Buffer& buff) {
        glGenBuffers(1, &buff.bo);
        glBindBuffer(buff.type, buff.bo);
        glBindBufferBase(buff.type, buff.binding, buff.bo);
        glBindBuffer(buff.type, 0);
    }
    void gen_buffer(Buffer& buff, size_t size) {
        glGenBuffers(1, &buff.bo);
        glBindBuffer(buff.type, buff.bo);
        glBindBufferBase(buff.type, buff.binding, buff.bo);
        glBufferData(
            buff.type, 
            size, nullptr, 
            buff.mode
        );
        glBindBuffer(buff.type, 0);
    }
    void del_buffer(Buffer& buff) {
        glDeleteBuffers(1, &buff.bo);
    }

    void Tex_pair::swap(bool read_write) {
        cntr++;
        idx_read = cntr % 2;
        idx_write = (cntr + 1) % 2;
        if (cntr % 2) {
            if (read_write) {
                tex[0].access = GL_READ_WRITE;
                tex[1].access = GL_READ_WRITE;
            } else {
                tex[0].access = GL_WRITE_ONLY;
                tex[1].access = GL_READ_ONLY;
            }
        } else {
            if (read_write) {
                tex[0].access = GL_READ_WRITE;
                tex[1].access = GL_READ_WRITE;
            } else {
                tex[0].access = GL_READ_ONLY;
                tex[1].access = GL_WRITE_ONLY;
            }
        }
    }

    const gl::Texture& Tex_pair::get_write_tex() const {
    if (idx_write > 1 || idx_write < 0) { LOG_ERR("Texpair getter failure."); assert(false); }
        return tex[idx_write];
    }

    const gl::Texture& Tex_pair::get_read_tex() const {
    if (idx_read > 1 || idx_read < 0) { LOG_ERR("Texpair getter failure."); assert(false); }
        return tex[idx_read];
    }

    Tex_pair::Tex_pair(GLenum access, GLuint width, GLuint height) {
        this->tex[0] = gl::Texture {
            .access = access,
            .width = width,
            .height = height
        };
        this->tex[1] = gl::Texture {
            .access = access,
            .width = width,
            .height = height
        };

        gl::gen_texture(tex[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        gl::gen_texture(tex[1]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Tex_pair::delete_textures() {
        gl::delete_texture(tex[0]);
        gl::delete_texture(tex[1]);
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
            LOG_DBG("    Included shader: \t {:30}", include_path);
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

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        // Get the length of the error log
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        // Retrieve the error log
        std::string infoLog(logLength, ' ');
        glGetShaderInfoLog(shader, logLength, nullptr, &infoLog[0]);
        LOG_ERR("Shader compilation failed: {}", infoLog);
        raise(SIGTRAP);

        // Cleanup the failed shader
        glDeleteShader(shader);
        return 0; // Indicate failure
    }
    return shader;
}

Compute_program::Compute_program(std::string filename) {
    LOG_DBG("Loading compute shader: {}", filename);
    compute = load_shader(GL_COMPUTE_SHADER, filename);

    program = glCreateProgram();

    glAttachShader(program, compute);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        LOG_ERR("ERROR::SHADER::PROGRAM::LINKING_FAILED {}", infoLog);
        raise(SIGTRAP);
    } else {
        LOG_DBG("Compute shader program created");
    }
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
    // LOG_DBG("Shader program deleted");
}

Compute_program::~Compute_program() {
    glDetachShader(program, compute);
    glDeleteShader(compute);
    glDeleteProgram(program);
    // LOG_DBG("Compute shader program deleted");
}

void Shader_core::use() const {
    glUseProgram(program);
}

void Compute_program::bind_uniform_block(const char* variable, gl::Buffer &buff) const  {
    GLuint idx = glGetUniformBlockIndex(program, variable);
    if (idx == GL_INVALID_INDEX) {
        LOG_ERR("ERROR: Invalid buffer block index");
        raise(SIGTRAP);
    }
    glUniformBlockBinding(program, idx, buff.binding);
}

void Compute_program::bind_image(const char* var_name, const gl::Texture &tex) {
    GLuint location = get_uniform_location(std::string(var_name));
    GLint bind;    
    glGetUniformiv(this->program, location, &bind);
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

void Compute_program::unbind_image(const char* var_name) {
    GLuint location = get_uniform_location(std::string(var_name));
    GLint bind;    
    glGetUniformiv(this->program, location, &bind);
    glBindImageTexture(bind, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void Compute_program::bind_texture(const char* var_name, const gl::Texture& tex) {
    GLuint location = get_uniform_location(std::string(var_name));
    GLint bind;    
    glGetUniformiv(this->program, location, &bind);
    glBindTextureUnit(bind, tex.texture);
}

void Compute_program::unbind_texture(const char* var_name) {
    GLuint location = get_uniform_location(std::string(var_name));
    GLint bind;    
    glGetUniformiv(this->program, location, &bind);
    glActiveTexture(GL_TEXTURE0 + bind);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Compute_program::bind_storage_buffer(const char* variable, gl::Buffer &buff) const {
    GLuint idx = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, variable);
    if (idx == GL_INVALID_INDEX) {
        LOG_ERR("ERROR: Invalid shader storage buffer index");
        raise(SIGTRAP);
    }
    glShaderStorageBlockBinding(program, idx, buff.binding);
}

/* GLuint Shader_core::get_attrib_location(const char* attribute) const {
    return glGetAttribLocation(program, attribute);
} */

void Compute_program::listActiveUniforms() {
    GLint numUniforms = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
    LOG_DBG("Number of active uniforms: {}", numUniforms);
    for (GLint i = 0; i < numUniforms; ++i) {
        char name[256];
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;

        // Query uniform information
        glGetActiveUniform(program, i, sizeof(name), &length, &size, &type, name);

        // Print uniform details
        LOG_DBG("Uniform # {} : {}", i, name);
        LOG_DBG("Type: {}, size: {}, ", type, size);
    }
}


GLuint Shader_core::get_uniform_location(std::string name) {
    auto count = cached_bindings.count(name);
    if (count < 1) {
        GLuint location = glGetUniformLocation(this->program, name.c_str());
        if (location == GL_INVALID_INDEX) {
            LOG_ERR("ERROR: Invalid shader uniform index: {}", name);
            raise(SIGTRAP);
        }
        cached_bindings[name] = location;
        return location;
    } else {
        return cached_bindings[name];
    }
}

template<>
void Shader_core::set_uniform(const char* id, bool const& v) {
    glUniform1i(this->get_uniform_location(id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uint const& v) {
    glUniform1ui(this->get_uniform_location(id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uvec2 const& v) {
    glUniform2uiv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec3 const& v) {
    glUniform3uiv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec4 const& v) {
    glUniform4uiv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, int const& v) {
    glUniform1i(this->get_uniform_location(id), v);
}

template<>
void Shader_core::set_uniform(const char* id, ivec2 const& v) {
    glUniform2iv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec3 const& v) {
    glUniform3iv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec4 const& v) {
    glUniform4iv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, float const& v) {
    glUniform1f(this->get_uniform_location(id), v);
}

template<>
void Shader_core::set_uniform(const char* id, vec2 const& v) {
    glUniform2fv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec3 const& v) {
    glUniform3fv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec4 const& v) {
    glUniform4fv(this->get_uniform_location(id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat2 const& v) {
    glUniformMatrix2fv(this->get_uniform_location(id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat3 const& v) {
    glUniformMatrix3fv(this->get_uniform_location(id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat4 const& v) {
    glUniformMatrix4fv(this->get_uniform_location(id), 1, GL_FALSE, glm::value_ptr(v));
}
