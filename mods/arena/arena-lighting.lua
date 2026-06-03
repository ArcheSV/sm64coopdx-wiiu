Arena=Arena or {}
Arena.lighting={r=255,g=255,b=255}
function arena_lighting_set(r,g,b)Arena.lighting={r=r or 255,g=g or 255,b=b or 255}return Arena.lighting end
function arena_lighting_get()return Arena.lighting end
