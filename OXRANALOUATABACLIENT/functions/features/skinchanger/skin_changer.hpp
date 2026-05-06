#pragma once

#include "../../main.hpp"
#include "../../valve/classes/c_cs_player_pawn.hpp"
#include "../shared/item_schema.hpp"
#include <unordered_map>

class c_skin_changer {
public:
	void run(int stage);
	void initialize();

	bool should_update = false;
	bool is_initialized() const { return m_initialized; }

private:
	bool m_initialized = false;
	uint16_t m_last_knife = 0;
	int m_last_knife_paint_kit_id = 0;
	float m_last_knife_wear = 0.0001f;
	int m_last_knife_seed = 0;

	c_base_entity* get_hud_weapon(c_base_entity* weapon, c_cs_player_pawn* local_pawn);
	void apply_skin(c_econ_entity* weapon, c_econ_item_view* item, int paint_kit_id, float wear, int seed, const char* custom_name, c_cs_player_pawn* local_pawn, uint16_t def_index = 0);
	void process_weapon(c_econ_entity* weapon, c_econ_item_view* item, c_cs_player_pawn* local_pawn, bool force_update, bool& did_update);
	void process_knife(c_econ_entity* weapon, c_econ_item_view* item, c_cs_player_pawn* local_pawn, bool force_update, bool& did_update);
};

inline const auto g_skin_changer = std::make_unique<c_skin_changer>();
