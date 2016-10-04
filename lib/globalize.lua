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
