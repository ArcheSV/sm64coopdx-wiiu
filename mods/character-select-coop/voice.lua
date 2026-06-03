CSVoices=CSVoices or {}
function custom_character_sound()return 0 end
function custom_character_snore()return 0 end
function config_character_sounds()return CSVoices end
function cs_voice_add(name,clips)CSVoices[name]=clips return clips end
function cs_voice_get(name)return CSVoices[name] end
charSelect=charSelect or {}
charSelect.voice=charSelect.voice or {}
charSelect.voice.sound=custom_character_sound
charSelect.voice.snore=custom_character_snore
