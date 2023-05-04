#version 450
#pragma shader_stage(vertex)

vec3 Vertices[] = { {0.f, 0.f, 0.f},
                    {1.f, 0.f, 0.f},
                    {1.f, 1.f, 0.f},
                    {0.f, 1.f, 0.f}
                  };

vec2 TexCoord[] = { {0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f} };

layout(location=0) out vec2 OutCoord;

void main()
{
    gl_Position = vec4(Vertices[gl_VertexIndex], 1.f);
    OutCoord = TexCoord[gl_VertexIndex];
}

