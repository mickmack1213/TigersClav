R""(
#version 130

uniform sampler2D Texture;

in vec2 Frag_UV;
in vec4 Frag_Color;

out vec4 Out_Color;

void main()
{
    vec4 col = texture(Texture, Frag_UV.st);
    Out_Color = vec4(col.x, col.x, col.x, 1.0f);
}
)""
