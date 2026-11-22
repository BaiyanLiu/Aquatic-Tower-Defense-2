# Aquatic Tower Defense - creates /Game/AquaticTowerDefense/M_ATD_Detail, the material of the higher-detail models.
# Run once from the editor (Python plugin): UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
#
# Lit, opaque: BaseColor = Color * VertexColor * relief tone, plus Roughness, Metallic and Specular parameters, and a procedural
# surface relief (pores, chiselled stone, wood grain, shell ridges, brain-coral grooves) computed in the mesh's local space, so it
# moves with the animated parts. The C++ side (TD::SharedDetailMaterial) sets one instance per colour and surface.

import unreal

PATH = '/Game/AquaticTowerDefense'
NAME = 'M_ATD_Detail'
MEL = unreal.MaterialEditingLibrary

RELIEF_HLSL = r'''
float3 q = P * BumpScale;
float fb = 0.0;
float amp = 0.5;
float3 x = q;
for (int o = 0; o < 3; o++)
{
    float3 i = floor(x);
    float3 f = frac(x);
    f = f * f * (3.0 - 2.0 * f);
    float3 k = float3(127.1, 311.7, 74.7);
    float n000 = frac(sin(dot(i, k)) * 43758.5453);
    float n100 = frac(sin(dot(i + float3(1, 0, 0), k)) * 43758.5453);
    float n010 = frac(sin(dot(i + float3(0, 1, 0), k)) * 43758.5453);
    float n110 = frac(sin(dot(i + float3(1, 1, 0), k)) * 43758.5453);
    float n001 = frac(sin(dot(i + float3(0, 0, 1), k)) * 43758.5453);
    float n101 = frac(sin(dot(i + float3(1, 0, 1), k)) * 43758.5453);
    float n011 = frac(sin(dot(i + float3(0, 1, 1), k)) * 43758.5453);
    float n111 = frac(sin(dot(i + float3(1, 1, 1), k)) * 43758.5453);
    float v = lerp(lerp(lerp(n000, n100, f.x), lerp(n010, n110, f.x), f.y), lerp(lerp(n001, n101, f.x), lerp(n011, n111, f.x), f.y), f.z);
    fb += amp * v;
    x = x * 2.03 + float3(17.0, 5.0, 11.0);
    amp *= 0.5;
}
float h = fb;
if (Pattern > 1.5 && Pattern < 2.5)
{
    h = fb + 0.35 * (1.0 - abs(fb * 2.0 - 0.9));           // stone: lumps and chipped ridges
}
else if (Pattern > 2.5 && Pattern < 3.5)
{
    h = sin(length(P.xy) * BumpScale * 6.0 + fb * 3.0) * 0.5 + 0.5 * fb;   // wood grain
}
else if (Pattern > 3.5 && Pattern < 4.5)
{
    h = sin(atan2(P.y, P.x) * 28.0) * 0.5 + fb * 0.3;      // shell ridges
}
else if (Pattern > 4.5)
{
    h = pow(abs(sin(fb * 18.0)), 0.5);                      // brain-coral grooves
}

float3 n = normalize(N);
float3 dpx = ddx(P);
float3 dpy = ddy(P);
float footprint = max(length(dpx), length(dpy)) * BumpScale;   // relief features per pixel
float fade = saturate(1.4 - footprint * 2.5);                     // fade out sub-pixel relief (it aliases into grain)
float dhx = ddx(h);
float dhy = ddy(h);
float3 r1 = cross(dpy, n);
float3 r2 = cross(n, dpx);
float det = dot(dpx, r1);
float3 g = sign(det) * (dhx * r1 + dhy * r2);
float3 bumped = normalize(abs(det) * n - g * Bump * fade);
if (Bump * fade <= 0.0 || abs(det) < 1e-12)
{
    bumped = n;
}
float tone = 1.0 + Tone * fade * (h - 0.45) * 1.2;
return float4(bumped, tone);
'''


def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def scalar(mat, name, default, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, -900, y)
    e.set_editor_property('parameter_name', name)
    e.set_editor_property('default_value', default)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError('could not connect %s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def main():
    full = PATH + '/' + NAME
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('tangent_space_normal', False)
    # creature parts are drawn by instanced components (ATDCrowd); a cooked game needs this usage flag set ahead of time
    mat.set_editor_property('used_with_instanced_static_meshes', True)

    color = expr(mat, unreal.MaterialExpressionVectorParameter, -900, -400)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    rough = scalar(mat, 'Roughness', 0.5, -200)
    metal = scalar(mat, 'Metallic', 0.0, -100)
    spec = scalar(mat, 'Specular', 0.5, 0)
    bump = scalar(mat, 'Bump', 0.15, 100)
    bump_scale = scalar(mat, 'BumpScale', 0.5, 200)
    pattern = scalar(mat, 'Pattern', 0.0, 300)
    tone = scalar(mat, 'Tone', 0.0, 400)

    vcol = expr(mat, unreal.MaterialExpressionVertexColor, -900, -300)

    # local position and normal, so the relief sticks to the moving parts
    try:
        local_pos = expr(mat, unreal.MaterialExpressionLocalPosition, -900, 500)
    except Exception:
        world_pos = expr(mat, unreal.MaterialExpressionWorldPosition, -1100, 500)
        local_pos = expr(mat, unreal.MaterialExpressionTransformPosition, -900, 500)
        local_pos.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
        local_pos.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        link(world_pos, '', local_pos, '')
    normal_ws = expr(mat, unreal.MaterialExpressionVertexNormalWS, -1100, 600)
    local_n = expr(mat, unreal.MaterialExpressionTransform, -900, 600)
    local_n.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    local_n.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(normal_ws, '', local_n, '')

    custom = expr(mat, unreal.MaterialExpressionCustom, -500, 400)
    custom.set_editor_property('description', 'Relief')
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('code', RELIEF_HLSL)
    inputs = []
    for n in ['P', 'N', 'Pattern', 'Bump', 'BumpScale', 'Tone']:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', n)
        inputs.append(ci)
    custom.set_editor_property('inputs', inputs)
    link(local_pos, '', custom, 'P')
    link(local_n, '', custom, 'N')
    link(pattern, '', custom, 'Pattern')
    link(bump, '', custom, 'Bump')
    link(bump_scale, '', custom, 'BumpScale')
    link(tone, '', custom, 'Tone')

    mask_n = expr(mat, unreal.MaterialExpressionComponentMask, -300, 400)
    for c, v in (('r', True), ('g', True), ('b', True), ('a', False)):
        mask_n.set_editor_property(c, v)
    link(custom, '', mask_n, '')
    world_n = expr(mat, unreal.MaterialExpressionTransform, -150, 400)
    world_n.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    world_n.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(mask_n, '', world_n, '')

    mask_t = expr(mat, unreal.MaterialExpressionComponentMask, -300, 250)
    for c, v in (('r', False), ('g', False), ('b', False), ('a', True)):
        mask_t.set_editor_property(c, v)
    link(custom, '', mask_t, '')

    tinted = expr(mat, unreal.MaterialExpressionMultiply, -500, -350)
    link(color, '', tinted, 'A')
    link(vcol, '', tinted, 'B')
    base = expr(mat, unreal.MaterialExpressionMultiply, -150, -300)
    link(tinted, '', base, 'A')
    link(mask_t, '', base, 'B')

    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(world_n, '', unreal.MaterialProperty.MP_NORMAL)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
    unreal.log('Created ' + full)


main()
