Arena=Arena or {}
Arena.skybox={enabled=false}
function arena_skybox_set(name)Arena.skybox={enabled=name~=nil,name=name}return Arena.skybox end
