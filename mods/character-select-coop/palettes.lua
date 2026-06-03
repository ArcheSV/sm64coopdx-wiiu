CSPalettes=CSPalettes or {}
function update_preset_palette(np)return np end
function cs_palette_add(name,palette)CSPalettes[name]=palette return palette end
function cs_palette_get(name)return CSPalettes[name] end
