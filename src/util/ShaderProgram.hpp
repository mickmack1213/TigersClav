#pragma once

#include <GL/gl3w.h>
#include <string>

class ShaderProgram
{
public:
    ShaderProgram(std::string vertexCode, std::string fragmentCode);
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    bool isGood() const { return good_; }
    operator bool() const { return good_; }

    void use();

    GLuint getAttribLocation(std::string name);
    GLuint getUniformLocation(std::string name);

private:
    GLuint program_;
    bool good_;
};
