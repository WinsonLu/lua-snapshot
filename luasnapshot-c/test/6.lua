snapshot = require "snapshot"

tbl = {
	['A'] = {1, 2, 3, 4, 5, 6},
	['B'] = {},
}

S1 = snapshot.snapshot(tbl, "tbl")

print("\t\t\t================ S1 has not been freed ================")
snapshot.print(S1)

snapshot.free(S1)

print("\t\t\t================ S1 has been freed ==================")
snapshot.print(S1)


