--- Give a module the ability to load itself into the global namespace.
-- @module globalize

--- @usage
local usage = [[
-- mymodule.lua:
local globalize = require 'globalize'

local M = {}

...

globalize(M)

return M

...

require 'mymodule'() -- This loads all of the symbols in 'mymodule' into the global namespace.
]]

local mt = {}

function mt:__call()
    for k,v in pairs(self) do
        _G[k] = v
    end
end

return setmetatable({}, {
    __call = function(self, module)
        setmetatable(module, mt)
    end
})
