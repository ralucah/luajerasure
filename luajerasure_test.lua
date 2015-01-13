require "luajerasure"

function readAll(file)
    local f = io.open(file, "rb")
    local content = f:read("*all")
    --local size = f:seek("end",0) -- in bytes
    f:close()
    return content
end

local content = readAll("127bytes")
-- in lua, it is safe to get length like this even if the file might contain \0
print(content)

local k = 7
local m = 7
local w = 8
local res = {encode(k,m,w, #content, content)} 

--print(#tostring(k+m))
print(res)

local data_device_size
for key,val in pairs(res) do
    io.write(key..": "..#val.." ")
    io.write("\n")
    data_device_size = (#val - 5) / (w/8)
end


local res_incomplete = {}
for key,val in pairs(res) do
    local deviceIndex = tonumber(val:sub(1,5))
    --io.write(key.." "..deviceIndex.."\n")
    if deviceIndex % 2 == 0 then
        --io.write("keep line "..deviceIndex.." \n")
        res_incomplete[#res_incomplete + 1] = val
    end
end
print(data_device_size)

decode(k, m, w, data_device_size, res_incomplete)

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