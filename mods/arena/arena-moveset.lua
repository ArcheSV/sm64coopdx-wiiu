Arena=Arena or {}
Arena.moveset={jumpLeniency=0,crouchLeniency=0}
function jump_leniency()return Arena.moveset.jumpLeniency end
function crouch_leniency()return Arena.moveset.crouchLeniency end
function arena_set_jump_leniency(v)Arena.moveset.jumpLeniency=tonumber(v)or 0 end
function arena_set_crouch_leniency(v)Arena.moveset.crouchLeniency=tonumber(v)or 0 end
