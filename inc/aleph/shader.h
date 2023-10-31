#ifndef ALEPH_SHADER_H
#define ALEPH_SHADER_H

#include <aleph/defs.h>

typedef struct {
    int id;
} Shader;

bool shader_load(Shader *shader, const char *vert_path, const char *frag_path);
void shader_free(Shader *shader);

#endif /* ALEPH_SHADER_H */
