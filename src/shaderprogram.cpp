#include "shaderprogram.hpp"
#include "utils.hpp"
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
std::string load_shader_file(std::string filename);
void resolve_includes(std::string& buff);

enum Log_type {
    SHADER,
    PROGRAM
};

void err_log_shader(GLuint program, Log_type type);

std::string load_shader_file(std::string filename) {
    LOG_DBG("   Shader filename {}", filename);
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

    err_log_shader(shader, SHADER);
    return shader;
}

void err_log_shader(GLuint handle, Log_type type) {
    // error handling x2
    int info_log_len = 0;
    int chars_written = 0;
    char *info_log;

    if (type == SHADER) {
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &info_log_len);
    } else if (type == PROGRAM) {
        glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &info_log_len);
    }
    if (info_log_len > 1) {
        info_log = new char[info_log_len];
        if (type == SHADER) {
            glGetShaderInfoLog(handle, info_log_len, &chars_written, info_log);
        } else if (type == PROGRAM) {
            glGetProgramInfoLog(handle, info_log_len, &chars_written, info_log);
        }
        LOG_NOFORMAT_DBG(info_log);
        delete []info_log;
    }

}

Compute_program::Compute_program(std::string filename) {
    LOG_DBG("Loading compute shader...");
    compute = load_shader(GL_COMPUTE_SHADER, filename);

    program = glCreateProgram();

    glAttachShader(program, compute);
    glLinkProgram(program);

    err_log_shader(program, PROGRAM);
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

    err_log_shader(program, PROGRAM);
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

void Shader_core::use() {
    glUseProgram(program);
}

void Compute_program::ub_bind(const char* variable, GLuint bind) {
    GLuint idx = glGetUniformBlockIndex(program, variable);
    if (idx == GL_INVALID_INDEX) {
        LOG_ERR("ERROR: Invalid buffer block index");
    }
    // glUniformBlockBinding(program, idx, bind);
    glBindBufferBase(GL_UNIFORM_BUFFER, idx, bind);
}

GLuint Shader_core::a(const char* attribute) {
    return glGetAttribLocation(program, attribute);
}

template<>
void Shader_core::set_uniform(const char* id, bool const& v) {
    glUniform1i(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uint const& v) {
    glUniform1ui(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, uvec2 const& v) {
    glUniform2uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec3 const& v) {
    glUniform3uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, uvec4 const& v) {
    glUniform4uiv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, int const& v) {
    glUniform1i(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, ivec2 const& v) {
    glUniform2iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec3 const& v) {
    glUniform3iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, ivec4 const& v) {
    glUniform4iv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, float const& v) {
    glUniform1f(glGetUniformLocation(program, id), v);
}

template<>
void Shader_core::set_uniform(const char* id, vec2 const& v) {
    glUniform2fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec3 const& v) {
    glUniform3fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, vec4 const& v) {
    glUniform4fv(glGetUniformLocation(program, id), 1, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat2 const& v) {
    glUniformMatrix2fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat3 const& v) {
    glUniformMatrix3fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}

template<>
void Shader_core::set_uniform(const char* id, mat4 const& v) {
    glUniformMatrix4fv(glGetUniformLocation(program, id), 1, GL_FALSE, glm::value_ptr(v));
}
