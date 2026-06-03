CSDialog=CSDialog or {replaceName=nil,replacements={}}
function dialog_set_replace_name(name)CSDialog.replaceName=name end
function cs_dialog_replace(id,text)CSDialog.replacements[id]=text return text end
function cs_dialog_get(id)return CSDialog.replacements[id] end
