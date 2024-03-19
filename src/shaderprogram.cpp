#include "shaderprogram.hpp"
#include "utils.hpp"

char* Shader_program::read_file(const char* filename) {
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

GLuint Shader_program::load_shader(GLenum shader_type, const char* filename) {
    // handle
    GLuint shader = glCreateShader(shader_type);
    const GLchar* shader_source = read_file(filename);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    delete []shader_source;

    // error handling 
    int info_log_len	= 0;
    int chars_written	= 0;
    char *info_log;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 1) {
	    info_log = new char[info_log_len];
	    glGetShaderInfoLog(shader, info_log_len, &chars_written, info_log);
	    LOG_DBG(infoLog);
	    delete []info_log;
    }
    return shader;
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

    // error handling x2
    int info_log_len	= 0;
    int chars_written	= 0;
    char *info_log;

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_len);
    if (info_log_len > 1) {
	    info_log = new char[info_log_len];
	    glGetProgramInfoLog(program, info_log_len, &chars_written, info_log);
	    LOG_DBG(infoLog);
	    delete []info_log;
    }
    LOG_DBG("Shader program created");
}

Shader_program::~Shader_program() {
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    glDeleteProgram(program);
    LOG_DBG("Shader program deleted");
}

void Shader_program::use() {
    glUseProgram(program);
}

GLuint Shader_program::u(const char* variable) {
    return glGetUniformLocation(program, variable);
}

GLuint Shader_program::a(const char* attribute) {
    return glGetAttribLocation(program, attribute);
}

void destroy_shader(Shader_program* sp) {
    delete sp;
};

Shader_program* create_shader(const char* vert, const char* frag) {
	Shader_program *sp = new Shader_program(vert, frag);
	return sp;
};
