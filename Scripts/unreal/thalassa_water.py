"""The water's look, shared by every map: light from above, the murk, and the camera's grade.

Values here are the look: tune them by capturing frames (docs/SEABED.md, "Tuning the look").
Used by create_test_map.py and create_ocean_map.py.
"""
import unreal


def add_water(actors):
    """Spawns the light, ambient light, water fog and post-process that make the underwater look."""
    LIGHT_FROM_ABOVE = 0.35                                    # lux: what filters down to 200 m. Bright sun is ~100,000.
    LIGHT_COLOUR = unreal.LinearColor(0.35, 0.75, 1.0, 1.0)    # Red is gone first underwater, then green.
    WATER_DENSITY = 0.07                                       # Fog density: how quickly things fade into the murk.
    WATER_COLOUR = unreal.LinearColor(0.0012, 0.008, 0.012, 1.0)  # What the murk itself glows, lit from above.
    SCATTER_ALBEDO = unreal.Color(110, 150, 175, 255)         # How the water tints light scattered in it.
    EXPOSURE_BIAS = 2.5                                        # Manual exposure, so the darkness stays dark.

    # Light from above: cold, dim, straight down, with beams through the murk.
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, -80, 20))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(LIGHT_FROM_ABOVE)
    sun.light_component.set_light_color(LIGHT_COLOUR)
    sun.light_component.set_editor_property("volumetric_scattering_intensity", 1.0)
    sun.light_component.set_editor_property("light_source_angle", 30.0)  # Light from above is diffuse down here: very soft shadows.

    # Ambient light: only from above. Below is the dark.
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_intensity(0.25)
    sky.light_component.set_editor_property("lower_hemisphere_is_black", True)
    sky.light_component.set_editor_property("lower_hemisphere_color", unreal.LinearColor(0, 0, 0, 1))

    # The water itself: even murk at every depth, and volumetric scattering so lamps cast beams.
    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    f = fog.component
    f.set_editor_property("fog_density", WATER_DENSITY)
    f.set_editor_property("fog_height_falloff", 0.0001)
    f.set_editor_property("fog_max_opacity", 1.0)
    f.set_editor_property("fog_inscattering_luminance", WATER_COLOUR)
    f.set_editor_property("enable_volumetric_fog", True)
    f.set_editor_property("volumetric_fog_scattering_distribution", 0.7)  # Forward scattering: beams glow toward you.
    f.set_editor_property("volumetric_fog_albedo", SCATTER_ALBEDO)
    f.set_editor_property("volumetric_fog_extinction_scale", 1.0)
    f.set_editor_property("volumetric_fog_distance", 12000.0)

    # The camera: fixed exposure, a cold grade, a dark edge and a little grain.
    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    post.set_editor_property("unbound", True)
    pp = post.get_editor_property("settings")
    for name, value in [
            ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
            ("auto_exposure_bias", EXPOSURE_BIAS),
            ("auto_exposure_apply_physical_camera_exposure", False),
            ("color_saturation", unreal.Vector4(0.85, 0.95, 1.0, 0.9)),
            ("color_gain", unreal.Vector4(0.92, 1.0, 1.04, 1.0)),
            ("vignette_intensity", 0.55),
            ("film_grain_intensity", 0.12),
            ("bloom_intensity", 0.8),
            ("scene_fringe_intensity", 0.6)]:
        pp.set_editor_property("override_" + name, True)
        pp.set_editor_property(name, value)
    post.set_editor_property("settings", pp)
