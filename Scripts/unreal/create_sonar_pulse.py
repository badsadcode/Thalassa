"""Creates /Game/Ship/Sonar/MPC_SonarPulse: what every material needs to know to draw the sonar ping's ring.

  PulseOrigin    (vector)  where the ping was sent, in world centimetres
  PulseRadius    (scalar)  how far the wave has spread, in centimetres
  PulseStrength  (scalar)  0 when there is no ping; fades as the wave spreads

UThalassaSonarComponent writes these every frame on every machine. The seabed and rock materials read them
(create_ocean_materials.py, PULSE_HLSL) and glow where the wave front passes, with height contours behind it.
Left alone if it exists, so the materials that use it keep their links.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

FOLDER = "/Game/Ship/Sonar"
PATH = FOLDER + "/MPC_SonarPulse"

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("THALASSA: " + PATH + " already exists; left as it is")
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mpc = tools.create_asset("MPC_SonarPulse", FOLDER, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())

    def scalar(name, value):
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", value)
        return p

    origin = unreal.CollectionVectorParameter()
    origin.set_editor_property("parameter_name", "PulseOrigin")
    origin.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    mpc.set_editor_property("vector_parameters", [origin])
    mpc.set_editor_property("scalar_parameters", [scalar("PulseRadius", 0.0), scalar("PulseStrength", 0.0)])
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log("THALASSA: created " + PATH)
