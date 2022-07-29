#include "ShaderProgram.hpp"
#include "easylogging++.h"

ShaderProgram::ShaderProgram(std::string vertexCode, std::string fragmentCode)
:good_(false)
{
    GLint result = GL_FALSE;
    int infoLogLength;

    // Compile Vertex Shader
    GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    char const* pVertexSource = vertexCode.c_str();
    glShaderSource(vertexShaderID, 1, &pVertexSource, NULL);
    glCompileShader(vertexShaderID);

    // Check Vertex Shader
    glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if(infoLogLength > 0)
    {
        std::string vertexShaderErrorMessage(infoLogLength + 1, 0);
        glGetShaderInfoLog(vertexShaderID, infoLogLength, NULL, vertexShaderErrorMessage.data());
        LOG(ERROR) << "Vertex shader compilation failed:\n" << vertexShaderErrorMessage;
        glDeleteShader(vertexShaderID);
        return;
    }

    // Compile Fragment Shader
    GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    char const* pFragmentSource = fragmentCode.c_str();
    glShaderSource(fragmentShaderID, 1, &pFragmentSource, NULL);
    glCompileShader(fragmentShaderID);

    // Check Fragment Shader
    glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
    if(infoLogLength > 0)
    {
        std::string fragmentShaderErrorMessage(infoLogLength + 1, 0);
        glGetShaderInfoLog(fragmentShaderID, infoLogLength, NULL, fragmentShaderErrorMessage.data());
        LOG(ERROR) << "Fragment shader compilation failed:\n" << fragmentShaderErrorMessage;
        glDeleteShader(vertexShaderID);
        glDeleteShader(fragmentShaderID);
        return;
    }

    // Link the program
    program_ = glCreateProgram();
    glAttachShader(program_, vertexShaderID);
    glAttachShader(program_, fragmentShaderID);
    glLinkProgram(program_);

    // Check the program
    glGetProgramiv(program_, GL_LINK_STATUS, &result);
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &infoLogLength);
    if(infoLogLength > 0)
    {
        std::string programErrorMessage(infoLogLength + 1, 0);
        glGetProgramInfoLog(program_, infoLogLength, NULL, programErrorMessage.data());
        LOG(ERROR) << "Shader program linking failed:\n" << programErrorMessage;
        glDeleteShader(vertexShaderID);
        glDeleteShader(fragmentShaderID);
        return;
    }

    glDetachShader(program_, vertexShaderID);
    glDetachShader(program_, fragmentShaderID);

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    good_ = true;
}

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(program_);
}

void ShaderProgram::use()
{
    if(!good_)
        return;

    glUseProgram(program_);
}

GLuint ShaderProgram::getAttribLocation(std::string name)
{
    if(!good_)
        return 0;

    return (GLuint)glGetAttribLocation(program_, name.c_str());
}

GLuint ShaderProgram::getUniformLocation(std::string name)
{
    if(!good_)
        return 0;

    return (GLuint)glGetUniformLocation(program_, name.c_str());
}
