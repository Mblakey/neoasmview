local M = {}

local build = require("neoasmview.build")
M.commands = require("neoasmview.commands")

M.root_dir = ""

function M.setup()
  M.commands.setup()
  M.root = vim.fn.getcwd()
  print("[vimasm]", M.root)
end

return M
