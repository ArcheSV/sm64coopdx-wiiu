-- name: [CS] The Originals
if not charSelectExists then return end
local cs=charSelect
_G.CT_OGS_LUIGI=cs.character_add("VL-Tone Luigi",{"Wii U metadata port"},"VL-Tone",nil,"wiiu-og-luigi")
cs.character_add_costume(_G.CT_OGS_LUIGI,"Cjes Luigi",{"Wii U metadata port"},"Cjes",nil,"wiiu-og-cjes-luigi")
_G.CT_OGS_TOAD=cs.character_add("Djoslin Toad",{"Wii U metadata port"},"Djoslin0",nil,"wiiu-og-toad")
_G.CT_OGS_WARIO=cs.character_add("Fluffa Wario",{"Wii U metadata port"},"FluffaMario",nil,"wiiu-og-wario")
_G.CT_OGS_WALUIGI=cs.character_add("Keeb Waluigi",{"Wii U metadata port"},"Keeberghrh",nil,"wiiu-og-waluigi")
cs.character_add_costume(_G.CT_OGS_WALUIGI,"Fluffa Waluigi",{"Wii U metadata port"},"FluffaMario",nil,"wiiu-og-fluffa-waluigi")
cs.credit_add("[CS] The Originals","Wii U port","Metadata scaffold")
