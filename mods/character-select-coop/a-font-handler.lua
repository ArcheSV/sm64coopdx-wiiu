CSFont={current=0,fonts={}}
function djui_hud_add_font(texture,info,spacing,offset,backup,scale)local id=#CSFont.fonts+1;CSFont.fonts[id]={texture=texture,info=info,spacing=spacing,offset=offset,backup=backup,scale=scale}return id end
function djui_hud_set_font(fontType)CSFont.current=fontType or 0 end
function djui_hud_effect_shake()return 0 end
function djui_hud_effect_wave()return 0 end
function djui_hud_print_text(message)return tostring(message or "") end
function djui_hud_print_text_interpolated(message)return tostring(message or "") end
function djui_hud_measure_text(message)return #tostring(message or "")*8 end
