#version 450
#pragma shader_stage(fragment)

// Uniforms
layout(set = 0, binding=0) uniform sampler2D InTexture;

// Input
layout(location=0) in vec2 inCoord;

// Output
layout(location=0) out vec4 OutColor;

void main()
{
  OutColor = texture(InTexture, inCoord);
}

