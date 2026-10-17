import unreal

def dump_mat(path):
	mat = unreal.EditorAssetLibrary.load_asset(path)
	if not mat:
		unreal.log_error("MISSING " + path)
		return
	unreal.log("==== " + path + " ====")
	unreal.log("class=" + str(type(mat)))
	try:
		unreal.log("blend=" + str(mat.get_editor_property("blend_mode")))
	except Exception as e:
		unreal.log("blend err " + str(e))
	# list expressions / parameters via material editing library
	exprs = unreal.MaterialEditingLibrary.get_material_expressions(mat) if hasattr(unreal.MaterialEditingLibrary, "get_material_expressions") else []
	unreal.log("expr_count=" + str(len(exprs) if exprs else 0))
	for e in (exprs or []):
		name = e.get_name()
		cname = e.get_class().get_name()
		unreal.log("  expr " + cname + " " + name)
		if "VectorParameter" in cname or "ScalarParameter" in cname:
			try:
				unreal.log("    parameter_name=" + str(e.get_editor_property("parameter_name")))
			except Exception:
				pass

dump_mat("/Game/Game/Materials/Road")
dump_mat("/Game/Game/Materials/Wall")
