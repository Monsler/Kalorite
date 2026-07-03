#pragma once

struct lua_State;

namespace Kalorite {

    class PluginManager;

    // Installs the global `kalorite` table (player / mixer / playlist / net /
    // ui / storage / log sub-tables) into the given lua_State. The owning
    // PluginManager pointer is stashed in the Lua registry so every C binding
    // can reach the player and network stack.
    //
    // Trusted model: the standard Lua/LuaJIT libraries (io, os, ffi, require,
    // ...) remain fully available to plugins in addition to this API.
    namespace LuaApi {
        void install(lua_State* L, PluginManager* manager);

        // Retrieve the PluginManager stashed for this state (never null once
        // install() has run).
        PluginManager* manager(lua_State* L);
    }

} // namespace Kalorite
