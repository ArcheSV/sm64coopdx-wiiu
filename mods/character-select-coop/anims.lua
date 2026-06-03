CSAnims=CSAnims or {}
function cs_anim_add(name,data)CSAnims[name]=data return data end
function cs_anim_get(name)return CSAnims[name] end
