charSelectExists=true
charSelectData=charSelectData or {characters={},credits={},voices={},caps={},palettes={},animations={},movesets={},options={},current={[0]=0},menu=false}
charSelect=charSelect or {}
charSelect._data=charSelectData
local d=charSelectData
local function put(t,k,v)if v~=nil then t[k]=v end end
local function get_char(n)return d.characters[n or 0] end
local function add(name,description,credit,color,modelInfo,baseChar,lifeIcon,camScale)
    local id=#d.characters+1
    d.characters[id]={name=name or ("Character "..id),description=description,credit=credit,color=color,model=modelInfo,base=baseChar,lifeIcon=lifeIcon,camScale=camScale,costumes={}}
    return id
end
local function add_costume(charNum,name,description,credit,color,modelInfo,baseChar,lifeIcon,camScale)
    local c=get_char(charNum)
    if not c then return 0 end
    local id=#c.costumes+1
    c.costumes[id]={name=name,description=description,credit=credit,color=color,model=modelInfo,base=baseChar,lifeIcon=lifeIcon,camScale=camScale}
    return id
end
local function edit(charNum,name,description,credit,color,modelInfo,baseChar,lifeIcon,camScale)
    local c=get_char(charNum)
    if not c then return end
    put(c,"name",name)put(c,"description",description)put(c,"credit",credit)put(c,"color",color)put(c,"model",modelInfo)put(c,"base",baseChar)put(c,"lifeIcon",lifeIcon)put(c,"camScale",camScale)
end
local function edit_costume(charNum,charAlt,name,description,credit,color,modelInfo,baseChar,lifeIcon,camScale)
    local c=get_char(charNum)
    local x=c and c.costumes[charAlt or 0]
    if not x then return end
    put(x,"name",name)put(x,"description",description)put(x,"credit",credit)put(x,"color",color)put(x,"model",modelInfo)put(x,"base",baseChar)put(x,"lifeIcon",lifeIcon)put(x,"camScale",camScale)
end
local function key(v)return tostring(v or 0)end
function character_get_number_from_string(name)local n=tostring(name or ""):lower()for i,c in ipairs(d.characters)do if tostring(c.name):lower()==n then return i end end return 0 end
function character_get_number_from_model(model)for i,c in ipairs(d.characters)do if c.model==model then return i end end return 0 end
local function current_number(i)return d.current[i or 0] or 0 end
local function current_table(i)return get_char(current_number(i)) end
local function set_current(n)d.current[0]=n or 0 return d.current[0] end
local function add_voice(model,clips)d.voices[key(model)]=clips return clips end
function character_get_voice(model)return d.voices[key(model)] end
local function add_caps(model,caps)d.caps[key(model)]=caps return caps end
local function get_caps(model)return d.caps[key(model)] end
local function add_palette(model,palette,name)d.palettes[key(model)]={palette=palette,name=name}return palette end
local function add_anims(model,anim,eye,hand)d.animations[key(model)]={anim=anim,eye=eye,hand=hand}return anim end
local function get_anims(model)return d.animations[key(model)] end
local function hook_moveset(charNum,hookEventType,func)local t=d.movesets[charNum]or{}d.movesets[charNum]=t;t[#t+1]={hook=hookEventType,func=func}return #t end
local function get_moveset(charNum)return d.movesets[charNum] end
local function credit_add(modName,creditee,credit)d.credits[#d.credits+1]={mod=modName,name=creditee,credit=credit}return #d.credits end
local function add_option(name,default,max,names,description,save)local id=#d.options+1;d.options[id]={name=name,value=default or 0,max=max or 1,names=names,description=description,save=save}return id end
local function get_option(id)return d.options[id or 0]and d.options[id].value or 0 end
local function set_option(id,v)if d.options[id]then d.options[id].value=v end end
charSelect.character_add=add
charSelect.character_add_costume=add_costume
charSelect.character_edit=edit
charSelect.character_edit_costume=edit_costume
charSelect.character_get_current_number=current_number
charSelect.character_get_current_table=current_table
charSelect.character_get_full_table=function()return d.characters end
charSelect.character_set_current_number=set_current
charSelect.character_get_number_from_string=character_get_number_from_string
charSelect.character_get_number_from_model=character_get_number_from_model
charSelect.character_add_voice=add_voice
charSelect.character_get_voice=character_get_voice
charSelect.character_add_caps=add_caps
charSelect.character_get_caps=get_caps
charSelect.character_add_palette_preset=add_palette
charSelect.character_add_animations=add_anims
charSelect.character_get_animations=get_anims
charSelect.character_hook_moveset=hook_moveset
charSelect.character_get_moveset=get_moveset
charSelect.credit_add=credit_add
charSelect.version_get=function()return 0 end
charSelect.version_get_full=function()return "wiiu-stub" end
charSelect.is_menu_open=function()return d.menu end
charSelect.set_menu_open=function(v)d.menu=not not v end
charSelect.add_option=add_option
charSelect.get_option=get_option
charSelect.set_options_status=set_option
charSelect.get_options_status=get_option
charSelect.voice=charSelect.voice or {}
charSelect.voice.sound=function()return 0 end
charSelect.voice.snore=function()return 0 end
