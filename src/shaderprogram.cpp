#include "shaderprogram.hpp"
#include "utils.hpp"

std::string read_file(const std::string& filename);

enum Log_type {
    SHADER,
    PROGRAM
};

void err_log_shader(GLuint program, Log_type type);

std::string read_file(const std::string& filename) {
    int f_size;
    FILE *file;
    char *data;

    file = fopen(filename.c_str(), "rb");
    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        f_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        data = new char[f_size + 1];
        int readSize = fread(data, 1, f_size, file);
        data[f_size] = 0;
        fclose(file);

        return data;
    }
    return NULL;
}

GLuint Shader_core::load_shader(GLenum shader_type, std::initializer_list<std::string> filenames) {
    // handle
    GLuint shader = glCreateShader(shader_type);

    std::string source_str = "";
    for (const auto& filename: filenames) {
        std::string append_str = read_file(filename);
        source_str += '\n' + append_str;
    }
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

Compute_program::Compute_program(std::initializer_list<std::string> comput_files) {
    LOG_DBG("Loading compute shader...");
    compute = load_shader(GL_COMPUTE_SHADER, comput_files);

    program = glCreateProgram();

    glAttachShader(program, compute);
    glLinkProgram(program);

    err_log_shader(program, PROGRAM);
    LOG_DBG("Compute shader program created");
}

Shader_program::Shader_program(const char* vert_file, const char* frag_file) {
    LOG_DBG("Loading vertex shader...");
    vertex = load_shader(GL_VERTEX_SHADER, {vert_file});

    LOG_DBG("Loading fragment shader...");
    fragment = load_shader(GL_FRAGMENT_SHADER, {frag_file});

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

GLuint Shader_core::u(const char* variable) {
    return glGetUniformLocation(program, variable);
}

void Compute_program::ub_bind(GLchar* variable, GLint bind) {
    GLuint idx = glGetUniformBlockIndex(program, variable);
    glUniformBlockBinding(program, idx, bind);
}

GLuint Shader_core::a(const char* attribute) {
    return glGetAttribLocation(program, attribute);
}
