#version 450
// Neutro X: copia o quadro da CPU (textura linear A8R8G8B8, amostragem por ponto).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_frame;

void main()
{
	o_color = texture(u_frame, v_uv);
}
