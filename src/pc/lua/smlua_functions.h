#ifndef SMLUA_FUNCTIONS_H
#define SMLUA_FUNCTIONS_H

bool smlua_functions_valid_param_count(lua_State* L, int expected);
bool smlua_functions_valid_param_range(lua_State* L, int min, int max);
void smlua_bind_functions(void);
void smlua_bind_table_functions(void);
#if defined(TARGET_WII_U)
void smlua_bind_wiiu_builtin_helpers(void);
void smlua_bind_wiiu_read_only_constants(void);
#endif

#endif
