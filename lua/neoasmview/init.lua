local M = {}

local build = require("neoasmview.build")
M.server = require("neoasmview.server")

function M.setup()
  M.server.setup()
end

return M
