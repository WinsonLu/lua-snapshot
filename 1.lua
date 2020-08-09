require("snapshot")

list = {"Hello", 2, "good", "bye"}
a = snapshot.snapshot(_G, "_G")
table.insert(list, "G")
table.insert(list, "B")
list2 = {"Ok"}
list2['hello'] = {1, 2, 3}
func = function()
	print("hello world")
end
b = snapshot.snapshot(_G, "_G")

c = snapshot.incr(a, b)
snapshot.to_jsonfile(c, "incr.txt")
snapshot.print(c)
snapshot.to_file(c, "incr.txt2")

i = 0
--[[
while true do
	i = i + 1
	print(i)
	k = snapshot.snapshot()
	snapshot.free(k)
end

]]--
