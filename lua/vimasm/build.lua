local M = {}

local function run_cmd(cmd)
    local ok = vim.fn.system(cmd)
    local code = vim.v.shell_error
    if code ~= 0 then
        vim.notify("[vimasm] Build failed: " .. table.concat(cmd, " "), vim.log.levels.ERROR)
    end
    return code == 0
end

function M.build()
    local cwd = "/home/mikey/Workspace/vimasm"
    local build_dir = cwd .. "/build"

    vim.fn.mkdir(build_dir, "p")

    run_cmd({ "cmake", "-S", cwd, "-B", build_dir })
    run_cmd({ "cmake", "--build", build_dir })
end

return M
