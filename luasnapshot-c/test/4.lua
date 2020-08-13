snapshot = require "snapshot"

S1 = snapshot.snapshot(_G, "_G")

tbl = {
	["a"] = {1, 2, 3},
	["c"] = "this is a string in table",
	["d"] = {
		["aa"] =  {},
		["ab"] = "this is a string in a table in table"
	}
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

S2 = snapshot.snapshot(_G, "_G")

increment = snapshot.incr(S1, S2)
snapshot.print(increment)

