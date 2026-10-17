# Create solid WallWhite / WallBlack materials (no parameters) for reliable zebra walls.
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

FOLDER = "/Game/Game/Materials"


def _connect(expr, prop):
	for out_name in ("", "RGB", "Output", "R"):
		if MEL.connect_material_property(expr, out_name, prop):
			return True
	return False


def make_solid(asset_name, color):
	path = FOLDER + "/" + asset_name
	if EAL.does_asset_exist(path):
		mat = EAL.load_asset(path)
	else:
		factory = unreal.MaterialFactoryNew()
		mat = ASSET_TOOLS.create_asset(asset_name, FOLDER, unreal.Material, factory)

	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
	mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

	MEL.delete_all_material_expressions(mat)

	const3 = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
	const3.set_editor_property("constant", color)
	_connect(const3, unreal.MaterialProperty.MP_BASE_COLOR)

	rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 160)
	rough.set_editor_property("r", 0.55)
	_connect(rough, unreal.MaterialProperty.MP_ROUGHNESS)

	MEL.layout_material_expressions(mat)
	MEL.recompile_material(mat)
	EAL.save_asset(path)
	unreal.log("Created/updated " + path)


make_solid("WallWhite", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
make_solid("WallBlack", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
unreal.log("FixWallMaterials.py done")
