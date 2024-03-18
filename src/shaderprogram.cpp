#include "shaderprogram.hpp"
#include "utils.hpp"

char* ShaderProgram::readFile(const char* fileName) {
    int fSize;
    FILE *file;
    char *data;

    file = fopen(fileName, "rb");
    if (file != NULL) {
	fseek(file, 0, SEEK_END);
	fSize = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = new char[fSize + 1];
	int readSize = fread(data, 1, fSize, file);
	data[fSize] = 0;
	fclose(file);

	return data;
    }
    return NULL;
}

GLuint ShaderProgram::loadShader(GLenum shaderType, const char* fileName) {
    // handle
    GLuint shader = glCreateShader(shaderType);
    const GLchar* shaderSource = readFile(fileName);
    glShaderSource(shader, 1, &shaderSource, NULL);
    glCompileShader(shader);

    delete []shaderSource;

    // error handling 
    int infoLogLength	= 0;
    int charsWritten	= 0;
    char *infoLog;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 1) {
	infoLog = new char[infoLogLength];
	glGetShaderInfoLog(shader, infoLogLength, &charsWritten, infoLog);
	LOG_DBG(infoLog);
	delete []infoLog;
    }
    return shader;
}

ShaderProgram::ShaderProgram(const char* vShaderFile, const char* fShaderFile) {
    LOG_DBG("Loading vertex shader...");
    vertexShader = loadShader(GL_VERTEX_SHADER, vShaderFile);

    LOG_DBG("Loading fragment shader...");
    fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderFile);

    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // error handling x2
    int infoLogLength	= 0;
    int charsWritten	= 0;
    char *infoLog;

    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLogLength);
    if (infoLogLength > 1) {
	infoLog = new char[infoLogLength];
	glGetProgramInfoLog(shaderProgram, infoLogLength, &charsWritten, infoLog);
	LOG_DBG(infoLog);
	delete []infoLog;
    }
    LOG_DBG("Shader program created");
}

ShaderProgram::~ShaderProgram() {
    glDetachShader(shaderProgram, vertexShader);
    glDetachShader(shaderProgram, fragmentShader);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glDeleteProgram(shaderProgram);
    LOG_DBG("Shader program deleted");
}

void ShaderProgram::use() {
    glUseProgram(shaderProgram);
}

GLuint ShaderProgram::u(const char* varName) {
    return glGetUniformLocation(shaderProgram, varName);
}

GLuint ShaderProgram::a(const char* attName) {
    return glGetAttribLocation(shaderProgram, attName);
}
