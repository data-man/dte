local char, concat = string.char, table.concat
local Indent = {}

function Indent:__index(depth)
    assert(depth < 12)
    local indent = self[1]:rep(depth)
    self[depth] = indent
    return indent
end

function Indent.new(width)
    local i1 = (" "):rep(width or 4)
    return setmetatable({[0] = "", [1] = i1}, Indent)
end

local function make_trie(keys)
    local trie = {}
    for key, value in pairs(keys) do
        local key_length = #key
        local node = trie[key_length]
        if not node then
            node = {type = "branch", child_count = 0}
            trie[key_length] = node
        end

        for i = 1, key_length do
            local byte = assert(key:sub(i, i):byte())
            local min, max = node.min, node.max
            if not min or byte < min then
                node.min = byte
            end
            if not max or byte > max then
                node.max = byte
            end

            if i == key_length then
                assert(node[byte] == nil)
                node[byte] = {type = "leaf", key = key, value = value}
                node.child_count = node.child_count + 1
            else
                if not node[byte] then
                    node[byte] = {type = "branch", child_count = 0}
                    node.child_count = node.child_count + 1
                end
                node = node[byte]
            end
        end
    end
    return trie
end

local function mark_leaf_nodes_compressed(node)
    for i = node.min, node.max do
        local child = node[i]
        if child then
            if child.type == "leaf" then
                child.compressed = true
            else
                assert(child.type == "branch")
                mark_leaf_nodes_compressed(child)
            end
        end
    end
end

local function compress(node)
    local skip = 0
    while node.type == "branch" do
        if node.child_count > 1 then
            if skip > 1 then
                mark_leaf_nodes_compressed(node)
                return skip, node
            end
            return false
        end
        skip = skip + 1
        node = node[node.min]
    end
    assert(node.type == "leaf")
    return skip, node
end

local function make_index(keys)
    local index, n, longest = {}, 0, 0
    for k in pairs(keys) do
        assert(k:match("^[#-~]+$"), k)
        n = n + 1
        index[n] = k
        local klen = #k
        if klen > longest then
            longest = klen
        end
    end
    table.sort(index)
    for i = 1, n do
        local key = index[i]
        index[key] = i
    end
    index.n = n
    index.longest_key = longest
    return index
end

local function write_index(index, buf, defs)
    local key_type
    if index.longest_key <= 8 then
        key_type = "const char key[8]"
    elseif index.longest_key <= 16 then
        key_type = "const char key[16]"
    else
        key_type = "const char *const key"
    end
    local n = #index
    buf:write (
        "static const struct {\n",
        "    ", key_type, ";\n",
        "    const ", defs.return_type, " val;\n",
        "} ", defs.function_name, "_table[", tostring(n), "] = {\n"
    )
    for i = 1, n do
        local k = index[i]
        buf:write('    {"', k, '", ', defs.keys[k], "},\n")
    end
    buf:write("};\n\n")
end

local function cmp(node, index, buf, last_char_only)
    assert(node.type == "leaf")
    local macro = last_char_only and "CMPN" or "CMP"
    local key = node.key
    buf:write(" ", macro, "(", index[key] - 1, "); // ", key, "\n")
end

local function write_trie(node, buf, defs, index)
    local default_return = defs.default_return
    local indents = Indent.new(4)
    local function serialize(node, level, indent_depth)
        local indent = indents[indent_depth]
        if node.type == "branch" then
            local skip, next_node = compress(node)
            if skip then
                if next_node.type == "branch" then
                    node = next_node
                    level = level + skip
                else
                    local last_only = (skip == 1 and not next_node.compressed)
                    cmp(next_node, index, buf, last_only)
                    return
                end
            end

            buf:write("\n", indent, "switch (s[", tostring(level), "]) {\n")
            for i = node.min, node.max do
                local v = node[i]
                if v then
                    buf:write(indent, "case '", char(i), "':")
                    serialize(v, level + 1, indent_depth + 1)
                end
            end
            buf:write(indent, "}\n", indent, "break;\n")
        else
            assert(node.type == "leaf")
            if node.compressed then
                cmp(node, index, buf)
            else
                buf:write(" return ", node.value, ";\n")
            end
        end
    end

    local indent = indents[1]
    buf:write(indent, "switch (len) {\n")
    for i = 1, index.longest_key do
        local v = node[i]
        if v then
            buf:write(indent, "case ", tostring(i), ":")
            serialize(v, 0, 2)
        end
    end
    buf:write(indent, "}\n", indent, "return ", default_return, ";\n")
end

local function expand_template(template, context)
    return (template:gsub('$(%b{})', function(match)
        local key = assert(match:sub(2, -2))
        return context[key] or error("Missing template field: " .. key, 4)
    end))
end

local input_filename = assert(arg[1], "Usage: " .. arg[0] .. " INPUT-FILE")
local output_filename, output = arg[2]
if output_filename then
    output = assert(io.open(output_filename, "w"))
    output:setvbuf("full")
else
    output = assert(io.stdout)
end

local fn = assert(loadfile(input_filename, "t", {}))
local defs = assert(fn())
local keys = assert(defs.keys, "No keys defined")
assert(defs.function_name, "No function_name defined")
assert(defs.return_type, "No return_type defined")
assert(defs.default_return, "No default_return defined")
local index = assert(make_index(keys))
local trie = assert(make_trie(keys))

local prelude = [[
#define CMP(i) idx = i; goto compare
#define CMPN(i) idx = i; goto compare_last_char
#define KEY ${function_name}_table[idx].key
#define VAL ${function_name}_table[idx].val

static ${return_type} ${function_name}(const char *s, size_t len)
{
    size_t idx;
]]

local postlude = [[
compare:
    return (memcmp(s, KEY, len) == 0) ? VAL : ${default_return};
compare_last_char:
    return (s[len - 1] == KEY[len - 1]) ? VAL : ${default_return};
}

#undef CMP
#undef CMPN
#undef KEY
#undef VAL
]]

write_index(index, output, defs)
output:write(expand_template(prelude, defs))
write_trie(trie, output, defs, index)
output:write(expand_template(postlude, defs))
output:flush()
