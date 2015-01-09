require "luajerasure"

local res = {encode(7,7,8)} -- should also take data to encode

--[[for k,v in pairs(res) do
  --print(k)
  for i,j in pairs(v) do
    if i == 1 then
        io.write(j.." ")
    else
        io.write(string.format("%02x ", j))
    end
  end
  print("")
end]]--

local res_incomplete = {}
for k,v in pairs(res) do
        for i,j in pairs(v) do
            if i == 1 then
                if j % 2 == 0 then
                    --print("keep line "..j)
                    res_incomplete[#res_incomplete + 1] = v
                end
            end
        end
end

--[[for k,v in pairs(res_incomplete) do
  --print(k)
  for i,j in pairs(v) do
    if i == 1 then
        io.write(j.." ")
    else
        io.write(string.format("%02x ", j))
    end
  end
  print("")
end]]--

--decode(7,7,8,res)
decode(7,7,8,res_incomplete)

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
