function rank_str(r)return r==1 and "1st" or r==2 and "2nd" or r==3 and "3rd" or tostring(r).."th" end
function rank_color_g(r)return clamp and clamp(255-32*(r or 0),0,255)or 255 end
function update_ranking_descriptions()return Arena and Arena.players or {} end
function render_game_mode()return Arena and Arena.mode or 0 end
function render_single_team_score(team)return calculate_team_score and calculate_team_score(team)or 0 end
function render_team_score()return {render_single_team_score(1),render_single_team_score(2)} end
function render_local_rank()return Arena and Arena.players and Arena.players[0] and Arena.players[0].rank or 0 end
function render_server_message()end
function render_health()end
function render_hud_icon()end
function on_hud_render()return render_team_score()end
