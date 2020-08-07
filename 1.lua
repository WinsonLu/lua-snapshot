require("snapshot")

a = snapshot.snapshot()

k = {1,2 ,3 ,4 ,5, 6, 7, 8, ['hello'] = '1238'}

for i =1,10000,1 do
	table.insert(k, i)
end

b = snapshot.snapshot()


c = snapshot.snapshot_added(a, b)
snapshot.snapshot_tofile(c, "1.txt")

snapshot.snapshot_tofile(b, "2.txt")
