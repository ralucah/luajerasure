socket = require "socket" -- to get time in ms

luajerasure = require "luajerasure"

if #arg < 4 then
    print("How to run: "..arg[0].." k m w file")
    print("* k = number of data devices")
    print("* m = number of coding devices")
    print("* w = word size; 8, 16, or 32")
    print("* file = name of file to be encoded/decoded")
    print("* k + m must be <= 2^w")
    os.exit()
end

local k = tonumber(arg[1])
local m = tonumber(arg[2])
local w = tonumber(arg[3])
local file = arg[4]

function readFile(file)
    local f = io.open(file, "rb")
    local content = f:read("*all")
    f:close()
    return content, #content -- \0 is ignored
end

function trimEncodedDevices(encoded, numToKeep)
    local trimmed = {}
    local dataDeviceSize
    local indices = {}

    while numToKeep > 0 do
        local index
        repeat
            index = math.random(1, #encoded)
        until indices[index] == nil

        indices[index] = 1
        numToKeep = numToKeep - 1
    end
    for index,_ in pairs(indices) do
        trimmed[#trimmed + 1] = encoded[index]
    end
    dataDeviceSize = (#trimmed[1] - 4) / (w/8)

    return trimmed, dataDeviceSize
end

function printContent(content)
    for i=1,#content do
        local c = content:sub(i,i)
        io.write(string.byte(c).." ")
    end
    io.write("\n")
end

function compareContent(content1, content2)
    for i=1,#content1 do
        local c1 = content1:sub(i,i)
        local c2 = content2:sub(i,i)
        if string.byte(c1) ~= string.byte(c2) then
            print("different content!")
            os.exit()
        end
    end
    --print("Decoded content matches original content")
end

--print("k:"..k.." m:"..m.." w:"..w.." file:"..file)
local content, size = readFile(file)
local t1, t2
t1  = socket.gettime()*1000
local encoded = {luajerasure.encode(k, m, w, size, content)}
t2 = socket.gettime()*1000
--print("encode: "..k.." "..m.." "..w.." "..file.." "..string.format("%.2f", (t2-t1)).."ms")
--print(string.format("%.2f", (t2-t1)))

local trimmed, dataDeviceSize = trimEncodedDevices(encoded, k)

t1  = socket.gettime()*1000
local decoded = luajerasure.decode(k, m, w, dataDeviceSize, trimmed)
t2 = socket.gettime()*1000
--print("decode: "..k.." "..m.." "..w.." "..file.." "..string.format("%.2f", (t2-t1)).."ms")
print(string.format("%.2f", (t2-t1)))

compareContent(content, decoded)s