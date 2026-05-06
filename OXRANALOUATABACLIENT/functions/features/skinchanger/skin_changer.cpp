#include "skin_changer.hpp"
#include "../shared/econ_item_attribute_manager.hpp"
#include "../shared/item_schema.hpp"
#include "../../valve/interfaces/interfaces.hpp"
#include "../../valve/schema/schema.hpp"
#include "../../valve/interfaces/vtables/i_econ_item_system.hpp"
#include "../../menu/menu.hpp"

c_base_entity* c_skin_changer::get_hud_weapon(c_base_entity* weapon, c_cs_player_pawn* local_pawn) {
	auto arms_handle = local_pawn->m_hud_model_arms();
	if (!arms_handle.is_valid())
		return nullptr;

	auto* hud_arms = reinterpret_cast<c_base_entity*>(
		g_interfaces->m_entity_system->get_base_entity(arms_handle.get_entry_index())
	);
	if (!valid_ptr(hud_arms))
		return nullptr;

	auto* arms_node = hud_arms->m_scene_node();
	if (!valid_ptr(arms_node))
		return nullptr;

	for (auto* vm = arms_node->m_child(); valid_ptr(vm); vm = vm->m_next_sibling()) {
		auto* vm_owner = vm->m_owner();
		if (!valid_ptr(vm_owner))
			continue;

		auto* vm_entity = reinterpret_cast<c_base_entity*>(vm_owner);
		auto owner_handle = vm_entity->m_owner_entity();
		if (!owner_handle.is_valid())
			continue;

		if (g_interfaces->m_entity_system->get_base_entity(owner_handle.get_entry_index()) == weapon)
			return vm_entity;
	}
	return nullptr;
}

void c_skin_changer::apply_skin(c_econ_entity* weapon, c_econ_item_view* item, int paint_kit_id, float wear, int seed, const char* custom_name, c_cs_player_pawn* local_pawn, uint16_t def_index) {
	econ_item_attribute_manager::remove(item);
	econ_item_attribute_manager::create(item, paint_kit_id, wear, seed);

	weapon->m_paint_kit() = paint_kit_id;
	weapon->m_wear() = wear;
	weapon->m_seed() = seed;

	if (custom_name && custom_name[0] != '\0')
		strcpy_s(item->m_custom_name(), 161, custom_name);

	bool uses_old_model = false;
	c_paint_kit* pk = nullptr;
	if ((pk = g_interfaces->m_source2_client->get_econ_item_system()->get_econ_item_schema()->get_paint_kits().find_by_key(paint_kit_id)))
		uses_old_model = pk->uses_old_model();

	uint64_t mesh_mask = uses_old_model ? 2 : 1;

	if (auto* scene_node = weapon->m_scene_node())
		scene_node->set_mesh_group_mask(mesh_mask);

	if (auto* hud_weapon = get_hud_weapon(weapon, local_pawn))
		if (auto* hud_node = hud_weapon->m_scene_node())
			hud_node->set_mesh_group_mask(mesh_mask);

	weapon->update_skin(true);
	weapon->update_weapon_data();
	item->m_name_description_ptr() = 0;
}

void c_skin_changer::initialize() {
	if (m_initialized)
		return;

	if (!g_item_schema->is_initialized())
		g_item_schema->initialize();

	m_initialized = g_item_schema->is_initialized();
}

void c_skin_changer::process_weapon(c_econ_entity* weapon, c_econ_item_view* item, c_cs_player_pawn* local_pawn, bool force_update, bool& did_update) {
	uint16_t def_index = item->m_definition_index();
	int config_index = c_config::skin_changer_t::get_config_index(def_index);
	if (config_index == 0)
		return;

	auto& skin = g_cfg->skin_changer.weapon_skins[config_index];
	if (skin.paint_kit == 0)
		return;

	int paint_kit_id = g_item_schema->get_paint_kit_id_for_item(def_index, skin.paint_kit);
	if (paint_kit_id == 0 || (weapon->m_paint_kit() == paint_kit_id && !force_update))
		return;

	apply_skin(weapon, item, paint_kit_id, skin.wear, skin.seed, skin.custom_name, local_pawn, def_index);
	c_hud::clear_hud_weapon_icon_for(weapon);
	did_update = true;
}

void c_skin_changer::process_knife(c_econ_entity* weapon, c_econ_item_view* item, c_cs_player_pawn* local_pawn, bool force_update, bool& did_update) {
	if (g_cfg->knife_changer.m_knife == 0)
		return;
	if (!g_item_schema->is_initialized()
		|| g_cfg->knife_changer.m_knife >= (int)g_item_schema->knives.size())
		return;

	const uint16_t def_index      = item->m_definition_index();
	const uint16_t selected_knife = g_item_schema->knives[g_cfg->knife_changer.m_knife].definition_index;
	if (selected_knife == 0)
		return;

	int paint_kit_id = g_item_schema->get_paint_kit_id_for_item(selected_knife, g_cfg->knife_changer.m_paint_kit);
	bool config_changed = (m_last_knife != selected_knife) ||
	                      (m_last_knife_paint_kit_id != paint_kit_id) ||
	                      (m_last_knife_wear != g_cfg->knife_changer.m_wear) ||
	                      (m_last_knife_seed != g_cfg->knife_changer.m_seed);

	if (def_index == selected_knife && !config_changed && !force_update)
		return;

	item->m_definition_index() = selected_knife;
	item->m_entity_quality() = QUALITY_UNUSUAL;

	if (const char* model_path = g_item_schema->knives[g_cfg->knife_changer.m_knife].model_path) {
		weapon->set_model(model_path);
		if (auto* hud_weapon = get_hud_weapon(weapon, local_pawn))
			hud_weapon->set_model(model_path);
	}

	econ_item_attribute_manager::remove(item);
	if (paint_kit_id > 0)
		econ_item_attribute_manager::create(item, paint_kit_id, g_cfg->knife_changer.m_wear, g_cfg->knife_changer.m_seed);

	bool uses_old_model = false;
	if (paint_kit_id > 0)
		if (auto* pk = g_interfaces->m_source2_client->get_econ_item_system()->get_econ_item_schema()->get_paint_kits().find_by_key(paint_kit_id))
			uses_old_model = pk->uses_old_model();

	uint64_t mesh_mask = uses_old_model ? 1 : 2;
	if (auto* scene_node = weapon->m_scene_node())
		scene_node->set_mesh_group_mask(mesh_mask);
	if (auto* hud_weapon = get_hud_weapon(weapon, local_pawn))
		if (auto* hud_node = hud_weapon->m_scene_node())
			hud_node->set_mesh_group_mask(mesh_mask);

	if (g_cfg->knife_changer.m_custom_name[0] != '\0')
		strcpy_s(item->m_custom_name(), 161, g_cfg->knife_changer.m_custom_name);
	else
		item->m_custom_name()[0] = '\0';

	weapon->update_subclass(selected_knife);
	weapon->update_skin(true);
	weapon->update_weapon_data();
	item->m_name_description_ptr() = 0;

	m_last_knife = selected_knife;
	m_last_knife_paint_kit_id = paint_kit_id;
	m_last_knife_wear = g_cfg->knife_changer.m_wear;
	m_last_knife_seed = g_cfg->knife_changer.m_seed;
	c_hud::clear_hud_weapon_icon_for(weapon);
	did_update = true;
}

void c_skin_changer::run(int stage) {
	if (stage != 7)
		return;

	const bool skin_enabled  = g_cfg->skin_changer.m_enabled;
	const bool knife_enabled = g_cfg->knife_changer.m_enabled;
	if (!skin_enabled && !knife_enabled) {
		should_update = false;
		return;
	}

	if (!g_ctx->m_local_pawn)
		return;

	auto* local_pawn = reinterpret_cast<c_cs_player_pawn*>(g_ctx->m_local_pawn);
	if (!valid_ptr(local_pawn) || local_pawn->m_health() <= 0)
		return;

	if (local_pawn->m_is_buy_menu_open())
		should_update = true;

	auto* weapon_service = local_pawn->m_weapon_services();
	if (!valid_ptr(weapon_service))
		return;

	auto& my_weapons   = weapon_service->my_weapons();
	auto* entity_system = g_interfaces->m_entity_system;
	const bool force_update = should_update;
	bool did_update = false;

	for (unsigned int i = 0; i < my_weapons.m_size; i++) {
		auto* weapon = reinterpret_cast<c_econ_entity*>(
			entity_system->get_base_entity(my_weapons.m_elements[i].get_entry_index())
		);
		if (!weapon)
			continue;

		auto* identity = weapon->m_entity();
		if (!identity || !identity->is_valid() || !identity->is_safe_to_modify())
			continue;

		auto* attr_mgr = weapon->m_attribute_manager();
		if (!valid_ptr(attr_mgr))
			continue;

		auto* item = attr_mgr->m_item();
		if (!valid_ptr(item))
			continue;

		const uint16_t def_index = item->m_definition_index();
		const bool is_knife = (def_index == WEAPON_KNIFE || def_index == WEAPON_KNIFE_T
		                   || (def_index >= 500 && def_index <= 526));

		if (is_knife && knife_enabled)
			process_knife(weapon, item, local_pawn, force_update, did_update);
		else if (!is_knife && skin_enabled)
			process_weapon(weapon, item, local_pawn, force_update, did_update);
	}

	if (did_update)
		c_hud::regenerate_skins();

	should_update = false;
}
