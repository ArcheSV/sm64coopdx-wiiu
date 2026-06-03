CSMovesets=CSMovesets or {}
function cs_moveset_add(charNum,name)CSMovesets[charNum]=CSMovesets[charNum]or{};CSMovesets[charNum][#CSMovesets[charNum]+1]=name return #CSMovesets[charNum] end
function cs_moveset_get(charNum)return CSMovesets[charNum] end
function cs_moveset_is_restricted()return false end
