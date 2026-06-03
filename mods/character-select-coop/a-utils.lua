function string_underscore_to_space(s)return tostring(s or ""):gsub("_"," ") end
function string_space_to_underscore(s)return tostring(s or ""):gsub(" ","_") end
function string_split(s,sep)local t={}sep=sep or " "for p in tostring(s or ""):gmatch("[^%"..sep.."]+")do t[#t+1]=p end return t end
function switch(p,c)return c and (c[p]or c.default) end
function define_valid_global(s,v)if not s then return v end if _G[s]==nil then _G[s]=v end return _G[s] end
function num_power_of_two(n)local p=1 while p<n do p=p*2 end return p end
function angle_from_2d_points()return 0 end
function hash(w)local h=0 for i=1,#tostring(w or "")do h=(h*31+tostring(w):byte(i))%2147483647 end return h end
function lerp(a,b,t)return a+(b-a)*(t or 0) end
function num_wrap(n,a,b)if n<a then return b elseif n>b then return a end return n end
function startup_init_stall()return true end
function is_mario_in_vanilla_action()return true end
function log_to_console_once()end
function is_power_of_two(n)n=tonumber(n)or 0 if n<1 then return false end while n%2==0 do n=n/2 end return n==1 end
function is_texture_valid(tex)return tex~=nil end
function run_func_or_get_var(x,...)if type(x)=="function"then return x(...)end return x end
function mirror_mode_number(x)return x end
function string_sim(a,b)return a==b and 1 or 0 end
