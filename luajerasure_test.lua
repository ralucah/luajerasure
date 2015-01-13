require "luajerasure"

function readAll(file)
    local f = io.open(file, "rb")
    local content = f:read("*all")
    f:close()
    return content
end

local content = readAll("127bytes")
-- in lua, it is safe to get length like this even if the file might contain \0
--print(#content)

local k = 7
local m = 7
local w = 8
local res = {encode(k,m,w, #content, content)} 

--print(#tostring(k+m))
--print(res)

local data_device_size
local res_incomplete = {}
for key,val in pairs(res) do
    if key % 2 ~= 0 then
        res_incomplete[#res_incomplete + 1] = val
    end
    data_device_size = (#val - 4) / (w/8)
end
--print(data_device_size)

resDec = decode(k, m, w, data_device_size, res_incomplete)

print(content)
print("\n")
print(resDec)

for i=1,#content do
    local c = content:sub(i,i)
    io.write(string.byte(c).." ")
end
io.write("\n")
for i=1,#resDec do
    local c = resDec:sub(i,i)
    if i > #content then
        break
    end
    io.write(string.byte(c).." ")
end

--[[num = 5
t1 = {1, 2, 3}
t2 = {4, 5, 6}
t = {t1, t2}
x = {1,2,3,4,5}]]--
--test(num, t)

--[[
function pack(...) 
      return { ... }, select("#", ...) 
    end 

function f()
  return "a","b"
end

function g()
  return 1,"c",{}
end


print( f() , select("#",f()) ) --f() returns 2 results
print( g() , select("#",g()) ) --g() returns 3 results

t = pack(f())
print(#t, t[1], t[2])
]]--