"""Creates the materials for water inside the ship (/Game/Ship/Water):

M_InteriorWater, the top of the water in the airlock and in flooded compartments (UThalassaWaterSurfaceComponent):
  - Vertex shader: real water waves (deep-water speeds, each wave length moving at its own pace), bigger when the
    water is churned up (parameter Churn), smaller when it's shallow (WaveScale), and the whole surface tilted by
    the slosh the game works out from how the ship moves (Slosh: rise per cm along x and y).
  - Pixel shader: the same waves plus fine ripples (capillary speeds) for the normal; what's under the water seen
    through it, bent by the ripples (refraction), and coloured by how much water the light went through, red going
    first (Beer-Lambert, per colour: Absorb); a fresnel reflection; foam where it meets walls and when churned.
    Seen from below: Snell's window, and total internal reflection outside it.
  Lit by the ship's lamps (forward shading), so the alarm lights colour it red and lamps glint on the ripples.

M_PP_InsideWater, the view with your head under that water (AThalassaCharacter::UpdateInWaterView):
  colours fading with the distance the light travels through water (to the surface at most: SurfaceZ), towards
  the colour of the light around, and a slight waver.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import unreal

FOLDER = "/Game/Ship/Water"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

# How fast each colour is lost in the water, per cm: red first, then green. Murkier than clear sea water.
ABSORB = (0.0065, 0.0022, 0.0018)
# The water's own colour, lit by the lamps: light scattered back out of it.
ALBEDO = (0.012, 0.075, 0.085)
# Under it: the same losses over longer distances (it's the view), and the colour the light fades to.
ABSORB_VIEW = (0.0042, 0.0015, 0.0012)
SCATTER_TINT = (0.20, 0.62, 0.70)


def fresh(name):
    path = FOLDER + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew()), path


class Graph:
    def __init__(self, m):
        self.m = m
        self.y = 0

    def node(self, cls, x=-900, **props):
        e = mel.create_material_expression(self.m, cls, x, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def custom(self, name, code, out, inputs, x=-500):
        """A Custom (HLSL) node. inputs: list of (pin name, expression, output name)."""
        c = self.node(unreal.MaterialExpressionCustom, x)
        c.set_editor_property("description", name)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", out)
        pins = []
        for pin, _, _ in inputs:
            i = unreal.CustomInput()
            i.set_editor_property("input_name", pin)
            pins.append(i)
        c.set_editor_property("inputs", pins)
        for pin, e, e_out in inputs:
            ok = mel.connect_material_expressions(e, e_out, c, pin)
            if not ok:
                unreal.log_warning("THALASSA: could not connect %s to %s" % (pin, name))
        return c

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, -1300, parameter_name=name, default_value=value)

    def vector(self, name, rgb):
        return self.node(unreal.MaterialExpressionVectorParameter, -1300, parameter_name=name,
                         default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))

    def transform(self, e, source, target, e_out=""):
        t = self.node(unreal.MaterialExpressionTransform, -700, transform_source_type=source, transform_type=target)
        mel.connect_material_expressions(e, e_out, t, "")
        return t

    def sub(self, a, b):
        s = self.node(unreal.MaterialExpressionSubtract, -1000)
        mel.connect_material_expressions(a, "", s, "A")
        mel.connect_material_expressions(b, "", s, "B")
        return s


F1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
F2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
TANGENT = unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT
LOCAL = unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL
TO_WORLD = unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD
TO_VIEW = unreal.MaterialVectorCoordTransform.TRANSFORM_VIEW

# The waves, shared by the vertex shader (height) and the pixel shader (slope). P is in the surface's own
# centimetres, T seconds. Deep-water waves: w = sqrt(g k).
WAVES = """
float amp = (0.35 + 3.4 * C * C) * S;
const float Lengths[6] = { 173.0, 97.0, 61.0, 41.0, 29.0, 19.0 };
const float Weights[6] = { 1.0, 0.75, 0.55, 0.4, 0.28, 0.18 };
const float Angles[6] = { 0.2, 1.87, 3.75, 5.1, 2.6, 0.95 };
[unroll] for (int i = 0; i < 6; i++)
{
    float2 d = float2(cos(Angles[i]), sin(Angles[i]));
    float k = 6.2831853 / Lengths[i];
    float w = sqrt(980.0 * k);
    float ph = k * dot(d, P.xy) - w * T + i * 1.93;
    float a = amp * Weights[i];
    h += a * sin(ph);
    g += a * k * cos(ph) * d;
}
"""

# In the vertex shader, waves too short for the grid (Grid: its spacing in cm) fade out: the ripples carry them.
WPO_HLSL = "float h = 0; float2 g = 0;\n" + WAVES.replace(
    "float a = amp * Weights[i];", "float a = amp * Weights[i] * saturate((Lengths[i] / max(Grid, 1.0) - 2.5) / 1.5);") + \
    "\nh += dot(Slosh.xy, P.xy);\nreturn float3(0, 0, h);"

NORMAL_HLSL = "float h = 0; float2 g = Slosh.xy;\n" + WAVES + """
// Fine ripples: short waves where surface tension matters, w = sqrt(g k + 72 k^3), slowed a little for the eye.
// Their paths are bent by a slow warp so they don't line up.
float2 q = P.xy + 9.0 * float2(sin(P.y * 0.031 + T * 0.23), sin(P.x * 0.027 - T * 0.19));
float ra = 0.025 + 0.11 * C;
[unroll] for (int j = 0; j < 8; j++)
{
    float ang = j * 2.39996 + 0.7;
    float2 d = float2(cos(ang), sin(ang));
    float k = 6.2831853 / (3.5 + j * 2.3);
    float w = sqrt(980.0 * k + 72.0 * k * k * k) * 0.6;
    float ph = k * dot(d, q) - w * T + j * 4.1;
    g += ra * k * cos(ph) * d;
}
return normalize(float3(-g.x, -g.y, 1.0));
"""

FOAM_HLSL = """
// Water between the surface and whatever is behind it, along the view: little of it means a wall right there.
float gap = max(DO - PD, 0.0) * Dist / max(PD, 1.0);
float edge = saturate(1.0 - gap / (4.0 + 14.0 * C));
float2 q = P.xy * 0.09;
float n = sin(q.x * 1.7 + T * 0.9 + sin(q.y * 2.3 - T * 0.7) * 1.5) * sin(q.y * 1.9 - T * 0.6 + sin(q.x * 1.3 + T) * 1.5);
n = n * 0.5 + 0.5;
// Churned water whitens in patches.
float caps = saturate((n - (1.05 - 0.55 * C)) * 4.0);
return saturate(edge * (0.3 + 0.7 * n) + caps);
"""

REFRACT_HLSL = """
// Bend the view through the ripples: only their part of the normal, not the flat surface's tilt to the view.
// Less where the water is thin, so things touching the surface stay put. Much more from below.
float d = saturate((DO - PD) / 60.0);
float s = TS >= 0 ? 0.05 : 0.12;
return UV + (NV.xy - NF.xy) * float2(1, -1) * s * d;
"""

COMMON = """
float vs = Dist / max(PD, 1.0);
// The bent view, unless it lands on something in front of the water: then the straight one.
bool bent = DR > PD + 2.0;
float3 Behind = bent ? CR : CO;
float D = bent ? DR : DO;
float3 V = -Ray / max(Dist, 0.001);
float cosI = saturate(abs(dot(normalize(NW), V)));
float thick = max(D - PD, 0.0) * vs;
float3 Tr = exp(-thick * Absorb.rgb);
// From below: light from above only gets through inside Snell's window; outside it the surface is a mirror.
float sinT = 1.333 * sqrt(saturate(1.0 - cosI * cosI));
float window = saturate((1.0 - sinT) * 6.0);
"""

EMIT_HLSL = COMMON + """
if (TS >= 0)
{
    float F = 0.02 + 0.98 * pow(1.0 - cosI, 5.0);
    return Behind * Tr * (1.0 - F) * (1.0 - Foam);
}
float cosT = sqrt(saturate(1.0 - sinT * sinT));
float F = 0.02 + 0.98 * pow(1.0 - cosT, 5.0);
return Behind * window * (1.0 - F) * (1.0 - Foam);
"""

BASE_HLSL = COMMON + """
if (TS >= 0)
{
    return Albedo.rgb * (1.0 - dot(Tr, 1.0 / 3.0)) + Foam * 0.75;
}
return Albedo.rgb * (1.0 - window * 0.75) + Foam * 0.75;
"""


def make_surface():
    m, path = fresh("M_InteriorWater")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property("two_sided", True)
    try:
        # What's behind it is read already fogged; don't fog it twice.
        m.set_editor_property("use_translucency_vertex_fog", False)
    except Exception as error:
        unreal.log_warning("THALASSA: fog setting: %s" % error)
    g = Graph(m)

    churn = g.scalar("Churn", 0.1)
    scale = g.scalar("WaveScale", 1.0)
    slosh = g.vector("Slosh", (0.0, 0.0, 0.0))
    absorb = g.vector("Absorb", ABSORB)
    albedo = g.vector("Albedo", ALBEDO)
    local = g.node(unreal.MaterialExpressionLocalPosition)
    time = g.node(unreal.MaterialExpressionTime)
    wave_in = [("P", local, ""), ("T", time, ""), ("C", churn, ""), ("S", scale, ""), ("Slosh", slosh, "")]

    # Vertex shader: the height, in the surface's own space, turned into the world's.
    grid = g.scalar("GridCm", 12.0)
    wpo = g.custom("Waves", WPO_HLSL, F3, wave_in + [("Grid", grid, "")])
    mel.connect_material_property(g.transform(wpo, LOCAL, TO_WORLD), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # Pixel shader.
    normal = g.custom("Ripples", NORMAL_HLSL, F3, wave_in)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    nw = g.transform(normal, TANGENT, TO_WORLD)
    nv = g.transform(normal, TANGENT, TO_VIEW)
    nf = g.transform(g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 1)), TANGENT, TO_VIEW)

    screen = g.node(unreal.MaterialExpressionScreenPosition)
    pixel_depth = g.node(unreal.MaterialExpressionPixelDepth)
    depth_here = g.node(unreal.MaterialExpressionSceneDepth)
    colour_here = g.node(unreal.MaterialExpressionSceneColor)
    ray = g.sub(g.node(unreal.MaterialExpressionWorldPosition), g.node(unreal.MaterialExpressionCameraPositionWS))
    dist = g.node(unreal.MaterialExpressionLength, -800)
    mel.connect_material_expressions(ray, "", dist, "")
    sign = g.node(unreal.MaterialExpressionTwoSidedSign)

    foam = g.custom("Foam", FOAM_HLSL, F1, [("DO", depth_here, ""), ("PD", pixel_depth, ""), ("Dist", dist, ""),
                                           ("P", local, ""), ("T", time, ""), ("C", churn, "")])
    bent_uv = g.custom("Refraction", REFRACT_HLSL, F2, [("UV", screen, "ViewportUV"), ("NV", nv, ""), ("NF", nf, ""),
                                                       ("DO", depth_here, ""), ("PD", pixel_depth, ""), ("TS", sign, "")])
    depth_bent = g.node(unreal.MaterialExpressionSceneDepth, input_mode=unreal.MaterialSceneAttributeInputMode.COORDINATES)
    mel.connect_material_expressions(bent_uv, "", depth_bent, "")
    colour_bent = g.node(unreal.MaterialExpressionSceneColor, input_mode=unreal.MaterialSceneAttributeInputMode.COORDINATES)
    mel.connect_material_expressions(bent_uv, "", colour_bent, "")

    shared = [("CO", colour_here, ""), ("CR", colour_bent, ""), ("DO", depth_here, ""), ("DR", depth_bent, ""),
              ("PD", pixel_depth, ""), ("Dist", dist, ""), ("Ray", ray, ""), ("NW", nw, ""), ("TS", sign, ""),
              ("Foam", foam, ""), ("Absorb", absorb, ""), ("Albedo", albedo, "")]
    emit = g.custom("Through", EMIT_HLSL, F3, shared, x=-250)
    base = g.custom("Body", BASE_HLSL, F3, shared, x=-250)
    mel.connect_material_property(emit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = g.custom("Roughness", "return 0.035 + Foam * 0.5;", F1, [("Foam", foam, "")], x=-250)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.node(unreal.MaterialExpressionConstant, r=0.25), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(g.node(unreal.MaterialExpressionConstant, r=1.0), "", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(m)
    eal.save_asset(path)
    unreal.log("THALASSA: created " + path)


WAVER_HLSL = """
return UV + 0.0022 * float2(sin(UV.y * 38.0 + T * 2.1) + 0.5 * sin(UV.y * 71.0 - T * 3.3),
                            cos(UV.x * 33.0 + T * 1.7) + 0.5 * cos(UV.x * 63.0 + T * 2.9));
"""

VIEW_HLSL = """
float dist = length(Ray);
float3 dir = Ray / max(dist, 0.001);
// Only the water counts: up to its surface, if the view goes up through it.
float path = dist;
if (dir.z > 0.0001)
{
    path = min(path, max(SZ - Cam, 0.0) / dir.z);
}
float3 Tr = exp(-path * Absorb.rgb);
// The light the water scatters back: the colour of the light all around, tinted.
float3 around = 0;
[unroll] for (int i = 0; i < 8; i++)
{
    float a = i * 0.785398 + 0.3;
    float2 at = clamp(UV + float2(cos(a), sin(a)) * float2(0.09, 0.16), 0.01, 0.99);
    around += SceneTextureLookup(Parameters, ViewportUVToBufferUV(at), PPI_PostProcessInput0, false).rgb;
}
around /= 8.0;
float3 scatter = Tint.rgb * dot(around, float3(0.3, 0.59, 0.11)) * 0.45;
return Scene * Tr + scatter * (1.0 - Tr);
"""


def make_view():
    m, path = fresh("M_PP_InsideWater")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_DOF)
    g = Graph(m)
    surface = g.scalar("SurfaceZ", 0.0)
    absorb = g.vector("Absorb", ABSORB_VIEW)
    tint = g.vector("Tint", SCATTER_TINT)
    screen = g.node(unreal.MaterialExpressionScreenPosition)
    time = g.node(unreal.MaterialExpressionTime)
    waver = g.custom("Waver", WAVER_HLSL, F2, [("UV", screen, "ViewportUV"), ("T", time, "")])
    scene = g.node(unreal.MaterialExpressionSceneTexture, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    mel.connect_material_expressions(waver, "", scene, "UVs")
    camera = g.node(unreal.MaterialExpressionCameraPositionWS)
    ray = g.sub(g.node(unreal.MaterialExpressionWorldPosition), camera)
    cam_z = g.node(unreal.MaterialExpressionComponentMask, -800, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(camera, "", cam_z, "")
    view = g.custom("InWater", VIEW_HLSL, F3, [("Scene", scene, "Color"), ("UV", screen, "ViewportUV"), ("Ray", ray, ""),
                                               ("Cam", cam_z, ""), ("SZ", surface, ""), ("Absorb", absorb, ""), ("Tint", tint, "")])
    mel.connect_material_property(view, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_asset(path)
    unreal.log("THALASSA: created " + path)


make_surface()
make_view()
