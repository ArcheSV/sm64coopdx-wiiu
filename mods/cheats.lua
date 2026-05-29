-- name: Cheats
-- incompatible: cheats

hook_mod_menu_checkbox("Moon Jump", false, wiiu_set_cheat)
hook_mod_menu_checkbox("God Mode", false, wiiu_set_cheat)
hook_mod_menu_checkbox("Infinite Lives", false, wiiu_set_cheat)
hook_mod_menu_checkbox("Super Speed", false, wiiu_set_cheat)
hook_mod_menu_checkbox("Responsive Controls", false, wiiu_set_cheat)
hook_mod_menu_checkbox("Rapid Fire", false, wiiu_set_cheat)
hook_mod_menu_checkbox("BLJ Anywhere", false, wiiu_set_cheat)
hook_mod_menu_checkbox("Always Triple Jump", false, wiiu_set_cheat)
hook_event(HOOK_UPDATE, wiiu_cheats_tick)
