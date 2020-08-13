snapshot = require("snapshot")

tbl = {
	["A"] = {},
	["B"] = {
		["BB"] = {}
	},
}

a = snapshot.snapshot()
--snapshot.print(a)
tbl["B"]["BB"] = nil

b = snapshot.snapshot()
--snapshot.print(b)

increment = snapshot.decr(a, b)

snapshot.print(increment)
