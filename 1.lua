require("snapshot")

list = {"Hello", 2, "good", "bye"}
a = snapshot.snapshot()
table.insert(list, "G")
table.insert(list, "B")
b = snapshot.snapshot()

c = snapshot.incr(a, b)
snapshot.tofilefmt(c, "incr.txt")
