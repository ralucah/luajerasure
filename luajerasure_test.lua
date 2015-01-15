luajerasure = require "luajerasure"

local file = "127bytes"
local k = 7
local m = 7
local w = 8

function readFile(file)
    local f = io.open(file, "rb")
    local content = f:read("*all")
    f:close()
    return content, #content -- \0 is ignored
end

function trimEncodedDevices(encoded, numToTrim)
    local trimmed = {}
    local dataDeviceSize
    local indices = {}

    while numToTrim > 0 do
        local index
        repeat
            index = math.random(1, #encoded)
        until indices[index] == nil

        indices[index] = 1
        numToTrim = numToTrim - 1
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
    print("Decoded content matches original content")
end

local content, size = readFile(file)
local encoded = {luajerasure.encode(k, m, w, size, content)}

local trimmed, dataDeviceSize = trimEncodedDevices(encoded, m)
local decoded = luajerasure.decode(k, m, w, dataDeviceSize, trimmed)

compareContent(content, decoded)