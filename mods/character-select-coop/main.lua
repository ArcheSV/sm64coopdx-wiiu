-- name: Character Select
-- pausable: false
CS_WIIU=true
charSelectExists=true
charSelectData=charSelectData or {characters={},credits={},voices={},caps={},palettes={},animations={},movesets={},options={},current={[0]=0},menu=false}
charSelect=charSelect or {}
charSelect._data=charSelectData
charSelect.version="wiiu-stub"
local f=function(...)return 0 end
local n=function(...)end
charSelectExists=true
charSelect.voice=charSelect.voice or {sound=f,snore=f}
charSelect.character_add=charSelect.character_add or f
charSelect.character_add_costume=charSelect.character_add_costume or f
charSelect.character_edit=charSelect.character_edit or n
charSelect.character_edit_costume=charSelect.character_edit_costume or n
charSelect.character_add_caps=charSelect.character_add_caps or n
charSelect.character_add_voice=charSelect.character_add_voice or n
charSelect.character_add_palette_preset=charSelect.character_add_palette_preset or n
charSelect.character_add_animations=charSelect.character_add_animations or n
charSelect.character_hook_moveset=charSelect.character_hook_moveset or n
charSelect.credit_add=charSelect.credit_add or n
charSelect.character_get_current_number=charSelect.character_get_current_number or f
charSelect.character_get_number_from_model=charSelect.character_get_number_from_model or f
charSelect.character_get_voice=charSelect.character_get_voice or n
