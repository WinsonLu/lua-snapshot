require ("snapshot")
list = {1, 2, 3, 4}
a = snapshot.snapshot()

list[#list] = nil

b = snapshot.snapshot()


snapshot.print(a)
print("--------------------")
snapshot.print(b)
print("--------------------")
c = snapshot.decr(a, b)
snapshot.print(c)

