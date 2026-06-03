Arena=Arena or {}
Arena.music=nil
function music()return Arena.music end
function handleMusic()return music() end
function hud_render()return Arena.music end
