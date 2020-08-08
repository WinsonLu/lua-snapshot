require("snapshot")

list = {"Hello", 2, "good", "bye"}
a = snapshot.snapshot()
table.insert(list, "G")
table.insert(list, "B")
b = snapshot.snapshot()

c = snapshot.incr(a, b)
snapshot.tofilefmt(c, "incr.txt")

i = 0
while true do
	i = i + 1
	print(i)
	k = snapshot.snapshot()
	snapshot.free(k)
end

