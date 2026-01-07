local M = {}

local build = require("neoasmview.build")
M.commands = require("neoasmview.commands")

function M.setup()
  M.commands.setup()
end

return M
