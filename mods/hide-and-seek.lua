-- name: Hide and Seek

local e = false
local function c()
    e = not e
    return true
end
hook_chat_command("has", "toggle", c)
