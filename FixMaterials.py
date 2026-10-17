# Rebuild Road / Wall so C++ MID SetVectorParameterValue("BaseColor") works.
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def _connect(expr, prop):
	for out_name in ("", "RGB", "Output"):
		if MEL.connect_material_property(expr, out_name, prop):
			return True
	unreal.log_error("Failed to connect to " + str(prop))
	return False


def fix_road():
	path = "/Game/Game/Materials/Road"
	mat = EAL.load_asset(path)
	if not mat:
		unreal.log_error("MISSING " + path)
		return

	# Translucent unlit: BaseColor drives visible tint cleanly
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
	mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
	mat.set_editor_property("two_sided", True)

	MEL.delete_all_material_expressions(mat)

	base = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, -40)
	base.set_editor_property("parameter_name", "BaseColor")
	base.set_editor_property("default_value", unreal.LinearColor(1.0, 0.0, 0.0, 1.0))
	_connect(base, unreal.MaterialProperty.MP_BASE_COLOR)
	_connect(base, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

	opacity = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -480, 180)
	opacity.set_editor_property("parameter_name", "Opacity")
	opacity.set_editor_property("default_value", 0.0)
	_connect(opacity, unreal.MaterialProperty.MP_OPACITY)

	MEL.layout_material_expressions(mat)
	MEL.recompile_material(mat)
	EAL.save_asset(path)
	unreal.log("Road OK: BaseColor + Opacity (Unlit Translucent)")


def fix_wall():
	path = "/Game/Game/Materials/Wall"
	mat = EAL.load_asset(path)
	if not mat:
		unreal.log_error("MISSING " + path)
		return

	# Lit so white walls stay readable against a bright sky (Unlit white washes out)
	mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
	mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

	MEL.delete_all_material_expressions(mat)

	base = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -480, -40)
	base.set_editor_property("parameter_name", "BaseColor")
	base.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
	_connect(base, unreal.MaterialProperty.MP_BASE_COLOR)

	rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -480, 120)
	rough.set_editor_property("r", 0.6)
	_connect(rough, unreal.MaterialProperty.MP_ROUGHNESS)

	MEL.layout_material_expressions(mat)
	MEL.recompile_material(mat)
	EAL.save_asset(path)
	unreal.log("Wall OK: BaseColor (Default Lit Opaque)")


def verify(path):
	mat = EAL.load_asset(path)
	exprs = MEL.get_material_expressions(mat)
	unreal.log("VERIFY " + path + " exprs=" + str(len(exprs)))
	for e in exprs:
		cname = e.get_class().get_name()
		if "Parameter" in cname:
			try:
				pname = str(e.get_editor_property("parameter_name"))
			except Exception:
				pname = "?"
			unreal.log("  " + cname + " -> " + pname)


fix_road()
fix_wall()
verify("/Game/Game/Materials/Road")
verify("/Game/Game/Materials/Wall")
unreal.log("FixMaterials.py done")
