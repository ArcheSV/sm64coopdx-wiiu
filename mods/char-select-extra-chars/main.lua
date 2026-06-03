-- name: [CS] Extra Characters
if not charSelectExists then return end
local cs=charSelect
local chars={"Toadette","Peach","Daisy","Yoshi","Birdo","Spike","Pauline","Rosalina"}
_G.CS_EXTRA_WIIU={}
for i,n in ipairs(chars)do
    _G.CS_EXTRA_WIIU[n]=cs.character_add(n,{"Wii U metadata port","Models and movesets disabled for now"},"Extra Characters",nil,"wiiu-extra-"..i,nil,nil,1)
end
cs.credit_add("[CS] Extra Characters","FunkyLion","Original pack")
cs.credit_add("[CS] Extra Characters","Wii U port","Metadata scaffold")
