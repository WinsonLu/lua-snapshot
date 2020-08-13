
a = {}

snapshot = require "snapshot"
init = snapshot.snapshot(_G, "_G")
i = 0
while true do
	--table.insert(a, "Hello")
	i = i + 1
	collectgarbage("step")
	curr = snapshot.snapshot(_G, "_G")
	diff = snapshot.incr(init, curr)
	if i % 200 == 0 then
		snapshot.print(diff)
	end
end
