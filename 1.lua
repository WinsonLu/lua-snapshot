require("snapshot")

m = snapshot.snapshot()

local tmp = {
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

local a = {}
local c = {}
a.b = c
c.d = a

local msg = "bar"
local foo = function()
    print(msg)
end

local co = coroutine.create(function ()
    print("hello world")
end)

n = snapshot.snapshot()

diff = snapshot.snapshot_added(m, n)
decreased = snapshot.snapshot_decreased(n, m)
snapshot.snapshot_tofile(diff, "1.txt")
--snapshot.snapshot_tofile(m, "2.txt")
--snapshot.snapshot_tofile(n, "3.txt")
snapshot.snapshot_tofile(decreased, "2.txt")
