#include <stdio.h>

#include <GL/glew.h>
#include <GL/gl.h>

#include <aleph/shader.h>
#include <aleph/membuf.h>

bool shader_load(Shader *shader, const char *vert_path, const char *frag_path) {
    Membuf vert_buf, frag_buf;
    if (!membuf_load(&vert_buf, vert_path)) {
        printf("cannot load %s\n", vert_path);
        return false;
    }

    if (!membuf_load(&frag_buf, frag_path)) {
        printf("cannot load %s\n", frag_path);
        return false;
    }

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, (const GLchar **) &vert_buf.data, NULL);
    glCompileShader(vert);

    int success;
    char info_log[512];
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert, 512, NULL, info_log);
        printf("ERROR: %s: \n%s", vert_path, info_log);
        exit(EXIT_FAILURE);
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, (const GLchar **) &frag_buf.data, NULL);
    glCompileShader(frag);

    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, 512, NULL, info_log);
        printf("ERROR: %s: \n%s", frag_path, info_log);
        exit(EXIT_FAILURE);
    }

    shader->id = glCreateProgram();
    glAttachShader(shader->id, vert);
    glAttachShader(shader->id, frag);
    glLinkProgram(shader->id);

    glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader->id, 512, NULL, info_log);
        printf("ERROR: %s %s: \n%s", vert_path, frag_path, info_log);
        exit(EXIT_FAILURE);
    }

    glUseProgram(shader->id);

    return true;
}

void shader_free(Shader *shader) {
    glDeleteProgram(shader->id);
}