local M = {}

local build = require("vimasm.build")
M.commands = require("vimasm.commands")

M.root_dir = ""

function M.setup()
  M.commands.setup()
  M.root = vim.fn.getcwd()
  print("[vimasm]", M.root)
end

return M
