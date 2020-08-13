snapshot = require "snapshot"


tbl1 = {1, 2, 3, 4}
tbl2 = {
	['A'] = {1, 2, 3, 4, 5, 6},
	['B'] = {},
}
S1 = snapshot.snapshot(_G, "_G")

tbl1[#tbl1] = nil
tbl2['A'][#tbl2['A']] = nil
tbl2['B'] = nil

S2 = snapshot.snapshot(_G, "_G")

decrement = snapshot.decr(S1, S2)
snapshot.print(decrement)
