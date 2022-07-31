R""(
#version 130

uniform sampler2D Texture;
uniform sampler2D TextureU;
uniform sampler2D TextureV;

in vec2 Frag_UV;
in vec4 Frag_Color;

out vec4 Out_Color;

void main() 
{
    vec3 yuv, rgb;
    vec3 yuv2r = vec3(1.164, 0.0, 1.596);
    vec3 yuv2g = vec3(1.164, -0.391, -0.813);
    vec3 yuv2b = vec3(1.164, 2.018, 0.0);

    yuv.x = texture(Texture, Frag_UV.st).r - 0.0625;
    yuv.y = texture(TextureU, Frag_UV.st).r - 0.5;
    yuv.z = texture(TextureV, Frag_UV.st).r - 0.5;

    rgb.x = dot(yuv, yuv2r);
    rgb.y = dot(yuv, yuv2g);
    rgb.z = dot(yuv, yuv2b);

    Out_Color = vec4(rgb, 1.0);
}

)""
