require ("snapshot")

local S1 = snapshot.snapshot(_G, "_G")

snapshot = require "snapshot"
tmp = {
    player = {
        uid = 1,
        camps = {
            {campid = 1},
            {campid = 2},
        },
    },
    player2 = {
        roleid = 2,
    },
    [3] = {
        player1 = 1,
    },
}

a = {}
c = {}
a.b = c
c.d = a

msg = "bar"
foo = function()
    print(msg)
end

co = coroutine.create(function ()
    print("hello world")
end)

local S2 = snapshot.snapshot(_G, "_G")
local SD = snapshot.incr(S1, S2)

snapshot.print(SD)



snapshot = require "snapshot"

registry_snapshot = snapshot.snapshot() --返回记录registry表内存快照的snapshot对象
_G_snapshot = snapshot.snapshot(_G, "_G") --返回记录_G表内存快照的snapshot对象，
										  --  并将根表的名称设置为 "_G"

