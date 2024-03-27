#include "shaderprogram.hpp"
#include "utils.hpp"

char* read_file(const char* filename);

enum Log_type {
    SHADER,
    PROGRAM
};

void err_log_shader(GLuint program, Log_type type);

char* read_file(const char* filename) {
    int f_size;
    FILE *file;
    char *data;

    file = fopen(filename, "rb");
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

GLuint Shader_core::load_shader(GLenum shader_type, const char* filename) {
    // handle
    GLuint shader = glCreateShader(shader_type);
    const GLchar* shader_source = read_file(filename);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    delete []shader_source;

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

Compute_program::Compute_program(const char* comput_file) {
    LOG_DBG("Loading compute shader...");
    compute = load_shader(GL_COMPUTE_SHADER, comput_file);

    program = glCreateProgram();

    glAttachShader(program, compute);
    glLinkProgram(program);

    err_log_shader(program, PROGRAM);
    LOG_DBG("Compute shader program created");
}

Shader_program::Shader_program(const char* vert_file, const char* frag_file) {
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

void Shader_core::use() {
    glUseProgram(program);
}

GLuint Shader_core::u(const char* variable) {
    return glGetUniformLocation(program, variable);
}

GLuint Shader_core::a(const char* attribute) {
    return glGetAttribLocation(program, attribute);
}

void destroy_shader(Shader_program* sp) {
    delete sp;
};

Shader_program* create_shader(const char* vert, const char* frag) {
    Shader_program *sp = new Shader_program(vert, frag);
    return sp;
};
